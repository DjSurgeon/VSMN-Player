#pragma once

#include <span>
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
   * @param compressed_data Raw bytes of the compressed packet. Passed as a span to avoid
   *                        copying; the caller retains ownership and guarantees the pointed-to
   *                        bytes outlive the call.
   * @return std::vector<DecodedFrame> The uncompressed audio frames produced, in output order.
   *         Empty if the packet produced no frame.
   */
  virtual std::vector<DecodedFrame> decode(std::span<const uint8_t> compressed_data) = 0;

  /**
   * @brief Flushes internal decoder buffers, draining any delayed frames.
   * @return std::vector<DecodedFrame> Remaining frames in output order, empty if none remain.
   */
  virtual std::vector<DecodedFrame> flush() = 0;

  /**
   * @brief Retrieves active codec information.
   * @return CodecInfo details about the audio codec.
   */
  virtual CodecInfo getCodecInfo() const = 0;
};

}  // namespace iptv::decoder
