#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace iptv::decoder {

/**
 * @brief Categorization of errors reported by the FFmpeg decoder wrappers.
 */
enum class DecoderError : uint8_t {
    InvalidCodec,
    AllocationFailed,
    CorruptInput,
    EndOfStream,
    UnknownError
};

/**
 * @brief Human-readable label for an error category.
 */
[[nodiscard]] inline const char* describe(DecoderError error) noexcept {
    switch (error) {
        case DecoderError::InvalidCodec:     return "invalid codec";
        case DecoderError::AllocationFailed: return "allocation failed";
        case DecoderError::CorruptInput:     return "corrupt input";
        case DecoderError::EndOfStream:      return "end of stream";
        case DecoderError::UnknownError:     return "unknown error";
    }
    return "unknown decoder error";
}

/**
 * @brief Exception carrying a categorized error code for decoder failures.
 */
class DecoderException : public std::runtime_error {
 public:
    explicit DecoderException(DecoderError error, const std::string& message = "")
        : std::runtime_error(message), error_(error) {}

    ~DecoderException() noexcept override = default;

    DecoderException(const DecoderException&) = default;
    DecoderException& operator=(const DecoderException&) = default;
    DecoderException(DecoderException&&) noexcept = default;
    DecoderException& operator=(DecoderException&&) noexcept = default;

    [[nodiscard]] inline DecoderError code() const noexcept { return error_; }

  private:
    DecoderError error_;
};

}  // namespace iptv::decoder
