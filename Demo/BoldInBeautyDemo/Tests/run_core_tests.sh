#!/usr/bin/env sh
# Compiles and runs the engine-independent rules tests with Unreal-like flags
# (no exceptions, no RTTI, shadowing and conversions treated as errors).
#   run_core_tests.sh            rules + meta + hit feel + operator kit + account (save, tutorial, rotation) + campaign (13 chapters, balance) + tester panel + ... + Ark hub + angel brain tests
#                                + Ultimate Custom Night unit tests and its beatability suite
#   run_core_tests.sh --balance  also prints the kit balance table (add an operator name for its per-ability breakdown)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
RULES="$HERE/../Source/BoldInBeautyDemo/BIBRules"
OUT="${TMPDIR:-/tmp}/bib_core_tests"
CXX="${CXX:-c++}"
FLAGS="-std=c++20 -O2 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Wno-missing-field-initializers -Werror -fno-exceptions -fno-rtti"
KITS="$RULES/BIBKitStatus.cpp $RULES/BIBKitRegistry.cpp $RULES/BIBKitRuntime.cpp $RULES/BIBKitSteps.cpp $RULES/BIBKitCombat.cpp $RULES/BIBKitHooks.cpp $RULES/BIBKitData1.cpp $RULES/BIBKitData2.cpp $RULES/BIBKitData3.cpp"
SOURCES="$RULES/BIBCore.cpp $RULES/BIBCampaign.cpp $RULES/BIBMeta.cpp $RULES/BIBMetaOps.cpp $RULES/BIBPages.cpp $RULES/BIBJson.cpp $RULES/BIBSave.cpp $RULES/BIBTutorial.cpp $RULES/BIBTester.cpp $RULES/BIBArcade.cpp $RULES/BIBArena.cpp $RULES/BIBLiveOps.cpp $RULES/BIBOnline.cpp $RULES/BIBPagesExtra.cpp $RULES/BIBSynth.cpp $RULES/BIBMiniGames.cpp $RULES/BIBRally.cpp $RULES/BIBElement.cpp $RULES/BIBInventory.cpp $RULES/BIBEvents.cpp $RULES/BIBPagesGacha.cpp $RULES/BIBArk.cpp $KITS $RULES/BIBUltimateNight.cpp $RULES/BIBUcnSubjects.cpp"
for TEST in core_tests meta_tests kit_tests account_tests campaign_tests tester_tests arcade_tests minigame_tests gacha_tests ark_tests; do
  # shellcheck disable=SC2086
  "$CXX" $FLAGS "$HERE/$TEST.cpp" $SOURCES -o "$OUT-$TEST"
  "$OUT-$TEST"
done
# Ultimate Custom Night (owner request 2026-10-14): unit tests, then the 200-seed solvability / beatability suite.
UCN="$RULES/BIBUcnBots.cpp"   # the rules themselves are in SOURCES (the arcade page lists the mode)
for TEST in ucn_tests ucn_beatability; do
  # shellcheck disable=SC2086
  "$CXX" $FLAGS "$HERE/$TEST.cpp" $UCN $SOURCES -o "$OUT-$TEST"
  "$OUT-$TEST"
done
# Zenless-style combat rules: Daze / stun / combo and the angels' adaptive mind (owner request 2026-10-09).
"$CXX" $FLAGS "$HERE/angel_tests.cpp" "$RULES/BIBStagger.cpp" "$RULES/BIBAngelMind.cpp" -o "$OUT-angel_tests"
# Behaviour layer of the angels: rolling model of the commander, squad board (attack tokens, roles, slots), steering (2026-10-12).
"$CXX" $FLAGS "$HERE/tactics_tests.cpp" "$RULES/BIBPlayerModel.cpp" "$RULES/BIBSquad.cpp" "$RULES/BIBSteer.cpp" -o "$OUT-tactics_tests"
"$OUT-tactics_tests"
"$OUT-angel_tests"
# The angels' tactical brain: knobs per tier, perception delay, habits, crowd tokens, every behaviour, fairness and clean-play
# duels per angel type, and the [BIB.AngelAI] config (owner request 2026-10-13, goal 2).
"$CXX" $FLAGS "$HERE/angel_brain_tests.cpp" "$RULES/BIBAngelBrain.cpp" "$RULES/BIBAngelMind.cpp" -o "$OUT-angel_brain_tests"
"$OUT-angel_brain_tests" "$HERE/../Config/DefaultGame.ini"
# Hit feel: weights, hit stop, reactions, poses, the [BIB.CombatFeel] config and the kits' hit origins (owner request 2026-10-13).
# shellcheck disable=SC2086
"$CXX" $FLAGS "$HERE/feel_tests.cpp" "$RULES/BIBHitFeel.cpp" "$RULES/BIBStagger.cpp" $KITS -o "$OUT-feel_tests"
"$OUT-feel_tests" "$HERE/../Config/DefaultGame.ini"
# Whole-game playthroughs: a bot plays 13 chapters x 8 levels with the real rules (fails on a soft-lock). --seeds 30 keeps it quick;
# run Tests/playthrough.cpp directly with --stages for the per-stage grind table.
# shellcheck disable=SC2086
"$CXX" $FLAGS "$HERE/playthrough.cpp" $SOURCES -o "$OUT-playthrough"
"$OUT-playthrough" --seeds 30 | tail -n 3
if [ "${1:-}" = "--balance" ]; then
  # shellcheck disable=SC2086
  "$CXX" $FLAGS "$HERE/kit_balance.cpp" $SOURCES -o "$OUT-kit_balance"
  shift
  "$OUT-kit_balance" "$@"
fi
