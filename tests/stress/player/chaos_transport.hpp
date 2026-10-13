#pragma once

#include <gmock/gmock.h>

#include <array>
#include <atomic>
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
#include <vector>

#include "iptv/concurrency/concurrent_queue.hpp"
#include "iptv/network/network_component.hpp"

namespace iptv::test::chaos {

inline constexpr std::string_view kBaseUrl = "http://chaos.invalid/media";
inline constexpr std::size_t kSpinBudget = 4'000'000;

inline constexpr std::array<std::uint32_t, 8> kVariantBandwidths{
    500'000, 1'000'000, 2'000'000, 4'000'000, 8'000'000, 12'000'000, 20'000'000, 35'000'000};
inline constexpr std::array<std::array<std::uint32_t, 2>, 8> kVariantResolutions{{{320, 180},
                                                                                  {640, 360},
                                                                                  {854, 480},
                                                                                  {1280, 720},
                                                                                  {1920, 1080},
                                                                                  {2560, 1440},
                                                                                  {3840, 2160},
                                                                                  {3840, 2160}}};

inline constexpr std::size_t kSegmentPayloadBytes = 15'000;
inline constexpr std::int64_t kSegmentMinDurationUs = 98'000;
inline constexpr std::int64_t kSegmentMaxDurationUs = 102'000;
inline constexpr std::size_t kForcedChaosOrdinal = 12;
inline constexpr int kChaosPercent = 15;

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

/**
 * @brief FNV-1a over a byte range, used to assert payload integrity end to end.
 */
[[nodiscard]] std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size);

/**
 * @brief In-process stand-in for the HTTP layer, driven by a seeded RNG.
 *
 * Given a seed it replays exactly the same request sequence, the same chaos
 * injections and the same segment bytes on every run, so a failure is
 * reproducible. It answers manifests and segments, records per-kind counters,
 * and signals a terminal event once the last segment has been served or a
 * non-retryable failure ends the run.
 */
class ChaosTransport {
 public:
  explicit ChaosTransport(const ChaosConfig& config);

  /**
   * @brief Produces the response for one request and advances the run's state.
   */
  [[nodiscard]] iptv::network::HttpResponse respond(const std::string& url,
                                                    const std::stop_token& stop_token);

  /**
   * @brief Blocks until the run reaches a terminal state.
   */
  [[nodiscard]] bool waitUntilTerminal();

  [[nodiscard]] std::size_t totalRequests() const noexcept;
  [[nodiscard]] std::size_t masterRequests() const noexcept;
  [[nodiscard]] std::size_t segmentRequests() const noexcept;
  [[nodiscard]] std::size_t successfulSegmentServes() const noexcept;
  [[nodiscard]] std::size_t chaosRequests() const noexcept;
  [[nodiscard]] std::size_t transportFailures() const noexcept;
  [[nodiscard]] int highestVariantServed() const noexcept;

  /**
   * @brief Checksum recorded for a segment the first time it was served.
   * @return 0 for an index never served.
   */
  [[nodiscard]] std::uint64_t expectedChecksum(int segment_index) const;

 private:
  void recordRequest(const Request& request);
  void recordBody(const Request& request, Outcome outcome, std::string& body);
  void recordTerminal(const Request& request, Outcome outcome);
  [[nodiscard]] iptv::network::HttpResponse errorResponseFor(Outcome outcome) const;

  void signalTerminal();
  void countOutcome(Outcome outcome);
  static bool isErrorOutcome(Outcome outcome);
  [[nodiscard]] Request classify(std::string_view url) const;
  Outcome plan(const Request& request);
  Outcome chaosOutcome();
  std::int64_t nextSegmentDurationUs();
  std::string buildSegmentPayload(int segment_index);
  [[nodiscard]] iptv::network::HttpResponse successResponse(const std::string& body,
                                                            std::int64_t duration_us) const;

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

/**
 * @brief Mock HTTP client that forwards every download to a ChaosTransport.
 */
class ChaosHttpClient : public iptv::network::IHttpClient {
 public:
  MOCK_METHOD(iptv::network::HttpResponse, download,
              (const std::string& url, std::chrono::milliseconds timeout,
               const std::stop_token& stop_token),
              (override));
  MOCK_METHOD(void, setNetworkConfig, (const iptv::network::NetworkConfig& config), (override));
  MOCK_METHOD(void, setRetryPolicy, (const iptv::network::RetryPolicy& policy), (override));
};

[[nodiscard]] std::unique_ptr<::testing::NiceMock<ChaosHttpClient>> makeClient(
    ChaosTransport& transport);

/**
 * @brief Drains up to `expected` bundles from the queue, yielding while it is empty.
 *
 * Bounded by kSpinBudget so a producer that stalls shows up as a short vector
 * instead of a hung test binary.
 */
[[nodiscard]] std::vector<iptv::network::MediaSegmentBundle> collect(
    iptv::ConcurrentQueue<iptv::network::MediaSegmentBundle>& queue, std::size_t expected);

}  // namespace iptv::test::chaos
