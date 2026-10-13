#pragma once

#include <curl/curl.h>

#include <chrono>

#include "iptv/network/http_client.hpp"
#include "iptv/network/http_response.hpp"
#include "iptv/network/http_retry_policy.hpp"

namespace iptv::network {

/**
 * @brief Hidden implementation of HttpClient owning the curl easy handle.
 *
 * Defined behind a pimpl so libcurl stays out of the public headers. The handle
 * lifetime is strict: constructed in the ctor, released in the dtor, never
 * copied or moved.
 */
class HttpClient::Impl {
  friend class HttpClient;

 public:
  Impl();
  ~Impl();

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] CURL* get() const noexcept { return handle_; }

  /**
   * @brief Applies the URL and the optional total timeout to the handle.
   */
  void prepareHandle(const std::string& url, std::chrono::milliseconds timeout);

  /**
   * @brief Performs the transfer, measuring its wall-clock duration.
   * @param duration Out-parameter receiving the measured transfer duration.
   * @return The curl result code of the attempt.
   */
  CURLcode executeTransfer(std::chrono::microseconds& duration);

  /**
   * @brief Records the timing side of the metrics on the response.
   */
  void collectTiming(CURLcode res, const std::chrono::microseconds& duration,
                     HttpResponse& response);

  /**
   * @brief Records the status side of the metrics on the response.
   *
   * A transfer that never completed reports Unknown regardless of what curl
   * reports, so the caller cannot mistake a partial body for a served one.
   */
  void collectStatusCode(CURLcode res, HttpResponse& response);

  RetryPolicy policy_;

 private:
  CURL* handle_ = nullptr;
};

}  // namespace iptv::network
