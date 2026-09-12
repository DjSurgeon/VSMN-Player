#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace iptv {

/**
 * @brief Global state of the playback engine.
 */
enum class PlaybackState {
  Stopped,    ///< No media is loaded or playing.
  Playing,    ///< Media is currently playing.
  Paused,     ///< Media is paused by the user.
  Buffering,  ///< Media is paused internally due to buffer underrun.
  Error       ///< A fatal error occurred preventing playback.
};

/**
 * @brief Zero-copy ownership semantic for raw memory chunks.
 *
 * @invariant The buffer capacity is managed internally. Data must be moved, never copied.
 */
struct ByteBuffer {
  std::vector<uint8_t> data;

  ByteBuffer() = default;
  ~ByteBuffer() = default;

  // Explicitly delete copy semantics to enforce Zero-Copy
  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;

  ByteBuffer(ByteBuffer&&) noexcept = default;
  ByteBuffer& operator=(ByteBuffer&&) noexcept = default;
};

/**
 * @brief A/V Multiplexed packet from Network or Demuxer.
 */
struct Packet {
  ByteBuffer buffer;
  std::chrono::microseconds pts{0};  ///< Presentation Timestamp
  std::chrono::microseconds dts{0};  ///< Decoding Timestamp
  bool is_video{false};
  bool is_keyframe{false};
};

/**
 * @brief Decoded Video Frame.
 *
 * @pre pixel_data must contain valid YUV/RGB data representing width x height pixels.
 */
struct VideoFrame {
  ByteBuffer pixel_data;
  std::chrono::microseconds pts{0};
  uint32_t width{0};
  uint32_t height{0};
};

/**
 * @brief Decoded Audio Frame.
 *
 * @pre pcm_data must contain raw uncompressed audio samples.
 */
struct AudioFrame {
  ByteBuffer pcm_data;
  std::chrono::microseconds pts{0};
  int sample_rate{0};
  int channels{0};
};

/**
 * @brief Variant representing a decoded frame (either Video or Audio).
 */
using DecodedFrame = std::variant<VideoFrame, AudioFrame>;

/**
 * @brief Representation of an IPTV channel from an M3U playlist.
 */
struct Channel {
  std::string name;
  std::string uri;
  std::string logo_url;
  std::string epg_id;
};

// =========================================================================
// Telemetry and Statistics
// =========================================================================

/**
 * @brief Real-time metrics for the Network Subsystem.
 */
struct NetworkStats {
  double throughput_mbps{0.0};        ///< EWMA filtered throughput
  uint64_t bytes_downloaded{0};       ///< Cumulative bytes downloaded
  int active_connections{0};          ///< Number of open TCP connections
  double buffer_health_seconds{0.0};  ///< Current elastic buffer length
};

/**
 * @brief Real-time metrics for the Decoder Subsystem.
 */
struct DecoderStats {
  uint64_t frames_decoded{0};       ///< Cumulative frames successfully decoded
  uint64_t dropped_packets{0};      ///< Packets dropped due to errors or discontinuities
  double decoding_latency_ms{0.0};  ///< Average time to decode a frame
};

/**
 * @brief Real-time metrics for the Render Subsystem.
 */
struct RenderStats {
  double fps{0.0};                 ///< Current rendering frames per second
  uint64_t dropped_frames{0};      ///< Frames discarded (e.g. video lagging audio)
  double a_v_sync_offset_ms{0.0};  ///< Current delta between video and audio clocks
};

}  // namespace iptv
