#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "iptv/common/concurrent_queue.hpp"
#include "iptv/network/network_component.hpp"
#include "iptv/player/playback_orchestrator.hpp"

using iptv::ConcurrentQueue;
using namespace iptv::network;
using namespace iptv::player;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace {

constexpr std::string_view kBaseUrl = "http://chaos.invalid/media";
constexpr std::size_t kSpinBudget = 4'000'000;

constexpr std::array<std::uint32_t, 8> kVariantBandwidths{
    500'000, 1'000'000, 2'000'000, 4'000'000, 8'000'000, 12'000'000, 20'000'000, 35'000'000};
constexpr std::array<std::array<std::uint32_t, 2>, 8> kVariantResolutions{{{320, 180},
                                                                           {640, 360},
                                                                           {854, 480},
                                                                           {1280, 720},
                                                                           {1920, 1080},
                                                                           {2560, 1440},
                                                                           {3840, 2160},
                                                                           {3840, 2160}}};

constexpr std::size_t kSegmentPayloadBytes = 15'000;
constexpr std::int64_t kSegmentMinDurationUs = 98'000;
constexpr std::int64_t kSegmentMaxDurationUs = 102'000;
constexpr std::size_t kForcedChaosOrdinal = 12;
constexpr int kChaosPercent = 15;

enum class Outcome { Ok, InternalError, Unavailable, GatewayTimeout, Timeout, CorruptBody };

struct ChaosConfig {
  std::uint32_t seed{0};
  int variant_count{static_cast<int>(kVariantBandwidths.size())};
  int segment_count{48};
  bool inject_chaos{false};
};

struct Request {
  enum class Kind { Master, VariantPlaylist, Segment, Unknown };
  Kind kind{Kind::Unknown};
  int variant{-1};
  int segment{-1};
};

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= static_cast<std::uint64_t>(data[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

int parseDigitsEndingAt(std::string_view url, std::size_t end_pos) {
  std::size_t begin = end_pos;
  while (begin > 0 && url[begin - 1] >= '0' && url[begin - 1] <= '9') {
    --begin;
  }
  if (begin == end_pos) {
    return -1;
  }
  int value = 0;
  const auto result = std::from_chars(url.data() + begin, url.data() + end_pos, value);
  return result.ec == std::errc{} ? value : -1;
}

class ChaosTransport {
 public:
  explicit ChaosTransport(const ChaosConfig& config)
      : config_(config),
        manifest_(buildMasterManifest()),
        segment_checksums_(static_cast<std::size_t>(config.segment_count), 0) {}

  HttpResponse respond(const std::string& url, const std::stop_token& stop_token) {
    const Request request = classify(url);
    const Outcome outcome = plan(request);

    std::string body;
    std::int64_t duration_us = 0;
    {
      std::lock_guard<std::mutex> guard(mutex_);
      countOutcome(outcome);

      switch (request.kind) {
        case Request::Kind::Master:
          master_requests_.fetch_add(1, std::memory_order_relaxed);
          break;
        case Request::Kind::VariantPlaylist:
          body = buildMediaManifest(clampVariant(request.variant));
          break;
        case Request::Kind::Segment:
          segment_requests_.fetch_add(1, std::memory_order_relaxed);
          if (request.variant >= 0) {
            highest_variant_served_.store(request.variant, std::memory_order_relaxed);
          }
          break;
        case Request::Kind::Unknown:
          break;
      }

      switch (request.kind) {
        case Request::Kind::Master:
          body = manifest_;
          break;
        case Request::Kind::VariantPlaylist:
          body = buildMediaManifest(clampVariant(request.variant));
          break;
        case Request::Kind::Segment:
          body = buildSegmentPayload(request.segment);
          if (outcome == Outcome::CorruptBody) {
            body.assign(body.size(), '!');
          }
          break;
        case Request::Kind::Unknown:
          body = "#EXTM3U\n";
          break;
      }

      duration_us = request.kind == Request::Kind::Segment ? nextSegmentDurationUs() : 100'000;

      if (request.kind == Request::Kind::Segment) {
        if (isErrorOutcome(outcome)) {
          signalTerminal();
        } else {
          successful_segment_serves_.fetch_add(1, std::memory_order_relaxed);
          if (request.segment == config_.segment_count - 1) {
            signalTerminal();
          }
        }
      } else if (request.kind == Request::Kind::VariantPlaylist) {
        if (outcome != Outcome::Ok) {
          signalTerminal();
        }
      }
    }

    switch (outcome) {
      case Outcome::InternalError:
        return HttpResponse(HttpStatusCode::InternalServerError);
      case Outcome::Unavailable:
        return HttpResponse(HttpStatusCode::ServiceUnavailable);
      case Outcome::GatewayTimeout:
        return HttpResponse(HttpStatusCode::GatewayTimeout);
      case Outcome::Timeout:
        return HttpResponse(HttpStatusCode::Unknown);
      case Outcome::Ok:
      case Outcome::CorruptBody:
        break;
    }

    if (stop_token.stop_requested()) {
      return HttpResponse(HttpStatusCode::Unknown);
    }

    HttpResponse response(HttpStatusCode::Ok, body.size());
    response.appendToBody(reinterpret_cast<const std::uint8_t*>(body.data()), body.size());

    NetworkMetrics& metrics = response.getMetricsRef();
    metrics.bytes_downloaded = response.getBytesDownloaded();
    metrics.total_duration = std::chrono::microseconds(duration_us);
    metrics.ttfb = std::chrono::microseconds(20'000);
    return response;
  }

  bool waitUntilTerminal() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return terminal_; });
    return true;
  }

  std::size_t totalRequests() const noexcept {
    return total_requests_.load(std::memory_order_relaxed);
  }
  std::size_t masterRequests() const noexcept {
    return master_requests_.load(std::memory_order_relaxed);
  }
  std::size_t segmentRequests() const noexcept {
    return segment_requests_.load(std::memory_order_relaxed);
  }
  std::size_t successfulSegmentServes() const noexcept {
    return successful_segment_serves_.load(std::memory_order_relaxed);
  }
  std::size_t chaosRequests() const noexcept {
    return chaos_requests_.load(std::memory_order_relaxed);
  }
  std::size_t transportFailures() const noexcept {
    return transport_failures_.load(std::memory_order_relaxed);
  }
  int highestVariantServed() const noexcept {
    return highest_variant_served_.load(std::memory_order_relaxed);
  }
  std::uint64_t expectedChecksum(int segment_index) const {
    if (segment_index < 0 || static_cast<std::size_t>(segment_index) >= segment_checksums_.size()) {
      return 0;
    }
    return segment_checksums_[static_cast<std::size_t>(segment_index)];
  }

 private:
  int clampVariant(int variant) const {
    if (variant < 0 || variant >= config_.variant_count) {
      return 0;
    }
    return variant;
  }

  void signalTerminal() {
    terminal_ = true;
    cv_.notify_all();
  }

  void countOutcome(Outcome outcome) {
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    if (outcome != Outcome::Ok) {
      chaos_requests_.fetch_add(1, std::memory_order_relaxed);
    }
    if (isErrorOutcome(outcome)) {
      transport_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  static bool isErrorOutcome(Outcome outcome) {
    return outcome != Outcome::Ok && outcome != Outcome::CorruptBody;
  }

  Request classify(std::string_view url) const {
    if (url.ends_with("/master.m3u8")) {
      return {Request::Kind::Master, -1, -1};
    }
    if (url.ends_with(".ts")) {
      const std::size_t slash = url.rfind('/');
      const std::size_t dir_end = slash == std::string_view::npos ? 0 : slash;
      return {Request::Kind::Segment, parseDigitsEndingAt(url, dir_end),
              parseDigitsEndingAt(url, url.size() - 3)};
    }
    if (url.ends_with(".m3u8")) {
      return {Request::Kind::VariantPlaylist, parseDigitsEndingAt(url, url.size() - 5), -1};
    }
    return {Request::Kind::Unknown, -1, -1};
  }

  Outcome plan(const Request& request) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!config_.inject_chaos || request.kind == Request::Kind::Master) {
      return Outcome::Ok;
    }
    if (++ordinal_ == kForcedChaosOrdinal) {
      return chaosOutcome();
    }
    std::uniform_int_distribution<int> distribution(1, 100);
    if (distribution(rng_) > kChaosPercent) {
      return Outcome::Ok;
    }
    return chaosOutcome();
  }

  Outcome chaosOutcome() {
    std::uniform_int_distribution<int> distribution(0, 4);
    switch (distribution(rng_)) {
      case 0:
        return Outcome::InternalError;
      case 1:
        return Outcome::Unavailable;
      case 2:
        return Outcome::Timeout;
      case 3:
        return Outcome::CorruptBody;
      default:
        return Outcome::GatewayTimeout;
    }
  }

  std::int64_t nextSegmentDurationUs() {
    std::uniform_int_distribution<std::int64_t> distribution(kSegmentMinDurationUs,
                                                             kSegmentMaxDurationUs);
    return distribution(rng_);
  }

  std::string buildSegmentPayload(int segment_index) {
    std::uniform_int_distribution<int> distribution(0, 255);
    std::string payload(kSegmentPayloadBytes, '\0');
    for (std::size_t i = 0; i < kSegmentPayloadBytes; ++i) {
      payload[i] = static_cast<char>(static_cast<std::uint8_t>(distribution(rng_)));
    }
    if (segment_index >= 0 && static_cast<std::size_t>(segment_index) < segment_checksums_.size()) {
      segment_checksums_[static_cast<std::size_t>(segment_index)] =
          fnv1a(reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size());
    }
    return payload;
  }

  std::string buildMasterManifest() const {
    std::string manifest;
    manifest.reserve(static_cast<std::size_t>(config_.variant_count) * 96 + 16);
    manifest += "#EXTM3U\n";
    for (int v = 0; v < config_.variant_count; ++v) {
      const std::size_t index = static_cast<std::size_t>(v);
      manifest += "#EXT-X-STREAM-INF:BANDWIDTH=";
      manifest += std::to_string(kVariantBandwidths[index]);
      manifest += ",RESOLUTION=";
      manifest += std::to_string(kVariantResolutions[index][0]);
      manifest += 'x';
      manifest += std::to_string(kVariantResolutions[index][1]);
      manifest += ",CODECS=\"avc1.64002a,mp4a.40.2\"\n";
      manifest += 'v';
      manifest += std::to_string(v);
      manifest += ".m3u8\n";
    }
    return manifest;
  }

  std::string buildMediaManifest(int variant) const {
    std::string manifest;
    manifest.reserve(static_cast<std::size_t>(config_.segment_count) * 24 + 64);
    manifest += "#EXTM3U\n";
    manifest += "#EXT-X-TARGETDURATION:4\n";
    manifest += "#EXT-X-MEDIA-SEQUENCE:0\n";
    for (int s = 0; s < config_.segment_count; ++s) {
      manifest += "#EXTINF:4.000000,\n";
      manifest += 'v';
      manifest += std::to_string(variant);
      manifest += "/s";
      manifest += std::to_string(s);
      manifest += ".ts\n";
    }
    manifest += "#EXT-X-ENDLIST\n";
    return manifest;
  }

  ChaosConfig config_;
  std::string manifest_;
  std::vector<std::uint64_t> segment_checksums_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::mt19937 rng_{config_.seed};
  std::size_t ordinal_{0};
  bool terminal_{false};
  std::atomic<std::size_t> total_requests_{0};
  std::atomic<std::size_t> master_requests_{0};
  std::atomic<std::size_t> segment_requests_{0};
  std::atomic<std::size_t> successful_segment_serves_{0};
  std::atomic<std::size_t> chaos_requests_{0};
  std::atomic<std::size_t> transport_failures_{0};
  std::atomic<int> highest_variant_served_{-1};
};

class ChaosHttpClient : public IHttpClient {
 public:
  MOCK_METHOD(HttpResponse, download,
              (const std::string& url, std::chrono::milliseconds timeout,
               const std::stop_token& stop_token),
              (override));
  MOCK_METHOD(void, setNetworkConfig, (const NetworkConfig& config), (override));
  MOCK_METHOD(void, setRetryPolicy, (const RetryPolicy& policy), (override));
};

std::unique_ptr<NiceMock<ChaosHttpClient>> makeClient(ChaosTransport& transport) {
  auto client = std::make_unique<NiceMock<ChaosHttpClient>>();
  EXPECT_CALL(*client, download(_, _, _))
      .WillRepeatedly(Invoke([&transport](const std::string& url, std::chrono::milliseconds,
                                          const std::stop_token& stop_token) {
        return transport.respond(url, stop_token);
      }));
  EXPECT_CALL(*client, setNetworkConfig(_)).Times(::testing::AnyNumber());
  EXPECT_CALL(*client, setRetryPolicy(_)).Times(::testing::AnyNumber());
  return client;
}

std::vector<MediaSegmentBundle> collect(ConcurrentQueue<MediaSegmentBundle>& queue,
                                        std::size_t expected) {
  std::vector<MediaSegmentBundle> bundles;
  bundles.reserve(expected);
  for (std::size_t spins = 0; bundles.size() < expected && spins < kSpinBudget; ++spins) {
    if (auto item = queue.tryPop()) {
      bundles.push_back(std::move(*item));
    } else {
      std::this_thread::yield();
    }
  }
  return bundles;
}

}  // namespace

class OrchestratorChaosTest : public ::testing::Test {
 protected:
  void logSeed(const std::string& label, std::uint32_t seed) {
    ::testing::Test::RecordProperty(label, std::to_string(seed));
    GTEST_LOG_(INFO) << "chaos seed (" << label << ") = " << seed;
  }
};

TEST_F(OrchestratorChaosTest, HappyPathUnderLoad) {
  constexpr std::uint32_t kSeed = 0x00C0FFEEu;
  constexpr int kSegmentCount = 48;
  logSeed("happy_path_seed", kSeed);

  ChaosConfig config;
  config.seed = kSeed;
  config.segment_count = kSegmentCount;
  config.inject_chaos = false;

  ChaosTransport transport(config);
  PlaybackOrchestrator orchestrator(std::make_unique<NetworkComponent>(makeClient(transport)));

  orchestrator.start(std::string(kBaseUrl) + "/master.m3u8");

  ConcurrentQueue<MediaSegmentBundle>& queue = orchestrator.getQueue();
  std::vector<MediaSegmentBundle> bundles = collect(queue, kSegmentCount);

  orchestrator.stop();

  ASSERT_EQ(bundles.size(), static_cast<std::size_t>(kSegmentCount));
  EXPECT_EQ(transport.masterRequests(), 1u);
  EXPECT_EQ(transport.segmentRequests(), static_cast<std::size_t>(kSegmentCount));
  EXPECT_EQ(transport.chaosRequests(), 0u);
  EXPECT_EQ(transport.highestVariantServed(), 0);

  for (std::size_t i = 0; i < bundles.size(); ++i) {
    const MediaSegmentBundle& bundle = bundles[i];
    ASSERT_EQ(bundle.raw_buffer.size(), kSegmentPayloadBytes) << "segment " << i;
    EXPECT_GT(bundle.metrics.throughputMbps(), 1.1) << "segment " << i;
    EXPECT_LT(bundle.metrics.throughputMbps(), 1.3) << "segment " << i;
    EXPECT_EQ(fnv1a(bundle.raw_buffer.data(), bundle.raw_buffer.size()),
              transport.expectedChecksum(static_cast<int>(i)))
        << "segment " << i;
  }

  EXPECT_TRUE(queue.empty());
}

TEST_F(OrchestratorChaosTest, DeterministicNetworkChaos) {
  constexpr std::uint32_t kBaseSeed = 0x0BADC0DEu;
  constexpr int kRounds = 6;
  constexpr int kSegmentCount = 48;

  std::size_t total_bundles = 0;
  std::size_t total_chaos = 0;

  for (int round = 0; round < kRounds; ++round) {
    const std::uint32_t seed = kBaseSeed + static_cast<std::uint32_t>(round);
    logSeed("chaos_seed_round_" + std::to_string(round), seed);

    ChaosConfig config;
    config.seed = seed;
    config.segment_count = kSegmentCount;
    config.inject_chaos = true;

    ChaosTransport transport(config);
    PlaybackOrchestrator orchestrator(std::make_unique<NetworkComponent>(makeClient(transport)));

    orchestrator.start(std::string(kBaseUrl) + "/master.m3u8");

    ConcurrentQueue<MediaSegmentBundle>& queue = orchestrator.getQueue();
    ASSERT_TRUE(transport.waitUntilTerminal());

    const std::size_t expected = transport.successfulSegmentServes();
    std::vector<MediaSegmentBundle> bundles = collect(queue, expected);

    orchestrator.stop();

    EXPECT_EQ(transport.masterRequests(), 1u) << "round " << round;
    EXPECT_GT(transport.chaosRequests(), 0u) << "round " << round;
    EXPECT_EQ(bundles.size(), expected) << "round " << round;
    EXPECT_EQ(transport.segmentRequests(),
              transport.successfulSegmentServes() + transport.transportFailures())
        << "round " << round;
    EXPECT_TRUE(queue.empty()) << "round " << round;

    for (std::size_t i = 0; i < bundles.size(); ++i) {
      ASSERT_EQ(bundles[i].raw_buffer.size(), kSegmentPayloadBytes)
          << "round " << round << " segment " << i;
      EXPECT_GT(bundles[i].metrics.throughputMbps(), 1.1) << "round " << round << " segment " << i;
      EXPECT_LT(bundles[i].metrics.throughputMbps(), 1.3) << "round " << round << " segment " << i;
    }

    total_bundles += bundles.size();
    total_chaos += transport.chaosRequests();
  }

  EXPECT_GT(total_bundles, 0u);
  EXPECT_GT(total_chaos, 0u);
}