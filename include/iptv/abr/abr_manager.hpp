#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "iptv/manifest/playlist.hpp"

namespace iptv::abr {

/**
 * @brief Adaptive Bitrate Manager
 *
 * Deterministic mathematical module to select the optimal video quality based
 * on the measured bandwidth, using Exponential Smoothing (EWMA) and
 * directional Hysteresis to prevent the accordion effect (rapid quality switching).
 */
class AbrManager {
 public:
  explicit AbrManager(double smoothing_factor = 0.3) : alpha_(smoothing_factor) {}

  /**
   * @brief Selects the best possible variant based on the smoothed throughput.
   *
   * @param variants List of available variants (must be sorted by bitrate, from lowest to highest).
   * @param measured_throughput_mbps Actual throughput measured in the last segment (Mbps).
   * @return Reference to the optimal chosen variant.
   * @throws std::invalid_argument if the variant list is empty.
   */
  [[nodiscard]] const manifest::VariantStreamRef& selectVariant(
      const std::vector<manifest::VariantStreamRef>& variants, double measured_throughput_mbps);

  [[nodiscard]] double currentSmoothedThroughput() const noexcept { return smoothed_throughput_; }

  // Exposes the current index for testing or debugging purposes
  [[nodiscard]] std::size_t getCurrentVariantIndex() const noexcept {
    return current_variant_index_;
  }

 private:
  /**
   * @brief Initializes the EWMA and selects the best initial variant.
   *
   * @param variants List of available variants.
   * @param measured_throughput_mbps The throughput measured in the very first segment.
   */
  void initializeFirstSegment(const std::vector<manifest::VariantStreamRef>& variants,
                              double measured_throughput_mbps);

  /**
   * @brief Updates the Exponentially Weighted Moving Average (EWMA) of the throughput.
   *
   * @param measured_throughput_mbps The throughput measured in the latest segment.
   */
  void updateSmoothedThroughput(double measured_throughput_mbps) noexcept;

  /**
   * @brief Calculates the ideal variant index mathematically, applying a safety margin.
   *
   * @param variants List of available variants.
   * @return The index of the ideal variant that fits the current smoothed throughput.
   */
  [[nodiscard]] std::size_t calculateIdealVariantIndex(
      const std::vector<manifest::VariantStreamRef>& variants) const noexcept;

  /**
   * @brief Applies directional hysteresis to prevent rapid quality switching.
   *
   * Upgrades require consecutive favorable measurements, whereas downgrades are immediate.
   *
   * @param ideal_index The ideal variant index calculated mathematically.
   */
  void applyHysteresis(std::size_t ideal_index) noexcept;

  double alpha_{0.3};
  double smoothed_throughput_{0.0};
  int consecutive_upgrades_{0};
  std::size_t current_variant_index_{0};
};

}  // namespace iptv::abr
