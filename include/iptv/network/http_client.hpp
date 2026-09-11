#pragma once

#include <memory>
#include <string>

#include "iptv/network/i_http_client.hpp"

namespace iptv::network {

/**
 * @brief Concrete implementation of IHttpClient using the Pimpl idiom.
 *
 * This class completely hides the underlying network library (e.g., libcurl)
 * from the public headers, improving compilation times and ABI stability.
 */
class HttpClient final : public IHttpClient {
 public:
  /**
   * @brief Constructs a new HTTP client.
   */
  HttpClient();

  /**
   * @brief Destroys the HTTP client.
   * Defined in the source file due to std::unique_ptr with incomplete type.
   */
  ~HttpClient() override;

  // Rule of 5: Move-only semantics
  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  /**
   * @brief Move constructor. Defined in source file.
   */
  HttpClient(HttpClient&& other) noexcept;

  /**
   * @brief Move assignment. Defined in source file.
   */
  HttpClient& operator=(HttpClient&& other) noexcept;

  // IHttpClient interface implementation
  HttpResponse download(const std::string& url,
                        std::chrono::milliseconds timeout =
                            std::chrono::milliseconds{0}) override;

  void setNetworkConfig(const NetworkConfig& config) override;

  void setRetryPolicy(const RetryPolicy& policy) override;

 private:
  class Impl;  // Forward declaration of the hidden implementation
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace iptv::network
