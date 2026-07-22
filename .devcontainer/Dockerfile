FROM debian:bookworm AS structured_dev

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    clang \
    clang-format \
    clang-tidy \
    lldb \
    gdb \
    mold \
    ccache \
    git \
    file \
    curl \
    ca-certificates \
    openssh-client \
    jq \
    ripgrep \
    python3 \
    libspdlog-dev \
    libgtest-dev \
    libgtk-3-dev \
    libx11-dev \
    libxrandr-dev \
    x11-xserver-utils \
    cppcheck \
    v4l-utils \
    bubblewrap \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Codex CLIをインストール
RUN curl -fsSL https://chatgpt.com/codex/install.sh | sh \
    && CODEX_PATH="$(find /root -type f -name codex -perm -111 2>/dev/null | head -n 1)" \
    && test -n "${CODEX_PATH}" \
    && install -m 0755 "${CODEX_PATH}" /usr/local/bin/codex \
    && /usr/local/bin/codex --version

WORKDIR /workspace

ENV CC=clang \
    CXX=clang++ \
    CCACHE_DIR=/workspace/.ccache \
    CCACHE_TEMPDIR=/workspace/.ccache/tmp

RUN mkdir -p /workspace/.ccache/tmp \
    && git config --global --add safe.directory /workspace