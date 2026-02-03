#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

namespace kvstore {
namespace raft {

enum class NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

struct LogEntry {
    int32_t term;
    int32_t index;
    std::string command;
};

class RaftNode {
public:
    RaftNode(int32_t id, const std::vector<std::string>& peer_addresses)
        : id_(id), peer_addresses_(peer_addresses), state_(NodeState::FOLLOWER) {
        // Initialize state
        current_term_ = 0;
        voted_for_ = -1;
        commit_index_ = 0;
        last_applied_ = 0;
    }

    void Start() {
        // Start background thread for election timeouts and heartbeats
        background_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Tick
                Tick();
            }
        });
    }

    void Stop() {
        running_ = false;
        if (background_thread_.joinable()) {
            background_thread_.join();
        }
    }

    // Handles incoming AppendEntries RPC
    bool HandleAppendEntries(int32_t term, int32_t leader_id, int32_t prev_log_index, int32_t prev_log_term, const std::vector<LogEntry>& entries, int32_t leader_commit) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (term < current_term_) {
            return false;
        }

        if (term > current_term_) {
            current_term_ = term;
            state_ = NodeState::FOLLOWER;
            voted_for_ = -1;
        }

        last_heartbeat_ = std::chrono::steady_clock::now();
        // Log replication logic omitted for brevity in prototype
        return true;
    }

    // Handles incoming RequestVote RPC
    bool HandleRequestVote(int32_t term, int32_t candidate_id, int32_t last_log_index, int32_t last_log_term) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (term < current_term_) return false;

        if (term > current_term_) {
            current_term_ = term;
            state_ = NodeState::FOLLOWER;
            voted_for_ = -1;
        }

        if (voted_for_ == -1 || voted_for_ == candidate_id) {
            // Simplified log safety check
            voted_for_ = candidate_id;
            last_heartbeat_ = std::chrono::steady_clock::now();
            return true;
        }
        return false;
    }

private:
    void Tick() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        if (state_ == NodeState::FOLLOWER || state_ == NodeState::CANDIDATE) {
            // Check election timeout (simplified to 1-2 seconds)
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat_).count() > 1500) {
                StartElection();
            }
        } else if (state_ == NodeState::LEADER) {
            // Send heartbeats
            SendHeartbeats();
        }
    }

    void StartElection() {
        state_ = NodeState::CANDIDATE;
        current_term_++;
        voted_for_ = id_;
        last_heartbeat_ = std::chrono::steady_clock::now();
        // Election logic omitted (would send RequestVote RPCs via gRPC)
    }

    void SendHeartbeats() {
        // Send AppendEntries RPCs to all peers
    }

    int32_t id_;
    std::vector<std::string> peer_addresses_;
    
    std::mutex mutex_;
    NodeState state_;
    int32_t current_term_;
    int32_t voted_for_;
    std::vector<LogEntry> log_;
    int32_t commit_index_;
    int32_t last_applied_;
    
    std::chrono::steady_clock::time_point last_heartbeat_ = std::chrono::steady_clock::now();
    
    std::atomic<bool> running_{true};
    std::thread background_thread_;
};

} // namespace raft
} // namespace kvstore
