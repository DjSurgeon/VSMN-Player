#include "integration/network_subsystem_fixture.hpp"

namespace iptv::test::integration {

std::size_t jitterChunkSize(std::size_t step) {
  static const std::vector<std::size_t> kPattern = [] {
    std::mt19937 gen(7);
    std::uniform_int_distribution<std::size_t> dist(1024, 16384);
    std::vector<std::size_t> pattern(64);
    for (auto& size : pattern) {
      size = dist(gen);
    }
    return pattern;
  }();
  return kPattern[step % kPattern.size()];
}

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

std::vector<uint8_t> generateRandomPayload(std::size_t size) {
  std::vector<uint8_t> payload(size);
  std::mt19937 gen(42);  // deterministic seed
  std::uniform_int_distribution<uint16_t> dist(0, 255);
  for (std::size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<uint8_t>(dist(gen));
  }
  return payload;
}

void NetworkSubsystemIntegrationTest::registerHappyPathRoutes() {
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
}

void NetworkSubsystemIntegrationTest::registerIntermittentErrorRoute() {
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
}

void NetworkSubsystemIntegrationTest::registerJitterRoute() {
  svr.Get("/jitter", [this](const httplib::Request& /*req*/, httplib::Response& res) {
    // httplib allows custom content providers for chunked or raw responses
    res.set_content_provider(
        segment_payload.size(),  // Content length
        "video/MP2T", [this](size_t offset, size_t length, httplib::DataSink& sink) {
          if (offset >= segment_payload.size()) {
            sink.done();
            return true;
          }

          // Jitter attack: bursty chunk sizes, so the client's view of arrival rate is
          // irregular. Deterministic and free, unlike a fixed pause per chunk.
          size_t chunk = std::min(length, jitterChunkSize(offset / 1024));
          sink.write(reinterpret_cast<const char*>(segment_payload.data() + offset), chunk);
          return true;
        });
  });
}

void NetworkSubsystemIntegrationTest::registerCorruptionRoute() {
  svr.Get("/corrupt.ts", [this](const httplib::Request& /*req*/, httplib::Response& res) {
    auto corrupt_payload = segment_payload;
    corrupt_payload[corrupt_payload.size() / 2] ^= 0xFF;  // Bit flip in the middle
    res.set_content(reinterpret_cast<const char*>(corrupt_payload.data()), corrupt_payload.size(),
                    "video/MP2T");
  });
}

std::string NetworkSubsystemIntegrationTest::urlFor(const std::string& path) const {
  return "http://127.0.0.1:" + std::to_string(port) + path;
}

std::unique_ptr<iptv::network::NetworkComponent>
NetworkSubsystemIntegrationTest::createNetworkComponent(int retries) {
  auto client = std::make_unique<iptv::network::HttpClient>();
  iptv::network::RetryPolicy policy;
  policy.max_retries = static_cast<uint32_t>(retries);
  policy.initial_delay = std::chrono::milliseconds(10);
  policy.max_delay = std::chrono::milliseconds(50);
  policy.strategy = iptv::network::BackoffStrategy::Exponential;
  client->setRetryPolicy(policy);
  return std::make_unique<iptv::network::NetworkComponent>(std::move(client));
}

void NetworkSubsystemIntegrationTest::SetUp() {
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

  registerHappyPathRoutes();
  registerIntermittentErrorRoute();
  registerJitterRoute();
  registerCorruptionRoute();

  svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
    std::cerr << "[MOCK SERVER] " << req.method << " " << req.path << " -> " << res.status << "\n";
  });
  port = svr.bind_to_any_port("127.0.0.1");
  server_thread = std::jthread([this]() { svr.listen_after_bind(); });
}

void NetworkSubsystemIntegrationTest::TearDown() {
  svr.stop();
  // server_thread joined automatically via jthread
}

}  // namespace iptv::test::integration
