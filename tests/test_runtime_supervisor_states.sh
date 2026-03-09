#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"
legacy_plugin_file="src/dsp/airplay_plugin.c"
build_script="scripts/build.sh"
ui_file="src/ui.js"
help_file="src/help.json"
readme_file="README.md"

for f in "$build_script" "$ui_file" "$help_file" "$readme_file"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: Missing $f" >&2
    exit 1
  fi
done

if [ ! -f "$plugin_file" ]; then
  echo "FAIL: Missing $plugin_file" >&2
  exit 1
fi

if [ -f "$legacy_plugin_file" ]; then
  echo "FAIL: Legacy plugin source still present at $legacy_plugin_file" >&2
  exit 1
fi

if ! rg -q 'src/dsp/sconnect_plugin\.c' "$build_script"; then
  echo "FAIL: build script must compile src/dsp/sconnect_plugin.c" >&2
  exit 1
fi

for state in starting waiting_for_spotify authenticating ready error; do
  if ! rg -q "\"$state\"" "$plugin_file"; then
    echo "FAIL: plugin must include runtime state '$state'" >&2
    exit 1
  fi
done

for key in device_name restart reset_auth; do
  if ! rg -q "strcmp\(key, \"$key\"\)" "$plugin_file"; then
    echo "FAIL: plugin must handle control key '$key'" >&2
    exit 1
  fi
done

if ! rg -q 'backend_state' "$plugin_file"; then
  echo "FAIL: plugin must expose backend_state param" >&2
  exit 1
fi

if ! rg -q "starting" "$ui_file"; then
  echo "FAIL: UI must render starting state" >&2
  exit 1
fi

if ! rg -q "waiting_for_spotify" "$ui_file"; then
  echo "FAIL: UI must render waiting_for_spotify state" >&2
  exit 1
fi

if ! rg -q "authenticating" "$ui_file"; then
  echo "FAIL: UI must render authenticating state" >&2
  exit 1
fi

if ! rg -q "ready" "$ui_file"; then
  echo "FAIL: UI must render ready state" >&2
  exit 1
fi

if rg -q 'AirPlay' "$help_file" "$readme_file"; then
  echo "FAIL: README/help should not use AirPlay branding anymore" >&2
  exit 1
fi

echo "PASS: sconnect runtime supervisor states and controls are wired"
