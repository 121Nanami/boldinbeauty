"""Checks the BB character shader in the editor (owner request 2026-10-15; assets from Tools/build_character_shader.py).

Opens /Game/BIBCharacter/Maps/L_BIBCharacterShaderTest (before row = M_BIBTint, today's look; after row = the new instances on
the operators' own shapes, plus a reference operator in each row) and:
  1. measures the shader cost of the master, the outline and every instance (MaterialEditingLibrary statistics for the editor's
     shader platform: vertex / pixel instructions, samplers, texture samples) against the budget below;
  2. takes before / after screenshots in four lighting setups - key light, backlight, the dark horror scene, a subtle dream-core
     beat - by setting MPC_BIBCharacter and turning the scene's key light to match;
  3. switches to Material Quality Low (r.MaterialQualityLevel 0, this session only) for one more screenshot of the fallback
     permutation, then restores High.
Changes nothing on disk except Saved/: the MPC values and the console variable live in this editor session only.

    UnrealEditor <BoldInBeautyDemo.uproject> -ExecCmds="py Tools/verify_character_shader.py"
    BIB_VERIFY_KEEP_OPEN=1 keeps the editor open afterwards.

Writes Saved/VerifyCharacterShader.log (last line RESULT PASS|FAIL) and Saved/CharacterShader/*.png.
"""

import glob
import os
import shutil
import time
import traceback

import unreal

TEST_MAP = "/Game/BIBCharacter/Maps/L_BIBCharacterShaderTest"
MPC_PATH = "/Game/BIBCharacter/MPC_BIBCharacter"
SAVED = unreal.Paths.project_saved_dir()
OUT = os.path.join(SAVED, "CharacterShader")
LOG = []
FAILS = []
lib = unreal.MaterialEditingLibrary

# Budget (PROPOSED, console / mobile-capable quality): pixel instructions and samplers on the editor's desktop platform.
BUDGET_FULL_PS = 250
BUDGET_FULL_SAMPLERS = 4
BUDGET_LOW_PS = 120
BUDGET_LOW_SAMPLERS = 1
BUDGET_OUTLINE_PS = 60
ASSETS = [
    ("M_BIBCharacter", "/Game/BIBCharacter/Materials/M_BIBCharacter", "full"),
    ("M_BIBCharacterOutline", "/Game/BIBCharacter/Materials/M_BIBCharacterOutline", "outline"),
    ("MI_BIBChar_Skin", "/Game/BIBCharacter/Instances/MI_BIBChar_Skin", "full"),
    ("MI_BIBChar_Face", "/Game/BIBCharacter/Instances/MI_BIBChar_Face", "full"),
    ("MI_BIBChar_Hair", "/Game/BIBCharacter/Instances/MI_BIBChar_Hair", "full"),
    ("MI_BIBChar_Eye", "/Game/BIBCharacter/Instances/MI_BIBChar_Eye", "full"),
    ("MI_BIBChar_Cloth", "/Game/BIBCharacter/Instances/MI_BIBChar_Cloth", "full"),
    ("MI_BIBChar_Metal", "/Game/BIBCharacter/Instances/MI_BIBChar_Metal", "full"),
    ("MI_BIBChar_Emissive", "/Game/BIBCharacter/Instances/MI_BIBChar_Emissive", "full"),
    ("MI_BIBChar_Cloth_Low", "/Game/BIBCharacter/Instances/MI_BIBChar_Cloth_Low", "low"),
    ("MI_BIBChar_Outline", "/Game/BIBCharacter/Instances/MI_BIBChar_Outline", "outline"),
]
# name: (key light direction TO the light, key colour, ambient, dread, liminal, horror, uncanny, sun lux, sky intensity)
SETUPS = [
    ("key", (-0.55, -0.45, 0.7), (1.0, 0.97, 0.94), (0.66, 0.6, 0.86), 0.0, 0.0, 0.0, 0.0, 8.0, 1.0),
    ("backlight", (0.8, 0.15, 0.45), (1.0, 0.93, 0.88), (0.6, 0.56, 0.82), 0.0, 0.0, 0.0, 0.0, 8.0, 0.8),
    ("horror", (-0.2, 0.7, 0.25), (0.45, 0.38, 0.6), (0.22, 0.18, 0.32), 0.6, 0.3, 0.85, 0.8, 1.5, 0.3),
    ("dream_subtle", (-0.4, -0.6, 0.65), (1.0, 0.95, 1.0), (0.72, 0.66, 0.9), 0.15, 0.6, 0.0, 0.3, 6.0, 1.0),
]


def log(text):
    LOG.append(text)
    unreal.log("[verify_character_shader] " + text)


def check(ok, text):
    log(("OK   " if ok else "FAIL ") + text)
    if not ok:
        FAILS.append(text)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def tagged(tag):
    return [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if unreal.Name(tag) in a.get_editor_property("tags")]


def wait(seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        yield 0.1


def measure():
    for name, path, kind in ASSETS:
        asset = unreal.load_asset(path)
        if asset is None:
            check(False, "%s exists (run Tools/build_character_shader.sh)" % name)
            continue
        try:
            stats = lib.get_statistics(asset)
        except Exception as error:
            check(False, "%s statistics (%s)" % (name, error))
            continue
        ps = stats.num_pixel_shader_instructions
        vs = stats.num_vertex_shader_instructions
        samplers = stats.num_samplers
        log("cost: %-22s VS %4d  PS %4d  samplers %2d  PS texture samples %2d  interpolator scalars %2d" % (
            name, vs, ps, samplers, stats.num_pixel_texture_samples, stats.num_interpolator_scalars))
        check(ps > 0, "%s compiled (pixel shader has instructions)" % name)
        if kind == "full":
            check(ps <= BUDGET_FULL_PS and samplers <= BUDGET_FULL_SAMPLERS,
                  "%s within the full budget (PS <= %d, samplers <= %d)" % (name, BUDGET_FULL_PS, BUDGET_FULL_SAMPLERS))
        elif kind == "low":
            check(ps <= BUDGET_LOW_PS and samplers <= BUDGET_LOW_SAMPLERS,
                  "%s within the low-end budget (PS <= %d, samplers <= %d)" % (name, BUDGET_LOW_PS, BUDGET_LOW_SAMPLERS))
        else:
            check(ps <= BUDGET_OUTLINE_PS, "%s within the outline budget (PS <= %d)" % (name, BUDGET_OUTLINE_PS))


def set_setup(setup):
    name, direction, key, ambient, dread, liminal, horror, uncanny, lux, sky = setup
    mpc = unreal.load_asset(MPC_PATH)
    w = world()
    kml = unreal.KismetMaterialLibrary
    kml.set_scalar_parameter_value(w, mpc, "KeyOverride", 1.0)
    kml.set_vector_parameter_value(w, mpc, "KeyLightDir", unreal.LinearColor(direction[0], direction[1], direction[2], 0.0))
    kml.set_vector_parameter_value(w, mpc, "KeyLightColor", unreal.LinearColor(key[0], key[1], key[2], 1.0))
    kml.set_vector_parameter_value(w, mpc, "AmbientColor", unreal.LinearColor(ambient[0], ambient[1], ambient[2], 1.0))
    for param, value in (("Dread", dread), ("Liminal", liminal), ("Horror", horror), ("Uncanny", uncanny)):
        kml.set_scalar_parameter_value(w, mpc, param, value)
    # the real key light (lights the before row and the floor) points the same way
    for light in tagged("KeyLight"):
        rotation = unreal.MathLibrary.make_rot_from_x(unreal.Vector(-direction[0], -direction[1], -direction[2]))
        light.set_actor_rotation(rotation, False)
        light.light_component.set_intensity(lux)
    for sky_light in [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.SkyLight)]:
        sky_light.light_component.set_intensity(sky)
    log("setup %s: L=(%.2f, %.2f, %.2f) dread %.2f liminal %.2f horror %.2f uncanny %.2f" % (
        name, direction[0], direction[1], direction[2], dread, liminal, horror, uncanny))


def screenshot(name):
    cameras = tagged("Camera")
    if not cameras:
        check(False, "test camera found")
        return
    os.makedirs(OUT, exist_ok=True)
    folder = os.path.join(SAVED, "Screenshots")
    before = set(glob.glob(os.path.join(folder, "**", "*.png"), recursive=True))
    task = unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "BIBCharacterShader_" + name, camera=cameras[0])
    end = time.monotonic() + 60.0
    while time.monotonic() < end and not task.is_task_done():
        yield 0.2
    yield 1.0
    fresh = set(glob.glob(os.path.join(folder, "**", "*.png"), recursive=True)) - before
    matches = [p for p in fresh if "BIBCharacterShader_" + name in os.path.basename(p)] or sorted(fresh, key=os.path.getmtime)
    if matches:
        target = os.path.join(OUT, name + ".png")
        shutil.copy(matches[-1], target)
        check(True, "screenshot %s -> Saved/CharacterShader/%s.png" % (name, name))
    else:
        check(False, "screenshot %s written" % name)


def flow():
    yield from wait(5.0)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(TEST_MAP):
        check(False, "test map loads (run Tools/build_character_shader.sh)")
        return
    yield from wait(8.0)   # shaders compile on first open
    measure()
    for setup in SETUPS:
        set_setup(setup)
        yield from wait(2.0)
        yield from screenshot(setup[0])
    # the fallback permutation: Material Quality Low for this session only
    set_setup(SETUPS[0])
    unreal.SystemLibrary.execute_console_command(world(), "r.MaterialQualityLevel 0")
    yield from wait(20.0)
    yield from screenshot("key_quality_low")
    unreal.SystemLibrary.execute_console_command(world(), "r.MaterialQualityLevel 1")
    yield from wait(5.0)


class Runner:
    def __init__(self):
        self.gen = flow()
        self.wait = 0.0
        self.busy = False
        self.done = False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def finish(self):
        self.done = True
        unreal.unregister_slate_post_tick_callback(self.handle)
        log("RESULT %s %d problems" % ("PASS" if not FAILS else "FAIL", len(FAILS)))
        with open(os.path.join(SAVED, "VerifyCharacterShader.log"), "w") as handle:
            handle.write("\n".join(LOG) + "\n")
        if os.environ.get("BIB_VERIFY_KEEP_OPEN") != "1":
            unreal.SystemLibrary.quit_editor()

    def tick(self, delta_seconds):
        if self.busy or self.done:
            return
        self.busy = True
        try:
            if self.wait > 0.0:
                self.wait -= delta_seconds
                return
            self.wait = self.gen.send(None) or 0.0
        except StopIteration:
            self.finish()
        except Exception:
            log(traceback.format_exc())
            FAILS.append("exception")
            self.finish()
        finally:
            self.busy = False


Runner()
