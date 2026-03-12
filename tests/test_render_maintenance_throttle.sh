#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if ! rg -q 'RENDER_MAINTENANCE_INTERVAL_MS' "$plugin_file"; then
  echo "FAIL: plugin must define render maintenance interval" >&2
  exit 1
fi

if ! rg -q 'last_maintenance_ms' "$plugin_file"; then
  echo "FAIL: plugin must track last maintenance timestamp" >&2
  exit 1
fi

if ! rg -q 'do_maintenance' "$plugin_file"; then
  echo "FAIL: render path must gate maintenance with do_maintenance" >&2
  exit 1
fi

echo "PASS: render maintenance throttling wiring is present"
