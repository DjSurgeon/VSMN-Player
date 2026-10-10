#pragma once

#include <span>
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
   * @param compressed_data Raw bytes of the compressed packet. Passed as a span to avoid
   *                        copying; the caller retains ownership and guarantees the pointed-to
   *                        bytes outlive the call.
   * @return std::vector<DecodedFrame> The uncompressed video frames produced, in output order.
   *         Empty if the packet produced no frame.
   */
  virtual std::vector<DecodedFrame> decode(std::span<const uint8_t> compressed_data, int64_t pts_us = -1, int64_t dts_us = -1) = 0;

  /**
   * @brief Flushes internal decoder buffers, draining any delayed frames.
   * @return std::vector<DecodedFrame> Remaining frames in output order, empty if none remain.
   */
  virtual std::vector<DecodedFrame> flush() = 0;

  /**
   * @brief Retrieves active codec information.
   * @return CodecInfo details about the video codec.
   */
  virtual CodecInfo getCodecInfo() const = 0;
};

}  // namespace iptv::decoder
