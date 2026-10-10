#include "iptv/renderer/pts_frame_buffer.hpp"

#include <algorithm>

namespace iptv::renderer {

PtsFrameBuffer::PtsFrameBuffer(std::size_t max_capacity) : max_capacity_(max_capacity) {}

bool PtsFrameBuffer::push(iptv::decoder::DecodedFrame frame) {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_full_.wait(lock, [this]() { return queue_.size() < max_capacity_ || aborted_; });

  if (aborted_) {
    return false;
  }

  queue_.push_back(QueueItem{std::move(frame), next_seq_num_++});
  std::push_heap(queue_.begin(), queue_.end(), PtsComparator{});
  
  lock.unlock();
  cv_empty_.notify_one();
  return true;
}

std::optional<iptv::decoder::DecodedFrame> PtsFrameBuffer::pop() {
  std::unique_lock<std::mutex> lock(mutex_);
  cv_empty_.wait(lock, [this]() { return !queue_.empty() || aborted_; });

  if (aborted_ && queue_.empty()) {
    return std::nullopt;
  }
  
  if (aborted_) {
    return std::nullopt;
  }

  std::pop_heap(queue_.begin(), queue_.end(), PtsComparator{});
  iptv::decoder::DecodedFrame frame = std::move(queue_.back().frame);
  queue_.pop_back();

  lock.unlock();
  cv_full_.notify_one();
  return std::move(frame);
}

void PtsFrameBuffer::abort() {
  std::lock_guard<std::mutex> lock(mutex_);
  aborted_ = true;
  cv_empty_.notify_all();
  cv_full_.notify_all();
}

}  // namespace iptv::renderer
