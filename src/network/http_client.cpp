#include "iptv/network/http_client.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace iptv::network {

// libcurl write callback
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (userdata == nullptr) {
        return 0;
    }
    
    auto* response = static_cast<HttpResponse*>(userdata);
    std::size_t total_size = size * nmemb;
    response->appendToBody(reinterpret_cast<const uint8_t*>(ptr), total_size);
    return total_size;
}

// Define the hidden implementation class with strict RAII
class HttpClient::Impl {
public:
    Impl() {
        handle_ = curl_easy_init();
        if (!handle_) {
            throw std::runtime_error("Failed to initialize curl easy handle.");
        }
        curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, writeCallback);
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

void HttpClient::setNetworkConfig(const NetworkConfig& config) {
    curl_easy_setopt(pimpl_->get(), CURLOPT_USERAGENT, config.user_agent.c_str());
    curl_easy_setopt(pimpl_->get(), CURLOPT_FOLLOWLOCATION, config.follow_redirects ? 1L : 0L);
    curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYPEER, config.ssl_verify ? 1L : 0L);
    curl_easy_setopt(pimpl_->get(), CURLOPT_SSL_VERIFYHOST, config.ssl_verify ? 2L : 0L);
    curl_easy_setopt(pimpl_->get(), CURLOPT_TIMEOUT_MS, static_cast<long>(config.timeout.count()));
}

void HttpClient::setRetryPolicy(const RetryPolicy& /*policy*/) {
    // Will be implemented in NET-07
}

} // namespace iptv::network
