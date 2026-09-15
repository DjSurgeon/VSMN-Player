#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "iptv/network/http_response.hpp"
#include "iptv/network/network_component.hpp"

using namespace iptv;
using namespace iptv::network;
using namespace iptv::manifest;

class FakeHttpClient : public IHttpClient {
 public:
  HttpResponse fake_response{HttpStatusCode::Ok};

  HttpResponse download(const std::string& /*url*/,
                        std::chrono::milliseconds /*timeout_ms*/) override {
    return std::move(fake_response);
  }

  void setNetworkConfig(const NetworkConfig& /*config*/) override {}
  void setRetryPolicy(const RetryPolicy& /*policy*/) override {}
};

TEST(NetworkComponentTest, DownloadSuccess) {
  auto fake_client = std::make_unique<FakeHttpClient>();

  std::string mock_m3u8 = "#EXTM3U\n#EXTINF:10.0,\nhttp://example.com/seg1.ts\n";
  fake_client->fake_response.setStatusCode(HttpStatusCode::Ok);
  fake_client->fake_response.appendToBody(reinterpret_cast<const uint8_t*>(mock_m3u8.data()),
                                          mock_m3u8.size());

  NetworkComponent component(std::move(fake_client));
  auto result = component.downloadPlaylist("http://example.com/test.m3u8");

  ASSERT_TRUE(std::holds_alternative<ParsedPlaylistBundle>(result));
  auto bundle = std::get<ParsedPlaylistBundle>(std::move(result));

  EXPECT_EQ(bundle.playlist.segments.size(), 1);
  EXPECT_EQ(bundle.playlist.segments[0].uri, "http://example.com/seg1.ts");
}

TEST(NetworkComponentTest, HttpError) {
  auto fake_client = std::make_unique<FakeHttpClient>();
  fake_client->fake_response.setStatusCode(HttpStatusCode::NotFound);

  NetworkComponent component(std::move(fake_client));
  auto result = component.downloadPlaylist("http://example.com/test.m3u8");

  ASSERT_TRUE(std::holds_alternative<NetworkError>(result));
  auto error = std::get<NetworkError>(result);
  EXPECT_EQ(error.message, "HTTP Request failed with status 404");
}

TEST(NetworkComponentTest, ParseError) {
  auto fake_client = std::make_unique<FakeHttpClient>();

  std::string mock_m3u8 = "TRASH DATA NOT M3U8";
  fake_client->fake_response.setStatusCode(HttpStatusCode::Ok);
  fake_client->fake_response.appendToBody(reinterpret_cast<const uint8_t*>(mock_m3u8.data()),
                                          mock_m3u8.size());

  NetworkComponent component(std::move(fake_client));
  auto result = component.downloadPlaylist("http://example.com/test.m3u8");

  ASSERT_TRUE(std::holds_alternative<ParseError>(result));
}
