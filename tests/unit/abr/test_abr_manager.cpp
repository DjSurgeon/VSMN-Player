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
    // Test variants sorted by bitrate (2 Mbps, 5 Mbps, 8 Mbps)
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

  // Throughput: 10 Mbps. 80% is 8 Mbps. Should select variant 2 (8 Mbps).
  const auto& selected = abr.selectVariant(variants_, 10.0);
  EXPECT_EQ(selected.bandwidth, 8'000'000);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);
}

TEST_F(AbrManagerTest, EwmaIgnoresTransientDrops) {
  AbrManager abr(0.3);  // alpha = 0.3

  // First very fast download at 20 Mbps -> selects 1080p (8 Mbps)
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);

  // Micro-drop: segment drops to 5 Mbps average.
  // EWMA: 0.3 * 5.0 + 0.7 * 20.0 = 1.5 + 14.0 = 15.5 Mbps
  // Safety margin: 15.5 * 0.8 = 12.4 Mbps
  // Since 12.4 Mbps > 8 Mbps (1080p), the ideal_index remains 2.
  // We ignore the micro-drop without downgrading resolution.
  const auto& selected = abr.selectVariant(variants_, 5.0);
  EXPECT_EQ(selected.bandwidth, 8'000'000);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);
}

TEST_F(AbrManagerTest, HysteresisBlocksPrematureUpgrades) {
  AbrManager abr(0.3);

  // Start low (3 Mbps). 80% = 2.4 Mbps -> Selects 360p (2 Mbps)
  abr.selectVariant(variants_, 3.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Massive network improvement to 20 Mbps.
  // EWMA: 0.3 * 20 + 0.7 * 3.0 = 6.0 + 2.1 = 8.1 Mbps
  // 8.1 * 0.8 = 6.48 Mbps -> Ideal index is 1 (720p at 5Mbps).

  // Favorable segment 1: Hysteresis blocks upgrade
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Favorable segment 2: Hysteresis blocks upgrade
  abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 0);

  // Favorable segment 3: Hysteresis yields and upgrades! EWMA converged to >11Mbps,
  // so the ideal index is 2 (1080p).
  const auto& selected = abr.selectVariant(variants_, 20.0);
  EXPECT_EQ(abr.getCurrentVariantIndex(), 2);  // 1080p
  EXPECT_EQ(selected.bandwidth, 8'000'000);
}
