#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if ! rg -q 'TRANSITION_PREBUFFER_MS' "$plugin_file"; then
  echo "FAIL: plugin must define transition prebuffer duration" >&2
  exit 1
fi

if ! rg -q 'TRANSITION_PREBUFFER_SAMPLES' "$plugin_file"; then
  echo "FAIL: plugin must define transition prebuffer sample target" >&2
  exit 1
fi

if ! rg -q 'pending_transition_prebuffer' "$plugin_file"; then
  echo "FAIL: plugin must track pending transition prebuffer state" >&2
  exit 1
fi

if ! rg -q 'maybe_release_transition_prebuffer' "$plugin_file"; then
  echo "FAIL: plugin must implement transition prebuffer release helper" >&2
  exit 1
fi

if ! rg -q 'strcmp\(inst->playback_event, "track_changed"\) == 0' "$plugin_file"; then
  echo "FAIL: track_changed events must trigger transition prebuffer" >&2
  exit 1
fi

echo "PASS: transition prebuffer wiring is present"
