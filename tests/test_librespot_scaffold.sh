#!/usr/bin/env bash
set -euo pipefail

build_librespot="scripts/build_librespot.sh"
build_script="scripts/build.sh"
dockerfile="scripts/Dockerfile"

for f in "$build_script" "$dockerfile"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: Missing $f" >&2
    exit 1
  fi
done

if [ ! -x "$build_librespot" ]; then
  echo "FAIL: Missing executable $build_librespot" >&2
  exit 1
fi

if ! rg -q 'LIBRESPOT_VERSION="\$\{LIBRESPOT_VERSION:-v0\.8\.0\}"' "$build_librespot"; then
  echo "FAIL: build_librespot.sh must pin default LIBRESPOT_VERSION=v0.8.0" >&2
  exit 1
fi

if ! rg -q 'cargo build --release --target "\$TARGET"' "$build_librespot"; then
  echo "FAIL: build_librespot.sh must build librespot with cargo target" >&2
  exit 1
fi

if ! rg -q 'dist/sconnect/bin/librespot' "$build_librespot"; then
  echo "FAIL: build_librespot.sh must stage binary at dist/sconnect/bin/librespot" >&2
  exit 1
fi

if ! rg -q './scripts/build_librespot.sh' "$build_script"; then
  echo "FAIL: scripts/build.sh must invoke scripts/build_librespot.sh" >&2
  exit 1
fi

if ! rg -q 'rustup target add aarch64-unknown-linux-gnu' "$dockerfile"; then
  echo "FAIL: scripts/Dockerfile must install rust target aarch64-unknown-linux-gnu" >&2
  exit 1
fi

if ! rg -q 'ENV CARGO_HOME=/usr/local/cargo' "$dockerfile"; then
  echo "FAIL: scripts/Dockerfile must set CARGO_HOME to a shared path" >&2
  exit 1
fi

if ! rg -q 'ENV PATH=/usr/local/cargo/bin:\$PATH' "$dockerfile"; then
  echo "FAIL: scripts/Dockerfile must expose cargo in PATH for non-root builds" >&2
  exit 1
fi

if ! rg -q 'libasound2-dev:arm64' "$dockerfile"; then
  echo "FAIL: scripts/Dockerfile must include libasound2-dev:arm64 for librespot build deps" >&2
  exit 1
fi

if ! rg -q 'libssl-dev:arm64' "$dockerfile"; then
  echo "FAIL: scripts/Dockerfile must include libssl-dev:arm64 for librespot TLS deps" >&2
  exit 1
fi

echo "PASS: librespot scaffold build integration is present"
