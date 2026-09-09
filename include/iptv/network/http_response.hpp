#pragma once

#include "iptv/network/http_status_code.hpp"
#include <chrono>
#include <cstdint>
#include <vector>

namespace iptv::network {

/**
 * @brief Represents the outcome of an HTTP request with strict move-only semantics.
 * 
 * Enforces Zero-Copy architecture by deleting copy operations. It uses an explicit
 * pre-allocation model for the binary payload to avoid reallocations during large 
 * MPEG-TS segment downloads.
 */
class HttpResponse {
public:
    /**
     * @brief Explicit constructor with pre-allocation hint.
     * 
     * @param code HTTP status code.
     * @param expected_body_size Pre-allocation size for the payload vector.
     * @param latency Request latency.
     */
    explicit HttpResponse(
        HttpStatusCode code,
        std::size_t expected_body_size = 0,
        std::chrono::milliseconds latency = std::chrono::milliseconds{0})
        : status_code_(code),
          latency_(latency) 
    {
        if (expected_body_size > 0) {
            body_.reserve(expected_body_size);
        }
    }

    ~HttpResponse() = default;

    // Rule of 5: Move-only semantics (Zero-Copy)
    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;
    HttpResponse(HttpResponse&&) noexcept = default;
    HttpResponse& operator=(HttpResponse&&) noexcept = default;

    /**
     * @brief Appends data to the response body without full copy reallocations.
     * 
     * @param data Pointer to the raw bytes.
     * @param size Number of bytes to append.
     */
    void appendToBody(const uint8_t* data, std::size_t size) {
        if (data == nullptr || size == 0) {
            return;
        }
        body_.insert(body_.end(), data, data + size);
    }

    // Accessors - const-correctness enforced
    
    [[nodiscard]] HttpStatusCode getStatusCode() const noexcept { 
        return status_code_; 
    }
    
    [[nodiscard]] const std::vector<uint8_t>& getBody() const noexcept {
        return body_;
    }
    
    [[nodiscard]] std::size_t getBytesDownloaded() const noexcept {
        return body_.size();
    }
    
    [[nodiscard]] std::chrono::milliseconds getLatency() const noexcept {
        return latency_;
    }

private:
    HttpStatusCode status_code_;
    std::vector<uint8_t> body_;
    std::chrono::milliseconds latency_;
};

} // namespace iptv::network
