#pragma once

#include <memory>
#include <string>

#include "iptv/decoder/i_video_decoder.hpp"

namespace iptv::decoder {

/**
 * @brief RAII wrapper for FFmpeg video decoding.
 *
 * Implements IVideoDecoder using the Pimpl idiom to prevent FFmpeg C headers from leaking
 * into the public interface. Manages internal allocations (AVCodecContext, AVFrame, AVPacket)
 * ensuring safe destruction and strict move-only semantics.
 */
class FFmpegDecoder : public IVideoDecoder {
 public:
  /**
   * @brief Constructs a decoder for the specified codec.
   * @param codec_hint The FFmpeg codec name (e.g., "h264").
   * @throws std::runtime_error if the codec cannot be found or opened.
   */
  explicit FFmpegDecoder(const std::string& codec_hint);

  ~FFmpegDecoder() override;

  // Move-only semantics. Implemented in the source file where Impl is complete.
  FFmpegDecoder(const FFmpegDecoder&) = delete;
  FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;
  FFmpegDecoder(FFmpegDecoder&&) noexcept;
  FFmpegDecoder& operator=(FFmpegDecoder&&) noexcept;

  std::vector<DecodedFrame> decode(std::span<const uint8_t> compressed_data) override;
  std::vector<DecodedFrame> flush() override;
  CodecInfo getCodecInfo() const override;

 private:
  struct Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace iptv::decoder
