#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"

if [ ! -f "$plugin_file" ]; then
  echo "FAIL: Missing $plugin_file" >&2
  exit 1
fi

if ! rg -q 'QUALITY_PREF_PATH' "$plugin_file"; then
  echo "FAIL: plugin must define QUALITY_PREF_PATH for persisted bitrate" >&2
  exit 1
fi

if ! rg -q 'save_quality_pref' "$plugin_file"; then
  echo "FAIL: plugin must implement save_quality_pref helper" >&2
  exit 1
fi

if ! rg -q 'load_quality_pref' "$plugin_file"; then
  echo "FAIL: plugin must implement load_quality_pref helper" >&2
  exit 1
fi

if ! rg -q 'load_quality_pref\(inst\);' "$plugin_file"; then
  echo "FAIL: create_instance must load persisted quality" >&2
  exit 1
fi

if ! rg -q 'save_quality_pref\(inst\);' "$plugin_file"; then
  echo "FAIL: quality setters must persist selected quality" >&2
  exit 1
fi

echo "PASS: quality persistence wiring present"
