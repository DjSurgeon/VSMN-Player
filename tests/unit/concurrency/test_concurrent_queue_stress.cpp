#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "iptv/concurrency/concurrent_queue.hpp"

using iptv::ConcurrentQueue;

class ConcurrentQueueStressTest : public ::testing::Test {};

// TSan Stress Test (Multi-Producer Multi-Consumer)
TEST_F(ConcurrentQueueStressTest, TSanStressTest) {
  ConcurrentQueue<int> q;
  const int num_producers = 4;
  const int num_consumers = 4;
  const int items_per_producer = 5000;

  std::atomic<int> total_consumed{0};

  std::vector<std::jthread> producers;
  std::vector<std::jthread> consumers;

  // Start consumers
  for (int i = 0; i < num_consumers; ++i) {
    consumers.emplace_back([&](std::stop_token st) {
      while (!st.stop_requested()) {
        auto val = q.pop(st);
        if (val) {
          total_consumed++;
        }
      }
    });
  }

  // Start producers
  for (int i = 0; i < num_producers; ++i) {
    producers.emplace_back([&]() {
      for (int j = 0; j < items_per_producer; ++j) {
        q.push(j);
      }
    });
  }

  // Wait for producers to finish
  for (auto& p : producers) {
    p.join();
  }

  // Allow consumers to drain the queue
  while (q.size() > 0) {
    std::this_thread::yield();
  }

  // Tell consumers to exit and wait for them to finish processing
  for (auto& c : consumers) {
    c.request_stop();
    c.join();
  }

  EXPECT_EQ(total_consumed.load(), num_producers * items_per_producer);
}
