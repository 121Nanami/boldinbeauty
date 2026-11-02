#!/bin/zsh
# The bb-Showreel motion pack in the game (owner 2026-10-09). Generated content, git-ignored like the Elite bundle:
#   1. Tools/motion_export.py  - renders every UI piece and page transition at 12 fps in game mode, packs atlas pages,
#                                writes Content/BIBMotion/motion.json (needs node / npx hyperframes, ffmpeg, numpy)
#   2. Tools/motion_import.py  - imports the pages as /Game/BIBMotion/<id>/T_<id>_NN (editor commandlet)
#   3. Tools/motion_movies.sh  - ads, download trailer, sheets, transitions showcase and master reel -> Content/Movies
# Usage: Tools/import_motion_pack.sh [path/to/bb-Showreel] [path/to/UE_5.8]
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
REEL="${1:-$HOME/Downloads/motion-studio/videos/bb-Showreel}"
ENGINE="${2:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"
PY=python3
[ -x "$HOME/Downloads/motion-studio/.venv/bin/python" ] && PY="$HOME/Downloads/motion-studio/.venv/bin/python"
"$PY" "$DEMO/Tools/motion_export.py"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$DEMO/BoldInBeautyDemo.uproject" -run=pythonscript -script="$DEMO/Tools/motion_import.py" \
	-unattended -nosplash -nullrhi -stdout -NoSound > "$DEMO/Saved/MotionImport.stdout.txt" 2>&1 || true
tail -3 "$DEMO/Saved/MotionImport.log"
"$DEMO/Tools/motion_movies.sh" "$REEL"
grep -q "^STATUS OK" "$DEMO/Saved/MotionImport.log"
