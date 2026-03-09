#!/bin/sh
set -eu

STATE_FILE="/data/UserData/move-anything/cache/sconnect-nowplaying.env"
TMP_FILE="${STATE_FILE}.tmp.$$"

clean_value() {
  printf '%s' "${1:-}" | tr '\n' ',' | tr '\r' ',' | sed 's/[[:space:]][[:space:]]*/ /g; s/^ //; s/ $//'
}

event="$(clean_value "${PLAYER_EVENT:-}")"
name="$(clean_value "${NAME:-${TITLE:-}}")"
artists="$(clean_value "${ARTISTS:-${ARTIST:-}}")"
album="$(clean_value "${ALBUM:-}")"
uri="$(clean_value "${URI:-}")"
position_ms="$(clean_value "${POSITION_MS:-}")"
prev_name=""
prev_artists=""
prev_album=""
prev_uri=""

if [ -f "$STATE_FILE" ]; then
  while IFS='=' read -r key value; do
    case "$key" in
      name) prev_name="$value" ;;
      artists) prev_artists="$value" ;;
      album) prev_album="$value" ;;
      uri) prev_uri="$value" ;;
    esac
  done < "$STATE_FILE"
fi

[ -n "$name" ] || name="$prev_name"
[ -n "$artists" ] || artists="$prev_artists"
[ -n "$album" ] || album="$prev_album"
[ -n "$uri" ] || uri="$prev_uri"

{
  printf 'event=%s\n' "$event"
  printf 'name=%s\n' "$name"
  printf 'artists=%s\n' "$artists"
  printf 'album=%s\n' "$album"
  printf 'uri=%s\n' "$uri"
  printf 'position_ms=%s\n' "$position_ms"
} > "$TMP_FILE"

mv "$TMP_FILE" "$STATE_FILE"
