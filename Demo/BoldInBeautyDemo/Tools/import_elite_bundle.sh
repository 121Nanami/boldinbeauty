#!/bin/zsh
# Copies the owner's Elite_Bundle (Dekogon / Meshingun / Scansfactory packs) into Content/Elite_Bundle, where its assets'
# package paths (/Game/Elite_Bundle/...) expect it. Run once per machine before Tools/build_greybox.sh; the greybox build then
# dresses every chapter map and the Ark with it (Tools/greybox_bundle.py). Store content, about 2 GB: kept out of Git.
# Usage: Tools/import_elite_bundle.sh [path/to/Elite_Bundle]   (defaults to the documentation root's Elite_Bundle)
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-${BIB_ELITE_BUNDLE:-$DEMO/../../Elite_Bundle}}"
[ -d "$SRC" ] || SRC="$DEMO/../../../../../Elite_Bundle"   # from a worktree under BoldInBeauty-Project/worktrees
[ -d "$SRC" ] || { echo "Elite_Bundle not found (pass its path or set BIB_ELITE_BUNDLE)"; exit 1; }
rsync -a --delete --exclude .DS_Store "$SRC/" "$DEMO/Content/Elite_Bundle/"
echo "Elite_Bundle copied: $(find "$DEMO/Content/Elite_Bundle" -name '*.uasset' | wc -l | tr -d ' ') assets, $(du -sh "$DEMO/Content/Elite_Bundle" | cut -f1)"
