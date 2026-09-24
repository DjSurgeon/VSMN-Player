#include <gtest/gtest.h>
#include <httplib.h>
#include <thread>
#include <vector>
#include "iptv/network/http_client.hpp"

using namespace iptv::network;

class HttpClientStressTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<httplib::Server>();
    server_->Get("/stress", [](const httplib::Request&, httplib::Response& res) {
      res.set_content("OK", "text/plain");
    });
    port_ = server_->bind_to_any_port("127.0.0.1");
    server_thread_ = std::thread([this]() { server_->listen_after_bind(); });
  }

  void TearDown() override {
    server_->stop();
    if (server_thread_.joinable()) server_thread_.join();
  }

  std::string getUrl() const { return "http://127.0.0.1:" + std::to_string(port_) + "/stress"; }

  std::unique_ptr<httplib::Server> server_;
  std::thread server_thread_;
  int port_ = 0;
};

// Massive concurrency test (100 threads, 100 requests each = 10,000 reqs)
TEST_F(HttpClientStressTest, MassiveConcurrency) {
  const int num_threads = 100;
  const int downloads_per_thread = 100;
  std::vector<std::thread> workers;

  for (int i = 0; i < num_threads; ++i) {
    workers.emplace_back([this]() {
      HttpClient client;
      for (int j = 0; j < downloads_per_thread; ++j) {
        auto response = client.download(getUrl(), std::chrono::milliseconds(5000));
        EXPECT_TRUE(response.isSuccess());
      }
    });
  }

  for (auto& w : workers) w.join();
}
