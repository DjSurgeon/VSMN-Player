#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "iptv/decoder/decoded_frame.hpp"

namespace {

using iptv::decoder::AudioFormat;
using iptv::decoder::AudioFrameGeometry;
using iptv::decoder::DecodedFrame;
using iptv::decoder::FrameTiming;
using iptv::decoder::PixelFormat;
using iptv::decoder::VideoFrameGeometry;

/**
 * @brief Builds a planar YUV420P video frame with coherent plane strides.
 *
 * Owns the YUV420P-specific geometry, deriving the half-resolution chroma strides from the
 * width, which is exactly the invariant the render path relies on. Everything else - including
 * the fact that this is a picture and not a sound - is the constructor's job, and the audio
 * fields stay zeroed because no one can ask for them here.
 */
DecodedFrame makeVideoFrame(std::vector<uint8_t> payload, int64_t pts, int width, int height,
                            int64_t duration = 3000) {
  const VideoFrameGeometry geometry{
      .width = width,
      .height = height,
      .pixel_format = PixelFormat::YUV420P,
      .linesize = std::array<int, DecodedFrame::kLinesizeSlots>{width, width / 2, width / 2, 0},
  };
  return DecodedFrame(FrameTiming{.pts = pts, .duration = duration}, geometry, std::move(payload));
}

/**
 * @brief Builds a signed 16-bit PCM audio frame, deriving the declared format from the rate.
 *
 * Delegates to the strict audio constructor, so the picture fields stay zeroed.
 */
DecodedFrame makeAudioFrame(std::vector<uint8_t> payload, int64_t pts, int sample_rate,
                            int channels, int64_t duration = 1024) {
  const AudioFrameGeometry geometry{
      .sample_rate = sample_rate,
      .channels = channels,
      .audio_format =
          sample_rate == 48000 ? AudioFormat::PCM_S16_48KHZ : AudioFormat::PCM_S16_44KHZ,
  };
  return DecodedFrame(FrameTiming{.pts = pts, .duration = duration}, geometry, std::move(payload));
}

/**
 * @brief Builds a frame vector by moving each frame in.
 *
 * std::vector's initializer_list constructor requires a copy-constructible element type, so with a
 * move-only frame the frames have to be emplaced one by one.
 */
template <typename... Frames>
std::vector<DecodedFrame> makeFrames(Frames&&... frames) {
  std::vector<DecodedFrame> out;
  out.reserve(sizeof...(Frames));
  (out.emplace_back(std::forward<Frames>(frames)), ...);
  return out;
}

}  // namespace
