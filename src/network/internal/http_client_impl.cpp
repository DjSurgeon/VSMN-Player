#include "internal/http_client_impl.hpp"

#include <stdexcept>

#include "internal/transfer_callbacks.hpp"

namespace iptv::network {

HttpClient::Impl::Impl() : handle_(curl_easy_init()) {
  if (handle_ == nullptr) {
    throw std::runtime_error("Failed to initialize curl easy handle.");
  }
  internal::registerTransferCallbacks(handle_);
}

HttpClient::Impl::~Impl() {
  if (handle_ != nullptr) {
    curl_easy_cleanup(handle_);
    handle_ = nullptr;
  }
}

void HttpClient::Impl::prepareHandle(const std::string& url, std::chrono::milliseconds timeout) {
  curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
  if (timeout.count() > 0) {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));
  }
}

CURLcode HttpClient::Impl::executeTransfer(std::chrono::microseconds& duration) {
  const auto start_wall_clock = std::chrono::steady_clock::now();
  const CURLcode res = curl_easy_perform(handle_);
  const auto end_wall_clock = std::chrono::steady_clock::now();
  duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end_wall_clock - start_wall_clock);
  return res;
}

void HttpClient::Impl::collectTiming(CURLcode /*res*/, const std::chrono::microseconds& duration,
                                     HttpResponse& response) {
  auto& metrics = response.getMetricsRef();
  metrics.total_duration = duration;

  curl_off_t starttransfer_us = 0;
  curl_easy_getinfo(handle_, CURLINFO_STARTTRANSFER_TIME_T, &starttransfer_us);
  metrics.ttfb = std::chrono::microseconds(starttransfer_us);
  metrics.bytes_downloaded = response.getBytesDownloaded();
}

void HttpClient::Impl::collectStatusCode(CURLcode res, HttpResponse& response) {
  if (res != CURLE_OK) {
    response.setStatusCode(HttpStatusCode::Unknown);
    return;
  }

  long status = 0;
  curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &status);
  response.setStatusCode(static_cast<HttpStatusCode>(status));
}

}  // namespace iptv::network
