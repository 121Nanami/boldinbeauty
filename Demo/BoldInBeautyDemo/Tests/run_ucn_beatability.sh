#!/usr/bin/env sh
# Ultimate Custom Night: only the solvability / beatability suite (perfect, human-model and do-nothing bots, fixed seeds).
#   run_ucn_beatability.sh               200 seeds per mode
#   run_ucn_beatability.sh --seeds 1000  more seeds
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
RULES="$HERE/../Source/BoldInBeautyDemo/BIBRules"
OUT="${TMPDIR:-/tmp}/bib_ucn_beatability"
CXX="${CXX:-c++}"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers -Werror -fno-exceptions -fno-rtti"
KITS="$RULES/BIBKitStatus.cpp $RULES/BIBKitRegistry.cpp $RULES/BIBKitRuntime.cpp $RULES/BIBKitSteps.cpp $RULES/BIBKitCombat.cpp $RULES/BIBKitHooks.cpp $RULES/BIBKitData1.cpp $RULES/BIBKitData2.cpp $RULES/BIBKitData3.cpp"
SOURCES="$RULES/BIBCore.cpp $RULES/BIBCampaign.cpp $RULES/BIBMeta.cpp $RULES/BIBMetaOps.cpp $RULES/BIBPages.cpp $RULES/BIBJson.cpp $RULES/BIBSave.cpp $RULES/BIBTutorial.cpp $RULES/BIBTester.cpp $RULES/BIBArcade.cpp $RULES/BIBArena.cpp $RULES/BIBLiveOps.cpp $RULES/BIBOnline.cpp $RULES/BIBPagesExtra.cpp $RULES/BIBSynth.cpp $RULES/BIBMiniGames.cpp $RULES/BIBElement.cpp $RULES/BIBInventory.cpp $RULES/BIBEvents.cpp $RULES/BIBPagesGacha.cpp $RULES/BIBArk.cpp $RULES/BIBUltimateNight.cpp $RULES/BIBUcnSubjects.cpp $KITS"
UCN="$RULES/BIBUcnBots.cpp"   # the rules themselves are in SOURCES (the arcade page lists the mode)
# shellcheck disable=SC2086
"$CXX" $FLAGS "$HERE/ucn_beatability.cpp" $UCN $SOURCES -o "$OUT"
"$OUT" "$@"
