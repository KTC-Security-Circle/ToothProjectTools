FROM debian:bookworm AS structured_dev

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    clang \
    mold \
    ccache \
    git \
    curl \
    ca-certificates \
    libopencv-dev \
    libspdlog-dev \
    libgtest-dev \
    cppcheck \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# devcontainer 用の作業ディレクトリ
WORKDIR /workspace

# 推奨: リンカ高速化 ＆ ccache を既定に
ENV CC=clang CXX=clang++ \
    CCACHE_DIR=/opt/ccache

RUN git config --global --add safe.directory /workspace
