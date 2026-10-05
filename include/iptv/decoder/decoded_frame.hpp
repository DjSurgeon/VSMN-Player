#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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
 * Unknown is the zero-value default so a default-constructed frame never claims a layout it
 * does not have.
 */
enum class PixelFormat : uint8_t {
  YUV420P,  ///< Planar 8-bit YUV 4:2:0: Y plane, then U plane, then V plane.
  RGB24,    ///< Packed 8-bit R, G, B triples.
  Unknown,  ///< Undetermined layout.
};

/**
 * @brief Sample layout of an audio frame buffer.
 *
 * Unknown is the zero-value default so a default-constructed frame never claims a layout it
 * does not have.
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
 * review convention. A decoder default-constructs the frame, fills the fields, and then hands it
 * on by move.
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

  DecodedFrame() = default;
  ~DecodedFrame() = default;

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
