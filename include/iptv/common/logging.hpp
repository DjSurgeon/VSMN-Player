#pragma once

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

// Standardized logging macros to decouple the codebase from spdlog directly.
// This allows swapping the underlying logger in the future if needed,
// and ensures consistent log formatting across all subsystems.

#define IPTV_LOG_TRACE(...)    spdlog::trace(__VA_ARGS__)
#define IPTV_LOG_DEBUG(...)    spdlog::debug(__VA_ARGS__)
#define IPTV_LOG_INFO(...)     spdlog::info(__VA_ARGS__)
#define IPTV_LOG_WARN(...)     spdlog::warn(__VA_ARGS__)
#define IPTV_LOG_ERROR(...)    spdlog::error(__VA_ARGS__)
#define IPTV_LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)

namespace iptv::logging {

/**
 * @brief Initializes the global logging system (e.g., formatting, console/file sinks).
 */
void initialize();

}  // namespace iptv::logging
