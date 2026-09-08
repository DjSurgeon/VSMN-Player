#pragma once

#include "iptv/network/http_status_code.hpp"
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
     * @brief Extensible metadata for HTTP responses.
     */
    struct Metadata {
        std::map<std::string, std::string> headers;
        std::optional<std::string> content_type;
        std::optional<std::string> final_url; // After redirects
        int retry_count{0};
        bool was_redirected{false};
        int64_t timestamp{0}; // Epoch time when received

        Metadata() = default;
        ~Metadata() = default;

        // Force move-only for metadata as well to prevent accidental copies of large headers
        Metadata(const Metadata&) = delete;
        Metadata& operator=(const Metadata&) = delete;
        Metadata(Metadata&&) noexcept = default;
        Metadata& operator=(Metadata&&) noexcept = default;
    };

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
          bytes_downloaded_(0),
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
        if (!data || size == 0) return;
        body_.insert(body_.end(), data, data + size);
        bytes_downloaded_ += size;
    }

    // Accessors - const-correctness enforced
    
    [[nodiscard]] HttpStatusCode getStatusCode() const noexcept { 
        return status_code_; 
    }
    
    [[nodiscard]] const std::vector<uint8_t>& getBody() const noexcept {
        return body_;
    }
    
    [[nodiscard]] std::size_t getBytesDownloaded() const noexcept {
        return bytes_downloaded_;
    }
    
    [[nodiscard]] std::chrono::milliseconds getLatency() const noexcept {
        return latency_;
    }

    [[nodiscard]] const Metadata& getMetadata() const noexcept {
        return metadata_;
    }

    Metadata& getMutableMetadata() noexcept {
        return metadata_;
    }

    // --- Placeholders for Integrity Verification (Phase 2/3) ---

    /**
     * @brief Compute SHA256 of the body (Placeholder).
     * @return Hex string of SHA256 hash.
     */
    [[nodiscard]] std::string computeBodyHash() const noexcept {
        return "placeholder_hash";
    }

    /**
     * @brief Verify body integrity against expected hash (Placeholder).
     * @param expected_hash Expected SHA256 hex string.
     * @return true if matches.
     */
    [[nodiscard]] bool verifyBodyHash(std::string_view expected_hash) const noexcept {
        return computeBodyHash() == expected_hash;
    }

private:
    HttpStatusCode status_code_;
    std::vector<uint8_t> body_;
    std::size_t bytes_downloaded_;
    std::chrono::milliseconds latency_;
    Metadata metadata_;

    // Allow builder to inject raw constructed objects if needed
    friend class HttpResponseBuilder;
};

/**
 * @brief Builder pattern for HttpResponse to ensure valid and fluent construction.
 */
class HttpResponseBuilder {
public:
    HttpResponseBuilder() : response_(HttpStatusCode::Unknown) {}

    HttpResponseBuilder& withStatusCode(HttpStatusCode code) {
        response_.status_code_ = code;
        return *this;
    }

    HttpResponseBuilder& withPreallocatedBody(std::size_t expected_size) {
        response_.body_.reserve(expected_size);
        return *this;
    }

    HttpResponseBuilder& withBody(std::vector<uint8_t> body) {
        response_.body_ = std::move(body);
        response_.bytes_downloaded_ = response_.body_.size();
        return *this;
    }

    HttpResponseBuilder& withLatency(std::chrono::milliseconds latency) {
        response_.latency_ = latency;
        return *this;
    }

    HttpResponseBuilder& withMetadata(HttpResponse::Metadata metadata) {
        response_.metadata_ = std::move(metadata);
        return *this;
    }

    /**
     * @brief Validates and builds the final response.
     * @return The constructed HttpResponse.
     * @throws std::invalid_argument if the state is invalid.
     */
    HttpResponse build() {
        if (response_.status_code_ == HttpStatusCode::Unknown) {
            throw std::invalid_argument("HttpResponseBuilder: Status code must be explicitly set.");
        }
        return std::move(response_);
    }

private:
    HttpResponse response_;
};

} // namespace iptv::network
