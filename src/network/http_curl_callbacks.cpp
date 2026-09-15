#include "iptv/network/http_curl_callbacks.hpp"

#include <charconv>
#include <stop_token>
#include <string_view>

#include "iptv/network/http_response.hpp"

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

int progressCallback(void* clientp, long long /*dltotal*/, long long /*dlnow*/,
                     long long /*ultotal*/, long long /*ulnow*/) {
  if (clientp == nullptr) {
    return 0;
  }
  auto* st = static_cast<const std::stop_token*>(clientp);
  if (st->stop_requested()) {
    return 1;  // Return non-zero to trigger CURLE_ABORTED_BY_CALLBACK
  }
  return 0;
}

}  // namespace iptv::network::detail
