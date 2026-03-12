#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if ! rg -q 'overrun_count' "$plugin_file"; then
  echo "FAIL: plugin must track overrun_count" >&2
  exit 1
fi

if ! rg -q '#define ENABLE_TELEMETRY_LOGS 0' "$plugin_file"; then
  echo "FAIL: telemetry logs must be disabled by default" >&2
  exit 1
fi

echo "PASS: overrun telemetry counters are present and logs are disabled by default"
