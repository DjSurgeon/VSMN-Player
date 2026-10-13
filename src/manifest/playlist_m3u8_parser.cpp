#include "iptv/manifest/playlist_m3u8_parser.hpp"

#include <algorithm>
#include <string_view>

#include "internal/line_reader.hpp"
#include "internal/playlist_line_parser.hpp"

namespace iptv::manifest {

namespace {

/**
 * @brief Pre-sizes the dominant container so a playlist costs one allocation.
 * @param content Raw manifest text, used only for its size.
 * @param playlist Output playlist whose containers are reserved.
 *
 * Master playlists fill #variants and media playlists fill #segments, so the
 * heuristic reserves whichever container the content implies.
 */
void preallocatePlaylist(std::string_view content, Playlist& playlist) {
  const bool is_master = content.find("#EXT-X-STREAM-INF:") != std::string_view::npos;
  const size_t estimated_elements = content.size() / 48;

  if (is_master) {
    playlist.variants.reserve(estimated_elements);
  } else {
    playlist.segments.reserve(estimated_elements);
  }
}

/**
 * @brief Strips a UTF-8 byte order mark if present.
 */
[[nodiscard]] std::string_view stripBom(std::string_view content) noexcept {
  if (content.starts_with("\xEF\xBB\xBF")) {
    content.remove_prefix(3);
  }
  return content;
}

/**
 * @brief Validates that the first line is the #EXTM3U tag.
 * @return A diagnostic if the header is missing, nullopt when it is present.
 */
[[nodiscard]] std::optional<ParseError> validateHeader(internal::LineReader& reader) {
  const auto first_line = reader.next();
  if (first_line && first_line->text.starts_with("#EXTM3U")) {
    return std::nullopt;
  }
  return ParseError{ParseErrorCode::InvalidHeader, first_line ? first_line->number : 0,
                    "Missing #EXTM3U tag"};
}

/**
 * @brief Assigns the final playlist type once every tag and URI has been read.
 *
 * Variants imply a master playlist; a playlist that never saw #EXT-X-ENDLIST is
 * a live (sliding window) playlist.
 */
void finalizePlaylist(Playlist& playlist) {
  if (!playlist.variants.empty()) {
    playlist.type = PlaylistType::Master;

    // Sort variants ascending by bandwidth (Fast Start strategy standard)
    std::sort(playlist.variants.begin(), playlist.variants.end(),
              [](const auto& left, const auto& right) { return left.bandwidth < right.bandwidth; });
  } else if (!playlist.has_endlist) {
    playlist.type = PlaylistType::MediaLive;
  }
}

}  // namespace

namespace M3u8Parser {

ParseResult parse(const ParseOptions& options) {
  const std::string_view content = stripBom(options.content);
  if (content.empty()) {
    return ParseError{ParseErrorCode::EmptyContent, 0, "Manifest content is empty"};
  }

  internal::LineReader reader(content);
  if (auto header_error = validateHeader(reader)) {
    return *header_error;
  }

  Playlist playlist;
  preallocatePlaylist(options.content, playlist);

  if (auto err = internal::parseLines(reader, playlist)) {
    return *err;
  }

  finalizePlaylist(playlist);

  return playlist;
}

}  // namespace M3u8Parser

}  // namespace iptv::manifest
