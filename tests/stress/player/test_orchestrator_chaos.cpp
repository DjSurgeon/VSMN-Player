#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "chaos_transport.hpp"
#include "iptv/network/network_component.hpp"
#include "iptv/player/playback_orchestrator.hpp"

using iptv::ConcurrentQueue;
using iptv::network::MediaSegmentBundle;
using iptv::network::NetworkComponent;
using iptv::player::PlaybackOrchestrator;
using iptv::test::chaos::ChaosConfig;
using iptv::test::chaos::ChaosTransport;
using iptv::test::chaos::collect;
using iptv::test::chaos::fnv1a;
using iptv::test::chaos::kBaseUrl;
using iptv::test::chaos::kSegmentPayloadBytes;
using iptv::test::chaos::makeClient;

namespace {

/**
 * @brief Asserts one downloaded bundle carries the payload the transport served.
 */
void expectIntactBundle(const MediaSegmentBundle& bundle, int segment_index, std::uint64_t crc) {
  ASSERT_EQ(bundle.raw_buffer.size(), kSegmentPayloadBytes) << "segment " << segment_index;
  EXPECT_EQ(fnv1a(bundle.raw_buffer.data(), bundle.raw_buffer.size()), crc)
      << "segment " << segment_index;
}

/**
 * @brief Asserts the throughput the transfer reported stays in the expected band.
 */
void expectSteadyThroughput(const MediaSegmentBundle& bundle, const std::string& context) {
  EXPECT_GT(bundle.metrics.throughputMbps(), 1.1) << context;
  EXPECT_LT(bundle.metrics.throughputMbps(), 1.3) << context;
}

}  // namespace

class OrchestratorChaosTest : public ::testing::Test {
 protected:
  void logSeed(const std::string& label, std::uint32_t seed) {
    ::testing::Test::RecordProperty(label, std::to_string(seed));
    GTEST_LOG_(INFO) << "chaos seed (" << label << ") = " << seed;
  }
};

TEST_F(OrchestratorChaosTest, HappyPathUnderLoad) {
  constexpr std::uint32_t kSeed = 0x00C0FFEEu;
  constexpr int kSegmentCount = 48;
  logSeed("happy_path_seed", kSeed);

  ChaosConfig config;
  config.seed = kSeed;
  config.segment_count = kSegmentCount;
  config.inject_chaos = false;

  ChaosTransport transport(config);
  PlaybackOrchestrator orchestrator(std::make_unique<NetworkComponent>(makeClient(transport)));

  orchestrator.start(std::string(kBaseUrl) + "/master.m3u8");

  ConcurrentQueue<MediaSegmentBundle>& queue = orchestrator.getQueue();
  std::vector<MediaSegmentBundle> bundles = collect(queue, kSegmentCount);

  orchestrator.stop();

  ASSERT_EQ(bundles.size(), static_cast<std::size_t>(kSegmentCount));
  EXPECT_EQ(transport.masterRequests(), 1u);
  EXPECT_EQ(transport.segmentRequests(), static_cast<std::size_t>(kSegmentCount));
  EXPECT_EQ(transport.chaosRequests(), 0u);
  EXPECT_EQ(transport.highestVariantServed(), 0);

  for (std::size_t i = 0; i < bundles.size(); ++i) {
    expectSteadyThroughput(bundles[i], "segment " + std::to_string(i));
    expectIntactBundle(bundles[i], static_cast<int>(i),
                       transport.expectedChecksum(static_cast<int>(i)));
  }

  EXPECT_TRUE(queue.empty());
}

TEST_F(OrchestratorChaosTest, DeterministicNetworkChaos) {
  constexpr std::uint32_t kBaseSeed = 0x0BADC0DEu;
  constexpr int kRounds = 6;
  constexpr int kSegmentCount = 48;

  std::size_t total_bundles = 0;
  std::size_t total_chaos = 0;

  for (int round = 0; round < kRounds; ++round) {
    const std::uint32_t seed = kBaseSeed + static_cast<std::uint32_t>(round);
    logSeed("chaos_seed_round_" + std::to_string(round), seed);

    ChaosConfig config;
    config.seed = seed;
    config.segment_count = kSegmentCount;
    config.inject_chaos = true;

    ChaosTransport transport(config);
    PlaybackOrchestrator orchestrator(std::make_unique<NetworkComponent>(makeClient(transport)));

    orchestrator.start(std::string(kBaseUrl) + "/master.m3u8");

    ConcurrentQueue<MediaSegmentBundle>& queue = orchestrator.getQueue();
    ASSERT_TRUE(transport.waitUntilTerminal());

    const std::size_t expected = transport.successfulSegmentServes();
    std::vector<MediaSegmentBundle> bundles = collect(queue, expected);

    orchestrator.stop();

    const std::string round_tag = "round " + std::to_string(round);
    EXPECT_EQ(transport.masterRequests(), 1u) << round_tag;
    EXPECT_GT(transport.chaosRequests(), 0u) << round_tag;
    EXPECT_EQ(bundles.size(), expected) << round_tag;
    EXPECT_EQ(transport.segmentRequests(),
              transport.successfulSegmentServes() + transport.transportFailures())
        << round_tag;
    EXPECT_TRUE(queue.empty()) << round_tag;

    for (std::size_t i = 0; i < bundles.size(); ++i) {
      ASSERT_EQ(bundles[i].raw_buffer.size(), kSegmentPayloadBytes)
          << round_tag << " segment " << i;
      expectSteadyThroughput(bundles[i], round_tag + " segment " + std::to_string(i));
    }

    total_bundles += bundles.size();
    total_chaos += transport.chaosRequests();
  }

  EXPECT_GT(total_bundles, 0u);
  EXPECT_GT(total_chaos, 0u);
}
