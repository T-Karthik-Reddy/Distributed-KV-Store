#include <benchmark/benchmark.h>
#include "../src/storage/kv_store.hpp"
#include <string>
#include <thread>
#include <vector>

using namespace kvstore::storage;

static void BM_KVStorePut(benchmark::State& state) {
    KVStore store(64);
    int i = 0;
    for (auto _ : state) {
        std::string key = "key" + std::to_string(i++);
        store.Put(key, "value");
    }
}
BENCHMARK(BM_KVStorePut)->Threads(1)->Threads(4)->Threads(8)->Threads(16);

static void BM_KVStoreGet(benchmark::State& state) {
    KVStore store(64);
    for (int i = 0; i < 10000; ++i) {
        store.Put("key" + std::to_string(i), "value");
    }

    int i = 0;
    for (auto _ : state) {
        std::string key = "key" + std::to_string(i++ % 10000);
        auto val = store.Get(key);
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_KVStoreGet)->Threads(1)->Threads(4)->Threads(8)->Threads(16);

#include "../src/concurrency/thread_pool.hpp"
#include <atomic>

static void BM_ThreadPoolEnqueue(benchmark::State& state) {
    kvstore::concurrency::ThreadPool pool(state.threads());
    std::atomic<int> counter{0};
    int limit = 10000;
    
    for (auto _ : state) {
        for (int i = 0; i < limit; ++i) {
            pool.Enqueue([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
        while (counter.load(std::memory_order_relaxed) < limit) {
            std::this_thread::yield();
        }
        counter.store(0, std::memory_order_relaxed);
    }
}
BENCHMARK(BM_ThreadPoolEnqueue)->Threads(1)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
