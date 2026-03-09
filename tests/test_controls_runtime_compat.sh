#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"

if [ ! -f "$plugin_file" ]; then
  echo "FAIL: Missing $plugin_file" >&2
  exit 1
fi

if rg -q -- '--method=' "$plugin_file"; then
  echo "FAIL: busybox wget path must not require --method" >&2
  exit 1
fi

if rg -q -- '--tries=' "$plugin_file"; then
  echo "FAIL: busybox wget path must not require --tries" >&2
  exit 1
fi

if rg -q -- '--timeout=' "$plugin_file"; then
  echo "FAIL: busybox wget path must not require --timeout" >&2
  exit 1
fi

if ! rg -q -- 'wget -S -O /dev/null -T 6' "$plugin_file"; then
  echo "FAIL: plugin must use busybox-compatible wget flags" >&2
  exit 1
fi

if ! rg -q -- 'X-HTTP-Method-Override: PUT' "$plugin_file"; then
  echo "FAIL: plugin must include PUT override header for wget fallback" >&2
  exit 1
fi

if ! rg -q 'set_controls_enabled\(inst, false\);' "$plugin_file"; then
  echo "FAIL: controls must disable on failed control request" >&2
  exit 1
fi

if ! rg -q '#define RING_SECONDS        3' "$plugin_file"; then
  echo "FAIL: ring buffer should be set to 3 seconds for stutter resilience" >&2
  exit 1
fi

if ! rg -q 'strcmp\(inst->playback_event, "paused"\) == 0' "$plugin_file"; then
  echo "FAIL: paused event handling must be present" >&2
  exit 1
fi

if ! rg -q 'clear_ring\(inst\);' "$plugin_file"; then
  echo "FAIL: ring must be flushed on pause/stopped events" >&2
  exit 1
fi

echo "PASS: controls runtime compatibility and pause-latency safeguards are present"
