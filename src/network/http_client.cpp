#include "iptv/network/http_client.hpp"

#include <curl/curl.h>

#include <stdexcept>
#include <thread>

namespace iptv::network {

// libcurl write callback
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  if (userdata == nullptr) {
    return 0;
  }

  auto* response = static_cast<HttpResponse*>(userdata);
  std::size_t total_size = size * nmemb;
  response->appendToBody(static_cast<const uint8_t*>(static_cast<const void*>(ptr)), total_size);
  return total_size;
}

// Define the hidden implementation class with strict RAII
class HttpClient::Impl {
  friend class HttpClient;

 public:
  Impl() : handle_(curl_easy_init()) {
    if (handle_ == nullptr) {
      throw std::runtime_error("Failed to initialize curl easy handle.");
    }
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
  }

  ~Impl() {
    if (handle_ != nullptr) {
      curl_easy_cleanup(handle_);
      handle_ = nullptr;
    }
  }

  // Rule of 5: Prohibit copy/move for Impl to ensure strict lifecycle of raw handle
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] CURL* get() const noexcept { return handle_; }

 private:
  CURL* handle_ = nullptr;
  RetryPolicy policy_;
};

// Lifecycle methods for HttpClient

HttpClient::HttpClient() : pimpl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&& other) noexcept : pimpl_(std::move(other.pimpl_)) {}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
  if (this != &other) {
    pimpl_ = std::move(other.pimpl_);
  }
  return *this;
}

// Interface implementations (Skeleton for now)

HttpResponse HttpClient::download(const std::string& url, std::chrono::milliseconds timeout) {
  auto retries = pimpl_->policy_.max_retries;
  const auto backoff_strategy = pimpl_->policy_.strategy;
  auto current_delay = pimpl_->policy_.initial_delay;

  // Config global for this specific download
  curl_easy_setopt(pimpl_->get(), CURLOPT_URL, url.c_str());
  if (timeout.count() > 0) {
    curl_easy_setopt(pimpl_->get(), CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));
  }

  for (std::uint32_t attempt = 0; attempt <= retries; ++attempt) {
    HttpResponse response(HttpStatusCode::Unknown);
    curl_easy_setopt(pimpl_->get(), CURLOPT_WRITEDATA, &response);

    const auto start_wall_clock = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(pimpl_->get());
    const auto end_wall_clock = std::chrono::steady_clock::now();

    auto& metrics = response.getMetricsRef();
    metrics.total_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_wall_clock - start_wall_clock);

    long status = 0;
    curl_easy_getinfo(pimpl_->get(), CURLINFO_RESPONSE_CODE, &status);
    response.setStatusCode(static_cast<HttpStatusCode>(status));

    curl_off_t starttransfer_us = 0;
    curl_easy_getinfo(pimpl_->get(), CURLINFO_STARTTRANSFER_TIME_T, &starttransfer_us);
    metrics.ttfb = std::chrono::microseconds(starttransfer_us);
    metrics.bytes_downloaded = response.getBytesDownloaded();

    if (res == CURLE_OK && response.isSuccess()) {
      return response;
    }

    // Determine if it's a permanent or transient error
    const bool is_transient = (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT ||
                               res == CURLE_COULDNT_RESOLVE_HOST) ||
                              (status >= 500 && status < 600) || (status == 429);

    if (!is_transient || attempt == retries) {
      return response;  // Fail fast or out of retries
    }

    // Apply backoff
    if (backoff_strategy != BackoffStrategy::None) {
      std::this_thread::sleep_for(current_delay);
      if (backoff_strategy == BackoffStrategy::Exponential) {
        current_delay *= 2;
        if (current_delay > pimpl_->policy_.max_delay) {
          current_delay = pimpl_->policy_.max_delay;
        }
      }
    }
  }

  return HttpResponse(HttpStatusCode::Unknown);
}

void HttpClient::setNetworkConfig(const NetworkConfig& config) {
  curl_easy_setopt(pimpl_->get(), CURLOPT_USERAGENT, config.user_agent.c_str());
  curl_easy_setopt(pimpl_->get(), CURLOPT_FOLLOWLOCATION, config.follow_redirects ? 1L : 0L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYPEER, config.ssl_verify ? 1L : 0L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYHOST, config.ssl_verify ? 2L : 0L);

  // Connection timeout vs general timeout anti-stall
  curl_easy_setopt(pimpl_->get(), CURLOPT_CONNECTTIMEOUT_MS, 5000L);  // 5 seconds connect max

  // Anti-stall safeguards: abort if < 10KB/s for 3s
  curl_easy_setopt(pimpl_->get(), CURLOPT_LOW_SPEED_LIMIT, 10240L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_LOW_SPEED_TIME, 3L);
}

void HttpClient::setRetryPolicy(const RetryPolicy& policy) { pimpl_->policy_ = policy; }

}  // namespace iptv::network
