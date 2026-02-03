#pragma once

#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <system_error>
#include <cstring>
#include <iostream>
#include <vector>
#include <mutex>

#if defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace kvstore {
namespace storage {

class WAL {
public:
    explicit WAL(const std::string& file_path) : file_path_(file_path) {
        int flags = O_WRONLY | O_CREAT | O_APPEND;
#if defined(__linux__)
        flags |= O_DIRECT | O_DSYNC; // O_DIRECT bypasses OS cache, O_DSYNC ensures data is written to disk
#endif
        fd_ = ::open(file_path_.c_str(), flags, 0644);
        if (fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to open WAL file");
        }
    }

    ~WAL() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    // A real WAL would write binary serialized records (e.g., OpType, KeyLen, Key, ValLen, Val)
    // For simplicity, we just append a formatted string.
    // NOTE: O_DIRECT requires memory to be aligned to logical block size (e.g., 512 or 4096).
    void Append(const std::string& key, const std::string& value, bool is_delete = false) {
        std::lock_guard<std::mutex> lock(mutex_); // Serialize writes to the log
        
        std::string record = is_delete ? "DEL " + key + "\n" : "PUT " + key + " " + value + "\n";
        
        // Ensure alignment for O_DIRECT
        size_t alignment = 4096;
        size_t size = (record.size() + alignment - 1) & ~(alignment - 1); // Round up to next multiple of alignment
        
        void* buffer = nullptr;
        if (posix_memalign(&buffer, alignment, size) != 0) {
            throw std::bad_alloc();
        }
        
        // Zero-fill the buffer, then copy the record
        std::memset(buffer, 0, size);
        std::memcpy(buffer, record.data(), record.size());
        
        ssize_t bytes_written = ::write(fd_, buffer, size);
        free(buffer);
        
        if (bytes_written < 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to write to WAL");
        }
    }

private:
    std::string file_path_;
    int fd_ = -1;
    std::mutex mutex_;
};

} // namespace storage
} // namespace kvstore
