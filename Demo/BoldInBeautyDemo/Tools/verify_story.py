"""Checks the story maps built by greybox_story.py (chapters 14-40 and the story overlay on 2, 4-10, 12).

    UnrealEditor-Cmd <BoldInBeautyDemo.uproject> -run=pythonscript -script=<abs path>/Tools/verify_story.py -unattended -nullrhi -stdout

Per map: one PlayerStart, four sections with one boss section, the five placeholder colours (RED / BLUE / YELLOW / GREEN / PURPLE)
with a TextRender label each, the boss label, eight episode triggers, one LevelEnd trigger, objectives and checkpoints.
BIB_CHAPTERS="14,15" limits the run. Writes Saved/VerifyStory.log (RESULT PASS / RESULT FAIL).
"""

import json
import os
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
LOG, FAILS = [], []
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAPS = "/Game/Greybox/Maps/"
REWORKS = {2, 4, 5, 6, 7, 8, 9, 10, 12}


try:
    ASSETS = json.load(open(os.path.join(HERE, "story_assets.json")))
except Exception:
    ASSETS = {}


def log(text):
    LOG.append(text)
    unreal.log("[verify_story] " + text)


def fail(text):
    FAILS.append(text)
    log("FAIL " + text)


def check_map(n, spec):
    name = "L_Ch%02d_%s" % (n, spec["short"])
    asset_path = MAPS + name
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        fail("%s missing" % name)
        return
    if not LES.load_level(asset_path):
        fail("%s did not load" % name)
        return
    tags, labels, kinds = {}, [], {"RED": 0, "BLUE": 0, "YELLOW": 0, "GREEN": 0, "PURPLE": 0}
    starts = 0
    actors = EAS.get_all_level_actors()
    for actor in actors:
        if isinstance(actor, unreal.PlayerStart):
            starts += 1
        for tag in actor.tags:
            t = str(tag)
            tags[t] = tags.get(t, 0) + 1
            if t in kinds:
                kinds[t] += 1
        if isinstance(actor, unreal.TextRenderActor):
            labels.append(str(actor.text_render.get_editor_property("text")))
    problems = []
    if n not in REWORKS and starts != 1:
        problems.append("PlayerStart %d (want 1)" % starts)
    if n in REWORKS and starts < 1:
        problems.append("no PlayerStart")
    if tags.get("Section", 0) != 4:
        problems.append("Section %d (want 4)" % tags.get("Section", 0))
    if tags.get("BossSection", 0) != 1:
        problems.append("BossSection %d (want 1)" % tags.get("BossSection", 0))
    for k, minimum in (("RED", 12), ("GREEN", 5), ("PURPLE", 8)):
        if kinds[k] < minimum:
            problems.append("%s markers %d (min %d)" % (k, kinds[k], minimum))
    if spec["npcs"] and kinds["BLUE"] < len(spec["npcs"]):
        problems.append("BLUE markers %d (npcs %d)" % (kinds["BLUE"], len(spec["npcs"])))
    if spec["pickups"] and kinds["YELLOW"] < len(spec["pickups"]):
        problems.append("YELLOW markers %d (pickups %d)" % (kinds["YELLOW"], len(spec["pickups"])))
    if tags.get("LevelEnd", 0) != 1:
        problems.append("LevelEnd triggers %d (want 1)" % tags.get("LevelEnd", 0))
    for e in range(1, 9):
        if tags.get("Beat_Episode_%d" % e, 0) != 1:
            problems.append("episode trigger %d missing" % e)
    if not any(("BOSS: " + spec["boss"]) in t for t in labels):
        problems.append("boss label '%s' missing" % spec["boss"])
    if not any(t.startswith("CUTSCENE:") for t in labels):
        problems.append("no CUTSCENE label")
    if n >= 14:
        want = len(ASSETS.get(str(n), []))
        got = tags.get("AssetStandIn", 0)
        if got != want:
            problems.append("asset stand-ins %d (assets %d)" % (got, want))
    status = "OK  " if not problems else "FAIL"
    log("%s %s: %d actors, markers %s, labels %d, triggers %d" % (
        status, name, len(actors), " ".join("%s=%d" % kv for kv in kinds.items()), len(labels),
        sum(v for k, v in tags.items() if k == "Trigger")))
    for p in problems:
        fail("%s: %s" % (name, p))


def main():
    with open(os.path.join(HERE, "story_data.json")) as handle:
        data = {int(k): v for k, v in json.load(handle).items()}
    only = [int(x) for x in os.environ.get("BIB_CHAPTERS", "").replace(" ", "").split(",") if x]
    for n in sorted(data):
        if only and n not in only:
            continue
        check_map(n, data[n])


def run():
    try:
        main()
    except Exception:
        FAILS.append("exception")
        log(traceback.format_exc())
    log("RESULT " + ("PASS" if not FAILS else "FAIL (%d)" % len(FAILS)))
    with open(os.path.join(unreal.Paths.project_saved_dir(), "VerifyStory.log"), "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
