#include <gtest/gtest.h>
#include "../src/concurrency/thread_pool.hpp"
#include <atomic>
#include <chrono>

using namespace kvstore::concurrency;

TEST(WorkQueueTest, PushPopSteal) {
    WorkQueue<int> q(16);
    EXPECT_TRUE(q.Push(1));
    EXPECT_TRUE(q.Push(2));

    auto val1 = q.Pop();
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), 2); // Pops from tail

    auto val2 = q.Steal();
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), 1); // Steals from head
}

TEST(ThreadPoolTest, ExecuteTasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i) {
        pool.Enqueue([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // Wait a bit for tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter.load(), 100);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
