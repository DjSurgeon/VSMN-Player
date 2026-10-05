#pragma once

#include <string>
#include <string_view>

namespace iptv::player::internal {

/**
 * @brief Resolves a possibly relative HLS URI against the playlist URL it came from.
 * @param base_url Absolute URL of the playlist or segment the URI was read from.
 * @param uri Absolute URI, or one relative to @p base_url.
 * @return An absolute URL string.
 *
 * Handles the three cases separately: already absolute, root-relative against
 * the origin, and path-relative against the containing directory. A base with
 * no path separator has no directory to be relative to, so the URI is
 * returned unchanged.
 */
[[nodiscard]] std::string resolveUrl(std::string_view base_url, std::string_view uri);

}  // namespace iptv::player::internal
