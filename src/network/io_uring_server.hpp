#pragma once

#include <string>
#include <iostream>
#include <functional>
#include <memory>

#if defined(__linux__)
#include <liburing.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#endif

namespace kvstore {
namespace network {

enum class EventType {
    ACCEPT,
    READ,
    WRITE
};

struct RequestContext {
    EventType type;
    int fd;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    size_t length;
    char buffer[];
};

class SpinLock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) { }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

class IoUringServer {
public:
    using MessageHandler = std::function<void(int fd, const std::string& msg)>;

    IoUringServer(int port, MessageHandler handler) 
        : port_(port), handler_(std::move(handler)) {
#if defined(__linux__)
        struct io_uring_params params = {};
        params.flags = IORING_SETUP_SQPOLL;
        params.sq_thread_idle = 2000;
        int ret = io_uring_queue_init_params(4096, &ring_, &params);
        if (ret < 0) {
            std::cerr << "io_uring_queue_init_params error: " << strerror(-ret) << std::endl;
            exit(1);
        }
        SetupSocket();
#else
        std::cerr << "io_uring is only supported on Linux." << std::endl;
#endif
    }

    ~IoUringServer() {
#if defined(__linux__)
        io_uring_queue_exit(&ring_);
        close(server_fd_);
#endif
    }

    static RequestContext* AllocateContext(size_t size) {
        return (RequestContext*)malloc(sizeof(RequestContext) + size);
    }

    static void FreeContext(RequestContext* req) {
        free(req);
    }

    void SendResponse(int client_fd, const std::string& response) {
#if defined(__linux__)
        std::lock_guard<SpinLock> lock(response_lock_);
        response_queues_[active_queue_].push_back({client_fd, response});
#endif
    }

    void Run() {
#if defined(__linux__)
        AddAcceptRequest();
        io_uring_submit(&ring_);
        std::cout << "Server listening on port " << port_ << " using io_uring\n";

        while (true) {
            struct io_uring_cqe *cqe;
            io_uring_wait_cqe(&ring_, &cqe);
            
            unsigned head;
            unsigned count = 0;
            io_uring_for_each_cqe(&ring_, head, cqe) {
                count++;
                auto* req = static_cast<RequestContext*>(io_uring_cqe_get_data(cqe));
                if (cqe->res < 0) {
                    if (req->type == EventType::ACCEPT) {
                        AddAcceptRequest();
                    } else {
                        close(req->fd);
                    }
                    FreeContext(req);
                } else {
                    HandleEvent(req, cqe->res);
                }
            }
            io_uring_cq_advance(&ring_, count);
            
            // Process queued responses
            int process_queue = 0;
            {
                std::lock_guard<SpinLock> lock(response_lock_);
                process_queue = active_queue_;
                active_queue_ = 1 - active_queue_;
            }

            auto& pending_responses = response_queues_[process_queue];
            for (const auto& resp : pending_responses) {
                auto* req = AllocateContext(resp.second.size());
                req->type = EventType::WRITE;
                req->fd = resp.first;
                req->length = resp.second.size();
                std::memcpy(req->buffer, resp.second.data(), resp.second.size());

                struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
                if (!sqe) {
                    io_uring_submit(&ring_);
                    sqe = io_uring_get_sqe(&ring_);
                    if (!sqe) {
                        FreeContext(req);
                        continue;
                    }
                }
                io_uring_prep_send(sqe, req->fd, req->buffer, req->length, 0);
                io_uring_sqe_set_data(sqe, req);
            }

            pending_responses.clear();

            // Batch submit all new sqes
            io_uring_submit(&ring_);
        }
#endif
    }

private:
    int port_;
    int server_fd_;
#if defined(__linux__)
    struct io_uring ring_;
    SpinLock response_lock_;
    int active_queue_ = 0;
    std::vector<std::pair<int, std::string>> response_queues_[2];
#endif
    std::function<void(int, const std::string&)> handler_;

#if defined(__linux__)
    void SetupSocket() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        bind(server_fd_, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd_, SOMAXCONN);
    }

    void AddAcceptRequest() {
        auto* req = AllocateContext(0);
        req->type = EventType::ACCEPT;
        req->fd = server_fd_;
        req->length = 0;
        req->client_len = sizeof(req->client_addr);
        

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            io_uring_submit(&ring_);
            sqe = io_uring_get_sqe(&ring_);
            if (!sqe) {
                FreeContext(req);
                return;
            }
        }
        io_uring_prep_accept(sqe, server_fd_, (struct sockaddr*)&req->client_addr, &req->client_len, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void AddReadRequest(int client_fd) {
        auto* req = AllocateContext(32768);
        req->type = EventType::READ;
        req->fd = client_fd;
        req->length = 32768;
        

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            io_uring_submit(&ring_);
            sqe = io_uring_get_sqe(&ring_);
            if (!sqe) {
                FreeContext(req);
                return;
            }
        }
        io_uring_prep_recv(sqe, client_fd, req->buffer, req->length, 0);
        io_uring_sqe_set_data(sqe, req);
    }

    void HandleEvent(RequestContext* req, int res) {
        if (req->type == EventType::ACCEPT) {
            int client_fd = res;
            AddAcceptRequest();
            AddReadRequest(client_fd);
            FreeContext(req);
        } else if (req->type == EventType::READ) {
            if (res == 0) {
                // Connection closed
                close(req->fd);
                FreeContext(req);
            } else {
                std::string msg(req->buffer, res);
                handler_(req->fd, msg); // Dispatch to handler
                AddReadRequest(req->fd); // Queue next read
                FreeContext(req);
            }
        } else if (req->type == EventType::WRITE) {
            // Write completed, we can safely delete the request context
            FreeContext(req);
        }
    }
#endif
};

} // namespace network
} // namespace kvstore
