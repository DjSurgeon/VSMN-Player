#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "iptv/network/http_status_code.hpp"

namespace iptv::network {

/**
 * @brief Telemetría detallada de la transferencia HTTP.
 */
struct NetworkMetrics {
  std::chrono::microseconds ttfb{0};  ///< Pure network transport latency (DNS + TLS + TTFB)
  std::chrono::microseconds total_duration{
      0};                      ///< End-to-end wall clock duration (including buffer copies)
  size_t bytes_downloaded{0};  ///< Total payload bytes received

  /**
   * @brief Calcula el throughput real percibido en Megabits por segundo (Mbps).
   */
  [[nodiscard]] double throughputMbps() const noexcept {
    if (total_duration.count() <= 0 || bytes_downloaded == 0) {
      return 0.0;
    }
    const double seconds = std::chrono::duration<double>(total_duration).count();
    const double bits = static_cast<double>(bytes_downloaded) * 8.0;
    return (bits / seconds) / 1'000'000.0;
  }
};

/**
 * @brief Represents the outcome of an HTTP request with strict move-only semantics.
 *
 * Enforces Zero-Copy architecture by deleting copy operations. It uses an explicit
 * pre-allocation model for the binary payload to avoid reallocations during large
 * MPEG-TS segment downloads.
 */
class HttpResponse {
 public:
  /**
   * @brief Explicit constructor with pre-allocation hint.
   *
   * @param code HTTP status code.
   * @param expected_body_size Pre-allocation size for the payload vector.
   * @param latency Request latency.
   */
  explicit HttpResponse(HttpStatusCode code, std::size_t expected_body_size = 0,
                        std::chrono::milliseconds latency = std::chrono::milliseconds{0})
      : status_code_(code), latency_(latency) {
    if (expected_body_size > 0) {
      body_.reserve(expected_body_size);
    }
  }

  ~HttpResponse() = default;

  // Rule of 5: Move-only semantics (Zero-Copy)
  HttpResponse(const HttpResponse&) = delete;
  HttpResponse& operator=(const HttpResponse&) = delete;
  HttpResponse(HttpResponse&&) noexcept = default;
  HttpResponse& operator=(HttpResponse&&) noexcept = default;

  /**
   * @brief Appends data to the response body without full copy reallocations.
   *
   * @param data Pointer to the raw bytes.
   * @param size Number of bytes to append.
   */
  void appendToBody(const uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) {
      return;
    }
    body_.insert(body_.end(), data, data + size);
  }

  // Accessors - const-correctness enforced

  [[nodiscard]] HttpStatusCode getStatusCode() const noexcept { return status_code_; }

  [[nodiscard]] bool isSuccess() const noexcept {
    const auto code = static_cast<int>(status_code_);
    return code >= 200 && code < 300;
  }

  [[nodiscard]] const std::vector<uint8_t>& getBody() const noexcept { return body_; }

  [[nodiscard]] std::size_t getBytesDownloaded() const noexcept { return body_.size(); }

  [[nodiscard]] std::chrono::milliseconds getLatency() const noexcept { return latency_; }

  [[nodiscard]] const NetworkMetrics& getMetrics() const noexcept { return metrics_; }

  // Mutable access for the HTTP client during construction
  NetworkMetrics& getMetricsRef() noexcept { return metrics_; }

  void setStatusCode(HttpStatusCode code) noexcept { status_code_ = code; }

 private:
  HttpStatusCode status_code_;
  std::vector<uint8_t> body_;
  std::chrono::milliseconds latency_;
  NetworkMetrics metrics_{};
};

}  // namespace iptv::network
