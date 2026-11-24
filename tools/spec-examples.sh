#!/usr/bin/env bash
# Run the tested examples of the specification (spec/, code blocks marked
# %example) through cesium0: each must be accepted, or rejected when its
# category is `rejects` (proposals/0000-documentation-system.md, "Examples in
# the specification are tested").
#
# usage: bash tools/spec-examples.sh [CESIUM0] [SKEIN]
#   CESIUM0  default: zig-out/bin/cesium0, after `zig build`
#   SKEIN    default: .skein-bin/bin/skein, from tools/fetch-skein.sh
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cesium0=${1:-$ROOT/zig-out/bin/cesium0}
skein=${2:-$ROOT/.skein-bin/bin/skein}
[ -x "$cesium0" ] || [ -x "$cesium0.exe" ] || { echo "no cesium0 at $cesium0: zig build first" >&2; exit 2; }
[ -x "$skein" ] || [ -x "$skein.exe" ] || { echo "no skein at $skein: bash tools/fetch-skein.sh first" >&2; exit 2; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"$skein" examples --root "$ROOT" --out "$out" > /dev/null

failed=0 count=0
# examples.json: one object per example, with "file" before "expect"
while IFS=$'\t' read -r file expect; do
  count=$((count + 1))
  if "$cesium0" --emit-mir -o "$out/out.mir" "$out/$file" > "$out/err.txt" 2>&1; then got=accept; else got=reject; fi
  if [ "$got" != "$expect" ]; then
    echo "::error::${file%.csm}: expected cesium0 to $expect it, and it did $got"
    sed 's/^/    /' "$out/err.txt"
    failed=$((failed + 1))
  fi
done < <(awk -F'"' '/"file":/ { f = $4 } /"expect":/ { print f "\t" $4 }' "$out/examples.json")

echo "$count examples of the specification, $failed not as their category says"
[ "$failed" -eq 0 ]
