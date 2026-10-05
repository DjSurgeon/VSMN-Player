#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

#include "chaos_transport.hpp"

namespace iptv::test::chaos {

namespace {

/**
 * @brief Reads the run of digits ending at `end_pos`, exclusive.
 * @return The parsed value, or -1 when no digits are there.
 */
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

}  // namespace

std::uint64_t fnv1a(const std::uint8_t* data, std::size_t size) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= static_cast<std::uint64_t>(data[i]);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::size_t ChaosTransport::totalRequests() const noexcept {
  return total_requests_.load(std::memory_order_relaxed);
}

std::size_t ChaosTransport::masterRequests() const noexcept {
  return master_requests_.load(std::memory_order_relaxed);
}

std::size_t ChaosTransport::segmentRequests() const noexcept {
  return segment_requests_.load(std::memory_order_relaxed);
}

std::size_t ChaosTransport::successfulSegmentServes() const noexcept {
  return successful_segment_serves_.load(std::memory_order_relaxed);
}

std::size_t ChaosTransport::chaosRequests() const noexcept {
  return chaos_requests_.load(std::memory_order_relaxed);
}

std::size_t ChaosTransport::transportFailures() const noexcept {
  return transport_failures_.load(std::memory_order_relaxed);
}

int ChaosTransport::highestVariantServed() const noexcept {
  return highest_variant_served_.load(std::memory_order_relaxed);
}

std::uint64_t ChaosTransport::expectedChecksum(int segment_index) const {
  if (segment_index < 0 || static_cast<std::size_t>(segment_index) >= segment_checksums_.size()) {
    return 0;
  }
  return segment_checksums_[static_cast<std::size_t>(segment_index)];
}

void ChaosTransport::signalTerminal() {
  terminal_ = true;
  cv_.notify_all();
}

void ChaosTransport::countOutcome(Outcome outcome) {
  total_requests_.fetch_add(1, std::memory_order_relaxed);
  if (outcome != Outcome::Ok) {
    chaos_requests_.fetch_add(1, std::memory_order_relaxed);
  }
  if (isErrorOutcome(outcome)) {
    transport_failures_.fetch_add(1, std::memory_order_relaxed);
  }
}

bool ChaosTransport::isErrorOutcome(Outcome outcome) {
  return outcome != Outcome::Ok && outcome != Outcome::CorruptBody;
}

Request ChaosTransport::classify(std::string_view url) const {
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

Outcome ChaosTransport::plan(const Request& request) {
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

Outcome ChaosTransport::chaosOutcome() {
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

std::int64_t ChaosTransport::nextSegmentDurationUs() {
  std::uniform_int_distribution<std::int64_t> distribution(kSegmentMinDurationUs,
                                                           kSegmentMaxDurationUs);
  return distribution(rng_);
}

std::string ChaosTransport::buildSegmentPayload(int segment_index) {
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

}  // namespace iptv::test::chaos
