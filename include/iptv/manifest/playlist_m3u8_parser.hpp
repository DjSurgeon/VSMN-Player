#pragma once

#include <string_view>

#include "iptv/manifest/playlist_parse_result.hpp"

namespace iptv::manifest {

/**
 * @brief Options for parsing a playlist.
 *
 * Only the raw content is needed: URIs are stored exactly as written, and
 * resolving them against the playlist URL is the network layer's job, so no
 * base URL is carried here.
 */
struct ParseOptions {
  std::string_view content;
};

/**
 * @brief Namespace for parsing HLS M3U8 playlists (Master and Media).
 */
namespace M3u8Parser {

/**
 * @brief Parses the M3U8 media playlist.
 *
 * @param options The struct containing the manifest content as a string_view.
 * @return ParseResult Encapsulating either a valid Playlist or a diagnostic ParseError.
 */
[[nodiscard]] ParseResult parse(const ParseOptions& options);

}  // namespace M3u8Parser

}  // namespace iptv::manifest
