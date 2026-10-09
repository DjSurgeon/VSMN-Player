#pragma once

#include <stdexcept>
#include <string>

namespace iptv::decoder {

enum class DecoderError {
    CodecNotFound,
    AllocationFailed,
    CodecOpenFailed,
    InvalidFormat,
    CorruptData,
    InitializationFailed,
    InvalidCodec,
    UnknownError,
    CorruptInput,
    Unknown
};

class DecoderException : public std::runtime_error {
public:
    DecoderException(DecoderError error, const std::string& message)
        : std::runtime_error(message), error_(error) {}

    [[nodiscard]] DecoderError error() const noexcept { return error_; }

private:
    DecoderError error_;
};

} // namespace iptv::decoder
