#!/usr/bin/env bash
set -euo pipefail

module_json="src/module.json"
release_json="release.json"
readme="README.md"
build_script="scripts/build.sh"
install_script="scripts/install.sh"

if ! command -v jq >/dev/null 2>&1; then
  echo "FAIL: jq is required to run this test" >&2
  exit 1
fi

for f in "$module_json" "$release_json" "$readme" "$build_script" "$install_script"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: Missing $f" >&2
    exit 1
  fi
done

module_id=$(jq -r '.id' "$module_json")
module_name=$(jq -r '.name' "$module_json")
version=$(jq -r '.version' "$release_json")
url=$(jq -r '.download_url' "$release_json")
repo_slug="${GITHUB_REPOSITORY:-}"
if [ -z "$repo_slug" ]; then
  origin_url="$(git remote get-url origin 2>/dev/null || true)"
  repo_slug="$(printf '%s\n' "$origin_url" \
    | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
fi

if [ -z "$repo_slug" ]; then
  echo "FAIL: unable to determine GitHub repository slug for release URL check" >&2
  exit 1
fi

expected_url="https://github.com/${repo_slug}/releases/download/v${version}/sconnect-module.tar.gz"

if [ "$module_id" != "sconnect" ]; then
  echo "FAIL: expected module id sconnect, got $module_id" >&2
  exit 1
fi

if [ "$module_name" != "SConnect" ]; then
  echo "FAIL: expected module name SConnect, got $module_name" >&2
  exit 1
fi

if [ "$url" != "$expected_url" ]; then
  echo "FAIL: release download_url mismatch: got=$url expected=$expected_url" >&2
  exit 1
fi

if ! rg -q "SConnect" "$readme"; then
  echo "FAIL: README should reference SConnect" >&2
  exit 1
fi

if ! rg -q 'OUTPUT_BASENAME="\$\{OUTPUT_BASENAME:-sconnect-module\}"' "$build_script"; then
  echo "FAIL: build.sh should default to sconnect-module" >&2
  exit 1
fi

if ! rg -q 'MODULE_ID="sconnect"' "$install_script"; then
  echo "FAIL: install.sh should target MODULE_ID=sconnect" >&2
  exit 1
fi

echo "PASS: sconnect scaffold identity is consistent"
