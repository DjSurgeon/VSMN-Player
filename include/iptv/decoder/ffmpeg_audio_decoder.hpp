#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "iptv/decoder/i_audio_decoder.hpp"

namespace iptv::decoder {

/**
 * @brief RAII wrapper for FFmpeg audio decoding and resampling.
 *
 * Implements IAudioDecoder using the Pimpl idiom to prevent FFmpeg C headers from leaking
 * into the public interface. Decodes compressed audio and resamples it to standard 48kHz,
 * 16-bit, stereo PCM.
 *
 * @par Moved-from state
 * A moved-from decoder holds no resources. Any subsequent call other than destruction or
 * reassignment throws std::logic_error instead of dereferencing a null implementation.
 */
class FFmpegAudioDecoder : public IAudioDecoder {
 public:
  /**
   * @brief Constructs an audio decoder for the specified codec.
   * @param codec_hint The FFmpeg codec name (e.g., "aac", "mp3").
   * @throws std::runtime_error if the codec cannot be found or opened.
   */
  explicit FFmpegAudioDecoder(const std::string& codec_hint);

  ~FFmpegAudioDecoder() override;

  // Move-only semantics. Implemented in the source file where Impl is complete.
  FFmpegAudioDecoder(const FFmpegAudioDecoder&) = delete;
  FFmpegAudioDecoder& operator=(const FFmpegAudioDecoder&) = delete;
  FFmpegAudioDecoder(FFmpegAudioDecoder&&) noexcept;
  FFmpegAudioDecoder& operator=(FFmpegAudioDecoder&&) noexcept;

  std::vector<DecodedFrame> decode(std::span<const uint8_t> compressed_data) override;
  std::vector<DecodedFrame> flush() override;
  CodecInfo getCodecInfo() const override;

 private:
  struct Impl;

  /**
   * @brief Returns the active implementation.
   * @throws std::logic_error if this decoder has been moved from.
   */
  const Impl& requireImpl() const;
  Impl& requireMutableImpl();

  std::unique_ptr<Impl> pimpl_;
};

}  // namespace iptv::decoder
