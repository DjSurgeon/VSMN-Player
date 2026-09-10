#pragma once

#include "iptv/network/i_http_client.hpp"
#include <memory>
#include <string>

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
    HttpClient(HttpClient&&) noexcept;

    /**
     * @brief Move assignment. Defined in source file.
     */
    HttpClient& operator=(HttpClient&&) noexcept;

    // IHttpClient interface implementation
    HttpResponse download(const std::string& url, std::chrono::milliseconds timeout) override;
    void setRetryPolicy(const RetryPolicy& policy) override;

private:
    class Impl; // Forward declaration of the hidden implementation
    std::unique_ptr<Impl> pimpl_;
};

} // namespace iptv::network
