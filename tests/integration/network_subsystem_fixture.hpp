#pragma once

#include <gtest/gtest.h>
#include <httplib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "iptv/concurrency/concurrent_queue.hpp"
#include "iptv/network/http_client.hpp"
#include "iptv/network/http_init.hpp"
#include "iptv/network/network_component.hpp"

namespace iptv::test::integration {

/**
 * @brief Blocks in ConcurrentQueue::pop() until the orchestrator pushes a bundle.
 *
 * Parks in the queue's own condition_variable instead of spinning on try_pop() between timer
 * naps, so the wait ends the instant the producer signals. The bounded future wait is only a
 * safety net for "the producer never arrived", which is a failure we want reported rather than
 * hung on.
 */
template <typename Bundle>
std::optional<Bundle> popWithin(iptv::ConcurrentQueue<Bundle>& queue,
                                std::chrono::milliseconds deadline) {
  std::promise<std::optional<Bundle>> delivered;
  std::future<std::optional<Bundle>> ready = delivered.get_future();

  std::jthread consumer([&](std::stop_token st) { delivered.set_value(queue.pop(st)); });

  // If the deadline expires, forward the cancellation into the parked pop so it unwinds
  // instead of blocking us forever.
  std::stop_source cancel;
  std::stop_callback forward(cancel.get_token(), [&consumer] { consumer.request_stop(); });

  if (ready.wait_for(deadline) != std::future_status::ready) {
    cancel.request_stop();
  }

  std::optional<Bundle> bundle = ready.get();
  consumer.join();
  return bundle;
}

/**
 * @brief Chunk size for one step of the jitter stream.
 *
 * Jitter is irregular inter-arrival timing, which a stream of uneven chunks reproduces far
 * better than a uniform pause between them: consecutive reads see instantaneous throughput
 * swinging by more than an order of magnitude instead of a flat, constant rate. Fixed seed,
 * so the burst sequence is byte-for-byte identical on every run.
 */
std::size_t jitterChunkSize(std::size_t step);

/**
 * @brief CRC32 over a payload, used to assert chunk integrity end to end.
 */
uint32_t calculateCrc32(const std::vector<uint8_t>& data);

/**
 * @brief Generates a random payload of the specified size from a fixed seed.
 */
std::vector<uint8_t> generateRandomPayload(std::size_t size);

/**
 * @brief Embedded HTTP server exposing a tiny HLS stream plus chaos routes.
 *
 * Routes are deterministic by construction: the segment bytes come from a fixed
 * seed and are never mutated, so a CRC mismatch can only come from the transfer
 * path under test, not from the fixture.
 */
class NetworkSubsystemIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { iptv::network::initialize(); }
  static void TearDownTestSuite() { iptv::network::shutdown(); }

  NetworkSubsystemIntegrationTest() = default;

  void SetUp() override;
  void TearDown() override;

  /**
   * @brief Registers the well-formed playlist and segment routes.
   */
  void registerHappyPathRoutes();

  /**
   * @brief Registers the route that alternates 500 and 200 responses.
   */
  void registerIntermittentErrorRoute();

  /**
   * @brief Registers the route that streams the payload in irregular chunks.
   */
  void registerJitterRoute();

  /**
   * @brief Registers the route that flips one byte mid-payload.
   */
  void registerCorruptionRoute();

  /**
   * @brief Builds the absolute URL of a route on this fixture's ephemeral port.
   */
  [[nodiscard]] std::string urlFor(const std::string& path) const;

  /**
   * @brief Builds a NetworkComponent pointed at localhost with a short retry policy.
   */
  [[nodiscard]] std::unique_ptr<iptv::network::NetworkComponent> createNetworkComponent(
      int retries = 3);

  httplib::Server svr;
  std::jthread server_thread;
  int port = 0;

  std::string master_m3u8;
  std::string variant_m3u8;
  std::vector<uint8_t> segment_payload;
  uint32_t expected_crc = 0;
};

}  // namespace iptv::test::integration
