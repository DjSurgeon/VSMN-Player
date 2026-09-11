#pragma once

#include <chrono>
#include <cstdint>

namespace iptv::network {

/**
 * @brief Strategy used to calculate the delay between network retries.
 */
enum class BackoffStrategy : std::uint8_t {
  /// No delay between retries (immediate retry).
  None,
  /// Wait a fixed amount of time (initial_delay) between each retry.
  Fixed,
  /// Double the wait time after each failure, capped at max_delay.
  Exponential
};

/**
 * @brief Configuration for network retries and backoff logic.
 *
 * Provides a strongly typed, decoupled configuration for HTTP clients.
 * Defaults are tuned for streaming media stability.
 */
struct RetryPolicy {
  /// Maximum number of retry attempts before giving up.
  uint32_t max_retries{3};

  /// Base delay before the first retry (or constant delay for Fixed).
  std::chrono::milliseconds initial_delay{1000};

  /// Maximum possible delay between retries (ceil for Exponential strategy).
  std::chrono::milliseconds max_delay{15000};

  /// The backoff calculation strategy to use.
  BackoffStrategy strategy{BackoffStrategy::Exponential};
};

}  // namespace iptv::network
