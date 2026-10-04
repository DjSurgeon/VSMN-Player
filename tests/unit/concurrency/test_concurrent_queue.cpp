#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "iptv/concurrency/concurrent_queue.hpp"

using namespace iptv;

namespace {

// Compile-time safety constraint: this type CANNOT be copied, only moved. If ConcurrentQueue
// ever tried to copy it, the build breaks instead of silently paying for a deep copy.
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

constexpr std::size_t kSegmentBytes = 5u * 1024u * 1024u;  // 5 MB media segment
constexpr std::size_t kTortureRounds = 400;
constexpr std::chrono::seconds kTortureDeadline{60};

std::uint8_t payload_byte(std::size_t index) { return static_cast<std::uint8_t>(index * 31u + 7u); }

// Turns a missed wakeup (an unbreakable block) into a deterministic failure with a readable
// message instead of a test binary that hangs until the CI job timeout kills it. It polls an
// atomic rather than parking on a condition_variable: a std::mutex inside a helper that lives
// in a GTest stack frame confuses ThreadSanitizer's mutex ownership tracking, since
// std::mutex never calls pthread_mutex_destroy and the address gets recycled by the next test.
class ProgressWatchdog {
 public:
  explicit ProgressWatchdog(std::chrono::seconds limit) {
    worker_ = std::jthread([this, limit] {
      const auto deadline = std::chrono::steady_clock::now() + limit;
      while (!finished_.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
          std::fputs(
              "\nConcurrentQueueTest: no progress within the deadline. A consumer is still "
              "blocked in ConcurrentQueue::pop(), so a wakeup was missed (deadlock).\n",
              stderr);
          std::fflush(stderr);
          std::abort();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    });
  }

  ProgressWatchdog(const ProgressWatchdog&) = delete;
  ProgressWatchdog& operator=(const ProgressWatchdog&) = delete;

  ~ProgressWatchdog() { mark_finished(); }

  void mark_finished() { finished_.store(true, std::memory_order_release); }

 private:
  // Declared before worker_ so the flag outlives the join.
  std::atomic<bool> finished_{false};
  std::jthread worker_;
};

enum class Action { kPush, kStop, kBoth };

}  // namespace

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

// Test 6: TSan torture test. Each round spawns a consumer that immediately blocks in pop(),
// then the main thread races a randomized action against the parking window: a push, a stop
// request, or both in a random order. Every round must terminate, otherwise a wakeup was
// missed. run under ThreadSanitizer this also hammers the stop_callback/cv paths hard enough
// to expose data races that the single-shot Cancellation test cannot reach.
TEST_F(ConcurrentQueueTest, StopTokenCancellation) {
  ConcurrentQueue<int> q;
  std::mt19937 rng{0xC0FFEEu};

  ProgressWatchdog watchdog(kTortureDeadline);

  std::size_t delivered = 0;
  std::size_t cancelled = 0;

  for (std::size_t round = 0; round < kTortureRounds; ++round) {
    const int payload = static_cast<int>(round);
    const auto action = static_cast<Action>(rng() % 3u);
    const bool stop_first = (rng() % 2u) == 0u;
    const unsigned spin_budget = rng() % 64u;

    std::optional<int> received;
    std::atomic<bool> entered{false};

    std::jthread consumer([&](std::stop_token st) {
      entered.store(true, std::memory_order_release);
      received = q.pop(st);
    });

    // Ranging the spin budget lands the action before, during and after the consumer parks.
    for (unsigned spin = 0; spin < spin_budget && !entered.load(std::memory_order_acquire);
         ++spin) {
      std::this_thread::yield();
    }

    const auto push_payload = [&] { q.push(payload); };
    const auto request_stop = [&] { consumer.request_stop(); };

    switch (action) {
      case Action::kPush:
        push_payload();
        break;
      case Action::kStop:
        request_stop();
        break;
      case Action::kBoth:
        if (stop_first) {
          request_stop();
          push_payload();
        } else {
          push_payload();
          request_stop();
        }
        break;
    }

    // Blocks forever if the wakeup was lost. The watchdog converts that hang into a failure.
    consumer.join();

    if (received.has_value()) {
      // The item reached the consumer: identical value, and the queue is drained.
      EXPECT_EQ(*received, payload) << "round " << round;
      EXPECT_TRUE(q.empty()) << "round " << round;
      ++delivered;
    } else {
      // pop() gives cancellation priority, so the item is still owned by the queue. It must be
      // recoverable: lost, duplicated or corrupted items break the pipeline contract.
      auto pending = q.try_pop();
      if (action == Action::kBoth) {
        ASSERT_TRUE(pending.has_value()) << "round " << round << ": item vanished after cancel";
        EXPECT_EQ(*pending, payload) << "round " << round;
        EXPECT_TRUE(q.empty()) << "round " << round;
      } else {
        EXPECT_FALSE(pending.has_value()) << "round " << round;
      }
      ++cancelled;
    }

    EXPECT_TRUE(q.empty()) << "round " << round;
  }

  EXPECT_EQ(delivered + cancelled, kTortureRounds);
  EXPECT_GT(delivered, 0u);
  EXPECT_GT(cancelled, 0u);

  watchdog.mark_finished();
}

// Test 7: Zero-copy proof for large media buffers. A std::vector<std::uint8_t> payload keeps
// its heap address across push -> pop and across a second hop, which is only possible if the
// queue moves the buffer instead of copying its bytes. Any accidental copy (const& overload,
// by-value push without std::move, extra local round trip) changes the address and fails here.
TEST_F(ConcurrentQueueTest, ZeroCopyMoveSemantics) {
  ConcurrentQueue<std::vector<std::uint8_t>> q;

  std::vector<std::uint8_t> segment(kSegmentBytes);
  for (std::size_t i = 0; i < segment.size(); ++i) {
    segment[i] = payload_byte(i);
  }

  const std::uint8_t* const payload_address = segment.data();
  ASSERT_NE(payload_address, nullptr);

  q.push(std::move(segment));
  EXPECT_EQ(q.size(), 1u);
  // Ownership left the caller entirely: moved, not duplicated.
  EXPECT_TRUE(segment.empty());

  std::optional<std::vector<std::uint8_t>> popped;
  std::jthread consumer([&](std::stop_token st) { popped = q.pop(st); });
  consumer.join();

  ASSERT_TRUE(popped.has_value());
  EXPECT_EQ(popped->size(), kSegmentBytes);
  EXPECT_EQ(popped->data(), payload_address) << "blocking pop copied the payload";
  EXPECT_EQ(static_cast<int>((*popped)[0]), static_cast<int>(payload_byte(0)));
  EXPECT_EQ(static_cast<int>((*popped)[kSegmentBytes - 1]),
            static_cast<int>(payload_byte(kSegmentBytes - 1)));

  // Taking ownership from the optional must not reallocate either.
  std::vector<std::uint8_t> owned = std::move(*popped);
  EXPECT_EQ(owned.data(), payload_address);
  EXPECT_EQ(owned.size(), kSegmentBytes);

  // Second hop through the queue, this time via try_pop(): same address, same bytes.
  q.push(std::move(owned));
  auto polled = q.try_pop();
  ASSERT_TRUE(polled.has_value());
  EXPECT_EQ(polled->data(), payload_address) << "try_pop() copied the payload";
  EXPECT_EQ(polled->size(), kSegmentBytes);
  EXPECT_EQ(static_cast<int>((*polled)[kSegmentBytes / 2]),
            static_cast<int>(payload_byte(kSegmentBytes / 2)));
  EXPECT_TRUE(q.empty());

  // Small payloads keep the same guarantee, in both directions of the API.
  std::vector<std::uint8_t> atom{0xDE, 0xAD, 0xBE, 0xEF};
  const std::uint8_t* const atom_address = atom.data();
  q.push(std::move(atom));
  EXPECT_TRUE(atom.empty());

  auto atom_out = q.try_pop();
  ASSERT_TRUE(atom_out.has_value());
  EXPECT_EQ(atom_out->data(), atom_address);
  EXPECT_EQ(atom_out->size(), 4u);
  EXPECT_EQ(atom_out->at(0), 0xDE);
  EXPECT_EQ(atom_out->at(3), 0xEF);
  EXPECT_TRUE(q.empty());
}
