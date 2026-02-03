#pragma once

#include <vector>
#include <deque>
#include <optional>
#include <mutex>
#include <functional>

namespace kvstore {
namespace concurrency {

template <typename T>
class WorkQueue {
public:
    explicit WorkQueue(size_t capacity = 4096) 
        : capacity_(capacity) {}

    bool Push(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(std::move(item));
        return true;
    }

    std::optional<T> Pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

    std::optional<T> Steal() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop_front();
        return item;
    }

private:
    size_t capacity_;
    std::deque<T> queue_;
    std::mutex mutex_;
};

} // namespace concurrency
} // namespace kvstore
