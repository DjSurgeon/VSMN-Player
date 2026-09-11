#include "iptv/network/http_init.hpp"
#include <curl/curl.h>
#include <stdexcept>

namespace iptv::network {

// Global Init/Shutdown implementations
void Initialize() {
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        throw std::runtime_error("Failed to initialize libcurl globally.");
    }
}

void Shutdown() {
    curl_global_cleanup();
}

} // namespace iptv::network
