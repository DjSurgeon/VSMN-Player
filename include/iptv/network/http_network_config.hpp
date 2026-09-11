#pragma once

#include <chrono>
#include <string>

namespace iptv::network {

/**
 * @brief Global networking configuration for HTTP clients.
 *
 * Provides flexible configuration for streaming requirements, including
 * user-agent spoofing (for bypassing strict IPTV servers) and SSL validation
 * adjustments for potentially broken streams.
 */
struct NetworkConfig {
  /**
   * @brief The user agent string sent in HTTP requests. Hybrid by default.
   */
  std::string user_agent{"VSMN-Player/0.1 (Compatible; VLC/3.0.18)"};

  /**
   * @brief Whether to perform strict TLS/SSL peer and host verification.
   */
  bool ssl_verify{true};

  /**
   * @brief Whether to follow HTTP 3xx redirects automatically.
   */
  bool follow_redirects{true};

  /**
   * @brief Maximum time allowed for the connection phase (TCP/TLS handshake).
   */
  std::chrono::milliseconds connect_timeout{3000};

  /**
   * @brief Maximum time allowed for the full payload download phase.
   */
  std::chrono::milliseconds timeout{5000};
};

}  // namespace iptv::network
