#pragma once

#include <string>
#include <vector>
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <mutex>

namespace kvstore {
namespace storage {

// A sharded concurrent key-value store to minimize lock contention.
// A fully lock-free map (e.g., using hazard pointers) is left for future iteration.
class KVStore {
public:
    explicit KVStore(size_t num_shards = 64) : shards_(num_shards) {}

    void Put(const std::string& key, const std::string& value) {
        auto& shard = GetShard(key);
        std::unique_lock lock(shard.mutex);
        shard.data[key] = value;
    }

    std::optional<std::string> Get(const std::string& key) const {
        auto& shard = GetShard(key);
        std::shared_lock lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool Delete(const std::string& key) {
        auto& shard = GetShard(key);
        std::unique_lock lock(shard.mutex);
        return shard.data.erase(key) > 0;
    }

private:
    struct alignas(64) Shard { // Align to cache line to prevent false sharing
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, std::string> data;
    };

    const Shard& GetShard(const std::string& key) const {
        return shards_[std::hash<std::string>{}(key) % shards_.size()];
    }

    Shard& GetShard(const std::string& key) {
        return shards_[std::hash<std::string>{}(key) % shards_.size()];
    }

    std::vector<Shard> shards_;
};

} // namespace storage
} // namespace kvstore
