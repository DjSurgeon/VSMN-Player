#pragma once

#include <optional>
#include <vector>

#include "iptv/decoder/codec_info.hpp"
#include "iptv/decoder/decoded_frame.hpp"

namespace iptv::decoder {

/**
 * @brief Pure virtual interface for video decoding.
 */
class IVideoDecoder {
 public:
  virtual ~IVideoDecoder() = default;

  // Rule of 5: Delete copy, allow move
  IVideoDecoder(const IVideoDecoder&) = delete;
  IVideoDecoder& operator=(const IVideoDecoder&) = delete;
  IVideoDecoder(IVideoDecoder&&) noexcept = default;
  IVideoDecoder& operator=(IVideoDecoder&&) noexcept = default;

  IVideoDecoder() = default;

  /**
   * @brief Decodes a compressed video packet.
   * @param compressed_data Raw bytes of the compressed packet.
   * @return DecodedFrame The uncompressed video frame.
   */
  virtual DecodedFrame decode(const std::vector<uint8_t>& compressed_data) = 0;

  /**
   * @brief Flushes internal decoder buffers.
   * @return std::optional<DecodedFrame> Remaining frame if any.
   */
  virtual std::optional<DecodedFrame> flush() = 0;

  /**
   * @brief Retrieves active codec information.
   * @return CodecInfo details about the video codec.
   */
  virtual CodecInfo getCodecInfo() const = 0;
};

}  // namespace iptv::decoder
