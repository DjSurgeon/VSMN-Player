#pragma once

#include <optional>
#include <vector>

#include "iptv/decoder/codec_info.hpp"
#include "iptv/decoder/decoded_frame.hpp"

namespace iptv::decoder {

/**
 * @brief Pure virtual interface for audio decoding.
 */
class IAudioDecoder {
 public:
  virtual ~IAudioDecoder() = default;

  // Rule of 5: Delete copy, allow move
  IAudioDecoder(const IAudioDecoder&) = delete;
  IAudioDecoder& operator=(const IAudioDecoder&) = delete;
  IAudioDecoder(IAudioDecoder&&) noexcept = default;
  IAudioDecoder& operator=(IAudioDecoder&&) noexcept = default;

  IAudioDecoder() = default;

  /**
   * @brief Decodes a compressed audio packet.
   * @param compressed_data Raw bytes of the compressed packet.
   * @return DecodedFrame The uncompressed audio frame.
   */
  virtual DecodedFrame decode(const std::vector<uint8_t>& compressed_data) = 0;

  /**
   * @brief Flushes internal decoder buffers.
   * @return std::optional<DecodedFrame> Remaining frame if any.
   */
  virtual std::optional<DecodedFrame> flush() = 0;

  /**
   * @brief Retrieves active codec information.
   * @return CodecInfo details about the audio codec.
   */
  virtual CodecInfo getCodecInfo() const = 0;
};

}  // namespace iptv::decoder
