#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>

namespace iptv {

/**
 * @brief Thread-safe Multiple-Producer Multiple-Consumer (MPMC) Queue.
 *
 * Used for safely passing decoded frames and packets between the network,
 * demuxer, decoder, and renderer threads.
 *
 * @tparam T The type of item stored in the queue.
 */
template <typename T>
class ConcurrentQueue {
 public:
  ConcurrentQueue() = default;
  ~ConcurrentQueue() = default;

  // Delete copy semantics
  ConcurrentQueue(const ConcurrentQueue&) = delete;
  ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

  // Delete move semantics (mutexes are generally not movable)
  ConcurrentQueue(ConcurrentQueue&&) = delete;
  ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;

  /**
   * @brief Pushes an item into the queue and notifies one waiting thread.
   */
  void push(T item) {
    std::scoped_lock lock(mutex_);
    queue_.push(std::move(item));
    cv_.notify_one();
  }

  /**
   * @brief Blocking pop with cancellation support (C++20 std::stop_token).
   *
   * @param st Token to safely abort the block if the thread needs to shutdown.
   * @return std::optional<T> containing the item, or nullopt if cancelled.
   */
  std::optional<T> pop(std::stop_token stop_token) {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, stop_token, [this] { return !queue_.empty(); });

    if (stop_token.stop_requested()) {
      return std::nullopt;
    }

    T item = std::move(queue_.front());
    queue_.pop();
    return item;
  }

  /**
   * @brief Non-blocking pop (Polling).
   *
   * Useful for consumers that need to perform other tasks (e.g. render a loading spinner)
   * when the queue is empty.
   *
   * @return std::optional<T> containing the item, or nullopt if empty.
   */
  std::optional<T> tryPop() {
    std::scoped_lock lock(mutex_);
    if (queue_.empty()) {
      return std::nullopt;
    }
    T item = std::move(queue_.front());
    queue_.pop();
    return item;
  }

  /**
   * @brief Non-blocking check for the queue size.
   */
  size_t size() const {
    std::scoped_lock lock(mutex_);
    return queue_.size();
  }

  /**
   * @brief Non-blocking check to see if the queue is empty.
   */
  bool empty() const {
    std::scoped_lock lock(mutex_);
    return queue_.empty();
  }

  /**
   * @brief Clears all pending items in the queue (e.g. on seek).
   */
  void clear() {
    std::scoped_lock lock(mutex_);
    std::queue<T> empty_queue;
    std::swap(queue_, empty_queue);
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable_any cv_;
  std::queue<T> queue_;
};

}  // namespace iptv
