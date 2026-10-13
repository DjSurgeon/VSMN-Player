#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <semaphore>
#include <string>
#include <thread>
#include <utility>

namespace iptv::test::concurrency {

/**
 * @brief Compile-time safety constraint: this type CANNOT be copied, only moved.
 *
 * If ConcurrentQueue ever tried to copy it, the build breaks instead of silently paying for a
 * deep copy.
 */
struct MoveOnlyBuffer {
  std::string data;
  bool corrupt{false};

  MoveOnlyBuffer() = default;
  explicit MoveOnlyBuffer(std::string payload, bool is_corrupt = false)
      : data(std::move(payload)), corrupt(is_corrupt) {}

  MoveOnlyBuffer(const MoveOnlyBuffer&) = delete;
  MoveOnlyBuffer& operator=(const MoveOnlyBuffer&) = delete;

  MoveOnlyBuffer(MoveOnlyBuffer&&) noexcept = default;
  MoveOnlyBuffer& operator=(MoveOnlyBuffer&&) noexcept = default;
};

inline constexpr std::size_t kSegmentBytes = 5u * 1024u * 1024u;  // 5 MB media segment
inline constexpr std::size_t kTortureRounds = 400;
inline constexpr std::chrono::seconds kTortureDeadline{60};

[[nodiscard]] std::uint8_t payloadByte(std::size_t index);

// The watchdog's completion signal lives at namespace scope, never inside a GTest stack frame.
// ThreadSanitizer identifies a mutex by its address, and a ProgressWatchdog built in a test
// frame hands that same address to the next test once the frame is gone; TSan then reads the
// next lock as a double-lock of a still-held mutex. Static storage keeps one address for the
// life of the process.
inline std::counting_semaphore<1> g_watchdog_done{0};

/**
 * @brief Turns a missed wakeup (an unbreakable block) into a deterministic failure.
 *
 * Reports a readable message instead of a test binary that hangs until the CI job timeout kills
 * it. It parks on a timed semaphore rather than polling an atomic on a timer, so it wakes exactly
 * at the deadline: no poll granularity to tune, no spinning while the test makes progress.
 *
 * A semaphore, not a condition_variable, because this toolchain's TSan reports a false "double
 * lock of a mutex" for any *timed* cv wait (reproduced on a 20-line standalone program; untimed
 * cv.wait is clean). try_acquire_for waits on a futex, so TSan has no mutex to mis-track.
 */
class ProgressWatchdog {
 public:
  explicit ProgressWatchdog(std::chrono::seconds limit) {
    // A leftover count from an earlier test would make the very first wait succeed.
    while (g_watchdog_done.try_acquire()) {  // NOLINT(bugprone-empty-try-loop)
    }
    worker_ = std::jthread([limit] {
      if (!g_watchdog_done.try_acquire_for(limit)) {
        std::fputs(
            "\nConcurrentQueueTest: no progress within the deadline. A consumer is still "
            "blocked in ConcurrentQueue::pop(), so a wakeup was missed (deadlock).\n",
            stderr);
        std::fflush(stderr);
        std::abort();
      }
    });
  }

  ProgressWatchdog(const ProgressWatchdog&) = delete;
  ProgressWatchdog& operator=(const ProgressWatchdog&) = delete;

  ~ProgressWatchdog() { mark_finished(); }

  void mark_finished() { g_watchdog_done.release(); }

 private:
  // The worker captures nothing, so it cannot outlive this object.
  std::jthread worker_;
};

}  // namespace iptv::test::concurrency
