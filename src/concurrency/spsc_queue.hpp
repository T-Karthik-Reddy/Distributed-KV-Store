#pragma once

#include <atomic>
#include <vector>
#include <optional>
#include <stdexcept>

namespace kvstore {
namespace concurrency {

template <typename T>
class SpscQueue {
public:
    explicit SpscQueue(size_t capacity = 16384) 
        : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) {
        if ((capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("Capacity must be a power of 2");
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    bool Push(T item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);

        if (t - h >= capacity_) {
            return false; // Queue full
        }

        buffer_[t & mask_] = std::move(item);
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> Pop() {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);

        if (h == t) {
            return std::nullopt; // Queue empty
        }

        T item = std::move(buffer_[h & mask_]);
        head_.store(h + 1, std::memory_order_release);
        return item;
    }

private:
    size_t capacity_;
    size_t mask_;
    std::vector<T> buffer_;
    alignas(64) std::atomic<size_t> head_; // Consumer
    alignas(64) std::atomic<size_t> tail_; // Producer
};

} // namespace concurrency
} // namespace kvstore
