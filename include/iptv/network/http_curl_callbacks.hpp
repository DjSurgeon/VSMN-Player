#pragma once

#include <cstddef>

namespace iptv::network::detail {

/**
 * @brief libcurl write callback to append data to the HttpResponse body.
 */
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

/**
 * @brief libcurl header callback for zero-allocation Content-Length
 * pre-reservation.
 */
size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);

/**
 * @brief libcurl progress callback used for fast cancellation.
 * Returns non-zero to abort the transfer if the stop_token is triggered.
 */
int progressCallback(void* clientp, long long dltotal, long long dlnow, long long ultotal,
                     long long ulnow);

}  // namespace iptv::network::detail
