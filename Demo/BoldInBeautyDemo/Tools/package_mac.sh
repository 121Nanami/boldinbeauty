#!/bin/zsh
# Packages a standalone macOS Development build of the demo with every map (hub, Chapter 1 stages, greybox maps).
# Usage: Tools/package_mac.sh [output dir] [path/to/UE_5.8]
#   output dir defaults to ~/Documents/BoldInBeauty-Builds/<date>; run Tools/build_assets.sh and build_greybox.sh first.
# Why three steps: on UE 5.8.3 BuildCookRun's cook skipped /Game/Greybox, and its -archive copied only the .app shell
# (no paks, no engine dylibs: "Library not loaded: @rpath/libtbb.12.dylib"). So: cook with the commandlet and an explicit
# map list, stage with UAT, then copy the whole staged folder. Start the game with <out>/BoldInBeautyDemo.app.
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$HOME/Documents/BoldInBeauty-Builds/$(date +%Y-%m-%d)}"
ENGINE="${2:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"
PROJECT="$DEMO/BoldInBeautyDemo.uproject"
MAPS=$(cd "$DEMO/Content" && find BIB/Maps Greybox/Maps -name '*.umap' 2>/dev/null | sed 's|^|/Game/|; s|\.umap$||' | paste -sd+ -)
echo "maps: $MAPS"
"$ENGINE/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT" -run=cook -targetplatform=Mac -map="$MAPS" -unattended -nosplash -NoSound -stdout \
	> "$DEMO/Saved/PackageMac.cook.txt" 2>&1
grep "Packages Cooked" "$DEMO/Saved/PackageMac.cook.txt" | tail -1
"$ENGINE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$PROJECT" -noP4 -utf8output -nocompileeditor -platform=Mac \
	-clientconfig=Development -build -skipcook -stage -pak -compressed > "$DEMO/Saved/PackageMac.stage.txt" 2>&1
tail -1 "$DEMO/Saved/PackageMac.stage.txt"
mkdir -p "$OUT"
rm -rf "$OUT/BoldInBeautyDemo-Mac"
ditto "$DEMO/Saved/StagedBuilds/Mac" "$OUT/BoldInBeautyDemo-Mac"
echo "build: $OUT/BoldInBeautyDemo-Mac/BoldInBeautyDemo.app"
