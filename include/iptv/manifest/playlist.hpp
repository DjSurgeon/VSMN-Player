#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace iptv::manifest {

/// Semantic alias for floating-point HLS segment durations (RFC 8216)
using FloatingSeconds = std::chrono::duration<double>;

/**
 * @brief Categorization of the parsed HLS playlist.
 */
enum class PlaylistType : uint8_t {
  Master,    ///< Master playlist containing multiple quality/bitrate variants.
  MediaVOD,  ///< Complete static playlist with the #EXT-X-ENDLIST tag.
  MediaLive  ///< Dynamic playlist with a sliding window of segments.
};

/**
 * @brief Dimensional resolution of a video stream.
 */
struct Resolution {
  uint32_t width{0};
  uint32_t height{0};

  [[nodiscard]] constexpr bool isValid() const noexcept { return width > 0 && height > 0; }
};

/**
 * @brief Individual media segment parsed from a media playlist.
 */
struct MediaSegmentRef {
  std::string uri{};                ///< URL of the segment (.ts / .m4s).
  FloatingSeconds duration{0.0};  ///< Precise duration from #EXTINF.
  uint64_t sequence_index{0};     ///< Absolute media sequence index.
  bool is_discontinuity{false};   ///< Timestamp jump marker (#EXT-X-DISCONTINUITY).
  std::optional<std::string>
      init_segment_uri;  ///< Initialization segment (#EXT-X-MAP) for CMAF/fMP4.
};

/**
 * @brief Quality variant stream within a master playlist (#EXT-X-STREAM-INF).
 */
struct VariantStreamRef {
  std::string uri{};          ///< URI of the media playlist.
  uint32_t bandwidth{0};    ///< Bitrate in bits/second.
  Resolution resolution{};  ///< Video width and height.
  double frame_rate{0.0};   ///< Frames per second (optional).
  std::string codecs{};       ///< RFC 6381 identifiers (e.g., "avc1.64002a,mp4a.40.2").
};

/**
 * @brief Pure data representation of a parsed HLS playlist.
 */
struct Playlist {
  PlaylistType type{PlaylistType::MediaVOD};
  std::chrono::seconds target_duration{0};  ///< #EXT-X-TARGETDURATION in whole seconds.
  uint64_t media_sequence{0};               ///< Initial base sequence (#EXT-X-MEDIA-SEQUENCE).
  bool has_endlist{false};                  ///< True if finalized with #EXT-X-ENDLIST.

  std::vector<MediaSegmentRef> segments{};   ///< Populated if type is MediaVOD or MediaLive.
  std::vector<VariantStreamRef> variants{};  ///< Populated if type is Master.
};

}  // namespace iptv::manifest
