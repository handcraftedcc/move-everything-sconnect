#!/usr/bin/env bash
set -euo pipefail

release_json="release.json"
module_json="src/module.json"
workflow_file=".github/workflows/release.yml"
dockerfile="scripts/Dockerfile"

if ! command -v jq >/dev/null 2>&1; then
  echo "FAIL: jq is required to run this test" >&2
  exit 1
fi

if [ ! -f "$release_json" ]; then
  echo "FAIL: Missing $release_json" >&2
  exit 1
fi

if [ ! -f "$module_json" ]; then
  echo "FAIL: Missing $module_json" >&2
  exit 1
fi

if [ ! -f "$workflow_file" ]; then
  echo "FAIL: Missing $workflow_file" >&2
  exit 1
fi

if [ ! -f "$dockerfile" ]; then
  echo "FAIL: Missing $dockerfile" >&2
  exit 1
fi

version=$(jq -r '.version' "$release_json")
module_version=$(jq -r '.version' "$module_json")

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
url=$(jq -r '.download_url' "$release_json")

if [ -z "$version" ] || [ "$version" = "null" ]; then
  echo "FAIL: release.json missing version" >&2
  exit 1
fi

if [ "$module_version" != "$version" ]; then
  echo "FAIL: version mismatch: src/module.json=$module_version release.json=$version" >&2
  exit 1
fi

if [ "$url" != "$expected_url" ]; then
  echo "FAIL: release.json download_url mismatch: got=$url expected=$expected_url" >&2
  exit 1
fi

if ! rg -q "Verify release\.json metadata" "$workflow_file"; then
  echo "FAIL: release workflow missing release.json verification step" >&2
  exit 1
fi

if ! rg -q "Build module" "$workflow_file"; then
  echo "FAIL: release workflow missing module build step" >&2
  exit 1
fi

if ! rg -q "softprops/action-gh-release@" "$workflow_file"; then
  echo "FAIL: release workflow missing GitHub release publish step" >&2
  exit 1
fi

if ! rg -q '^[[:space:]]*gcc[[:space:]]*\\?$' "$dockerfile"; then
  echo "FAIL: build Dockerfile must install host gcc for Rust build scripts" >&2
  exit 1
fi

echo "PASS: release metadata is present and release workflow builds and publishes artifacts"
exit 0
