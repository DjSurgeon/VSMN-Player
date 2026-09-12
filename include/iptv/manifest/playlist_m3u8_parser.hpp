#pragma once

#include "iptv/manifest/i_playlist_parser.hpp"

namespace iptv::manifest {

/**
 * @brief Concrete parser for HLS Media Playlists (VOD & Live).
 *
 * Implements a zero-copy line iteration approach to process M3U8 tags.
 */
class M3u8Parser final : public IPlaylistParser {
 public:
  /**
   * @brief Default constructor.
   */
  M3u8Parser() = default;

  /**
   * @brief Virtual destructor.
   */
  ~M3u8Parser() override = default;

  // Rule of 5: Moveable, non-copyable parser
  M3u8Parser(const M3u8Parser&) = delete;
  M3u8Parser& operator=(const M3u8Parser&) = delete;
  M3u8Parser(M3u8Parser&&) noexcept = default;
  M3u8Parser& operator=(M3u8Parser&&) noexcept = default;

  /**
   * @brief Parses the M3U8 media playlist.
   *
   * @param content Raw UTF-8 string view of the M3U8 payload.
   * @param base_url Absolute URL of the playlist used to resolve relative segment URIs.
   * @return ParseResult Encapsulating either a valid Playlist or a diagnostic ParseError.
   */
  [[nodiscard]] ParseResult parse(std::string_view content,
                                  std::string_view base_url) const override;
};

}  // namespace iptv::manifest
