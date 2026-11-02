#!/bin/zsh
# Builds, cooks and packages the iOS app (2026-10-09). One-time owner steps: Xcode > Settings > Accounts > add your Apple ID
# (a free personal team is enough to run on your own iPhone), then find the Team ID there (10 characters) and run:
#   BIB_IOS_TEAM=ABCDE12345 Tools/package_ios.sh [path/to/UE_5.8]
# The bundle id is com.boldinbeauty.demo (Config/DefaultEngine.ini); change it there if Apple says it is taken.
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$DEMO/../../UE_5.8}"
[ -d "$ENGINE" ] || ENGINE="$DEMO/../../../../../UE_5.8"
[ -n "$BIB_IOS_TEAM" ] || { echo "Set BIB_IOS_TEAM to your Apple Team ID (Xcode > Settings > Accounts)."; exit 1; }
INI="$DEMO/Config/DefaultEngine.ini"
if grep -q "^IOSTeamID=" "$INI"; then sed -i '' "s/^IOSTeamID=.*/IOSTeamID=$BIB_IOS_TEAM/" "$INI"
else sed -i '' "s/^bAutomaticSigning=True/bAutomaticSigning=True\nIOSTeamID=$BIB_IOS_TEAM/" "$INI"; fi
OUT="$HOME/Documents/BoldInBeauty-Builds/$(date +%Y-%m-%d)-ios"
MAPS=$(cd "$DEMO/Content" && find BIB/Maps Greybox/Maps -name '*.umap' 2>/dev/null | sed 's|^|/Game/|; s|\.umap$||' | paste -sd+ -)
"$ENGINE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun -project="$DEMO/BoldInBeautyDemo.uproject" -platform=IOS -clientconfig=Development \
	-build -cook -map="$MAPS" -stage -pak -package -archive -archivedirectory="$OUT" -unattended -utf8output
echo "iOS build: $OUT (install with Xcode > Window > Devices and Simulators, or Apple Configurator)"
