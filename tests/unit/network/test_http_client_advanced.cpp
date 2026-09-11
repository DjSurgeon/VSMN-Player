#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include "iptv/network/http_client.hpp"

using namespace iptv::network;

class HttpClientAdvancedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<httplib::Server>();

    // Success Route
    server_->Get("/success", [](const httplib::Request&, httplib::Response& res) {
      res.set_content("Hello World!", "text/plain");
    });

    // Redirect Storm Routes
    server_->Get("/redir1", [](const httplib::Request&, httplib::Response& res) {
      res.set_redirect("/redir2", 301);
    });
    server_->Get("/redir2", [](const httplib::Request&, httplib::Response& res) {
      res.set_redirect("/redir3", 301);
    });
    server_->Get("/redir3", [](const httplib::Request&, httplib::Response& res) {
      res.set_redirect("/target", 301);
    });
    server_->Get("/target", [](const httplib::Request&, httplib::Response& res) {
      res.set_content("Target Reached", "text/plain");
    });

    // Stall route
    server_->Get("/stall", [](const httplib::Request&, httplib::Response& res) {
      res.set_content_provider(100, "text/plain",
                               [](size_t /*offset*/, size_t /*length*/, httplib::DataSink& sink) {
                                 sink.write("1", 1);
                                 std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                 return true;
                               });
    });

    // Large payload for Move Semantics
    server_->Get("/large", [](const httplib::Request&, httplib::Response& res) {
      std::string payload(5 * 1024 * 1024, 'A');  // 5MB
      res.set_content(payload, "application/octet-stream");
    });

    port_ = server_->bind_to_any_port("127.0.0.1");
    server_thread_ = std::thread([this]() { server_->listen_after_bind(); });
  }

  void TearDown() override {
    server_->stop();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  std::string getUrl(const std::string& path) const {
    return "http://127.0.0.1:" + std::to_string(port_) + path;
  }

  std::unique_ptr<httplib::Server> server_;
  std::thread server_thread_;
  int port_ = 0;
};

// 1. TSan Fire Test: Concurrent multithreaded downloads
TEST_F(HttpClientAdvancedTest, ConcurrentDownloads) {
  const int num_threads = 4;
  const int downloads_per_thread = 10;
  std::vector<std::thread> workers;

  for (int i = 0; i < num_threads; ++i) {
    workers.emplace_back([this]() {
      HttpClient client;
      for (int j = 0; j < downloads_per_thread; ++j) {
        auto response = client.download(getUrl("/success"), std::chrono::milliseconds(5000));
        EXPECT_TRUE(response.isSuccess());
        const auto& body = response.getBody();
        std::string content(body.begin(), body.end());
        EXPECT_EQ(content, "Hello World!");
      }
    });
  }

  for (auto& w : workers) {
    w.join();
  }
}

// 2. Zero-Copy and Move Semantics
TEST_F(HttpClientAdvancedTest, MoveSemanticsZeroCopy) {
  HttpClient client1;
  auto response = client1.download(getUrl("/large"), std::chrono::milliseconds(10000));
  EXPECT_TRUE(response.isSuccess());
  EXPECT_EQ(response.getBody().size(), 5 * 1024 * 1024);

  // Move client
  HttpClient client2 = std::move(client1);

  // Moved client should work correctly
  auto response2 = client2.download(getUrl("/success"), std::chrono::milliseconds(5000));
  EXPECT_TRUE(response2.isSuccess());
}

// 3. Anti-Stall
TEST_F(HttpClientAdvancedTest, LowSpeedLimitStall) {
  HttpClient client;
  NetworkConfig config;
  config.timeout = std::chrono::milliseconds(10000);  // Plenty of timeout, but it stalls
  client.setNetworkConfig(config);

  RetryPolicy policy;
  policy.max_retries = 0;
  client.setRetryPolicy(policy);

  auto start = std::chrono::steady_clock::now();
  auto response = client.download(getUrl("/stall"), std::chrono::milliseconds(0));
  auto duration = std::chrono::steady_clock::now() - start;

  EXPECT_FALSE(response.isSuccess());
  // The stall time should be around 3 seconds (low speed time configured in http_client.cpp)
  EXPECT_GE(duration, std::chrono::seconds(3));
  EXPECT_LT(duration, std::chrono::seconds(5));
}

// 4. Redirect Storm
TEST_F(HttpClientAdvancedTest, RedirectStorm) {
  HttpClient client;
  NetworkConfig config;
  config.follow_redirects = true;
  client.setNetworkConfig(config);

  auto response = client.download(getUrl("/redir1"), std::chrono::milliseconds(5000));
  EXPECT_TRUE(response.isSuccess());

  const auto& body = response.getBody();
  std::string content(body.begin(), body.end());
  EXPECT_EQ(content, "Target Reached");
}
