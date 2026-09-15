#include "iptv/abr/abr_manager.hpp"

namespace iptv::abr {

const manifest::VariantStreamRef& AbrManager::selectVariant(
    const std::vector<manifest::VariantStreamRef>& variants, double measured_throughput_mbps) {
  if (variants.empty()) {
    throw std::invalid_argument("AbrManager: La lista de variantes no puede estar vacía.");
  }

  // 1. Inicialización (Primer segmento)
  if (smoothed_throughput_ == 0.0) {
    smoothed_throughput_ = measured_throughput_mbps;

    std::size_t best_index = 0;
    for (std::size_t i = 0; i < variants.size(); ++i) {
      double variant_mbps = static_cast<double>(variants[i].bandwidth) / 1'000'000.0;
      if (variant_mbps <= smoothed_throughput_ * 0.8) {
        best_index = i;
      } else {
        break;
      }
    }
    current_variant_index_ = best_index;
    return variants[current_variant_index_];
  }

  // 2. EWMA Update
  smoothed_throughput_ =
      (alpha_ * measured_throughput_mbps) + ((1.0 - alpha_) * smoothed_throughput_);

  // 3. Calculamos la variante ideal matemáticamente (con margen 80%)
  std::size_t ideal_index = 0;
  for (std::size_t i = 0; i < variants.size(); ++i) {
    double variant_mbps = static_cast<double>(variants[i].bandwidth) / 1'000'000.0;
    if (variant_mbps <= smoothed_throughput_ * 0.8) {
      ideal_index = i;
    } else {
      break;
    }
  }

  // 4. Lógica de Histéresis
  if (ideal_index < current_variant_index_) {
    // Descenso agresivo: Si el throughput es insuficiente, bajamos inmediatamente.
    current_variant_index_ = ideal_index;
    consecutive_upgrades_ = 0;
  } else if (ideal_index > current_variant_index_) {
    // Ascenso conservador: Exigimos 3 mediciones consecutivas favorables.
    consecutive_upgrades_++;
    if (consecutive_upgrades_ >= 3) {
      current_variant_index_ = ideal_index;
      consecutive_upgrades_ = 0;
    }
  } else {
    // Nos mantenemos en la misma
    consecutive_upgrades_ = 0;
  }

  return variants[current_variant_index_];
}

}  // namespace iptv::abr
