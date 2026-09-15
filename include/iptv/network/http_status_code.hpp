#pragma once

#include <cstdint>

namespace iptv::network {

/**
 * @brief Strictly typed HTTP status codes.
 *
 * Defines standard HTTP response status codes to avoid using naked integers.
 */
enum class HttpStatusCode : std::int16_t {
  // 2xx Success
  Ok = 200,

  // 4xx Client Error
  Forbidden = 403,
  NotFound = 404,

  // 5xx Server Error
  InternalServerError = 500,
  ServiceUnavailable = 503,

  // Custom internal codes
  Unknown = -1
};

}  // namespace iptv::network
