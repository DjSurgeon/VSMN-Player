#include "iptv/network/http_curl_callbacks.hpp"
#include "iptv/network/http_response.hpp"

#include <charconv>
#include <string_view>

namespace iptv::network::detail {

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  if (userdata == nullptr) {
    return 0;
  }

  auto* response = static_cast<HttpResponse*>(userdata);
  std::size_t total_size = size * nmemb;
  response->appendToBody(static_cast<const uint8_t*>(static_cast<const void*>(ptr)), total_size);
  return total_size;
}

size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
  if (userdata == nullptr) {
    return size * nitems;
  }
  size_t total = size * nitems;
  std::string_view line(buffer, total);

  if (line.starts_with("Content-Length:") || line.starts_with("content-length:")) {
    size_t pos = line.find(':');
    if (pos != std::string_view::npos) {
      std::string_view val = line.substr(pos + 1);
      auto first = val.find_first_not_of(" \t\r\n");
      if (first != std::string_view::npos) {
        val = val.substr(first);
        size_t content_length = 0;
        auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), content_length);
        if (ec == std::errc{}) {
          auto* response = static_cast<HttpResponse*>(userdata);
          response->reserveBody(content_length);
        }
      }
    }
  }
  return total;
}

}  // namespace iptv::network::detail
