#!/bin/bash
# usage: tools/snap.sh <id> <comma-times>   -> contact sheet path printed
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SP=/private/tmp/claude-501/-Users-local-user-Downloads-motion-studio/33278d3f-d7a2-4ff7-8126-cd3b3201afec/scratchpad
python3 "$ROOT/tools/build.py" "$1" >/dev/null || exit 1
cd "$ROOT/projects/$1" && npx hyperframes check < /dev/null 2>&1 | grep -E "✗|Check" | grep -v -E "text_occluded|content_overlap|text_not_painted" | head -12
rm -rf "$SP/snap-$1"; npx hyperframes snapshot --at "$2" --no-end -o "$SP/snap-$1" < /dev/null 2>&1 | grep -E "rror" | head -5
ls "$SP/snap-$1"/contact-sheet*.jpg
