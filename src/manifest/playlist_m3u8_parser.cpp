#include "iptv/manifest/playlist_m3u8_parser.hpp"

#include <charconv>
#include <string_view>
#include <system_error>

namespace iptv::manifest {

namespace {

/**
 * @brief Trim whitespace and carriage returns from a string view.
 */
[[nodiscard]] std::string_view trim(std::string_view view) noexcept {
  while (!view.empty() && (view.back() == '\r' || view.back() == ' ' || view.back() == '\t')) {
    view.remove_suffix(1);
  }
  while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
    view.remove_prefix(1);
  }
  return view;
}

/**
 * @brief Resolve relative URIs defensively without overengineering.
 */
[[nodiscard]] std::string resolveUri(std::string_view uri, std::string_view base_url) {
  if (uri.starts_with("http://") || uri.starts_with("https://")) {
    return std::string(uri);
  }

  if (uri.starts_with("/")) {
    // Relative to host root
    const auto scheme_end = base_url.find("://");
    if (scheme_end != std::string_view::npos) {
      const auto host_end = base_url.find('/', scheme_end + 3);
      const auto root =
          (host_end != std::string_view::npos) ? base_url.substr(0, host_end) : base_url;
      return std::string(root).append(uri);
    }
  }

  // Relative to current directory of base_url
  const auto last_slash = base_url.rfind('/');
  if (last_slash != std::string_view::npos) {
    return std::string(base_url.substr(0, last_slash + 1)).append(uri);
  }

  return std::string(base_url).append("/").append(uri);
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity,bugprone-easily-swappable-parameters)
ParseResult M3u8Parser::parse(std::string_view content, std::string_view base_url) const {
  if (content.empty()) {
    return ParseError{ParseErrorCode::EmptyContent, 0, "Manifest content is empty"};
  }

  Playlist playlist;
  size_t cursor = 0;
  uint32_t line_number = 0;
  bool header_verified = false;

  struct PendingSegment {
    bool active{false};
    FloatingSeconds duration{0.0};
    bool discontinuity{false};
  };

  PendingSegment pending_segment;

  while (cursor < content.size()) {
    const size_t newline_pos = content.find('\n', cursor);
    std::string_view line = (newline_pos == std::string_view::npos)
                                ? content.substr(cursor)
                                : content.substr(cursor, newline_pos - cursor);

    cursor = (newline_pos == std::string_view::npos) ? content.size() : newline_pos + 1;
    line_number++;
    line = trim(line);

    if (line.empty()) {
      continue;
    }

    // 1. Mandatory HLS header verification
    if (!header_verified) {
      if (line != "#EXTM3U") {
        return ParseError{ParseErrorCode::InvalidHeader, line_number, "Missing #EXTM3U tag"};
      }
      header_verified = true;
      continue;
    }

    // 2. Global directives
    if (line.starts_with("#EXT-X-TARGETDURATION:")) {
      const auto val_str = line.substr(22);
      uint32_t target_sec = 0;
      if (auto [ptr, ec] =
              std::from_chars(val_str.data(), val_str.data() + val_str.size(), target_sec);
          ec == std::errc{}) {
        playlist.target_duration = std::chrono::seconds(target_sec);
      }
    } else if (line.starts_with("#EXT-X-MEDIA-SEQUENCE:")) {
      const auto val_str = line.substr(22);
      uint64_t seq = 0;
      if (auto [ptr, ec] = std::from_chars(val_str.data(), val_str.data() + val_str.size(), seq);
          ec == std::errc{}) {
        playlist.media_sequence = seq;
      }
    } else if (line == "#EXT-X-ENDLIST") {
      playlist.has_endlist = true;
      playlist.type = PlaylistType::MediaVOD;
    } else if (line == "#EXT-X-DISCONTINUITY") {
      pending_segment.discontinuity = true;
    } else if (line.starts_with("#EXTINF:")) {
      // Extract duration (#EXTINF:10.043,title)
      auto val_str = line.substr(8);
      const auto comma_pos = val_str.find(',');
      if (comma_pos != std::string_view::npos) {
        val_str = val_str.substr(0, comma_pos);
      }

      double dur_sec = 0.0;
      if (auto [ptr, ec] =
              std::from_chars(val_str.data(), val_str.data() + val_str.size(), dur_sec);
          ec == std::errc{}) {
        pending_segment.duration = FloatingSeconds(dur_sec);
        pending_segment.active = true;
      } else {
        return ParseError{ParseErrorCode::InvalidFormat, line_number, "Malformed #EXTINF duration"};
      }
    } else if (!line.starts_with('#')) {
      // Segment URI line
      if (!pending_segment.active) {
        return ParseError{ParseErrorCode::MissingMandatoryTags, line_number,
                          "Segment URI without preceding #EXTINF"};
      }

      MediaSegmentRef segment;
      segment.uri = resolveUri(line, base_url);
      segment.duration = pending_segment.duration;
      segment.sequence_index = playlist.media_sequence + playlist.segments.size();
      segment.is_discontinuity = pending_segment.discontinuity;

      playlist.segments.push_back(std::move(segment));

      // Ephemeral state reset
      pending_segment.active = false;
      pending_segment.discontinuity = false;
    }
  }

  if (!playlist.has_endlist) {
    playlist.type = PlaylistType::MediaLive;
  }

  return playlist;
}

}  // namespace iptv::manifest
