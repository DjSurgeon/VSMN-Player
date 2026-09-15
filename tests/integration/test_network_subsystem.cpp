#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "iptv/manifest/playlist.hpp"
#include "iptv/network/http_client.hpp"
#include "iptv/network/http_init.hpp"
#include "iptv/network/network_component.hpp"
#include "iptv/player/playback_orchestrator.hpp"

using namespace iptv::network;
using namespace iptv::player;
using namespace iptv::manifest;
using namespace iptv::abr;

namespace {

/**
 * @brief Simple CRC32 implementation to verify chunk integrity.
 */
uint32_t calculateCrc32(const std::vector<uint8_t>& data) {
  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) {
      if ((crc & 1) != 0) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

/**
 * @brief Generates a random payload of specified size.
 */
std::vector<uint8_t> generateRandomPayload(size_t size) {
  std::vector<uint8_t> payload(size);
  std::mt19937 gen(42);  // deterministic seed
  std::uniform_int_distribution<uint16_t> dist(0, 255);
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<uint8_t>(dist(gen));
  }
  return payload;
}

}  // namespace

/**
 * @brief Test fixture that sets up an embedded HTTP server for Chaos Testing.
 */
class NetworkSubsystemIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { iptv::network::initialize(); }
  static void TearDownTestSuite() { iptv::network::shutdown(); }

  httplib::Server svr;
  std::jthread server_thread;
  int port = 0;

  std::string master_m3u8;
  std::string variant_m3u8;
  std::vector<uint8_t> segment_payload;
  uint32_t expected_crc = 0;

  void SetUp() override {
    // 1. Setup Data
    master_m3u8 =
        "#EXTM3U\n"
        "#EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080\n"
        "/variant.m3u8\n";

    variant_m3u8 =
        "#EXTM3U\n"
        "#EXT-X-TARGETDURATION:10\n"
        "#EXTINF:10.0,\n"
        "/segment.ts\n"
        "#EXTINF:10.0,\n"
        "/segment2.ts\n"
        "#EXT-X-ENDLIST\n";

    segment_payload = generateRandomPayload(1024 * 1024);  // 1 MB
    expected_crc = calculateCrc32(segment_payload);

    // 2. Setup Routes
    svr.Get("/master.m3u8", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      res.set_content(master_m3u8, "application/vnd.apple.mpegurl");
    });

    svr.Get("/variant.m3u8", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      res.set_content(variant_m3u8, "application/vnd.apple.mpegurl");
    });

    svr.Get("/segment.ts", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      res.set_content(reinterpret_cast<const char*>(segment_payload.data()), segment_payload.size(),
                      "video/MP2T");
    });

    svr.Get("/segment2.ts", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      res.set_content(reinterpret_cast<const char*>(segment_payload.data()), segment_payload.size(),
                      "video/MP2T");
    });

    // Chaos Routes

    // Scenario 1: Random 500 Errors
    svr.Get("/chaos_500.m3u8", [](const httplib::Request& /*req*/, httplib::Response& res) {
      static std::atomic<int> counter{0};
      if (counter++ % 2 == 0) {
        res.status = 500;
        res.set_content("Internal Server Error", "text/plain");
      } else {
        res.status = 200;
        res.set_content("#EXTM3U\n#EXT-X-ENDLIST\n", "application/vnd.apple.mpegurl");
      }
    });

    // Scenario 2: Massive Jitter
    svr.Get("/jitter", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      // httplib allows custom content providers for chunked or raw responses
      res.set_content_provider(
          segment_payload.size(),  // Content length
          "video/MP2T", [this](size_t offset, size_t length, httplib::DataSink& sink) {
            if (offset >= segment_payload.size()) {
              sink.done();
              return true;
            }

            // Jitter attack: Serve in tiny chunks with latency
            size_t chunk = std::min(length, static_cast<size_t>(8192));
            if (chunk > 1024) {
              std::this_thread::sleep_for(std::chrono::milliseconds(2));  // Introduce stall
            }

            sink.write(reinterpret_cast<const char*>(segment_payload.data() + offset), chunk);
            return true;
          });
    });

    // Scenario 3: Mid-stream Payload Corruption (Bit-Flip)
    svr.Get("/corrupt.ts", [this](const httplib::Request& /*req*/, httplib::Response& res) {
      auto corrupt_payload = segment_payload;
      corrupt_payload[corrupt_payload.size() / 2] ^= 0xFF;  // Bit flip in the middle
      res.set_content(reinterpret_cast<const char*>(corrupt_payload.data()), corrupt_payload.size(),
                      "video/MP2T");
    });

    // 3. Start Server on available port
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
      std::cerr << "[MOCK SERVER] " << req.method << " " << req.path << " -> " << res.status
                << "\n";
    });
    port = svr.bind_to_any_port("127.0.0.1");
    server_thread = std::jthread([this]() { svr.listen_after_bind(); });
  }

  void TearDown() override {
    svr.stop();
    // server_thread joined automatically via jthread
  }

  /**
   * @brief Helper to create a configured NetworkComponent pointing to localhost.
   */
  std::unique_ptr<NetworkComponent> createNetworkComponent(int retries = 3) {
    auto client = std::make_unique<HttpClient>();
    RetryPolicy policy;
    policy.max_retries = retries;
    policy.initial_delay = std::chrono::milliseconds(10);
    policy.max_delay = std::chrono::milliseconds(50);
    policy.strategy = BackoffStrategy::Exponential;
    client->setRetryPolicy(policy);
    return std::make_unique<NetworkComponent>(std::move(client));
  }
};

/**
 * @brief Normal End-to-End Flow validation without chaos.
 */
TEST_F(NetworkSubsystemIntegrationTest, EndToEndFlowIntegrity) {
  auto network = createNetworkComponent();
  PlaybackOrchestrator orchestrator(std::move(network));

  std::string master_url = "http://127.0.0.1:" + std::to_string(port) + "/master.m3u8";
  orchestrator.start(master_url);

  auto& queue = orchestrator.getQueue();

  // We expect 2 segments to be downloaded.
  int segments_popped = 0;
  for (int i = 0; i < 2; ++i) {
    MediaSegmentBundle bundle;
    // Timeout of 5 seconds max
    auto start_time = std::chrono::steady_clock::now();
    bool popped = false;
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
      if (auto item = queue.tryPop()) {
        bundle = std::move(*item);
        popped = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (popped) {
      EXPECT_EQ(bundle.raw_buffer.size(), segment_payload.size());
      EXPECT_EQ(calculateCrc32(bundle.raw_buffer), expected_crc);
      segments_popped++;
    }
  }

  EXPECT_EQ(segments_popped, 2);
  orchestrator.stop();
}

/**
 * @brief Verifies that RetryPolicy recovers from intermittent 500 errors.
 */
TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_Random500Recovery) {
  auto network = createNetworkComponent(3);  // 3 retries
  std::string url = "http://127.0.0.1:" + std::to_string(port) + "/chaos_500.m3u8";

  // Using raw downloadPlaylist to verify retry recovery
  auto res = network->downloadPlaylist(url, std::chrono::seconds(1));

  // Should succeed eventually due to retry policy resolving the 500
  ASSERT_TRUE(std::holds_alternative<ParsedPlaylistBundle>(res));
}

/**
 * @brief Validates massive jitter attack. Verifies that throughput is calculated safely and buffer
 * is intact.
 */
TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_JitterAttack) {
  auto network = createNetworkComponent();
  std::string url = "http://127.0.0.1:" + std::to_string(port) + "/jitter";

  auto res = network->downloadSegment(url);
  ASSERT_TRUE(std::holds_alternative<MediaSegmentBundle>(res));

  auto bundle = std::get<MediaSegmentBundle>(std::move(res));

  // Integrity check
  EXPECT_EQ(bundle.raw_buffer.size(), segment_payload.size());
  EXPECT_EQ(calculateCrc32(bundle.raw_buffer), expected_crc);

  // Throughput check (must not be zero/inf due to jitter)
  EXPECT_GT(bundle.metrics.throughputMbps(), 0.0);
}

/**
 * @brief Simulates silent mid-stream corruption (Bit-Flip).
 * The orchestrator or underlying layer doesn't CRC check itself (yet), but we as consumer will
 * reject it.
 */
TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_MidstreamCorruption) {
  auto network = createNetworkComponent();
  std::string url = "http://127.0.0.1:" + std::to_string(port) + "/corrupt.ts";

  auto res = network->downloadSegment(url);
  ASSERT_TRUE(std::holds_alternative<MediaSegmentBundle>(res));

  auto bundle = std::get<MediaSegmentBundle>(std::move(res));

  // Buffer size will match, but CRC will fail
  EXPECT_EQ(bundle.raw_buffer.size(), segment_payload.size());
  EXPECT_NE(calculateCrc32(bundle.raw_buffer), expected_crc);
}

/**
 * @brief Extreme concurrency thundering herd. Tests ConcurrentQueue and libcurl thread-safety under
 * TSan.
 */
TEST_F(NetworkSubsystemIntegrationTest, ChaosMonkey_ThunderingHerd) {
  constexpr int NUM_THREADS = 16;
  std::vector<std::jthread> threads;
  std::atomic<int> successes{0};

  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([this, &successes]() {
      auto network = createNetworkComponent();
      PlaybackOrchestrator orchestrator(std::move(network));

      std::string master_url = "http://127.0.0.1:" + std::to_string(port) + "/master.m3u8";
      orchestrator.start(master_url);

      auto& queue = orchestrator.getQueue();
      MediaSegmentBundle bundle;

      // Try to pop at least 1 segment
      auto start_time = std::chrono::steady_clock::now();
      bool popped = false;
      while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        if (auto item = queue.tryPop()) {
          bundle = std::move(*item);
          popped = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      if (popped) {
        if (calculateCrc32(bundle.raw_buffer) == expected_crc) {
          successes++;
        }
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
