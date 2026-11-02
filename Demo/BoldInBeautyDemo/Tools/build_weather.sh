#!/bin/zsh
# Rebuilds /Game/Weather (day / night and weather materials, MPC, sounds) with the Mac editor commandlet.
# Usage: Tools/build_weather.sh [path/to/UE_5.8]   (defaults to the documentation root's UE_5.8)
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"   # from a worktree under BoldInBeauty-Project/worktrees
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$DEMO/BoldInBeautyDemo.uproject" -run=pythonscript \
	-script="$DEMO/Tools/build_weather.py" -unattended -nosplash -nullrhi -stdout -NoSound > "$DEMO/Saved/BuildWeather.stdout.txt" 2>&1 || true
cat "$DEMO/Saved/BuildWeather.log"
grep -q "^STATUS OK" "$DEMO/Saved/BuildWeather.log"
