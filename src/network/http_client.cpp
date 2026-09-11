#include "iptv/network/http_client.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace iptv::network {

// Define the hidden implementation class with strict RAII
class HttpClient::Impl {
public:
    Impl() {
        handle_ = curl_easy_init();
        if (!handle_) {
            throw std::runtime_error("Failed to initialize curl easy handle.");
        }
    }

    ~Impl() {
        if (handle_) {
            curl_easy_cleanup(handle_);
            handle_ = nullptr;
        }
    }

    // Rule of 5: Prohibit copy/move for Impl to ensure strict lifecycle of raw handle
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    CURL* get() const noexcept { return handle_; }

private:
    CURL* handle_ = nullptr;
};

// Lifecycle methods for HttpClient

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

// Interface implementations (Skeleton for now)

HttpResponse HttpClient::download(const std::string& /*url*/, std::chrono::milliseconds /*timeout*/) {
    // We have the raw handle available here via pimpl_->get() for NET-07
    throw std::logic_error("HttpClient::download is not yet implemented");
}

void HttpClient::setRetryPolicy(const RetryPolicy& /*policy*/) {
    // Will be implemented in NET-07
}

} // namespace iptv::network
