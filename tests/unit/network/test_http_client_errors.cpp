#include <gtest/gtest.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "iptv/network/http_client.hpp"

using namespace iptv::network;

class HttpClientErrorsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_ = std::make_unique<httplib::Server>();

    // 1. Timeout Route: Sleeps longer than the client's configured timeout
    server_->Get("/timeout", [](const httplib::Request&, httplib::Response& res) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      res.set_content("Too late", "text/plain");
    });

    // 3. 404 Route: Fail-Fast testing
    server_->Get("/404", [this](const httplib::Request&, httplib::Response& res) {
      not_found_counter_++;
      res.status = 404;
      res.set_content("Not Found", "text/plain");
    });

    // 3.1. 403 Route: Fail-Fast testing (Auth Error)
    server_->Get("/403", [this](const httplib::Request&, httplib::Response& res) {
      forbidden_counter_++;
      res.status = 403;
      res.set_content("Forbidden", "text/plain");
    });

    // 4. 500 Route: Retry loop testing
    server_->Get("/500", [this](const httplib::Request&, httplib::Response& res) {
      internal_error_counter_++;
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
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

 public:
  std::atomic<int> not_found_counter_{0};
  std::atomic<int> forbidden_counter_{0};
  std::atomic<int> internal_error_counter_{0};
};

// 1. TimeoutThrowsGracefully
TEST_F(HttpClientErrorsTest, TimeoutThrowsGracefully) {
  HttpClient client;

  // Disable retries so we only measure the timeout itself
  RetryPolicy policy;
  policy.max_retries = 0;
  client.setRetryPolicy(policy);

  // Timeout of 10ms, server takes 50ms
  auto response = client.download(getUrl("/timeout"), std::chrono::milliseconds(10));

  EXPECT_FALSE(response.isSuccess());
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Unknown);  // Transport error overrides status
}

// 2. DnsFailureIsGraceful
TEST_F(HttpClientErrorsTest, DnsFailureIsGraceful) {
  HttpClient client;

  // Set 1 retry with 1ms backoff to test transient DNS failure quickly
  RetryPolicy policy;
  policy.max_retries = 1;
  policy.initial_delay = std::chrono::milliseconds(1);
  client.setRetryPolicy(policy);

  auto response =
      client.download("http://este-dominio-no-existe.local", std::chrono::milliseconds(5000));

  EXPECT_FALSE(response.isSuccess());
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Unknown);
}

// 3. FailFastOn404
TEST_F(HttpClientErrorsTest, FailFastOn404) {
  HttpClient client;

  // Set 10 retries. A 404 should ignore this and fail fast on the first try.
  RetryPolicy policy;
  policy.max_retries = 10;
  policy.initial_delay = std::chrono::milliseconds(1);
  client.setRetryPolicy(policy);

  auto response = client.download(getUrl("/404"), std::chrono::milliseconds(5000));

  EXPECT_FALSE(response.isSuccess());
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::NotFound);
  EXPECT_EQ(not_found_counter_.load(), 1);  // Exactamente 1 petición, sin reintentos
}

// 3.1. FailFastOn403Forbidden
TEST_F(HttpClientErrorsTest, FailFastOn403Forbidden) {
  HttpClient client;

  // Set 5 retries. A 403 is a client error, it should not retry.
  RetryPolicy policy;
  policy.max_retries = 5;
  policy.initial_delay = std::chrono::milliseconds(1);
  client.setRetryPolicy(policy);

  auto response = client.download(getUrl("/403"), std::chrono::milliseconds(5000));

  EXPECT_FALSE(response.isSuccess());
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::Forbidden);
  EXPECT_EQ(forbidden_counter_.load(), 1);  // Cero reintentos
}

// 4. RetryLoopRespectsMaxCount
TEST_F(HttpClientErrorsTest, RetryLoopRespectsMaxCount) {
  HttpClient client;

  // Set 3 retries (total 4 attempts) with 1ms backoff
  RetryPolicy policy;
  policy.max_retries = 3;
  policy.initial_delay = std::chrono::milliseconds(1);
  policy.strategy = BackoffStrategy::Fixed;
  client.setRetryPolicy(policy);

  auto response = client.download(getUrl("/500"), std::chrono::milliseconds(5000));

  EXPECT_FALSE(response.isSuccess());
  EXPECT_EQ(response.getStatusCode(), HttpStatusCode::InternalServerError);
  EXPECT_EQ(internal_error_counter_.load(), 4);  // 1 original + 3 reintentos
}
