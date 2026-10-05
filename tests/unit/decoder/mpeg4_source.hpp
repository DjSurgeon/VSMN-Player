#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace iptv::decoder::testing {

/**
 * @brief Encodes synthetic YUV420P pictures into mpeg4 elementary stream packets.
 *
 * The decoder tests need real compressed input, and nothing else in the project can produce one:
 * the FFmpeg build carries no H.264 encoder, so the tests drive mpeg4, which travels the same
 * avcodec_send_packet / avcodec_receive_frame path a broadcast codec does. Every picture is an
 * intra-coded key frame, so each packet stands on its own, and picture i carries a distinct luma
 * ramp, so the frame a decoder returns identifies which picture it came from.
 *
 * @param width Picture width in pixels. Must be a multiple of 16, as mpeg4 requires.
 * @param height Picture height in pixels. Must be a multiple of 16, as mpeg4 requires.
 * @param picture_count How many pictures to encode.
 * @param max_b_frames How many pictures the encoder may reorder. A value above zero makes the
 *        stream hand out pictures later than it encodes them, which is how the tests reach the
 *        decoder path where one packet does not yield one frame.
 * @return One packet per picture, in output order.
 * @throws std::runtime_error when the encoder cannot be opened or fed.
 */
[[nodiscard]] std::vector<std::vector<uint8_t>> encodeMpeg4Pictures(int width, int height,
                                                                    int picture_count,
                                                                    int max_b_frames = 0);

}  // namespace iptv::decoder::testing