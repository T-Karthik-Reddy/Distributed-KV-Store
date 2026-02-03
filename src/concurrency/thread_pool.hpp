#pragma once

#include "work_queue.hpp"
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#endif

namespace kvstore {
namespace concurrency {

using Task = std::function<void()>;

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) 
        : stop_(false) {
        
        queues_.resize(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            queues_[i] = std::make_unique<WorkQueue<Task>>();
        }

        auto worker = [this, num_threads](size_t id) {
#if defined(__linux__)
            // CPU Pinning (Thread Affinity)
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(id % std::thread::hardware_concurrency(), &cpuset);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif

            while (!stop_.load(std::memory_order_relaxed)) {
                std::optional<Task> task = queues_[id]->Pop();
                if (task) {
                    (*task)();
                    continue;
                }

                // Work Stealing
                bool stolen = false;
                for (size_t i = 1; i < num_threads; ++i) {
                    size_t target = (id + i) % num_threads;
                    task = queues_[target]->Steal();
                    if (task) {
                        (*task)();
                        stolen = true;
                        break;
                    }
                }

                if (!stolen) {
                    // Yield if no work found to prevent 100% CPU usage in idle, 
                    // though for extreme low latency, spin-waiting is better.
                    // For the blueprint 120K target, a short pause or yield is okay.
                    std::this_thread::yield();
                }
            }
        };

        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back(worker, i);
        }
    }

    ~ThreadPool() {
        stop_.store(true, std::memory_order_release);
        for (auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // Enqueue task into a specific thread's queue (e.g., hash(key) % num_threads)
    void Enqueue(Task task, size_t hint = 0) {
        size_t id = hint % queues_.size();
        while (true) {
            if (queues_[id]->Push(task)) { // Note: passing by copy is safer here as we might retry
                return;
            }
            // If full, try next
            for (size_t i = 1; i < queues_.size(); ++i) {
                if (queues_[(id + i) % queues_.size()]->Push(task)) {
                    return;
                }
            }
            // Yield and retry
            std::this_thread::yield();
        }
    }

private:
    std::vector<std::unique_ptr<WorkQueue<Task>>> queues_;
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_;
};

} // namespace concurrency
} // namespace kvstore
