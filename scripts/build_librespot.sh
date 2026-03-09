#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

LIBRESPOT_REPO="${LIBRESPOT_REPO:-https://github.com/librespot-org/librespot.git}"
LIBRESPOT_VERSION="${LIBRESPOT_VERSION:-v0.8.0}"
TARGET="${TARGET:-aarch64-unknown-linux-gnu}"
BUILD_DIR="$REPO_ROOT/build/librespot-src"
OUTPUT_BIN="$REPO_ROOT/dist/sconnect/bin/librespot"

require_cmd() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "ERROR: Missing required command: $cmd" >&2
    exit 1
  fi
}

require_cmd git
require_cmd cargo

if ! command -v rustup >/dev/null 2>&1; then
  echo "ERROR: rustup is required for cross-target setup" >&2
  exit 1
fi

mkdir -p "$REPO_ROOT/build" "$REPO_ROOT/dist/sconnect/bin"

if [ ! -d "$BUILD_DIR/.git" ]; then
  echo "Cloning librespot source..."
  git clone "$LIBRESPOT_REPO" "$BUILD_DIR"
fi

cd "$BUILD_DIR"

echo "Fetching librespot refs..."
git fetch --tags --prune origin

echo "Checking out librespot $LIBRESPOT_VERSION..."
git checkout "$LIBRESPOT_VERSION"

if [ "$TARGET" = "aarch64-unknown-linux-gnu" ]; then
  export CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER="${CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER:-${CROSS_PREFIX:-aarch64-linux-gnu-}gcc}"
  export PKG_CONFIG_ALLOW_CROSS="${PKG_CONFIG_ALLOW_CROSS:-1}"
  export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-/usr/lib/aarch64-linux-gnu/pkgconfig}"
  export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-$PKG_CONFIG_LIBDIR}"
fi

rustup target add "$TARGET" >/dev/null

echo "Building librespot for $TARGET..."
cargo build --release --target "$TARGET"

candidate="$BUILD_DIR/target/$TARGET/release/librespot"
if [ ! -x "$candidate" ]; then
  echo "ERROR: librespot binary missing at $candidate" >&2
  exit 1
fi

cat "$candidate" > "$OUTPUT_BIN"
chmod +x "$OUTPUT_BIN"

commit_sha="$(git rev-parse --short HEAD)"

printf 'librespot build complete\n'
printf '  version: %s\n' "$LIBRESPOT_VERSION"
printf '  commit:  %s\n' "$commit_sha"
printf '  target:  %s\n' "$TARGET"
printf '  output:  %s\n' "$OUTPUT_BIN"
