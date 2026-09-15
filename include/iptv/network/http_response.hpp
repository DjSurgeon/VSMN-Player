#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "iptv/network/http_status_code.hpp"

namespace iptv::network {

/**
 * @brief Detailed telemetry of the HTTP transfer.
 */
struct NetworkMetrics {
  std::chrono::microseconds ttfb{0};  ///< Pure network transport latency (DNS + TLS + TTFB)
  std::chrono::microseconds total_duration{
      0};                      ///< End-to-end wall clock duration (including buffer copies)
  size_t bytes_downloaded{0};  ///< Total payload bytes received

  /**
   * @brief Calculates the actual perceived throughput in Megabits per second
   * (Mbps).
   * @return double The calculated throughput in Mbps.
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
 * @brief Represents the outcome of an HTTP request with strict move-only
 * semantics.
 *
 * Enforces Zero-Copy architecture by deleting copy operations. It uses an
 * explicit pre-allocation model for the binary payload to avoid reallocations
 * during large MPEG-TS segment downloads.
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
  explicit HttpResponse(HttpStatusCode code, std::size_t expected_body_size = 0)
      : status_code_(code) {
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
   * @brief Appends data to the response body using SIMD-optimized memory copy.
   *
   * @param src Pointer to the raw bytes.
   * @param size Number of bytes to append.
   */
  void appendToBody(const uint8_t* src, std::size_t size) noexcept {
    if (src == nullptr || size == 0) {
      return;
    }
    const size_t current_size = body_.size();
    body_.resize(current_size + size);
    std::memcpy(body_.data() + current_size, src, size);
  }

  /**
   * @brief Clears the response state but retains memory capacity for reuse.
   */
  void clear() noexcept {
    body_.clear();
    status_code_ = HttpStatusCode::Unknown;
    metrics_ = NetworkMetrics{};
  }

  /**
   * @brief Explicitly reserves capacity in the internal vector.
   * @param capacity Expected total body size.
   */
  void reserveBody(std::size_t capacity) {
    if (capacity > body_.capacity()) {
      body_.reserve(capacity);
    }
  }

  /**
   * @brief Retrieves the HTTP status code of the response.
   * @return HttpStatusCode The HTTP status code.
   */
  [[nodiscard]] HttpStatusCode getStatusCode() const noexcept { return status_code_; }

  /**
   * @brief Checks if the HTTP request was successful (2xx status code).
   * @return true if successful, false otherwise.
   */
  [[nodiscard]] bool isSuccess() const noexcept {
    const auto code = static_cast<int>(status_code_);
    return code >= 200 && code < 300;
  }

  /**
   * @brief Retrieves the raw binary body of the response.
   * @return const std::vector<uint8_t>& The response body.
   */
  [[nodiscard]] const std::vector<uint8_t>& getBody() const noexcept { return body_; }

  /**
   * @brief Extracts the raw binary body via move semantics.
   * @return std::vector<uint8_t> The moved response body.
   */
  std::vector<uint8_t> extractBody() noexcept { return std::move(body_); }

  /**
   * @brief Retrieves the total number of bytes downloaded.
   * @return std::size_t Number of bytes in the body.
   */
  [[nodiscard]] std::size_t getBytesDownloaded() const noexcept { return body_.size(); }


  /**
   * @brief Retrieves detailed network metrics for the request.
   * @return const NetworkMetrics& The network metrics.
   */
  [[nodiscard]] const NetworkMetrics& getMetrics() const noexcept { return metrics_; }

  /**
   * @brief Retrieves mutable network metrics for internal tracking.
   * @return NetworkMetrics& The mutable network metrics.
   */
  NetworkMetrics& getMetricsRef() noexcept { return metrics_; }

  /**
   * @brief Updates the HTTP status code.
   * @param code The new HTTP status code.
   */
  void setStatusCode(HttpStatusCode code) noexcept { status_code_ = code; }

 private:
  HttpStatusCode status_code_;
  std::vector<uint8_t> body_;
  NetworkMetrics metrics_{};
};

}  // namespace iptv::network
