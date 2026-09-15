#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "iptv/network/network_component.hpp"
#include "iptv/player/playback_orchestrator.hpp"

using namespace iptv::player;
using namespace iptv::network;
using namespace iptv::manifest;
using ::testing::_;
using ::testing::Return;

class MockHttpClient : public IHttpClient {
 public:
  MOCK_METHOD(HttpResponse, download,
              (const std::string& url, std::chrono::milliseconds timeout,
               std::stop_token stop_token),
              (override));
  MOCK_METHOD(void, setNetworkConfig, (const NetworkConfig& config), (override));
  MOCK_METHOD(void, setRetryPolicy, (const RetryPolicy& policy), (override));
};

// Basic sanity test to ensure it compiles and starts/stops cleanly
TEST(PlaybackOrchestratorTest, StartAndStopAreThreadSafeAndClean) {
  auto mock_http = std::make_unique<MockHttpClient>();

  // We expect download to be called and return a fake response to avoid segfaults
  EXPECT_CALL(*mock_http, download(_, _, _)).WillRepeatedly([]() {
    return HttpResponse(HttpStatusCode::NotFound);  // Force fast exit
  });

  auto network = std::make_unique<NetworkComponent>(std::move(mock_http));

  PlaybackOrchestrator orchestrator(std::move(network));

  // Should start and stop cleanly
  orchestrator.start("dummy.m3u8");
  orchestrator.stop();

  // Double stop should be safe
  orchestrator.stop();

  // Starting again should work
  orchestrator.start("dummy2.m3u8");
  orchestrator.stop();
}
