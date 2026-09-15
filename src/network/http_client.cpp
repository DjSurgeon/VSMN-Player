#include "iptv/network/http_client.hpp"

#include <curl/curl.h>

#include <random>
#include <stdexcept>
#include <thread>

#include "iptv/network/http_curl_callbacks.hpp"

namespace iptv::network {

namespace {

[[nodiscard]] bool isTransientError(CURLcode res, HttpStatusCode status) noexcept {
  const auto code = static_cast<long>(status);
  return (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT ||
          res == CURLE_COULDNT_RESOLVE_HOST) ||
         (code >= 500 && code < 600) || (code == 429);
}

void applyBackoffDelay(BackoffStrategy strategy, std::chrono::milliseconds& current_delay,
                       std::chrono::milliseconds max_delay) {
  if (strategy == BackoffStrategy::None) {
    return;
  }

  thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<long long> dist(0, current_delay.count());
  std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));

  if (strategy == BackoffStrategy::Exponential) {
    current_delay = std::min(current_delay * 2, max_delay);
  }
}

}  // namespace

// Define the hidden implementation class with strict RAII
class HttpClient::Impl {
  friend class HttpClient;

 public:
  Impl() : handle_(curl_easy_init()) {
    if (handle_ == nullptr) {
      throw std::runtime_error("Failed to initialize curl easy handle.");
    }
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, detail::writeCallback);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, detail::headerCallback);
    curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION, detail::progressCallback);
    curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, 0L);
  }

  ~Impl() {
    if (handle_ != nullptr) {
      curl_easy_cleanup(handle_);
      handle_ = nullptr;
    }
  }

  // Rule of 5: Prohibit copy/move for Impl to ensure strict lifecycle of raw
  // handle
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;

  [[nodiscard]] CURL* get() const noexcept { return handle_; }

  void prepareHandle(const std::string& url, std::chrono::milliseconds timeout) {
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
    if (timeout.count() > 0) {
      curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));
    }
  }

  void bindResponseBuffers(HttpResponse& response, const std::stop_token& st) {
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &response);

    // We cast away const to void*, the callback casts it back to const std::stop_token*
    curl_easy_setopt(handle_, CURLOPT_XFERINFODATA,
                     const_cast<void*>(static_cast<const void*>(&st)));
  }

  CURLcode executeTransfer(std::chrono::microseconds& duration) {
    const auto start_wall_clock = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(handle_);
    const auto end_wall_clock = std::chrono::steady_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end_wall_clock - start_wall_clock);
    return res;
  }

  void collectMetrics(CURLcode res, const std::chrono::microseconds& duration,
                      HttpResponse& response) {
    auto& metrics = response.getMetricsRef();
    metrics.total_duration = duration;

    long status = 0;
    curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &status);
    if (res != CURLE_OK) {
      response.setStatusCode(HttpStatusCode::Unknown);
    } else {
      response.setStatusCode(static_cast<HttpStatusCode>(status));
    }

    curl_off_t starttransfer_us = 0;
    curl_easy_getinfo(handle_, CURLINFO_STARTTRANSFER_TIME_T, &starttransfer_us);
    metrics.ttfb = std::chrono::microseconds(starttransfer_us);
    metrics.bytes_downloaded = response.getBytesDownloaded();
  }

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

HttpResponse HttpClient::download(const std::string& url, std::chrono::milliseconds timeout,
                                  std::stop_token st) {
  pimpl_->prepareHandle(url, timeout);

  HttpResponse response(HttpStatusCode::Unknown);
  pimpl_->bindResponseBuffers(response, st);

  const auto retries = pimpl_->policy_.max_retries;
  auto current_delay = pimpl_->policy_.initial_delay;

  for (std::uint32_t attempt = 0; attempt <= retries; ++attempt) {
    response.clear();

    std::chrono::microseconds duration{0};
    const CURLcode res = pimpl_->executeTransfer(duration);

    pimpl_->collectMetrics(res, duration, response);

    if (res == CURLE_OK && response.isSuccess()) {
      return response;
    }

    // Do not retry if the operation was aborted by the user
    if (res == CURLE_ABORTED_BY_CALLBACK || st.stop_requested()) {
      return response;
    }

    if (!isTransientError(res, response.getStatusCode()) || attempt == retries) {
      return response;
    }

    applyBackoffDelay(pimpl_->policy_.strategy, current_delay, pimpl_->policy_.max_delay);
  }

  return response;
}

void HttpClient::setNetworkConfig(const NetworkConfig& config) {
  curl_easy_setopt(pimpl_->get(), CURLOPT_USERAGENT, config.user_agent.c_str());
  curl_easy_setopt(pimpl_->get(), CURLOPT_FOLLOWLOCATION, config.follow_redirects ? 1L : 0L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYPEER, config.ssl_verify ? 1L : 0L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYHOST, config.ssl_verify ? 2L : 0L);

  // Connection timeout vs general timeout anti-stall
  curl_easy_setopt(pimpl_->get(), CURLOPT_CONNECTTIMEOUT_MS,
                   static_cast<long>(config.connect_timeout.count()));

  // Anti-stall safeguards: abort if < 10KB/s for 3s
  curl_easy_setopt(pimpl_->get(), CURLOPT_LOW_SPEED_LIMIT, 10240L);
  curl_easy_setopt(pimpl_->get(), CURLOPT_LOW_SPEED_TIME, 3L);
}

void HttpClient::setRetryPolicy(const RetryPolicy& policy) { pimpl_->policy_ = policy; }

}  // namespace iptv::network
