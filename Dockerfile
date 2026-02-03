FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    g++-13 \
    clang-18 \
    liburing-dev \
    libgtest-dev \
    libbenchmark-dev \
    protobuf-compiler \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    git \
    gnupg \
    wrk \
    gdb \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Default command
CMD ["bash"]
