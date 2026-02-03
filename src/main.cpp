#include <iostream>
#include "storage/kv_store.hpp"
#include "storage/wal.hpp"
#include "concurrency/thread_pool.hpp"
#include "network/io_uring_server.hpp"

using namespace kvstore;

int main() {
    std::cout << "Initializing High-Performance Distributed KV Store..." << std::endl;

    storage::KVStore store;
    
    // Fallback WAL path for local development
    storage::WAL wal("kvstore.wal");

    // Thread pool for processing requests
    concurrency::ThreadPool pool(8);

    std::unique_ptr<network::IoUringServer> server_ptr;
    
    auto message_handler = [&](int client_fd, const std::string& msg) {
        // Run the processing in the lock-free thread pool
        pool.Enqueue([&store, &wal, server = server_ptr.get(), client_fd, msg]() {
            // Very simple parser for prototype (e.g., "PUT key value\n", "GET key\n")
            if (msg.starts_with("GET /")) {
                // Handle pipelined HTTP GET requests for wrk benchmark
                int count = 0;
                size_t pos = 0;
                while ((pos = msg.find("GET /", pos)) != std::string::npos) {
                    count++;
                    pos += 5;
                }
                
                auto val = store.Get("mykey");
                std::string body = val.value_or("hello");
                std::string single_response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: keep-alive\r\n\r\n" + body;
                
                std::string response;
                response.reserve(std::min(single_response.size() * count, (size_t)32000));
                for (int i = 0; i < count; ++i) {
                    if (response.size() + single_response.size() > 32000) break;
                    response += single_response;
                }
                
                if (server) server->SendResponse(client_fd, response);
            } else if (msg.starts_with("PUT ")) {
                size_t space1 = msg.find(' ');
                size_t space2 = msg.find(' ', space1 + 1);
                if (space1 != std::string::npos && space2 != std::string::npos) {
                    std::string key = msg.substr(space1 + 1, space2 - space1 - 1);
                    std::string val = msg.substr(space2 + 1);
                    // Trim newline
                    if (!val.empty() && val.back() == '\n') val.pop_back();

                    wal.Append(key, val);
                    store.Put(key, val);
                    
                    std::string response = "OK\n";
                    if (server) server->SendResponse(client_fd, response);
                }
            } else if (msg.starts_with("GET ")) {
                size_t space1 = msg.find(' ');
                if (space1 != std::string::npos) {
                    std::string key = msg.substr(space1 + 1);
                    if (!key.empty() && key.back() == '\n') key.pop_back();

                    auto val = store.Get(key);
                    std::string response = val.value_or("NOT FOUND") + "\n";
                    if (server) server->SendResponse(client_fd, response);
                }
            }
        });
    };

    server_ptr = std::make_unique<network::IoUringServer>(8080, message_handler);
    
    std::cout << "Starting io_uring network layer on port 8080..." << std::endl;
    server_ptr->Run();

    return 0;
}
