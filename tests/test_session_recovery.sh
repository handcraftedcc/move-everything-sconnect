#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"

if [ ! -f "$plugin_file" ]; then
  echo "FAIL: Missing $plugin_file" >&2
  exit 1
fi

if ! rg -q 'SESSION_RECOVERY_COOLDOWN_MS' "$plugin_file"; then
  echo "FAIL: plugin must define session recovery cooldown constant" >&2
  exit 1
fi

if ! rg -q 'pending_session_recover' "$plugin_file"; then
  echo "FAIL: plugin must track pending session recovery state" >&2
  exit 1
fi

if ! rg -q 'SESSION_DELETED' "$plugin_file"; then
  echo "FAIL: plugin must detect SESSION_DELETED log events" >&2
  exit 1
fi

if ! rg -q 'DEVICES_DISAPPEARED' "$plugin_file"; then
  echo "FAIL: plugin must detect DEVICES_DISAPPEARED log events" >&2
  exit 1
fi

if ! rg -q 'maybe_recover_session' "$plugin_file"; then
  echo "FAIL: plugin must include a guarded session recovery routine" >&2
  exit 1
fi

echo "PASS: session invalidation recovery wiring is present"
