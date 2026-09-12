#pragma once

#include <string_view>

#include "iptv/manifest/playlist_parse_result.hpp"

namespace iptv::manifest {

/**
 * @brief Abstract contract for HLS manifest parsers.
 */
class IPlaylistParser {
 public:
  /**
   * @brief Virtual destructor.
   */
  virtual ~IPlaylistParser() = default;

  // Rule of 5: Moveable, non-copyable interface contract
  IPlaylistParser(const IPlaylistParser&) = delete;
  IPlaylistParser& operator=(const IPlaylistParser&) = delete;
  IPlaylistParser(IPlaylistParser&&) noexcept = default;
  IPlaylistParser& operator=(IPlaylistParser&&) noexcept = default;

  /**
   * @brief Parses an HLS manifest into strong domain structures.
   *
   * @param content Raw UTF-8 string view of the M3U8 payload.
   * @param base_url Absolute URL of the playlist used to resolve relative segment URIs.
   * @return ParseResult Encapsulating either a valid Playlist or a diagnostic ParseError.
   */
  [[nodiscard]] virtual ParseResult parse(std::string_view content,
                                          std::string_view base_url) const = 0;

 protected:
  IPlaylistParser() = default;
};

}  // namespace iptv::manifest
