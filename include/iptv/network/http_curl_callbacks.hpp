#pragma once

#include <cstddef>

namespace iptv::network::detail {

/**
 * @brief libcurl write callback to append data to the HttpResponse body.
 */
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

/**
 * @brief libcurl header callback for zero-allocation Content-Length pre-reservation.
 */
size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);

}  // namespace iptv::network::detail
