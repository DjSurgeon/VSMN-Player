#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "iptv/network/i_http_client.hpp"

using namespace iptv::network;
using ::testing::_;

// Mock implementation of the abstract interface
class MockHttpClient : public IHttpClient {
 public:
  MOCK_METHOD(HttpResponse, download, (const std::string& url, std::chrono::milliseconds timeout),
              (override));
  MOCK_METHOD(void, setNetworkConfig, (const NetworkConfig& config), (override));
  MOCK_METHOD(void, setRetryPolicy, (const RetryPolicy& policy), (override));
};

TEST(IHttpClientTest, MockingIsSupported) {
  MockHttpClient mock_client;

  // We expect download to be called once with any string and any timeout.
  // It should return a dummy HttpResponse with status code 200 (Ok).
  EXPECT_CALL(mock_client, download(_, _)).Times(1).WillOnce([]() {
    return HttpResponse(HttpStatusCode::Ok);
  });

  // Act
  HttpResponse response =
      mock_client.download("http://fake.url/playlist.m3u8", std::chrono::milliseconds(5000));

  // Assert
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Ok);
  EXPECT_EQ(response.getBytesDownloaded(), 0);
}
