#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if ! rg -q 'PAUSE_FLUSH_DELAY_MS' "$plugin_file"; then
  echo "FAIL: plugin must define deferred pause flush delay" >&2
  exit 1
fi

if ! rg -q 'pending_pause_flush' "$plugin_file"; then
  echo "FAIL: plugin must track pending pause flush state" >&2
  exit 1
fi

if ! rg -q 'maybe_flush_paused' "$plugin_file"; then
  echo "FAIL: plugin must implement deferred paused flush helper" >&2
  exit 1
fi

if ! rg -q 'maybe_flush_paused\(inst\);' "$plugin_file"; then
  echo "FAIL: render loop must run deferred paused flush check" >&2
  exit 1
fi

echo "PASS: deferred pause flush wiring is present"
