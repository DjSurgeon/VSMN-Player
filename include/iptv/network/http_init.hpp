#pragma once

namespace iptv::network {

/**
 * @brief Global network initialization.
 * 
 * Must be called once at application startup before any HttpClient is created.
 * Wraps libcurl global initialization.
 */
void initialize();

/**
 * @brief Global network cleanup.
 * 
 * Must be called once at application shutdown after all HttpClients are destroyed.
 * Wraps libcurl global cleanup.
 */
void shutdown();

} // namespace iptv::network
