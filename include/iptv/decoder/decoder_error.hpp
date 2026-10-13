#pragma once

#include <stdexcept>
#include <string>

namespace iptv::decoder {

/**
 * @brief Represents specific errors that can occur during decoding.
 */
enum class DecoderError {
    CodecNotFound,        ///< The requested codec could not be found or loaded.
    AllocationFailed,     ///< Memory allocation failed during decoder initialization or operation.
    CodecOpenFailed,      ///< The codec could be found but failed to open.
    InvalidFormat,        ///< The input format is invalid or unsupported.
    CorruptData,          ///< The input data was corrupt or could not be parsed.
    InitializationFailed, ///< General initialization failure.
    InvalidCodec,         ///< An invalid or incompatible codec was provided.
    UnknownError,         ///< An unknown error occurred.
    CorruptInput,         ///< The input stream is corrupt.
    Unknown               ///< Fallback for undetermined error states.
};

/**
 * @brief Exception thrown when a decoder error occurs.
 */
class DecoderException : public std::runtime_error {
public:
    /**
     * @brief Constructs a DecoderException.
     * @param error The specific DecoderError that occurred.
     * @param message A human-readable description of the error.
     */
    DecoderException(DecoderError error, const std::string& message)
        : std::runtime_error(message), error_(error) {}

    /**
     * @brief Retrieves the specific DecoderError.
     * @return The decoder error code.
     */
    [[nodiscard]] DecoderError error() const noexcept { return error_; }

private:
    DecoderError error_; ///< The specific decoder error code.
};

} // namespace iptv::decoder
