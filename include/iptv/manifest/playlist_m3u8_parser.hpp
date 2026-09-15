#pragma once

#include <string_view>

#include "iptv/manifest/playlist_parse_result.hpp"

namespace iptv::manifest {

/**
 * @brief Options for parsing a playlist.
 */
struct ParseOptions {
  std::string_view content;
  std::string_view base_url;
};

/**
 * @brief Namespace for parsing HLS M3U8 playlists (Master and Media).
 */
namespace M3u8Parser {

/**
 * @brief Parses the M3U8 media playlist.
 *
 * @param options The struct containing the content string_view and base_url.
 * @return ParseResult Encapsulating either a valid Playlist or a diagnostic ParseError.
 */
[[nodiscard]] ParseResult parse(const ParseOptions& options);

}  // namespace M3u8Parser

}  // namespace iptv::manifest
