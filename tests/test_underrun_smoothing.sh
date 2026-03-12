#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin_file="$repo_root/src/dsp/sconnect_plugin.c"

if ! rg -q 'fill_underrun_tail' "$plugin_file"; then
  echo "FAIL: plugin must implement underrun tail smoothing helper" >&2
  exit 1
fi

if ! rg -q 'if \(got < needed\)' "$plugin_file"; then
  echo "FAIL: render path must detect underrun condition" >&2
  exit 1
fi

if ! rg -q 'fill_underrun_tail\(inst, out_interleaved_lr, got, needed\);' "$plugin_file"; then
  echo "FAIL: render path must smooth underrun tail instead of hard zeroing" >&2
  exit 1
fi

echo "PASS: underrun smoothing wiring is present"
