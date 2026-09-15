#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "iptv/manifest/playlist.hpp"
#include "iptv/manifest/playlist_m3u8_parser.hpp"
#include "iptv/network/i_http_client.hpp"

namespace iptv::network {

struct NetworkError {
  std::string message;
};

/**
 * @brief An indivisible bundle tying the lifetime of the raw buffer to the parsed Playlist.
 * This guarantees the Zero-Copy std::string_view pointers in the Playlist never dangle.
 */
struct ParsedPlaylistBundle {
  std::vector<uint8_t> raw_buffer;
  manifest::Playlist playlist;

  ParsedPlaylistBundle() = default;
  ~ParsedPlaylistBundle() = default;

  // Move-Only: A copy would reallocate raw_buffer, invalidating all string_views in playlist.
  ParsedPlaylistBundle(const ParsedPlaylistBundle&) = delete;
  ParsedPlaylistBundle& operator=(const ParsedPlaylistBundle&) = delete;

  ParsedPlaylistBundle(ParsedPlaylistBundle&&) noexcept = default;
  ParsedPlaylistBundle& operator=(ParsedPlaylistBundle&&) noexcept = default;
};

using PlaylistDownloadResult =
    std::variant<ParsedPlaylistBundle, NetworkError, manifest::ParseError>;

/**
 * @brief An indivisible bundle of the downloaded segment memory and its network metrics.
 */
struct MediaSegmentBundle {
  std::vector<uint8_t> raw_buffer;
  NetworkMetrics metrics;

  MediaSegmentBundle() = default;
  ~MediaSegmentBundle() = default;

  MediaSegmentBundle(const MediaSegmentBundle&) = delete;
  MediaSegmentBundle& operator=(const MediaSegmentBundle&) = delete;

  MediaSegmentBundle(MediaSegmentBundle&&) noexcept = default;
  MediaSegmentBundle& operator=(MediaSegmentBundle&&) noexcept = default;
};

using SegmentDownloadResult = std::variant<MediaSegmentBundle, NetworkError>;

class NetworkComponent {
 public:
  explicit NetworkComponent(std::unique_ptr<IHttpClient> http_client);
  ~NetworkComponent() = default;

  // Prevent copies
  NetworkComponent(const NetworkComponent&) = delete;
  NetworkComponent& operator=(const NetworkComponent&) = delete;

  // Allow moves
  NetworkComponent(NetworkComponent&&) noexcept = default;
  NetworkComponent& operator=(NetworkComponent&&) noexcept = default;

  /**
   * @brief Downloads a playlist and parses it without heap copying.
   *
   * @param url The URL of the playlist (M3U8).
   * @param timeout_ms The timeout for the HTTP request.
   * @return A variant containing either the successfully parsed bundle or an error.
   */
  PlaylistDownloadResult downloadPlaylist(
      const std::string& url, std::chrono::milliseconds timeout_ms = std::chrono::seconds(10));

  /**
   * @brief Downloads a raw media segment enforcing the retry policy.
   *
   * @param segment The segment reference from the parsed playlist.
   * @param st Token for fast cancellation from the ABR orchestrator.
   * @return A variant containing either the successfully downloaded bundle or an error.
   */
  SegmentDownloadResult downloadSegment(const manifest::MediaSegmentRef& segment,
                                        std::stop_token st = {});

 private:
  std::unique_ptr<IHttpClient> http_client_;
  manifest::M3u8Parser parser_;
};

}  // namespace iptv::network
