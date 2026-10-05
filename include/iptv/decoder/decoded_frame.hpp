#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace iptv::decoder {

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
 * @brief A single decoded multimedia frame in framework-agnostic form.
 *
 * The frame owns its sample bytes outright: it holds the only reference to #data, so handing a
 * frame to the render hot path is a pointer move and never a payload copy. Copying is deleted
 * and move is defaulted, which makes that guarantee a compile-time property rather than a code
 * review convention. The payload arrives by rvalue reference and is moved straight into #data,
 * so decoding hands over a buffer it has just filled instead of copying it into the frame.
 *
 * A frame is video or audio, never both and never neither, and that partition is a construction
 * invariant rather than a convention: there are exactly two constructors, one per media type,
 * and no default constructor. Choosing the constructor is how a caller declares which field
 * group is authoritative, the matching #media_type is stamped by the constructor rather than
 * supplied by the caller, and the field group belonging to the other media type stays zeroed.
 * There is therefore no reachable state in which a frame claims audio properties while also
 * carrying a picture geometry, or in which a video frame exists without its plane strides.
 *
 * Timing is expressed as presentation time (#pts) plus the on-screen duration (#duration) of the
 * frame, so a consumer can resynchronise audio against video from the frame alone.
 */
struct DecodedFrame {
  /**
   * @brief Number of stride slots, sized for YUV420P (Y, U, V) plus one reserved slot for a
   *        packed RGB or alpha plane.
   */
  static constexpr std::size_t kLinesizeSlots = 4;

  ~DecodedFrame() = default;

  /**
   * @brief Builds a picture frame with its plane strides, stamping #media_type as Video.
   *
   * Parameter names are prefixed so they do not shadow the public fields they initialise.
   *
   * @param frame_pts          Presentation time stamp in the stream's own time base.
   * @param frame_duration     On-screen duration, same time base as @p frame_pts.
   * @param frame_width        Visible width in pixels.
   * @param frame_height       Visible height in pixels.
   * @param frame_pixel_format Layout of #data.
   * @param frame_linesize     Per-plane stride in bytes, one slot per plane of @p
   * frame_pixel_format.
   * @param frame_data         Sample bytes, taken over by move.
   *
   * The audio group (#sample_rate, #channels, #audio_format) is left zeroed: an audio frame is
   * built with the other constructor, so this one cannot describe an audio payload.
   */
  DecodedFrame(int64_t frame_pts, int64_t frame_duration, int frame_width, int frame_height,
               PixelFormat frame_pixel_format,
               const std::array<int, kLinesizeSlots>& frame_linesize,
               std::vector<uint8_t>&& frame_data)
      : pts(frame_pts),
        duration(frame_duration),
        media_type(MediaType::Video),
        width(frame_width),
        height(frame_height),
        pixel_format(frame_pixel_format),
        linesize(frame_linesize),
        data(std::move(frame_data)) {}

  /**
   * @brief Builds an audio frame with its sample layout, stamping #media_type as Audio.
   *
   * Parameter names are prefixed so they do not shadow the public fields they initialise.
   *
   * @param frame_pts          Presentation time stamp in the stream's own time base.
   * @param frame_duration     Audible duration, same time base as @p frame_pts.
   * @param frame_sample_rate  Samples per second.
   * @param frame_channels     Channel count.
   * @param frame_audio_format Sample layout of #data.
   * @param frame_data         PCM bytes, taken over by move.
   *
   * The picture group (#width, #height, #pixel_format, #linesize) is left zeroed: a picture is
   * built with the other constructor, so this one cannot describe a video payload.
   */
  DecodedFrame(int64_t frame_pts, int64_t frame_duration, int frame_sample_rate, int frame_channels,
               AudioFormat frame_audio_format, std::vector<uint8_t>&& frame_data)
      : pts(frame_pts),
        duration(frame_duration),
        media_type(MediaType::Audio),
        sample_rate(frame_sample_rate),
        channels(frame_channels),
        audio_format(frame_audio_format),
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

  // Video geometry. Stride is tracked per plane because planar YUV420P is not tightly packed:
  // linesize[0] is the Y stride, linesize[1] and linesize[2] the half-resolution chroma strides,
  // and linesize[3] is reserved. Pixel rows therefore must be addressed by stride, never by
  // multiplying row index by width.
  int width{0};                                    ///< Visible width in pixels (0 for audio).
  int height{0};                                   ///< Visible height in pixels (0 for audio).
  PixelFormat pixel_format{PixelFormat::Unknown};  ///< Layout of #data for video frames.
  std::array<int, kLinesizeSlots> linesize{0, 0, 0, 0};  ///< Per-plane stride in bytes.

  // Audio geometry.
  int sample_rate{0};                              ///< Samples per second (0 for video).
  int channels{0};                                 ///< Channel count (0 for video).
  AudioFormat audio_format{AudioFormat::Unknown};  ///< Sample layout for audio frames.

  std::vector<uint8_t> data;  ///< Owned sample bytes: planes in order for video, PCM for audio.
};

}  // namespace iptv::decoder
