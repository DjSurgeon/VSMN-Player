#pragma once

#include <string_view>

#include "internal/playlist_state.hpp"
#include "iptv/manifest/playlist_parse_error.hpp"

namespace iptv::manifest::internal {

/**
 * @brief Canonical signature for an HLS tag handler.
 *
 * Every handler takes the whole line plus the shared parse context and reports a
 * diagnostic instead of throwing.
 */
using TagHandler = std::optional<ParseError> (*)(std::string_view line, ParseContext& ctx);

struct TagDispatchEntry {
  std::string_view prefix;
  TagHandler handler;
};

/**
 * @brief Routes a tag line to its handler via the static dispatch table.
 * @param line The trimmed tag line, e.g. "#EXTINF:10.0,".
 * @param ctx Mutable parsing state.
 * @return A diagnostic if the handler rejected the tag, nullopt otherwise.
 *
 * Lines matching no entry are ignored, matching the "unknown tags are skipped"
 * tolerance of RFC 8216.
 */
[[nodiscard]] std::optional<ParseError> dispatchTag(std::string_view line, ParseContext& ctx);

}  // namespace iptv::manifest::internal
