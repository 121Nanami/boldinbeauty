"""Screenshots of the second greybox detail pass (combat sections) in PIE, with the anime look on.

    UnrealEditor <BoldInBeautyDemo.uproject> -ExecCmds="py <abs path>/Tools/shoot_sections.py"

BIB_SHOOT_MAPS="L_Ch02_Ruins,L_Ch05_ArcTech" limits the maps. For every map: PIE (free-fly pawn), the camera is put south-west of
the boss section above head height, looking at its centre; the shot waits until the file exists. Saved/Sections/<map>.png.
"""

import math
import os
import traceback

import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Sections")
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
MAPS = os.environ.get("BIB_SHOOT_MAPS", "L_Ch02_Ruins,L_Ch03_Docks,L_Ch05_ArcTech,L_Ch07_TOMBS,L_Ch10_Hallucinations,L_Ch12_Paradise,L_Ch13_SecretiveSoul").split(",")


def log(text):
    unreal.log("[shoot_sections] " + text)


def flow():
    os.makedirs(OUT, exist_ok=True)
    for name in MAPS:
        LES.load_level("/Game/Greybox/Maps/" + name)
        yield 60
        boss = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                if unreal.Name("BossSection") in list(a.tags)]
        LES.editor_request_begin_play()
        for _ in range(600):
            if UES.get_game_world():
                break
            yield 1
        yield 120
        w = UES.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(w, 0) if w else None
        if pc and boss:
            c = boss[0].get_actor_location()
            eye = unreal.Vector(c.x - 2600, c.y - 2600, c.z + 1200)
            look = unreal.Vector(c.x, c.y, c.z + 100)
            d = look - eye
            yaw = math.degrees(math.atan2(d.y, d.x))
            pitch = math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
            pawn = pc.get_controlled_pawn()
            if pawn:
                pawn.set_actor_location(eye, False, True)
            pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        yield 90
        path = os.path.join(OUT, name + ".png")
        if os.path.exists(path):
            os.remove(path)
        unreal.SystemLibrary.execute_console_command(w, "HighResShot 1600x900 filename=%s" % path)
        for _ in range(900):
            if os.path.exists(path):
                break
            yield 1
        log("%s: %s" % (name, "written" if os.path.exists(path) else "NOT WRITTEN"))
        LES.editor_request_end_play()
        yield 60


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
