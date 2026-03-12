#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if rg -q 'strcmp\(inst->playback_event, "paused"\) == 0 \|\|' "$plugin_file"; then
  echo "FAIL: transient paused events must not flush the audio ring buffer" >&2
  exit 1
fi

if ! rg -q 'strcmp\(inst->playback_event, "stopped"\) == 0' "$plugin_file"; then
  echo "FAIL: stopped event should still flush the ring buffer" >&2
  exit 1
fi

echo "PASS: ring flush only occurs on stopped events"
