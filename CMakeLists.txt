cmake_minimum_required(VERSION 3.20)
project(DistributedKVStore VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Threading
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Include directories
include_directories(include)
include_directories(src)

# Let's define an executable for now
add_executable(kvstore_main src/main.cpp)
target_link_libraries(kvstore_main Threads::Threads uring)

# Testing
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_storage tests/test_storage.cpp)
target_link_libraries(test_storage ${GTEST_LIBRARIES} Threads::Threads)
add_test(NAME StorageTest COMMAND test_storage)

add_executable(test_concurrency tests/test_concurrency.cpp)
target_link_libraries(test_concurrency ${GTEST_LIBRARIES} Threads::Threads)
add_test(NAME ConcurrencyTest COMMAND test_concurrency)

# Benchmarks
find_package(benchmark REQUIRED)
add_executable(throughput_bench benchmarks/throughput_bench.cpp)
target_link_libraries(throughput_bench benchmark::benchmark Threads::Threads)
