FROM debian:bookworm AS structured_dev

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    clang \
    clang-format \
    mold \
    ccache \
    git \
    curl \
    ca-certificates \
    libopencv-dev \
    libspdlog-dev \
    libgtest-dev \
    libx11-dev \
    libxrandr-dev \
    x11-xserver-utils \
    cppcheck \
    v4l-utils \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# devcontainer 用の作業ディレクトリ
WORKDIR /workspace

# 推奨: リンカ高速化 ＆ ccache を既定に
ENV CC=clang CXX=clang++ \
    CCACHE_DIR=/workspace/.ccache \
    CCACHE_TEMPDIR=/workspace/.ccache/tmp

RUN git config --global --add safe.directory /workspace
