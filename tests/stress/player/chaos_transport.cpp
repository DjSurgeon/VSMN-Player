#include "chaos_transport.hpp"

#include <gmock/gmock.h>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "chaos_manifest.hpp"

namespace iptv::test::chaos {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;

using iptv::network::HttpResponse;
using iptv::network::HttpStatusCode;
using iptv::network::MediaSegmentBundle;
using iptv::network::NetworkMetrics;

}  // namespace

ChaosTransport::ChaosTransport(const ChaosConfig& config)
    : config_(config),
      manifest_(buildMasterManifest(config.variant_count)),
      segment_checksums_(static_cast<std::size_t>(config.segment_count), 0) {}

void ChaosTransport::recordRequest(const Request& request) {
  switch (request.kind) {
    case Request::Kind::Master:
      master_requests_.fetch_add(1, std::memory_order_relaxed);
      break;
    case Request::Kind::VariantPlaylist:
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
}

void ChaosTransport::recordBody(const Request& request, Outcome outcome, std::string& body) {
  switch (request.kind) {
    case Request::Kind::Master:
      body = manifest_;
      break;
    case Request::Kind::VariantPlaylist:
      body =
          buildMediaManifest(static_cast<int>(variantIndex(request.variant, config_.variant_count)),
                             config_.segment_count);
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
}

void ChaosTransport::recordTerminal(const Request& request, Outcome outcome) {
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

HttpResponse ChaosTransport::errorResponseFor(Outcome outcome) const {
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
  return HttpResponse(HttpStatusCode::Unknown);
}

HttpResponse ChaosTransport::successResponse(const std::string& body,
                                             std::int64_t duration_us) const {
  HttpResponse response(HttpStatusCode::Ok, body.size());
  response.appendToBody(reinterpret_cast<const std::uint8_t*>(body.data()), body.size());

  NetworkMetrics& metrics = response.getMetricsRef();
  metrics.bytes_downloaded = response.getBytesDownloaded();
  metrics.total_duration = std::chrono::microseconds(duration_us);
  metrics.ttfb = std::chrono::microseconds(20'000);
  return response;
}

HttpResponse ChaosTransport::respond(const std::string& url, const std::stop_token& stop_token) {
  const Request request = classify(url);
  const Outcome outcome = plan(request);

  std::string body;
  std::int64_t duration_us = 0;
  {
    std::lock_guard<std::mutex> guard(mutex_);
    countOutcome(outcome);
    recordRequest(request);
    recordBody(request, outcome, body);
    duration_us = request.kind == Request::Kind::Segment ? nextSegmentDurationUs() : 100'000;
    recordTerminal(request, outcome);
  }

  if (outcome != Outcome::Ok && outcome != Outcome::CorruptBody) {
    return errorResponseFor(outcome);
  }

  if (stop_token.stop_requested()) {
    return HttpResponse(HttpStatusCode::Unknown);
  }

  return successResponse(body, duration_us);
}

bool ChaosTransport::waitUntilTerminal() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock, [this] { return terminal_; });
  return true;
}

std::unique_ptr<NiceMock<ChaosHttpClient>> makeClient(ChaosTransport& transport) {
  auto client = std::make_unique<NiceMock<ChaosHttpClient>>();
  EXPECT_CALL(*client, download(_, _, _))
      .WillRepeatedly(Invoke([&transport](const std::string& url, std::chrono::milliseconds,
                                          const std::stop_token& stop_token) {
        return transport.respond(url, stop_token);
      }));
  EXPECT_CALL(*client, setNetworkConfig(_)).Times(AnyNumber());
  EXPECT_CALL(*client, setRetryPolicy(_)).Times(AnyNumber());
  return client;
}

std::vector<MediaSegmentBundle> collect(iptv::ConcurrentQueue<MediaSegmentBundle>& queue,
                                        std::size_t expected) {
  std::vector<MediaSegmentBundle> bundles;
  bundles.reserve(expected);
  for (std::size_t spins = 0; bundles.size() < expected && spins < kSpinBudget; ++spins) {
    if (auto item = queue.try_pop()) {
      bundles.push_back(std::move(*item));
    } else {
      std::this_thread::yield();
    }
  }
  return bundles;
}

}  // namespace iptv::test::chaos
