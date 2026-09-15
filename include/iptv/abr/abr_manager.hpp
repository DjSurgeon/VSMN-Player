#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "iptv/manifest/playlist.hpp"

namespace iptv::abr {

/**
 * @brief Adaptive Bitrate Manager
 *
 * Módulo matemático determinista para seleccionar la calidad de vídeo óptima en
 * función del ancho de banda medido, utilizando Suavizado Exponencial (EWMA) e
 * Histéresis direccional para evitar el efecto acordeón.
 */
class AbrManager {
 public:
  explicit AbrManager(double smoothing_factor = 0.3) : alpha_(smoothing_factor) {}

  /**
   * @brief Selecciona la mejor variante posible basada en el throughput suavizado.
   *
   * @param variants Lista de variantes disponibles (deben venir ordenadas por bitrate, de menor a
   * mayor).
   * @param measured_throughput_mbps Throughput real medido en el último segmento (Mbps).
   * @return Referencia a la variante óptima elegida.
   * @throws std::invalid_argument si la lista de variantes está vacía.
   */
  [[nodiscard]] const manifest::VariantStreamRef& selectVariant(
      const std::vector<manifest::VariantStreamRef>& variants, double measured_throughput_mbps);

  [[nodiscard]] double currentSmoothedThroughput() const noexcept { return smoothed_throughput_; }

  // Exponemos el índice para testing o depuración
  [[nodiscard]] std::size_t getCurrentVariantIndex() const noexcept {
    return current_variant_index_;
  }

 private:
  double alpha_{0.3};
  double smoothed_throughput_{0.0};
  int consecutive_upgrades_{0};
  std::size_t current_variant_index_{0};
};

}  // namespace iptv::abr
