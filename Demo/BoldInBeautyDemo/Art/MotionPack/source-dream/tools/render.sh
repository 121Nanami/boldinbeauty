#!/bin/bash
# usage: tools/render.sh id [id...]   renders 4K: alpha pieces -> out/alpha/<id>.mov (ProRes 4444), others -> out/silent/<id>.mp4
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/out/silent" "$ROOT/out/alpha"
for id in "$@"; do
  python3 "$ROOT/tools/build.py" "$id" >/dev/null || { echo "BUILD FAIL $id"; continue; }
  alpha=$(python3 -c "import json;print(1 if json.load(open('$ROOT/projects/$id/meta-src.json')).get('alpha') else 0)")
  cd "$ROOT/projects/$id"
  res=4k; case "$id" in *-linger) res=1080p;; esac
  if [ "$alpha" = 1 ]; then out="$ROOT/out/alpha/$id.mov"; npx hyperframes render --resolution $res --fps 13 --format mov -o "$out" < /dev/null > "$ROOT/out/render-$id.log" 2>&1
  else out="$ROOT/out/silent/$id.mp4"; npx hyperframes render --resolution 4k --fps 13 --quality delivery -o "$out" < /dev/null > "$ROOT/out/render-$id.log" 2>&1; fi
  if [ -f "$out" ]; then echo "OK $id $(du -h "$out" | cut -f1)"; else echo "FAIL $id"; fi
done
