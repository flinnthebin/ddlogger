// tsq.h
// https://www.geeksforgeeks.org/implement-thread-safe-queue-in-c/ <- 100%
// stolen from here.

#ifndef tsq_h
#define tsq_h

#include "eventstruct.h"
#include <condition_variable>

#include <iostream>
#include <mutex>
#include <queue>

class tsq {
public:
  auto push(const event item) -> void {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(item);
    condition_.notify_one();
  }

  auto pop() -> event {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() { return !queue_.empty(); });
    event item = queue_.front();
    queue_.pop();

    return item;
  }

  auto empty() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  auto size() -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  void shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    condition_.notify_all();
  }

private:
  std::queue<event> queue_;
  std::mutex mutex_;
  std::condition_variable condition_;
};

#endif // tsq_h
