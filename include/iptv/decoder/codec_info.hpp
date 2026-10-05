#pragma once

#include <cstdint>
#include <string>

namespace iptv::decoder {

/**
 * @brief Descriptive metadata for the codec a decoder is configured with.
 *
 * Move-only for the same reason as #DecodedFrame: the info object crosses thread and queue
 * boundaries on the media path, so it is handed over by move and never duplicated. Implementations
 * build it in place when the codec is opened and return it by move.
 */
struct CodecInfo {
  CodecInfo() = default;
  ~CodecInfo() = default;

  // Move-only.
  CodecInfo(const CodecInfo&) = delete;
  CodecInfo& operator=(const CodecInfo&) = delete;
  CodecInfo(CodecInfo&&) noexcept = default;
  CodecInfo& operator=(CodecInfo&&) noexcept = default;

  std::string name;     ///< Codec identifier, e.g. "h264" or "aac".
  std::string profile;  ///< Codec profile or level, e.g. "High"; empty when unconstrained.
  int64_t bitrate{0};   ///< Nominal bitrate in bits per second; 0 when unknown.
  bool hardware_accelerated{false};  ///< True when decoding runs on a hardware engine.

  // Video parameters; zero for audio-only codecs.
  int width{0};     ///< Decoded picture width in pixels.
  int height{0};    ///< Decoded picture height in pixels.
  double fps{0.0};  ///< Frame rate in frames per second; 0 when unknown.

  // Audio parameters; zero for video-only codecs.
  int sample_rate{0};  ///< Samples per second.
  int channels{0};     ///< Channel count.
};

}  // namespace iptv::decoder
