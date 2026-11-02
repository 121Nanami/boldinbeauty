#!/bin/zsh
# Rebuilds /Game/Greybox (kit import, materials, L_NeoTokyo, L_Ark, chapter maps L_Ch02..L_Ch13) with the Mac editor commandlet.
# Usage: Tools/build_greybox.sh [path/to/UE_5.8]   (defaults to the documentation root's UE_5.8)
# Env: BIB_KIT_ROOT (Fab vault folder), BIB_GREYBOX_ONLY=neotokyo|ark|chapters|ch03,ch11 (comma list)
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"   # from a worktree under BoldInBeauty-Project/worktrees
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$DEMO/BoldInBeautyDemo.uproject" -run=pythonscript \
	-script="$DEMO/Tools/build_greybox.py" -unattended -nosplash -nullrhi -stdout -NoSound > "$DEMO/Saved/BuildGreybox.stdout.txt" 2>&1 || true
cat "$DEMO/Saved/BuildGreybox.log"
grep -q "^STATUS OK" "$DEMO/Saved/BuildGreybox.log"
