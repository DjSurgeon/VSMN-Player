#include "iptv/network/http_client.hpp"
#include <stdexcept>

namespace iptv::network {

// Define the hidden implementation class (currently a skeleton)
class HttpClient::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    // We will add curl easy handle initialization here later
};

// Lifecycle methods (must be defined here where Impl is complete)

HttpClient::HttpClient() : pimpl_(std::make_unique<Impl>()) {}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&& other) noexcept 
    : IHttpClient(), pimpl_(std::move(other.pimpl_)) {}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept {
    if (this != &other) {
        pimpl_ = std::move(other.pimpl_);
    }
    return *this;
}

// Interface implementations (Skeleton)

HttpResponse HttpClient::download(const std::string& /*url*/, std::chrono::milliseconds /*timeout*/) {
    // Skeleton implementation: Will be implemented in NET-05
    throw std::logic_error("HttpClient::download is not yet implemented");
}

void HttpClient::setRetryPolicy(const RetryPolicy& /*policy*/) {
    // Skeleton implementation: Will be implemented in NET-05
}

} // namespace iptv::network
