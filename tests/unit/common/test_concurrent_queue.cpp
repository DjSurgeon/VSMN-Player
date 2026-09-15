#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "iptv/common/concurrent_queue.hpp"

using namespace iptv;

// 1. Compile-Time Safety Constraint
// This struct CANNOT be copied. It can only be moved.
// If the ConcurrentQueue accidentally tries to copy it (const or by-value without move),
// the compiler will throw a massive error and stop the build.
struct MoveOnlyBuffer {
  std::string data;
  bool corrupt{false};

  MoveOnlyBuffer() = default;
  explicit MoveOnlyBuffer(std::string d, bool c = false) : data(std::move(d)), corrupt(c) {}

  MoveOnlyBuffer(const MoveOnlyBuffer&) = delete;
  MoveOnlyBuffer& operator=(const MoveOnlyBuffer&) = delete;

  MoveOnlyBuffer(MoveOnlyBuffer&&) noexcept = default;
  MoveOnlyBuffer& operator=(MoveOnlyBuffer&&) noexcept = default;
};

class ConcurrentQueueTest : public ::testing::Test {};

// Test 1: Basic Push and Pop
TEST_F(ConcurrentQueueTest, BasicPushPop) {
  ConcurrentQueue<int> q;
  EXPECT_TRUE(q.empty());

  q.push(42);
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 1);

  std::jthread worker([&](std::stop_token st) {
    auto val = q.pop(st);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
  });
  worker.join();
}

// Test 2: Move Semantics and "Corrupt Data"
TEST_F(ConcurrentQueueTest, MoveSemanticsAndCorruptData) {
  ConcurrentQueue<MoveOnlyBuffer> q;

  // We push a perfectly valid buffer
  q.push(MoveOnlyBuffer{"ValidSegment", false});

  // We push a wildly corrupt buffer. The queue MUST transport it as-is.
  // It is the Decoder's job to validate it later. The Queue is just a dumb, fast pipe.
  q.push(MoveOnlyBuffer{"#$^#%@!@", true});

  std::jthread worker([&](std::stop_token st) {
    auto val1 = q.pop(st);
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1->data, "ValidSegment");
    EXPECT_FALSE(val1->corrupt);

    auto val2 = q.pop(st);
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2->data, "#$^#%@!@");
    EXPECT_TRUE(val2->corrupt);  // The corruption transited perfectly
  });
  worker.join();
}

// Test 3: try_pop() Non-blocking
TEST_F(ConcurrentQueueTest, TryPop) {
  ConcurrentQueue<int> q;

  // Empty queue
  auto val = q.try_pop();
  EXPECT_FALSE(val.has_value());

  q.push(99);

  // Now it has something
  val = q.try_pop();
  EXPECT_TRUE(val.has_value());
  EXPECT_EQ(*val, 99);

  // Empty again
  val = q.try_pop();
  EXPECT_FALSE(val.has_value());
}

// Test 4: Cancellation via std::stop_token
TEST_F(ConcurrentQueueTest, Cancellation) {
  ConcurrentQueue<int> q;

  std::atomic<bool> thread_started{false};
  std::atomic<bool> pop_returned_nullopt{false};

  std::jthread worker([&](std::stop_token st) {
    thread_started = true;
    auto val = q.pop(st);
    if (!val.has_value()) {
      pop_returned_nullopt = true;
    }
  });

  // Wait for thread to actually start waiting
  while (!thread_started) {
    std::this_thread::yield();
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Request stop. This will unblock the cv_.wait() instantly.
  worker.request_stop();
  worker.join();

  EXPECT_TRUE(pop_returned_nullopt);
}

// Test 5: TSan Stress Test (Multi-Producer Multi-Consumer)
TEST_F(ConcurrentQueueTest, TSanStressTest) {
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
