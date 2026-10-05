#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace iptv::decoder {

/**
 * @brief Descriptive metadata for the codec a decoder is configured with.
 *
 * Move-only for the same reason as #DecodedFrame: the info object crosses thread and queue
 * boundaries on the media path, so it is handed over by move and never duplicated. An
 * implementation builds it from what the codec reported at open time and returns it by move.
 *
 * Like a frame, a codec is a picture codec or an audio codec, never both, and the partition is a
 * construction invariant: there is exactly one constructor per media type and no default
 * constructor. Choosing the constructor is how an implementation declares which parameter group
 * applies, and the group belonging to the other media type stays zeroed. There is therefore no
 * reachable state in which, say, a video codec advertises 48 kHz stereo.
 */
struct CodecInfo {
  ~CodecInfo() = default;

  /**
   * @brief Builds the description of a picture codec.
   *
   * Parameter names are prefixed so they do not shadow the public fields they initialise.
   *
   * @param codec_name                 Codec identifier, e.g. "h264".
   * @param codec_profile              Codec profile or level, e.g. "High"; empty when
   *                                  unconstrained.
   * @param codec_bitrate              Nominal bitrate in bits per second; 0 when unknown.
   * @param codec_hardware_accelerated True when decoding runs on a hardware engine.
   * @param codec_width                Decoded picture width in pixels.
   * @param codec_height               Decoded picture height in pixels.
   * @param codec_fps                  Frame rate in frames per second; 0 when unknown.
   *
   * The audio group (#sample_rate, #channels) is left zeroed: an audio codec is built with the
   * other constructor, so this one cannot describe an audio codec.
   */
  CodecInfo(std::string codec_name, std::string codec_profile, int64_t codec_bitrate,
            bool codec_hardware_accelerated, int codec_width, int codec_height, double codec_fps)
      : name(std::move(codec_name)),
        profile(std::move(codec_profile)),
        bitrate(codec_bitrate),
        hardware_accelerated(codec_hardware_accelerated),
        width(codec_width),
        height(codec_height),
        fps(codec_fps) {}

  /**
   * @brief Builds the description of an audio codec.
   *
   * Parameter names are prefixed so they do not shadow the public fields they initialise.
   *
   * @param codec_name                 Codec identifier, e.g. "aac".
   * @param codec_profile              Codec profile or level, e.g. "LC"; empty when
   *                                  unconstrained.
   * @param codec_bitrate              Nominal bitrate in bits per second; 0 when unknown.
   * @param codec_hardware_accelerated True when decoding runs on a hardware engine.
   * @param codec_sample_rate          Samples per second.
   * @param codec_channels             Channel count.
   *
   * The picture group (#width, #height, #fps) is left zeroed: a picture codec is built with the
   * other constructor, so this one cannot describe a video codec.
   */
  CodecInfo(std::string codec_name, std::string codec_profile, int64_t codec_bitrate,
            bool codec_hardware_accelerated, int codec_sample_rate, int codec_channels)
      : name(std::move(codec_name)),
        profile(std::move(codec_profile)),
        bitrate(codec_bitrate),
        hardware_accelerated(codec_hardware_accelerated),
        sample_rate(codec_sample_rate),
        channels(codec_channels) {}

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
