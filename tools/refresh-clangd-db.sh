#!/usr/bin/env bash
# Filter build/compile_commands.json to only first-party TUs so clangd
# stops background-indexing assimp/glfw/zlib/etc. Re-run after cmake regenerates.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/build/compile_commands.json"
DST="$ROOT/compile_commands.json"

if [[ ! -f "$SRC" ]]; then
  echo "no $SRC — run cmake first" >&2
  exit 1
fi

# Drop the symlink if present
[[ -L "$DST" ]] && rm "$DST"

jq '[ .[] | select(.file | test("/(external|build)/") | not) ]' "$SRC" > "$DST"

echo "wrote $(jq length "$DST") first-party entries to $DST (source had $(jq length "$SRC"))"
