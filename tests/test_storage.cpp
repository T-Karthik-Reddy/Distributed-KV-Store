#include <gtest/gtest.h>
#include "../src/storage/kv_store.hpp"
#include "../src/storage/wal.hpp"
#include <thread>
#include <vector>

using namespace kvstore::storage;

TEST(KVStoreTest, BasicPutGet) {
    KVStore store(4); // 4 shards
    store.Put("key1", "value1");
    auto val = store.Get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "value1");

    auto val2 = store.Get("key2");
    EXPECT_FALSE(val2.has_value());
}

TEST(KVStoreTest, ConcurrentAccess) {
    KVStore store(16);
    auto worker = [&store](int id) {
        for (int i = 0; i < 100; ++i) {
            std::string key = "key" + std::to_string(id) + "_" + std::to_string(i);
            store.Put(key, "val");
            auto v = store.Get(key);
            EXPECT_TRUE(v.has_value());
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
}

TEST(WALTest, BasicAppend) {
    const std::string wal_file = "test_wal.log";
    // Remove if exists
    ::unlink(wal_file.c_str());

    {
        WAL wal(wal_file);
        EXPECT_NO_THROW(wal.Append("key1", "value1"));
        EXPECT_NO_THROW(wal.Append("key2", "", true)); // delete
    }

    // Cleanup
    ::unlink(wal_file.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
