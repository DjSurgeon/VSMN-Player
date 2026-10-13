#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "iptv/decoder/decoded_frame.hpp"

namespace iptv::renderer {

/**
 * @brief A queued frame item with its timestamp and sequence number.
 *
 * Holds a decoded frame and its associated sequence number for priority
 * queue ordering based on PTS with FIFO tie-breaking.
 */
struct QueueItem {
  iptv::decoder::DecodedFrame frame;
  uint64_t seq_num;
};

/**
 * * @brief Min-heap comparator for PTS-ordered frame scheduling.
 *
 * Handles 33-bit PTS wrap-around using a threshold. For identical PTS values,
 * sequence numbers ensure FIFO stability. Used as the priority queue comparator
 * for `PtsFrameBuffer`.
 */
struct PtsComparator {
  // Wrap threshold for 33-bit PTS in microseconds.
  // Max 33-bit PTS at 90kHz is (1LL << 33) = 8589934592 ticks.
  // In microseconds: 8589934592 * 100 / 9 = 95443717688 us.
  // Half is approx 47721858844 us. We use (1LL << 32) * 100 / 9 exactly.
  static constexpr int64_t kWrapThreshold = (1LL << 32) * 100 / 9;

  bool operator()(const QueueItem& lhs, const QueueItem& rhs) const {
    int64_t ptsA = lhs.frame.pts;
    int64_t ptsB = rhs.frame.pts;
    int64_t diff = ptsA - ptsB;

    if (std::abs(diff) > kWrapThreshold) {
      // The smaller PTS is the wrapped-around later time.
      if (ptsA < ptsB) {
        return true;  // ptsA is later, so A > B for min-heap
      } else {
        return false; // ptsA is earlier, so A < B for min-heap
      }
    }

    if (ptsA != ptsB) {
      return ptsA > ptsB; // min-heap: return true if lhs > rhs
    }

    // Sequence number ensures FIFO stability for identical PTS values
    return lhs.seq_num > rhs.seq_num;
  }
};

/**
 * @brief Pure virtual interface for a PTS-ordered frame buffer.
 *
 * Thread-safe bounded-capacity queue that stores decoded frames ordered by
 * presentation timestamp (PTS). Uses a min-heap priority queue with 33-bit
 * PTS wrap-around handling and FIFO tie-breaking via sequence numbers.
 */
class PtsFrameBuffer {
 public:
  explicit PtsFrameBuffer(std::size_t max_capacity);

  PtsFrameBuffer(const PtsFrameBuffer&) = delete;
  PtsFrameBuffer& operator=(const PtsFrameBuffer&) = delete;
  PtsFrameBuffer(PtsFrameBuffer&&) = delete;
  PtsFrameBuffer& operator=(PtsFrameBuffer&&) = delete;
  ~PtsFrameBuffer() = default;

  /**
   * @brief Blocks if full until space is available or aborted.
   * @return true if the frame was successfully pushed, false if aborted.
   */
  bool push(iptv::decoder::DecodedFrame frame);

  /**
   * @brief Blocks if empty until frame is available or aborted.
   * @return The decoded frame if available, or std::nullopt if aborted.
   */
  std::optional<iptv::decoder::DecodedFrame> pop();

  /**
   * @brief Wakes all blocked threads and sets abort flag.
   * All pending operations will complete with std::nullopt or aborted state.
   */
  void abort();

 private:
  std::size_t max_capacity_;
  uint64_t next_seq_num_{0};
  bool aborted_{false};

  std::vector<QueueItem> queue_;
  std::mutex mutex_;
  std::condition_variable cv_empty_;
  std::condition_variable cv_full_;
};

}  // namespace iptv::renderer