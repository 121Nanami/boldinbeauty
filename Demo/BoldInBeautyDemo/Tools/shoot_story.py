"""Overview screenshots of the story maps (chapters 14-40 and the reworks), from an elevated oblique camera over their sections.

    UnrealEditor <BoldInBeautyDemo.uproject> -ExecCmds="py <abs path>/Tools/shoot_story.py"

BIB_SHOOT_CHAPTERS="14,20,36" limits the run (default: every story map). Roofs and fog are hidden for the shot. Writes
Saved/StoryShots/<map>.png and quits the editor.
"""

import json
import math
import os
import traceback

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(unreal.Paths.project_saved_dir(), "StoryShots")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(text):
    unreal.log("[shoot_story] " + text)


def targets():
    data = {int(k): v for k, v in json.load(open(os.path.join(HERE, "story_data.json"))).items()}
    only = [int(x) for x in os.environ.get("BIB_SHOOT_CHAPTERS", "").replace(" ", "").split(",") if x]
    return [(n, "L_Ch%02d_%s" % (n, data[n]["short"])) for n in sorted(data) if not only or n in only]


def flow():
    os.makedirs(OUT, exist_ok=True)
    for n, name in targets():
        if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Greybox/Maps/" + name):
            log(name + " missing")
            continue
        LES.load_level("/Game/Greybox/Maps/" + name)
        yield 90
        pts = []
        for a in EAS.get_all_level_actors():
            label = a.get_actor_label()
            if label.startswith(("Roof", "HeightFog", "SkyAtmosphere", "AnimeStyle", "RoofSlab", "Ceiling")):
                a.set_is_temporarily_hidden_in_editor(True)
            if unreal.Name("Section") in list(a.tags) and isinstance(a, unreal.TargetPoint):
                pts.append(a.get_actor_location())
        if not pts:
            log(name + ": no sections")
            continue
        cx = sum(p.x for p in pts) / len(pts)
        cy = sum(p.y for p in pts) / len(pts)
        cz = sum(p.z for p in pts) / len(pts)
        span = max(max(p.x for p in pts) - min(p.x for p in pts), max(p.y for p in pts) - min(p.y for p in pts), 12000.0)
        dist = span * 0.95 + 9000.0
        eye = unreal.Vector(cx - dist * 0.55, cy - dist * 0.8, cz + dist * 0.62)
        look = unreal.Vector(cx, cy, cz)
        d = look - eye
        yaw = math.degrees(math.atan2(d.y, d.x))
        pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
        UES.set_level_viewport_camera_info(eye, unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        yield 90
        path = os.path.join(OUT, name + ".png")
        if os.path.exists(path):
            os.remove(path)
        unreal.SystemLibrary.execute_console_command(UES.get_editor_world(), "HighResShot 1600x900 filename=%s" % path)
        for _ in range(900):
            if os.path.exists(path):
                break
            yield 1
        log("%s: %s" % (name, "written" if os.path.exists(path) else "NOT WRITTEN"))
        yield 30


class Runner:
    def __init__(self):
        self.gen = flow()
        self.wait = 0
        self.boot = 0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, _dt):
        self.boot += 1
        if self.boot < 200:
            return
        if self.wait > 0:
            self.wait -= 1
            return
        try:
            self.wait = next(self.gen) or 0
        except StopIteration:
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()
        except Exception:
            log(traceback.format_exc())
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()


Runner()
