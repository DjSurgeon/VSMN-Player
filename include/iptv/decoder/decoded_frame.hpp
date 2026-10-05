#pragma once

#include <cstdint>
#include <vector>

namespace iptv::decoder {

/**
 * @brief Represents a raw, decoded multimedia frame.
 *
 * Agnostic of any specific multimedia framework (e.g., FFmpeg).
 */
struct DecodedFrame {
  std::vector<uint8_t> raw_data;
  int64_t pts{0};      ///< Presentation Time Stamp
  int64_t dts{0};      ///< Decoding Time Stamp
  int width{0};        ///< Video width (0 for audio)
  int height{0};       ///< Video height (0 for audio)
  int sample_rate{0};  ///< Audio sample rate (0 for video)
  int channels{0};     ///< Audio channels (0 for video)
};

}  // namespace iptv::decoder
