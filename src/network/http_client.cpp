#include "iptv/network/http_client.hpp"

#include <curl/curl.h>

#include <charconv>
#include <random>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace iptv::network {

namespace {

[[nodiscard]] bool isTransientError(CURLcode res, HttpStatusCode status) noexcept {
  const auto code = static_cast<long>(status);
  return (res == CURLE_OPERATION_TIMEDOUT || res == CURLE_COULDNT_CONNECT ||
          res == CURLE_COULDNT_RESOLVE_HOST) ||
         (code >= 500 && code < 600) || status == HttpStatusCode::TooManyRequests;
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

/**
 * @brief Contexto unificado de la transferencia HTTP actual.
 * Agrupa todo lo que los callbacks necesitan conocer sin contaminar la API.
 */
struct TransferContext {
  HttpResponse* response{nullptr};
  const std::stop_token* stop_token{nullptr};

  bool aborted_by_user{false};
};

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  if (userdata == nullptr) {
    return 0;
  }
  auto* ctx = static_cast<TransferContext*>(userdata);

  if (ctx->stop_token != nullptr && ctx->stop_token->stop_requested()) {
    ctx->aborted_by_user = true;
    return 0;  // abort
  }

  std::size_t total_size = size * nmemb;


  ctx->response->appendToBody(static_cast<const uint8_t*>(static_cast<const void*>(ptr)),
                              total_size);
  return total_size;
}

size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
  if (userdata == nullptr) {
    return size * nitems;
  }
  size_t total = size * nitems;
  std::string_view line(buffer, total);

  if (line.starts_with("Content-Length:") || line.starts_with("content-length:")) {
    size_t pos = line.find(':');
    if (pos != std::string_view::npos) {
      std::string_view val = line.substr(pos + 1);
      auto first = val.find_first_not_of(" \t\r\n");
      if (first != std::string_view::npos) {
        val = val.substr(first);
        size_t content_length = 0;
        auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), content_length);
        if (ec == std::errc{}) {
          auto* ctx = static_cast<TransferContext*>(userdata);
          ctx->response->reserveBody(content_length);
        }
      }
    }
  }
  return total;
}

int progressCallback(void* clientp, long long /*dltotal*/, long long /*dlnow*/,
                     long long /*ultotal*/, long long /*ulnow*/) {
  if (clientp == nullptr) {
    return 0;
  }
  auto* ctx = static_cast<TransferContext*>(clientp);
  if (ctx->stop_token != nullptr && ctx->stop_token->stop_requested()) {
    ctx->aborted_by_user = true;
    return 1;  // Return non-zero to trigger CURLE_ABORTED_BY_CALLBACK
  }
  return 0;
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
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION, progressCallback);
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

  void bindResponseBuffers(TransferContext& ctx) {
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, &ctx);
    curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, &ctx);
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

/**
 * @brief Constructs the HTTP client and its hidden implementation (Pimpl).
 */
HttpClient::HttpClient() : pimpl_(std::make_unique<Impl>()) {}

/**
 * @brief Destroys the HTTP client and cleans up the underlying curl handle.
 */
HttpClient::~HttpClient() = default;

/**
 * @brief Move constructor.
 */
HttpClient::HttpClient(HttpClient&& other) noexcept : pimpl_(std::move(other.pimpl_)) {}

/**
 * @brief Move assignment operator.
 */
HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
  if (this != &other) {
    pimpl_ = std::move(other.pimpl_);
  }
  return *this;
}

// Interface implementations (Skeleton for now)

/**
 * @brief Executes a synchronous HTTP GET request with retries and timeout support.
 */
HttpResponse HttpClient::download(const std::string& url, std::chrono::milliseconds timeout,
                                  const std::stop_token& stop_token) {
  pimpl_->prepareHandle(url, timeout);

  HttpResponse response(HttpStatusCode::Unknown);

  TransferContext ctx{.response = &response,
                      .stop_token = &stop_token,

                      .aborted_by_user = false};

  pimpl_->bindResponseBuffers(ctx);

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
    if (res == CURLE_ABORTED_BY_CALLBACK || ctx.aborted_by_user || stop_token.stop_requested()) {
      return response;
    }

    if (!isTransientError(res, response.getStatusCode()) || attempt == retries) {
      return response;
    }

    applyBackoffDelay(pimpl_->policy_.strategy, current_delay, pimpl_->policy_.max_delay);
  }

  return response;
}

/**
 * @brief Sets global configurations for the curl handle (TLS, User-Agent, Timeouts).
 */
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

/**
 * @brief Updates the backoff and retry policy for network requests.
 */
void HttpClient::setRetryPolicy(const RetryPolicy& policy) { pimpl_->policy_ = policy; }

}  // namespace iptv::network
