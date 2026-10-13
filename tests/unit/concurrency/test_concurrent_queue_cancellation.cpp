#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <latch>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include "concurrency/concurrent_queue_fixture.hpp"
#include "iptv/concurrency/concurrent_queue.hpp"

using iptv::ConcurrentQueue;
using iptv::test::concurrency::ProgressWatchdog;

namespace {

enum class Action { kPush, kStop, kBoth };

}  // namespace

class ConcurrentQueueCancellationTest : public ::testing::Test {};

// Test 1: Cancellation via std::stop_token
TEST_F(ConcurrentQueueCancellationTest, Cancellation) {
  ConcurrentQueue<int> q;

  std::atomic<bool> pop_returned_nullopt{false};
  std::latch in_pop{1};

  std::jthread worker([&](std::stop_token st) {
    // Signal that we are about to block, then block. request_stop() below is issued after
    // this latch, so the stop either lands while the consumer is parked in cv_.wait() or is
    // already visible to it via stop_requested(); both paths must unwind to nullopt. No sleep
    // is needed to guess whether the thread had parked yet.
    in_pop.count_down();
    auto val = q.pop(st);
    if (!val.has_value()) {
      pop_returned_nullopt = true;
    }
  });

  in_pop.wait();

  // Request stop. This will unblock the cv_.wait() instantly.
  worker.request_stop();
  worker.join();

  EXPECT_TRUE(pop_returned_nullopt);
}

// Test 2: TSan torture test. Each round spawns a consumer that immediately blocks in pop(),
// then the main thread races a randomized action against the parking window: a push, a stop
// request, or both in a random order. Every round must terminate, otherwise a wakeup was
// missed. Run under ThreadSanitizer this also hammers the stop_callback/cv paths hard enough
// to expose data races that the single-shot Cancellation test cannot reach.
TEST_F(ConcurrentQueueCancellationTest, StopTokenCancellation) {
  ConcurrentQueue<int> q;
  std::mt19937 rng{0xC0FFEEu};

  ProgressWatchdog watchdog(iptv::test::concurrency::kTortureDeadline);

  std::size_t delivered = 0;
  std::size_t cancelled = 0;

  for (std::size_t round = 0; round < iptv::test::concurrency::kTortureRounds; ++round) {
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

  EXPECT_EQ(delivered + cancelled, iptv::test::concurrency::kTortureRounds);
  EXPECT_GT(delivered, 0u);
  EXPECT_GT(cancelled, 0u);

  watchdog.mark_finished();
}
