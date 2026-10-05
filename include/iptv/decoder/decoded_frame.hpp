#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace iptv::decoder {

/**
 * @brief Number of stride slots, sized for YUV420P (Y, U, V) plus one reserved slot for a
 *        packed RGB or alpha plane.
 */
inline constexpr std::size_t kLinesizeSlots = 4;

/**
 * @brief Nature of the payload carried by a frame.
 *
 * Selects which field group of #DecodedFrame is authoritative; unused groups stay zeroed.
 */
enum class MediaType : uint8_t {
  Video,  ///< Picture payload, described by width/height/pixel_format/linesize.
  Audio,  ///< PCM payload, described by sample_rate/channels/audio_format.
};

/**
 * @brief Pixel layout of a video frame buffer.
 *
 * Unknown is the zero-value default so the fields of the media type a frame was not built for
 * never claim a layout that frame does not have.
 */
enum class PixelFormat : uint8_t {
  YUV420P,  ///< Planar 8-bit YUV 4:2:0: Y plane, then U plane, then V plane.
  RGB24,    ///< Packed 8-bit R, G, B triples.
  Unknown,  ///< Undetermined layout.
};

/**
 * @brief Sample layout of an audio frame buffer.
 *
 * Unknown is the zero-value default so the fields of the media type a frame was not built for
 * never claim a layout that frame does not have.
 */
enum class AudioFormat : uint8_t {
  PCM_S16_48KHZ,  ///< Signed 16-bit little-endian PCM at 48000 Hz.
  PCM_S16_44KHZ,  ///< Signed 16-bit little-endian PCM at 44100 Hz.
  Unknown,        ///< Undetermined layout.
};

/**
 * @brief When a frame is meant to be shown, and for how long.
 *
 * Grouped because both values share one time base: passing them together makes it impossible to
 * hand a duration measured in a different base than the presentation time it is paired with.
 */
struct FrameTiming {
  int64_t pts{0};       ///< Presentation Time Stamp: when the frame is meant to be shown.
  int64_t duration{0};  ///< How long the frame stays on screen, same time base as #pts.
};

/**
 * @brief Picture geometry of a video frame: size, pixel layout and per-plane strides.
 *
 * Stride is tracked per plane because planar YUV420P is not tightly packed: #linesize is the Y
 * stride, the next two slots are the half-resolution chroma strides, and the last is reserved.
 * Pixel rows must be addressed by stride, never by multiplying row index by width.
 *
 * Grouped because the strides describe the width: a caller cannot plausibly pass the strides of
 * one picture alongside the geometry of another.
 */
struct VideoFrameGeometry {
  int width{0};                                          ///< Visible width in pixels.
  int height{0};                                         ///< Visible height in pixels.
  PixelFormat pixel_format{PixelFormat::Unknown};        ///< Layout of the sample bytes.
  std::array<int, kLinesizeSlots> linesize{0, 0, 0, 0};  ///< Per-plane stride in bytes.
};

/**
 * @brief Sample layout of an audio frame: rate, channel count and PCM encoding.
 *
 * Grouped because the encoding is a function of the rate: #audio_format describes the samples
 * delivered at #sample_rate, so the three are not independently choosable.
 */
struct AudioFrameGeometry {
  int sample_rate{0};                              ///< Samples per second.
  int channels{0};                                 ///< Channel count.
  AudioFormat audio_format{AudioFormat::Unknown};  ///< Sample encoding.
};

/**
 * @brief A single decoded multimedia frame in framework-agnostic form.
 *
 * The frame owns its sample bytes outright: it holds the only reference to #data, so handing a
 * frame to the render hot path is a pointer move and never a payload copy. Copying is deleted
 * and move is defaulted, which makes that guarantee a compile-time property rather than a code
 * review convention. The payload arrives by rvalue reference and is moved straight into #data,
 * so decoding hands over a buffer it has just filled instead of copying it into the frame.
 *
 * A frame is video or audio, never both and never neither, and that partition is a construction
 * invariant rather than a convention: there are exactly two constructors, one per geometry type,
 * and no default constructor. Choosing the constructor is how a caller declares which field
 * group is authoritative, the matching #media_type is stamped by the constructor rather than
 * supplied by the caller, and the field group belonging to the other media type stays zeroed.
 * There is therefore no reachable state in which a frame claims audio properties while also
 * carrying a picture geometry, or in which a video frame exists without its plane strides.
 */
struct DecodedFrame {
  /**
   * @brief Number of stride slots, kept as a member so callers can size their own arrays.
   */
  static constexpr std::size_t kLinesizeSlots = decoder::kLinesizeSlots;

  ~DecodedFrame() = default;

  /**
   * @brief Builds a picture frame with its plane strides, stamping #media_type as Video.
   *
   * The audio group (#sample_rate, #channels, #audio_format) is left zeroed: an audio frame is
   * built with the other constructor, so this one cannot describe an audio payload.
   *
   * @param timing Presentation time and on-screen duration, in the stream's own time base.
   * @param geometry Picture size, pixel layout and per-plane strides.
   * @param frame_data Sample bytes, taken over by move.
   */
  DecodedFrame(FrameTiming timing, VideoFrameGeometry geometry, std::vector<uint8_t>&& frame_data)
      : pts(timing.pts),
        duration(timing.duration),
        media_type(MediaType::Video),
        width(geometry.width),
        height(geometry.height),
        pixel_format(geometry.pixel_format),
        linesize(geometry.linesize),
        data(std::move(frame_data)) {}

  /**
   * @brief Builds an audio frame with its sample layout, stamping #media_type as Audio.
   *
   * The picture group (#width, #height, #pixel_format, #linesize) is left zeroed: a picture is
   * built with the other constructor, so this one cannot describe a video payload.
   *
   * @param timing Presentation time and audible duration, in the stream's own time base.
   * @param geometry Sample rate, channel count and PCM encoding.
   * @param frame_data PCM bytes, taken over by move.
   */
  DecodedFrame(FrameTiming timing, AudioFrameGeometry geometry, std::vector<uint8_t>&& frame_data)
      : pts(timing.pts),
        duration(timing.duration),
        media_type(MediaType::Audio),
        sample_rate(geometry.sample_rate),
        channels(geometry.channels),
        audio_format(geometry.audio_format),
        data(std::move(frame_data)) {}

  // Move-only: the sample payload is transferred, never duplicated.
  DecodedFrame(const DecodedFrame&) = delete;
  DecodedFrame& operator=(const DecodedFrame&) = delete;
  DecodedFrame(DecodedFrame&&) noexcept = default;
  DecodedFrame& operator=(DecodedFrame&&) noexcept = default;

  // Synchronization timing, in the stream's own time base.
  int64_t pts{0};       ///< Presentation Time Stamp: when the frame is meant to be shown.
  int64_t duration{0};  ///< How long the frame stays on screen, same time base as #pts.

  MediaType media_type{MediaType::Video};  ///< Which field group below is authoritative.

  // Video geometry, authoritative for frames built from VideoFrameGeometry.
  int width{0};                                    ///< Visible width in pixels (0 for audio).
  int height{0};                                   ///< Visible height in pixels (0 for audio).
  PixelFormat pixel_format{PixelFormat::Unknown};  ///< Layout of #data for video frames.
  std::array<int, kLinesizeSlots> linesize{0, 0, 0, 0};  ///< Per-plane stride in bytes.

  // Audio geometry, authoritative for frames built from AudioFrameGeometry.
  int sample_rate{0};                              ///< Samples per second (0 for video).
  int channels{0};                                 ///< Channel count (0 for video).
  AudioFormat audio_format{AudioFormat::Unknown};  ///< Sample layout for audio frames.

  std::vector<uint8_t> data;  ///< Owned sample bytes: planes in order for video, PCM for audio.
};

}  // namespace iptv::decoder
