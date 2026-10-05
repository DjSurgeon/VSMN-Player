#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace iptv::decoder {

/**
 * @brief Which codec an instance is, independent of the parameters of its media type.
 *
 * Grouped because these four travel together for every codec: a name without a bitrate, or a
 * bitrate reported under a different name, describes nothing a decoder can act on.
 */
struct CodecIdentity {
  std::string name;     ///< Codec identifier, e.g. "h264" or "aac".
  std::string profile;  ///< Codec profile or level, e.g. "High"; empty when unconstrained.
  int64_t bitrate{0};   ///< Nominal bitrate in bits per second; 0 when unknown.
  bool hardware_accelerated{false};  ///< True when decoding runs on a hardware engine.
};

/**
 * @brief Output geometry of a picture codec.
 */
struct VideoCodecParameters {
  int width{0};     ///< Decoded picture width in pixels.
  int height{0};    ///< Decoded picture height in pixels.
  double fps{0.0};  ///< Frame rate in frames per second; 0 when unknown.
};

/**
 * @brief Output layout of an audio codec.
 */
struct AudioCodecParameters {
  int sample_rate{0};  ///< Samples per second.
  int channels{0};     ///< Channel count.
};

/**
 * @brief Descriptive metadata for the codec a decoder is configured with.
 *
 * Move-only for the same reason as #DecodedFrame: the info object crosses thread and queue
 * boundaries on the media path, so it is handed over by move and never duplicated. An
 * implementation builds it from what the codec reported at open time and returns it by move.
 *
 * Like a frame, a codec is a picture codec or an audio codec, never both, and the partition is a
 * construction invariant: there is exactly one constructor per parameter group and no default
 * constructor. Choosing the constructor is how an implementation declares which parameter group
 * applies, and the group belonging to the other media type stays zeroed. There is therefore no
 * reachable state in which, say, a video codec advertises 48 kHz stereo.
 */
struct CodecInfo {
  ~CodecInfo() = default;

  /**
   * @brief Builds the description of a picture codec.
   *
   * The audio group (#sample_rate, #channels) is left zeroed: an audio codec is built with the
   * other constructor, so this one cannot describe an audio codec.
   *
   * @param identity Name, profile, bitrate and acceleration, shared by every codec.
   * @param parameters Decoded picture geometry and frame rate.
   */
  CodecInfo(CodecIdentity identity, VideoCodecParameters parameters)
      : name(std::move(identity.name)),
        profile(std::move(identity.profile)),
        bitrate(identity.bitrate),
        hardware_accelerated(identity.hardware_accelerated),
        width(parameters.width),
        height(parameters.height),
        fps(parameters.fps) {}

  /**
   * @brief Builds the description of an audio codec.
   *
   * The picture group (#width, #height, #fps) is left zeroed: a picture codec is built with the
   * other constructor, so this one cannot describe a video codec.
   *
   * @param identity Name, profile, bitrate and acceleration, shared by every codec.
   * @param parameters Sample rate and channel count.
   */
  CodecInfo(CodecIdentity identity, AudioCodecParameters parameters)
      : name(std::move(identity.name)),
        profile(std::move(identity.profile)),
        bitrate(identity.bitrate),
        hardware_accelerated(identity.hardware_accelerated),
        sample_rate(parameters.sample_rate),
        channels(parameters.channels) {}

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
