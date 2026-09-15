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
 * @brief Concrete parser for HLS Media Playlists (VOD & Live).
 *
 * Implements a zero-copy line iteration approach to process M3U8 tags.
 */
class M3u8Parser final {
 public:
  /**
   * @brief Default constructor.
   */
  M3u8Parser() = default;

  /**
   * @brief Virtual destructor.
   */
  ~M3u8Parser() = default;

  // Rule of 5: Moveable, non-copyable parser
  M3u8Parser(const M3u8Parser&) = delete;
  M3u8Parser& operator=(const M3u8Parser&) = delete;
  M3u8Parser(M3u8Parser&&) noexcept = default;
  M3u8Parser& operator=(M3u8Parser&&) noexcept = default;

  /**
   * @brief Parses the M3U8 media playlist.
   *
   * @param options The struct containing the content string_view and base_url.
   * @return ParseResult Encapsulating either a valid Playlist or a diagnostic ParseError.
   */
  [[nodiscard]] static ParseResult parse(const ParseOptions& options);
};

}  // namespace iptv::manifest
