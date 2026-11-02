#!/bin/zsh
# Rebuilds /Game/BIB from Tools/build_assets.py with the Mac editor commandlet.
# Usage: Tools/build_assets.sh [path/to/UE_5.8]   (defaults to the documentation root's UE_5.8)
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"   # from a worktree under BoldInBeauty-Project/worktrees
[ -x "$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" ] || { echo "UnrealEditor-Cmd not found under $ENGINE"; exit 1; }
rm -f "$DEMO/Saved/BuildAssets.log"   # never report an older run's STATUS
rm -rf "$DEMO/Content/BIB"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$DEMO/BoldInBeautyDemo.uproject" -run=pythonscript \
	-script="$DEMO/Tools/build_assets.py" -unattended -nosplash -nullrhi -stdout -NoSound > "$DEMO/Saved/BuildAssets.stdout.txt" 2>&1 || true
cat "$DEMO/Saved/BuildAssets.log"
grep -q "^STATUS OK" "$DEMO/Saved/BuildAssets.log"
