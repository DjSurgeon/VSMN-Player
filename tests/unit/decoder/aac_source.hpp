#pragma once

#include <cstdint>
#include <vector>

namespace iptv::decoder::testing {

/**
 * @brief Encodes a synthetic sine wave into AAC elementary stream packets.
 *
 * The decoder tests need real compressed audio input. This helper generates a sine wave at
 * 440 Hz, feeds it through the AAC encoder via avcodec_send_frame / avcodec_receive_packet,
 * and returns the resulting packets. Each packet is self-contained (no inter-packet
 * dependencies beyond what AAC inherently requires).
 *
 * @param sample_rate Sample rate in Hz (e.g., 44100, 48000).
 * @param channels Number of audio channels (1 for mono, 2 for stereo).
 * @param duration_ms Duration of the sine wave in milliseconds.
 * @return One packet per encoded frame, in output order.
 * @throws std::runtime_error when the encoder cannot be opened or fed.
 */
[[nodiscard]] std::vector<std::vector<uint8_t>> encodeAacSineWave(int sample_rate,
                                                                  int channels,
                                                                  int duration_ms);

}  // namespace iptv::decoder::testing
