FROM debian:bookworm-slim

RUN dpkg --add-architecture arm64 && \
    apt-get update && apt-get install -y \
    gcc \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    make \
    git \
    pkg-config \
    curl \
    ca-certificates \
    libasound2-dev:arm64 \
    libssl-dev:arm64 \
    xxd \
    && rm -rf /var/lib/apt/lists/*

ENV CARGO_HOME=/usr/local/cargo
ENV RUSTUP_HOME=/usr/local/rustup
ENV PATH=/usr/local/cargo/bin:$PATH

RUN mkdir -p "$CARGO_HOME" "$RUSTUP_HOME" && \
    chmod 777 "$CARGO_HOME" "$RUSTUP_HOME" && \
    curl https://sh.rustup.rs -sSf | sh -s -- -y --profile minimal && \
    rustup target add aarch64-unknown-linux-gnu && \
    chmod -R a+rwX "$CARGO_HOME" "$RUSTUP_HOME"

ENV CROSS_PREFIX=aarch64-linux-gnu-
ENV PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig
WORKDIR /build
