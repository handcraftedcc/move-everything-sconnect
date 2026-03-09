#!/usr/bin/env bash
set -euo pipefail

plugin_file="src/dsp/sconnect_plugin.c"
ui_file="src/ui.js"
build_script="scripts/build.sh"

for f in "$plugin_file" "$ui_file" "$build_script"; do
  if [ ! -f "$f" ]; then
    echo "FAIL: Missing $f" >&2
    exit 1
  fi
done

if ! rg -q '"controls_enabled"' "$plugin_file"; then
  echo "FAIL: plugin must expose controls_enabled param" >&2
  exit 1
fi

if ! rg -q '"track_name"' "$plugin_file"; then
  echo "FAIL: plugin must expose track_name param" >&2
  exit 1
fi

if ! rg -q '"track_artist"' "$plugin_file"; then
  echo "FAIL: plugin must expose track_artist param" >&2
  exit 1
fi

for action in play_pause next previous enable_controls reset_auth; do
  if ! rg -q "\"$action\"" "$plugin_file"; then
    echo "FAIL: plugin must support $action control action" >&2
    exit 1
  fi
done

if ! rg -q '"quality"' "$plugin_file"; then
  echo "FAIL: plugin must support quality parameter" >&2
  exit 1
fi

if ! rg -q '"--onevent"' "$plugin_file"; then
  echo "FAIL: plugin must launch librespot with --onevent metadata hook" >&2
  exit 1
fi

if ! rg -q 'sconnect_event\.sh' "$build_script"; then
  echo "FAIL: build script must stage sconnect_event.sh runtime helper" >&2
  exit 1
fi

if rg -q 'TRANSPORT_CONTROLS_VISIBLE = false' "$ui_file"; then
  :
else
  if ! rg -q '\[Enable Controls\]' "$ui_file"; then
    echo "FAIL: UI must show Enable Controls action in simple mode" >&2
    exit 1
  fi

  for label in '\[Play/Pause\]' '\[Next\]' '\[Previous\]' '\[Reset Auth\]'; do
    if ! rg -q "$label" "$ui_file"; then
      echo "FAIL: UI must include $label for enhanced mode" >&2
      exit 1
    fi
  done
fi

if ! rg -q 'Quality' "$ui_file"; then
  echo "FAIL: UI must include quality selector" >&2
  exit 1
fi

echo "PASS: enhanced controls scaffold is wired"
