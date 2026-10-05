#pragma once

#include <optional>
#include <string_view>

#include "internal/line_reader.hpp"
#include "internal/playlist_state.hpp"
#include "iptv/manifest/playlist.hpp"
#include "iptv/manifest/playlist_parse_error.hpp"

namespace iptv::manifest::internal {

/**
 * @brief Commits a URI line to the pending variant, or to the pending segment.
 * @param uri The URI read from the line.
 * @param line_num 1-based line number, used for diagnostics.
 * @param ctx Mutable parsing state; the matching builder is reset on success.
 * @return A diagnostic if the line has no preceding tag to attach to.
 */
[[nodiscard]] std::optional<ParseError> commitUriLine(std::string_view uri, uint32_t line_num,
                                                      ParseContext& ctx);

/**
 * @brief Drains the reader, committing segments, variants and tag effects.
 * @param reader Line source positioned after the header.
 * @param playlist Output playlist, appended to in line order.
 * @return A diagnostic on the first rejected line, nullopt when the input parsed.
 */
[[nodiscard]] std::optional<ParseError> parseLines(LineReader& reader, Playlist& playlist);

}  // namespace iptv::manifest::internal
