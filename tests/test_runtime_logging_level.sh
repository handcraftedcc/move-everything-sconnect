#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if rg -q '"-v"' "$plugin_file"; then
  echo "FAIL: plugin must not launch librespot in verbose TRACE mode" >&2
  exit 1
fi

echo "PASS: plugin launches librespot without TRACE verbosity"
