#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"
event_hook="src/runtime/sconnect_event.sh"

for f in "$plugin_file" "$event_hook"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: Missing $f" >&2
    exit 1
  fi
done

if ! rg -q 'CONTROL_MIN_INTERVAL_MS' "$plugin_file"; then
  echo "FAIL: plugin must define CONTROL_MIN_INTERVAL_MS for request throttling" >&2
  exit 1
fi

if ! rg -q 'CONTROL_429_BACKOFF_MS' "$plugin_file"; then
  echo "FAIL: plugin must define CONTROL_429_BACKOFF_MS for rate-limit backoff" >&2
  exit 1
fi

if ! rg -q 'last_control_request_ms' "$plugin_file"; then
  echo "FAIL: plugin must track last control request timestamp" >&2
  exit 1
fi

if ! rg -q 'control_backoff_until_ms' "$plugin_file"; then
  echo "FAIL: plugin must track control backoff-until timestamp" >&2
  exit 1
fi

if ! rg -q 'control_backoff_until_ms > now' "$plugin_file"; then
  echo "FAIL: plugin must enforce cooldown while backoff is active" >&2
  exit 1
fi

if ! rg -q 'http_code == 429' "$plugin_file"; then
  echo "FAIL: plugin must handle HTTP 429 explicitly" >&2
  exit 1
fi

if ! rg -q 'control_backoff_until_ms = now_ms\(\) \+' "$plugin_file"; then
  echo "FAIL: plugin must advance backoff window on HTTP 429" >&2
  exit 1
fi

if ! rg -q 'Retry-After' "$plugin_file"; then
  echo "FAIL: plugin should parse Retry-After when available" >&2
  exit 1
fi

if ! rg -q '\[ -f "\$STATE_FILE" \]' "$event_hook"; then
  echo "FAIL: event hook must load previous now-playing state" >&2
  exit 1
fi

if ! rg -q 'prev_name' "$event_hook"; then
  echo "FAIL: event hook must preserve previous track name when event payload omits it" >&2
  exit 1
fi

if ! rg -q 'prev_artists' "$event_hook"; then
  echo "FAIL: event hook must preserve previous artists when event payload omits it" >&2
  exit 1
fi

echo "PASS: control throttle/backoff and metadata persistence are implemented"
