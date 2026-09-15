#include "iptv/network/network_component.hpp"

namespace iptv::network {

NetworkComponent::NetworkComponent(std::unique_ptr<IHttpClient> http_client)
    : http_client_(std::move(http_client)) {}

PlaylistDownloadResult NetworkComponent::downloadPlaylist(const std::string& url,
                                                          std::chrono::milliseconds timeout_ms) {
  // 1. Download
  HttpResponse response = http_client_->download(url, timeout_ms);

  if (!response.isSuccess()) {
    return NetworkError{"HTTP Request failed with status " +
                        std::to_string(static_cast<int>(response.getStatusCode()))};
  }

  // 2. Parse (Zero-Copy)
  const auto& body_ref = response.getBody();
  std::string_view content(reinterpret_cast<const char*>(body_ref.data()), body_ref.size());

  manifest::ParseResult parse_result = parser_.parse(content, url);

  if (!parse_result.hasValue()) {
    return parse_result.error();
  }

  // 3. Assemble the Bundle
  ParsedPlaylistBundle bundle;
  bundle.raw_buffer = response.extractBody();
  bundle.playlist = std::move(parse_result).value();

  return bundle;
}

}  // namespace iptv::network
