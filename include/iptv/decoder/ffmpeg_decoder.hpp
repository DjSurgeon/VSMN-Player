#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "iptv/decoder/i_video_decoder.hpp"

namespace iptv::decoder {

/**
 * @brief RAII wrapper for FFmpeg video decoding.
 *
 * Implements IVideoDecoder using the Pimpl idiom to prevent FFmpeg C headers from leaking
 * into the public interface. Manages internal allocations (AVCodecContext, AVFrame, AVPacket)
 * ensuring safe destruction and strict move-only semantics.
 *
 * @par Moved-from state
 * A moved-from decoder holds no resources. Any subsequent call other than destruction or
 * reassignment throws std::logic_error instead of dereferencing a null implementation.
 */
class FFmpegDecoder : public IVideoDecoder {
 public:
  /**
   * @brief Constructs a decoder for the specified codec.
   * @param codec_hint The FFmpeg codec name (e.g., "h264"). May be empty to enable automatic
   * codec detection from the bitstream.
   * @throws DecoderException if the codec cannot be found or opened, or if auto-detection fails.
   */
  explicit FFmpegDecoder(const std::string& codec_hint);

  ~FFmpegDecoder() override;

  // Move-only semantics. Implemented in the source file where Impl is complete.
  FFmpegDecoder(const FFmpegDecoder&) = delete;
  FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;
  FFmpegDecoder(FFmpegDecoder&&) noexcept;
  FFmpegDecoder& operator=(FFmpegDecoder&&) noexcept;

  /**
   * @brief Decodes one compressed packet, releasing every frame the codec completes on it.
   *
   * A packet does not map one-to-one onto a frame: the codec may hold pictures back for reordering
   * and may release several, or none, for any one packet, so every frame it produces is returned.
   * Frame times are synthesized in milliseconds from the rate the codec reports, because the
   * packets carry no timestamps of their own.
   *
   * @param compressed_data Raw bytes of the compressed packet. Borrowed for the duration of the
   * call and copied into a buffer the codec owns.
   * @return Frames in output order. Empty when the packet yields no picture, which includes the
   * codec rejecting the packet as corrupt: a bad packet is dropped and the decoder stays
   * usable for the next one.
   * @throws DecoderException when FFmpeg fails for a reason other than unusable input, or when
   * the codec produced a picture that is not 8-bit 4:2:0 YUV.
   * @throws std::logic_error when the decoder was moved from, or when it is fed after flush().
   */
  std::vector<DecodedFrame> decode(std::span<const uint8_t> compressed_data, int64_t pts_us = -1, int64_t dts_us = -1) override;

  /**
   * @brief Signals end of stream and drains the frames the codec was still holding.
   * @return Remaining frames in output order. Empty when nothing was buffered, and empty on a
   * repeated call: the first flush closes the codec to further input.
   * @throws DecoderException when FFmpeg fails to drain its buffers.
   * @throws std::logic_error when the decoder was moved from.
   */
  std::vector<DecodedFrame> flush() override;
  CodecInfo getCodecInfo() const override;

 private:
  struct Impl;

  /**
   * @brief Returns the active implementation.
   * @throws std::logic_error if this decoder has been moved from.
   */
  const Impl& requireImpl() const;
  Impl& requireImpl();

  std::unique_ptr<Impl> pimpl_;
};

}  // namespace iptv::decoder