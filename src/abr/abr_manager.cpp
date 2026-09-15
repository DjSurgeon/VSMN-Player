#include "iptv/abr/abr_manager.hpp"

namespace iptv::abr {

/**
 * @brief Selects the best possible variant based on the smoothed throughput.
 */
const manifest::VariantStreamRef& AbrManager::selectVariant(
    const std::vector<manifest::VariantStreamRef>& variants, double measured_throughput_mbps) {
  if (variants.empty()) {
    throw std::invalid_argument("AbrManager: Variant list cannot be empty.");
  }

  // 1. Initialization (First segment)
  if (smoothed_throughput_ == 0.0) {
    initializeFirstSegment(variants, measured_throughput_mbps);
    return variants[current_variant_index_];
  }

  // 2. EWMA Update
  updateSmoothedThroughput(measured_throughput_mbps);

  // 3. Mathematically calculate the ideal variant (with 80% safety margin)
  std::size_t ideal_index = calculateIdealVariantIndex(variants);

  // 4. Hysteresis Logic
  applyHysteresis(ideal_index);

  return variants[current_variant_index_];
}

/**
 * @brief Initializes the EWMA and selects the best initial variant.
 */
void AbrManager::initializeFirstSegment(const std::vector<manifest::VariantStreamRef>& variants,
                                        double measured_throughput_mbps) {
  smoothed_throughput_ = measured_throughput_mbps;
  current_variant_index_ = calculateIdealVariantIndex(variants);
}

/**
 * @brief Updates the Exponentially Weighted Moving Average (EWMA) of the throughput.
 */
void AbrManager::updateSmoothedThroughput(double measured_throughput_mbps) noexcept {
  smoothed_throughput_ =
      (alpha_ * measured_throughput_mbps) + ((1.0 - alpha_) * smoothed_throughput_);
}

/**
 * @brief Calculates the ideal variant index mathematically, applying a safety margin.
 */
std::size_t AbrManager::calculateIdealVariantIndex(
    const std::vector<manifest::VariantStreamRef>& variants) const noexcept {
  std::size_t ideal_index = 0;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    double variant_mbps = static_cast<double>(variants[i].bandwidth) / 1'000'000.0;
    if (variant_mbps <= smoothed_throughput_ * 0.8) {
      ideal_index = i;
    } else {
      break;
    }
  }
  return ideal_index;
}

/**
 * @brief Applies directional hysteresis to prevent rapid quality switching.
 */
void AbrManager::applyHysteresis(std::size_t ideal_index) noexcept {
  if (ideal_index < current_variant_index_) {
    // Aggressive downgrade: If throughput is insufficient, downgrade immediately.
    current_variant_index_ = ideal_index;
    consecutive_upgrades_ = 0;
  } else if (ideal_index > current_variant_index_) {
    // Conservative upgrade: Requires 3 consecutive favorable measurements.
    consecutive_upgrades_++;
    if (consecutive_upgrades_ >= 3) {
      current_variant_index_ = ideal_index;
      consecutive_upgrades_ = 0;
    }
  } else {
    // Maintain current quality
    consecutive_upgrades_ = 0;
  }
}

}  // namespace iptv::abr
