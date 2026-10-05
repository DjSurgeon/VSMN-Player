#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#include "integration/network_subsystem_fixture.hpp"
#include "iptv/network/network_component.hpp"
#include "iptv/player/playback_orchestrator.hpp"

using namespace iptv::test::integration;
using iptv::network::MediaSegmentBundle;
using iptv::network::NetworkComponent;
using iptv::network::ParsedPlaylistBundle;
using iptv::player::PlaybackOrchestrator;

TEST_F(NetworkSubsystemIntegrationTest, EndToEndFlowIntegrity) {
  auto network = createNetworkComponent();
  PlaybackOrchestrator orchestrator(std::move(network));

  orchestrator.start(urlFor("/master.m3u8"));

  auto& queue = orchestrator.getQueue();

  // We expect 2 segments to be downloaded.
  int segments_popped = 0;
  for (int i = 0; i < 2; ++i) {
    // Blocks until the orchestrator pushes the next bundle, or fails after 5 seconds.
    auto bundle = popWithin(queue, std::chrono::seconds(5));

    if (bundle.has_value()) {
      EXPECT_EQ(bundle->raw_buffer.size(), segment_payload.size());
      EXPECT_EQ(calculateCrc32(bundle->raw_buffer), expected_crc);
      segments_popped++;
    }
  }

  EXPECT_EQ(segments_popped, 2);
  orchestrator.stop();
}

TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_Random500Recovery) {
  auto network = createNetworkComponent(3);  // 3 retries

  // Using raw downloadPlaylist to verify retry recovery
  auto res = network->downloadPlaylist(urlFor("/chaos_500.m3u8"), std::chrono::seconds(1));

  // Should succeed eventually due to retry policy resolving the 500
  ASSERT_TRUE(std::holds_alternative<ParsedPlaylistBundle>(res));
}

TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_JitterAttack) {
  auto network = createNetworkComponent();

  auto res = network->downloadSegment(urlFor("/jitter"));
  ASSERT_TRUE(std::holds_alternative<MediaSegmentBundle>(res));

  auto bundle = std::get<MediaSegmentBundle>(std::move(res));

  // Integrity check
  EXPECT_EQ(bundle.raw_buffer.size(), segment_payload.size());
  EXPECT_EQ(calculateCrc32(bundle.raw_buffer), expected_crc);
}

TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_ThunderingHerd) {
  constexpr int NUM_THREADS = 16;
  std::vector<std::jthread> threads;
  std::atomic<int> successes{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([this, &successes]() {
      auto network = createNetworkComponent();
      PlaybackOrchestrator orchestrator(std::move(network));

      orchestrator.start(urlFor("/master.m3u8"));

      auto& queue = orchestrator.getQueue();

      // Try to pop at least 1 segment; blocks until the orchestrator pushes one.
      auto bundle = popWithin(queue, std::chrono::seconds(2));

      if (bundle.has_value() && calculateCrc32(bundle->raw_buffer) == expected_crc) {
        successes++;
      }
      orchestrator.stop();
    });
  }

  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  // Expect all threads to successfully download at least 1 intact segment concurrently
  EXPECT_EQ(successes.load(), NUM_THREADS);
}
