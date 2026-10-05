#pragma once

#include <cstdint>

#include "iptv/decoder/decoded_frame.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace iptv::decoder::internal {

/**
 * @brief Packs one decoded picture into a tightly packed DecodedFrame.
 *
 * FFmpeg hands out planes that are padded wider than the picture, so each visible row is copied
 * separately and the payload comes out plane after plane: Y, then U, then V, with no padding and
 * no gaps. The declared #DecodedFrame::linesize are therefore the packed strides of that payload,
 * which is exactly what a renderer addressing rows by stride expects.
 *
 * @param frame Frame the codec has just produced. Borrowed for the duration of the call.
 * @param pts Presentation time to stamp the frame with, in milliseconds.
 * @param duration How long the frame is shown, in the same time base as @p pts.
 * @return A video frame owning a copy of the picture's visible bytes.
 * @throws std::runtime_error when the picture is not 8-bit 4:2:0 YUV, has no area, or is missing
 *         one of its planes. No pixel format conversion is attempted: the render path is defined
 *         for 4:2:0 only, so any other layout is reported instead of quietly reshaped.
 */
[[nodiscard]] DecodedFrame extractDecodedFrame(const AVFrame& frame, int64_t pts, int64_t duration);

}  // namespace iptv::decoder::internal