#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "integration/network_subsystem_fixture.hpp"
#include "iptv/network/network_component.hpp"

using namespace iptv::test::integration;
using iptv::network::MediaSegmentBundle;

TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_JitterYieldsFiniteThroughput) {
  auto network = createNetworkComponent();

  auto res = network->downloadSegment(urlFor("/jitter"));
  ASSERT_TRUE(std::holds_alternative<MediaSegmentBundle>(res));

  auto bundle = std::get<MediaSegmentBundle>(std::move(res));

  // Throughput check. The whole point of this route is that arrivals are irregular, so
  // assert the arithmetic survived it: a finite, non-zero rate. NaN, inf and 0 are the three
  // ways a jittered transfer can corrupt the metric, and each is caught here.
  const double mbps = bundle.metrics.throughputMbps();
  EXPECT_TRUE(std::isfinite(mbps)) << "throughput is not finite: " << mbps;
  EXPECT_GT(mbps, 0.0);
}

TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_MidstreamCorruption) {
  auto network = createNetworkComponent();

  auto res = network->downloadSegment(urlFor("/corrupt.ts"));
  ASSERT_TRUE(std::holds_alternative<MediaSegmentBundle>(res));

  auto bundle = std::get<MediaSegmentBundle>(std::move(res));

  // Buffer size will match, but CRC will fail
  EXPECT_EQ(bundle.raw_buffer.size(), segment_payload.size());
  EXPECT_NE(calculateCrc32(bundle.raw_buffer), expected_crc);
}
