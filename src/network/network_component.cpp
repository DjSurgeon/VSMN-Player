#include "iptv/network/network_component.hpp"

namespace iptv::network {

/**
 * @brief Constructs the NetworkComponent with a specific HTTP client instance.
 */
NetworkComponent::NetworkComponent(std::unique_ptr<IHttpClient> http_client)
    : http_client_(std::move(http_client)) {}

/**
 * @brief Validates the HTTP response status code.
 */
std::optional<NetworkError> NetworkComponent::validateHttpResponse(
    const HttpResponse& response) noexcept {
  if (!response.isSuccess()) {
    return NetworkError{"HTTP Request failed with status " +
                        std::to_string(static_cast<int>(response.getStatusCode()))};
  }
  return std::nullopt;
}

/**
 * @brief Parses the manifest bytes into a Playlist structure without copying memory.
 */
manifest::ParseResult NetworkComponent::parseManifest(const HttpResponse& response,
                                                      const std::string& url) {
  const auto& body_ref = response.getBody();
  std::string_view content(static_cast<const char*>(static_cast<const void*>(body_ref.data())),
                           body_ref.size());

  return manifest::M3u8Parser::parse({content, url});
}

/**
 * @brief Assembles the ParsedPlaylistBundle by moving the raw buffer and the playlist.
 */
ParsedPlaylistBundle NetworkComponent::assembleBundle(HttpResponse&& response,
                                                      manifest::Playlist&& playlist) noexcept {
  ParsedPlaylistBundle bundle;
  bundle.raw_buffer = response.extractBody();
  bundle.playlist = std::move(playlist);
  return bundle;
}

/**
 * @brief High-level pipeline that downloads, validates, parses, and assembles a playlist.
 */
PlaylistDownloadResult NetworkComponent::downloadPlaylist(const std::string& url,
                                                          std::chrono::milliseconds timeout_ms) {
  HttpResponse response = http_client_->download(url, timeout_ms);

  if (auto error = validateHttpResponse(response)) {
    return *error;
  }

  manifest::ParseResult parse_result = parseManifest(response, url);

  if (!parse_result.hasValue()) {
    return parse_result.error();
  }

  return assembleBundle(std::move(response), std::move(parse_result).value());
}

/**
 * @brief Downloads a single media segment and encapsulates it with network metrics.
 */
SegmentDownloadResult NetworkComponent::downloadSegment(const std::string& absolute_url,
                                                        const std::stop_token& stop_token) {
  // Use a reasonable 30s timeout for media segments
  HttpResponse response =
      http_client_->download(absolute_url, std::chrono::seconds(30), stop_token);

  if (!response.isSuccess()) {
    if (stop_token.stop_requested()) {
      return NetworkError{"Segment download cancelled by orchestrator."};
    }
    return NetworkError{"HTTP Request failed with status " +
                        std::to_string(static_cast<int>(response.getStatusCode()))};
  }

  MediaSegmentBundle bundle;
  bundle.metrics = response.getMetrics();
  bundle.raw_buffer = response.extractBody();

  return bundle;
}

}  // namespace iptv::network
