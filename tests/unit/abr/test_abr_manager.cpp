#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "iptv/abr/abr_manager.hpp"
#include "iptv/manifest/playlist.hpp"

using namespace iptv::abr;
using namespace iptv::manifest;

class AbrManagerTest : public ::testing::Test {
 protected:
  std::vector<VariantStreamRef> variants_;

  void SetUp() override {
    // Variantes de prueba ordenadas por bitrate (2 Mbps, 5 Mbps, 8 Mbps)
    variants_.push_back(VariantStreamRef{.uri = "360p.m3u8", .bandwidth = 2'000'000});
    variants_.push_back(VariantStreamRef{.uri = "720p.m3u8", .bandwidth = 5'000'000});
    variants_.push_back(VariantStreamRef{.uri = "1080p.m3u8", .bandwidth = 8'000'000});
  }
};

TEST_F(AbrManagerTest, EmptyVariantsThrowsException) {
  AbrManager abr;
  std::vector<VariantStreamRef> empty_variants;
  EXPECT_THROW(abr.selectVariant(empty_variants, 10.0), std::invalid_argument);
}

TEST_F(AbrManagerTest, InitialSelectionRespectsSafetyMargin) {
  AbrManager abr;

  // Throughput: 10 Mbps. 80% es 8 Mbps. Debería elegir la variante 2 (8 Mbps).
  const auto& selected = abr.selectVariant(variants_, 10.0);
  EXPECT_EQ(selected.bandwidth, 8'000'000);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);
}

TEST_F(AbrManagerTest, EwmaIgnoresTransientDrops) {
  AbrManager abr(0.3);  // alpha = 0.3

  // Primera descarga muy rápida a 20 Mbps -> selecciona 1080p (8 Mbps)
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);

  // Microcorte: el segmento baja a 5 Mbps de media.
  // EWMA: 0.3 * 5.0 + 0.7 * 20.0 = 1.5 + 14.0 = 15.5 Mbps
  // Margen de seguridad: 15.5 * 0.8 = 12.4 Mbps
  // Como 12.4 Mbps > 8 Mbps (1080p), el ideal_index sigue siendo 2.
  // Ignoramos el microcorte sin bajar de resolución.
  const auto& selected = abr.selectVariant(variants_, 5.0);
  EXPECT_EQ(selected.bandwidth, 8'000'000);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);
}

TEST_F(AbrManagerTest, HysteresisBlocksPrematureUpgrades) {
  AbrManager abr(0.3);

  // Empezamos bajo (3 Mbps). 80% = 2.4 Mbps -> Selecciona 360p (2 Mbps)
  abr.selectVariant(variants_, 3.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Mejora masiva de red a 20 Mbps.
  // EWMA: 0.3 * 20 + 0.7 * 3.0 = 6.0 + 2.1 = 8.1 Mbps
  // 8.1 * 0.8 = 6.48 Mbps -> Ideal index es 1 (720p a 5Mbps).

  // Segmento 1 favorable: Histéresis bloquea
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Segmento 2 favorable: Histéresis bloquea
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Segmento 3 favorable: Histéresis cede y sube! El EWMA ya ha convergido a >11Mbps,
  // por lo que el ideal index es 2 (1080p).
  const auto& selected = abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);  // 1080p
  EXPECT_EQ(selected.bandwidth, 8'000'000);
}
