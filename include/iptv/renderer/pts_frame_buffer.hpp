#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "iptv/decoder/decoded_frame.hpp"

namespace iptv::renderer {

struct QueueItem {
  iptv::decoder::DecodedFrame frame;
  uint64_t seq_num;
};

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

class PtsFrameBuffer {
 public:
  explicit PtsFrameBuffer(std::size_t max_capacity);

  PtsFrameBuffer(const PtsFrameBuffer&) = delete;
  PtsFrameBuffer& operator=(const PtsFrameBuffer&) = delete;
  PtsFrameBuffer(PtsFrameBuffer&&) = delete;
  PtsFrameBuffer& operator=(PtsFrameBuffer&&) = delete;
  ~PtsFrameBuffer() = default;

  // Blocks if full until space is available or aborted.
  bool push(iptv::decoder::DecodedFrame frame);

  // Blocks if empty until frame is available or aborted.
  std::optional<iptv::decoder::DecodedFrame> pop();

  // Wakes all blocked threads and sets abort flag.
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
