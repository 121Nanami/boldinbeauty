#!/bin/zsh
# Rebuilds /Game/BIBCharacter (the BB character shader: master, functions, outline, instances, test map) with the Mac editor
# commandlet. Needs /Game/Weather (Tools/build_weather.sh) and, for the before row, /Game/BIB (Tools/build_assets.sh).
# Usage: Tools/build_character_shader.sh [path/to/UE_5.8]   (defaults to the documentation root's UE_5.8)
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"   # from a worktree under BoldInBeauty-Project/worktrees
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$DEMO/BoldInBeautyDemo.uproject" -run=pythonscript \
	-script="$DEMO/Tools/build_character_shader.py" -unattended -nosplash -nullrhi -stdout -NoSound > "$DEMO/Saved/BuildCharacterShader.stdout.txt" 2>&1 || true
cat "$DEMO/Saved/BuildCharacterShader.log"
grep -q "^STATUS OK" "$DEMO/Saved/BuildCharacterShader.log"
