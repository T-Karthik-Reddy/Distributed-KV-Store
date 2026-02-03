# Project Blueprint: High-Performance Distributed Key-Value Store

## 🎯 Goal
Build a highly available, fault-tolerant distributed key-value store from scratch that focuses on extreme low-latency and high throughput. 

## 🛠 Tech Stack
- **Language:** C++20
- **Network / Async I/O:** Linux `io_uring`, gRPC
- **Core Algorithms:** Raft Consensus
- **Concepts:** Zero-copy, lock-free programming, Write-Ahead Logging (WAL)

## ✅ Acceptance Criteria & Target Metrics

### 1. Throughput & Latency (The 120K Benchmark)
- **Target:** 120,000+ reads/sec.
- **Latency:** Sub-millisecond (p99 < 1ms).
- **Implementation:** Must utilize Linux `io_uring` for asynchronous I/O and zero-copy network transmission to bypass standard kernel network overhead.

### 2. Lock-Free Threading Model
- **Target:** Achieve a 3.2x throughput increase over standard `epoll` and `mutex` based architectures.
- **Implementation:** Engineer a custom lock-free thread pool.
- **Requirements:** Must implement work-stealing and CPU pinning (thread affinity) to eliminate context-switching overhead.

### 3. Distributed Consensus (Raft)
- **Target:** Zero data loss during node failures and network partitions across a 5-node cluster.
- **Implementation:** Implement the Raft consensus algorithm from scratch (or highly optimized).
- **Requirements:** Must include pipelined log replication and snapshotting.

### 4. Storage & Write-Ahead Log (WAL)
- **Target:** Deterministic durability with predictable latency.
- **Implementation:** Append-only Write-Ahead Log.
- **Requirements:** Must bypass the OS page cache entirely by using the `O_DIRECT` flag for disk writes.

---
**Advice for building:** Start by writing the single-node storage engine and the lock-free thread pool first. Benchmark it locally. Then, add the network layer (`io_uring`). Finally, wrap it in a 3-5 node Raft consensus cluster.
