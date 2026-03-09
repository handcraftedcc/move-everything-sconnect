#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
IMAGE_NAME="move-anything-sconnect-builder"
OUTPUT_BASENAME="${OUTPUT_BASENAME:-sconnect-module}"

if [ -z "${CROSS_PREFIX:-}" ] && [ ! -f "/.dockerenv" ]; then
  echo "=== SConnect Module Build (via Docker) ==="
  docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$REPO_ROOT"
  docker run --rm \
    -v "$REPO_ROOT:/build" \
    -u "$(id -u):$(id -g)" \
    -w /build \
    -e OUTPUT_BASENAME="$OUTPUT_BASENAME" \
    "$IMAGE_NAME" \
    ./scripts/build.sh
  exit 0
fi

CROSS_PREFIX="${CROSS_PREFIX:-aarch64-linux-gnu-}"

cd "$REPO_ROOT"
rm -rf build/module dist/sconnect
mkdir -p build/module dist/sconnect dist/sconnect/bin

# --- Build librespot ---
echo "Building librespot..."
export CROSS_PREFIX
./scripts/build_librespot.sh

# --- Build DSP plugin ---
echo "Compiling v2 DSP plugin..."
"${CROSS_PREFIX}gcc" -O3 -g -shared -fPIC \
  src/dsp/sconnect_plugin.c \
  -o build/module/dsp.so \
  -Isrc/dsp \
  -lpthread -lm

cat src/module.json > dist/sconnect/module.json
[ -f src/help.json ] && cat src/help.json > dist/sconnect/help.json
cat src/ui.js > dist/sconnect/ui.js
cat src/ui_chain.js > dist/sconnect/ui_chain.js
cat build/module/dsp.so > dist/sconnect/dsp.so
cat src/runtime/sconnect_event.sh > dist/sconnect/bin/sconnect_event.sh
chmod +x dist/sconnect/dsp.so
chmod +x dist/sconnect/bin/sconnect_event.sh

# --- Package ---
PKG_TMP_DIR="$(mktemp -d)"
rm -f "dist/${OUTPUT_BASENAME}.tar.gz"
cp -a dist/sconnect "$PKG_TMP_DIR/sconnect"
(
  cd "$PKG_TMP_DIR"
  tar -czvf "$REPO_ROOT/dist/${OUTPUT_BASENAME}.tar.gz" sconnect/
)
rm -rf "$PKG_TMP_DIR"

echo "=== Build Complete ==="
echo "Module dir: dist/sconnect"
echo "Tarball: dist/${OUTPUT_BASENAME}.tar.gz"
