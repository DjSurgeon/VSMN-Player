#include "iptv/network/http_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "internal/http_client_impl.hpp"
#include "internal/transfer_callbacks.hpp"

namespace iptv::network {

namespace {

/**
 * @brief Sleeps a jittered interval and advances the delay for the next attempt.
 * @param strategy The configured backoff strategy; None is a no-op.
 * @param current_delay In/out delay: read for the sleep, then advanced in place.
 * @param max_delay Ceiling applied when doubling.
 *
 * Jitter is drawn from [0, current_delay] so concurrent clients do not retry in
 * lockstep after a shared outage.
 */
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
 * @brief Decides whether the retry loop may stop after a failed attempt.
 * @param aborted True when the caller cancelled the request.
 * @param transient True when the failure class is worth retrying.
 * @param attempt The zero-based attempt number just completed.
 * @param retries The configured maximum retry count.
 * @return true when the loop should return the current response.
 */
[[nodiscard]] bool shouldStopRetrying(bool aborted, bool transient, uint32_t attempt,
                                      uint32_t retries) noexcept {
  if (aborted) {
    return true;
  }
  return !transient || attempt == retries;
}

}  // namespace

HttpClient::HttpClient() : pimpl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&& other) noexcept : pimpl_(std::move(other.pimpl_)) {}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
  if (this != &other) {
    pimpl_ = std::move(other.pimpl_);
  }
  return *this;
}

HttpResponse HttpClient::download(const std::string& url, std::chrono::milliseconds timeout,
                                  const std::stop_token& stop_token) {
  pimpl_->prepareHandle(url, timeout);

  HttpResponse response(HttpStatusCode::Unknown);

  internal::TransferContext ctx{
      .response = &response,
      .stop_token = &stop_token,
      .aborted_by_user = false,
  };

  internal::bindTransferContext(pimpl_->get(), ctx);

  const auto retries = pimpl_->policy_.max_retries;
  auto current_delay = pimpl_->policy_.initial_delay;

  for (uint32_t attempt = 0; attempt <= retries; ++attempt) {
    response.clear();

    std::chrono::microseconds duration{0};
    const CURLcode res = pimpl_->executeTransfer(duration);

    pimpl_->collectTiming(res, duration, response);
    pimpl_->collectStatusCode(res, response);

    if (res == CURLE_OK && response.isSuccess()) {
      return response;
    }

    // Do not retry if the operation was aborted by the user
    const bool aborted =
        res == CURLE_ABORTED_BY_CALLBACK || ctx.aborted_by_user || stop_token.stop_requested();
    const bool transient = internal::isTransientError(res, response.getStatusCode());
    if (shouldStopRetrying(aborted, transient, attempt, retries)) {
      return response;
    }

    applyBackoffDelay(pimpl_->policy_.strategy, current_delay, pimpl_->policy_.max_delay);
  }

  return response;
}

void HttpClient::setNetworkConfig(const NetworkConfig& config) {
  CURL* handle = pimpl_->get();
  curl_easy_setopt(handle, CURLOPT_USERAGENT, config.user_agent.c_str());
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, config.follow_redirects ? 1L : 0L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, config.ssl_verify ? 1L : 0L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, config.ssl_verify ? 2L : 0L);

  // Connection timeout vs general timeout anti-stall
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS,
                   static_cast<long>(config.connect_timeout.count()));

  // Anti-stall safeguards: abort if < 10KB/s for 3s
  curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 10240L);
  curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, 3L);
}

void HttpClient::setRetryPolicy(const RetryPolicy& policy) { pimpl_->policy_ = policy; }

}  // namespace iptv::network
