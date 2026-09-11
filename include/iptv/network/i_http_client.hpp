#pragma once

#include <chrono>
#include <string>

#include "iptv/network/http_network_config.hpp"
#include "iptv/network/http_response.hpp"
#include "iptv/network/http_retry_policy.hpp"

namespace iptv::network {

/**
 * @brief Abstract base interface for HTTP clients.
 *
 * Defines the contract for downloading data over HTTP. Designed to allow
 * dependency injection and mocking for higher-level components (like HLS
 * parsers).
 */
class IHttpClient {
 public:
  /**
   * @brief Virtual destructor to ensure proper cleanup of derived classes.
   */
  virtual ~IHttpClient() = default;

  // Rule of 5: Interfaces should not be copyable or movable by value
  IHttpClient(const IHttpClient&) = delete;
  IHttpClient& operator=(const IHttpClient&) = delete;
  IHttpClient(IHttpClient&&) = delete;
  IHttpClient& operator=(IHttpClient&&) = delete;

 protected:
  // Allow derived classes to construct
  IHttpClient() = default;

 public:
  /**
   * @brief Downloads data from the specified URL.
   *
   * @param url The endpoint to download from.
   * @param timeout The maximum time allowed for the request.
   * @return HttpResponse The result of the HTTP request.
   */
  virtual HttpResponse download(const std::string& url, std::chrono::milliseconds timeout) = 0;

  /**
   * @brief Configures global network options (TLS, User-Agent, redirects).
   *
   * @param config The network configuration to apply.
   */
  virtual void setNetworkConfig(const NetworkConfig& config) = 0;

  /**
   * @brief Configures retry behavior for failed requests.
   *
   * @param policy The configuration detailing backoff strategy and max retries.
   */
  virtual void setRetryPolicy(const RetryPolicy& policy) = 0;
};

}  // namespace iptv::network
