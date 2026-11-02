"""Story greybox maps: chapters 14-40 (new) and the story overlay for the reworked chapter maps 2, 4-10 and 12.

Owner request 2026-10-11 ("make all 14-40 chapters along with the prior chapter rework"). The story itself is in
docs/Story (StoryBible.md, Chapters/ChXX.md); Tools/story_data.json is the data this module reads (generated from the same
source: themes, markers, bosses and the eight episode beats of every chapter). Everything is basic engine shapes and the
greybox palette; no final art.

Placeholder colours (every map):
    RED    enemy groups, mini-bosses and the chapter boss      MARK_RED
    BLUE   NPCs and dialogue                                   MARK_BLUE
    YELLOW pickups and archive items                           MARK_YELLOW
    GREEN  objectives, checkpoints and the level end           MARK_GREEN
    PURPLE cutscenes and story beats (the eight episodes)      MARK_PURPLE
Each placeholder has a TextRender label, and cutscenes / episodes / the level end have a TriggerBox (tags Beat_*, LevelEnd).
Four sections per map; section 4 is the boss section (MAIN section 7, docs/chapter-maps.md).

Chapters 1, 3, 11 and 13 are LOCKED (owner): this module never builds or edits them.
"""

import json
import math
import os
import random

import unreal

from greybox_common import MAPS, GreyboxMap

HERE = os.path.dirname(os.path.abspath(__file__))
LOCKED = {1, 3, 11, 13}
KIND = {
    "red": ("NeonRed", "cylinder", (255, 80, 80)),
    "blue": ("MarkBlue", "cylinder", (90, 150, 255)),
    "yellow": ("NeonYellow", "sphere", (255, 230, 60)),
    "green": ("NeonGreen", "cube", (80, 255, 120)),
    "purple": ("NeonPurple", "sphere", (200, 110, 255)),
}
SECTION_R = 4500.0


def load():
    with open(os.path.join(HERE, "story_data.json")) as handle:
        return {int(k): v for k, v in json.load(handle).items()}


# ------------------------------------------------------------------ helpers
def _trigger(m, centre, size, label, tag, folder):
    actor = m.spawn(unreal.TriggerBox, centre, label=label, folder=folder, tags=["Trigger", tag])
    _, extent = actor.get_actor_bounds(False)
    base = (max(extent.x * 2, 1.0), max(extent.y * 2, 1.0), max(extent.z * 2, 1.0))
    actor.set_actor_scale3d(unreal.Vector(size[0] / base[0], size[1] / base[1], size[2] / base[2]))
    return actor


def _marker(m, kind, text, pos, folder, scale=1.0, trigger=None, size=1.0):
    mat, shape, colour = KIND[kind]
    h = 180.0 * scale
    m.block((pos[0], pos[1], pos[2] + h / 2), (110 * scale, 110 * scale, h), "MARK_" + kind.upper(), folder, mat, shape=shape,
            collision=False, tags=["Placeholder", kind.upper()])
    m.label(text, (pos[0], pos[1], pos[2] + h + 150 * scale), folder, size=130.0 * size, in_game=True, colour=colour)
    if trigger:
        _trigger(m, (pos[0], pos[1], pos[2] + 150), (700, 700, 400), "TRIGGER_" + trigger, trigger, folder)


def _az(centre, r, az, z=None):
    return (centre[0] + r * math.cos(math.radians(az)), centre[1] + r * math.sin(math.radians(az)),
            centre[2] if z is None else z)


def _bearing(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def _short(text, n=70):
    text = text.replace("\n", " ")
    return text if len(text) <= n else text[: n - 1] + "..."


# ------------------------------------------------------------------ story markers (new maps and reworked maps)
def story_markers(m, n, spec, sections, folder="Story", start=None, log=None):
    """`sections` = [(index 1-4, centre, radius, boss)]. Places every placeholder, the episode beats and the level end."""
    ep = spec["episodes"]
    centres = [s[1] for s in sections]
    r_of = {s[0]: s[2] for s in sections}
    npcs, picks, cuts, objs = list(spec["npcs"]), list(spec["pickups"]), list(spec["cuts"]), list(spec["objs"])
    for idx, centre, radius, boss in sections:
        i = idx - 1
        prev_c = centres[i - 1] if i > 0 else (centre[0] - 1000, centre[1], centre[2])
        next_c = centres[i + 1] if i + 1 < len(centres) else (centre[0] + 1000, centre[1], centre[2])
        entry_az = _bearing(centre, prev_c)
        exit_az = _bearing(centre, next_c)
        sf = "%s/Section%d" % (folder, idx)
        # RED: enemy groups (campaign mix, 10-30 a section) and the mini-boss or boss
        total = 12 + 3 * i + (n - 2) // 6
        mix = "basic %d, ranged %d, mage %d, brute %d, assassin %d" % (total * 0.4, total * 0.2, total * 0.1, total * 0.15, total * 0.15)
        for g in range(3):
            _marker(m, "red", "ENEMIES %s%d: %d (%s)" % ("ABC"[g], idx, total // 3, mix if g == 0 else "same mix"),
                    _az(centre, radius * 0.55, entry_az + 70 + g * 100), sf, size=0.8)
        if boss:
            for p in range(spec["phases"]):
                _marker(m, "red", "BOSS: %s - Phase %d" % (spec["boss"], p + 1), _az(centre, 450 + p * 450, exit_az + 90), sf,
                        scale=2.0, size=1.2)
        else:
            name = spec["mini"][i % len(spec["mini"])]
            _marker(m, "red", "MINI-BOSS: %s" % name, _az(centre, radius * 0.18, exit_az + 180), sf, scale=1.6, size=1.1)
        # GREEN: checkpoint at the entry; objectives
        _marker(m, "green", "CHECKPOINT: Section %d" % idx, _az(centre, radius * 0.85, entry_az), sf, size=0.9,
                trigger="Checkpoint_%d" % idx)
        # BLUE / YELLOW distributed over the sections
        for j, name in enumerate(npcs):
            if j % 4 == i:
                _marker(m, "blue", "NPC: %s (dialogue)" % name, _az(centre, radius * 0.3, entry_az + 40 + 25 * j), sf, size=0.9)
        for j, name in enumerate(picks):
            if (j + 1) % 4 == i:
                _marker(m, "yellow", "PICKUP: %s" % name, _az(centre, radius * 0.62, entry_az - 50 - 20 * j), sf, size=0.9)
    for j, name in enumerate(objs):
        sec = sections[min(len(sections) - 1, 1 + 2 * j)]
        _marker(m, "green", "OBJECTIVE: %s" % name, _az(sec[1], sec[2] * 0.4, 20 + 90 * j), folder + "/Objectives", size=1.0,
                trigger="Objective_%d" % (j + 1))
    for j, name in enumerate(cuts):
        sec = sections[min(len(sections) - 1, 1 + j)]
        _marker(m, "purple", "CUTSCENE: %s" % name, _az(sec[1], 600 + 300 * j, 200 + 60 * j), folder + "/Cutscenes", scale=1.4,
                trigger="Beat_Cutscene_%d" % (j + 1), size=1.1)
    # the eight episodes: purple beats along the critical path (E4 = midpoint swerve, E7 = low point, E8 = boss + stinger)
    where = [(1, 0.8, 180), (1, 0.15, 90), (2, 0.8, 180), (2, 0.15, 90), (3, 0.8, 180), (3, 0.2, 90), (3, 0.6, 0), (4, 0.2, 90)]
    for e, (sec_idx, f, az_off) in enumerate(where):
        sec = sections[sec_idx - 1]
        centre, radius = sec[1], sec[2]
        base = _bearing(centre, sections[sec_idx - 2][1]) if sec_idx > 1 else 180.0
        tag = "SWERVE" if e == 3 else ("LOW POINT" if e == 6 else ("BOSS + STINGER" if e == 7 else "BEAT"))
        _marker(m, "purple", "EP%d %s: %s" % (e + 1, tag, _short(ep[e], 64)), _az(centre, radius * f, base + az_off - 90),
                folder + "/Episodes", scale=0.9, trigger="Beat_Episode_%d" % (e + 1), size=0.9)
    # level end: trigger at the far edge of the boss section
    last = sections[-1]
    prev_last = sections[-2][1] if len(sections) > 1 else (last[1][0] - 1000, last[1][1], last[1][2])
    out_az = _bearing(prev_last, last[1])
    end = _az(last[1], last[2] * 0.9, out_az)
    _marker(m, "green", "LEVEL END (stinger: %s)" % _short(ep[7].split("Stinger:")[-1].strip(), 48), end, folder + "/Gameplay", scale=1.5,
            trigger="LevelEnd")
    if start is not None:
        m.spawn(unreal.PlayerStart, (start[0], start[1], start[2] + 120), yaw=0.0, label="PlayerStart", folder="Gameplay")


# ------------------------------------------------------------------ props (primitives only)
def _p_crates(m, x, y, z, rng, f, s):
    for k in range(rng.randint(2, 5)):
        m.block((x + rng.uniform(-90, 90), y + rng.uniform(-90, 90), z + 60 + 120 * (k // 3)), (120, 120, 120), "Crate", f, s["wall"],
                yaw=rng.uniform(0, 90))


def _p_pillar(m, x, y, z, rng, f, s):
    m.block((x, y, z + 300), (120, 120, 600), "Pillar", f, s["wall"], shape="cylinder")
    m.block((x, y, z + 620), (180, 180, 40), "PillarCap", f, s["accent"])


def _p_desk(m, x, y, z, rng, f, s):
    yaw = rng.choice([0, 90])
    m.block((x, y, z + 75), (180, 80, 8), "DeskTop", f, s["wall"], yaw=yaw)
    for dx in (-80, 80):
        m.block((x + (dx if yaw == 0 else 0), y + (0 if yaw == 0 else dx), z + 37), (10, 70, 74), "DeskLeg", f, "Dark", yaw=yaw)
    m.block((x, y - 55 if yaw == 0 else y, z + 90), (40, 4, 28), "Screen", f, s["accent"], collision=False)


def _p_shelf(m, x, y, z, rng, f, s):
    yaw = rng.choice([0, 90])
    m.block((x, y, z + 110), (240, 50, 220), "Shelf", f, "Wood", yaw=yaw)
    for k in range(3):
        m.block((x, y, z + 40 + k * 70), (230, 54, 6), "ShelfBoard", f, s["accent"], yaw=yaw, collision=False)


def _p_lamp(m, x, y, z, rng, f, s):
    m.block((x, y, z + 220), (14, 14, 440), "LampPost", f, "Metal", shape="cylinder")
    m.block((x, y, z + 450), (60, 60, 30), "LampHead", f, s["accent"], shape="sphere", collision=False)


def _p_tent(m, x, y, z, rng, f, s):
    m.block((x, y, z + 130), (300, 300, 260), "Tent", f, s["accent"], shape="cone")


def _p_cage(m, x, y, z, rng, f, s):
    m.block((x, y, z + 100), (200, 200, 6), "CageFloor", f, "Metal")
    for dx in (-100, 100):
        for dy in (-100, 100):
            m.block((x + dx, y + dy, z + 110), (8, 8, 220), "CageBar", f, "Metal")
    m.block((x, y, z + 215), (210, 210, 8), "CageTop", f, "Metal")


def _p_rack(m, x, y, z, rng, f, s):
    m.block((x, y, z + 130), (80, 70, 260), "Rack", f, "Dark", yaw=rng.choice([0, 90]))
    for k in range(5):
        m.block((x, y, z + 40 + k * 45), (60, 74, 6), "RackLight", f, s["accent"], yaw=0, collision=False)


def _p_tree(m, x, y, z, rng, f, s):
    m.block((x, y, z + 150), (40, 40, 300), "Trunk", f, "Trunk", shape="cylinder")
    m.block((x, y, z + 380), (260, 260, 200), "Canopy", f, "Tree", shape="sphere")


def _p_statue(m, x, y, z, rng, f, s):
    m.block((x, y, z + 40), (140, 140, 80), "Plinth", f, "Stone")
    m.block((x, y, z + 190), (70, 50, 220), "StatueBody", f, "Stone")
    m.block((x, y, z + 340), (60, 60, 60), "StatueHead", f, "Stone", shape="sphere")


def _p_gear(m, x, y, z, rng, f, s):
    m.block((x, y, z + 200), (400, 400, 40), "Gear", f, "Metal", shape="cylinder", pitch=90, yaw=rng.uniform(0, 180))
    m.block((x, y, z + 200), (60, 60, 60), "GearHub", f, s["accent"], shape="cylinder", pitch=90, yaw=0, collision=False)


def _p_banner(m, x, y, z, rng, f, s):
    m.block((x, y, z + 250), (16, 160, 500), "BannerPole", f, "Metal", yaw=rng.choice([0, 90]))
    m.block((x, y, z + 300), (10, 140, 300), "Banner", f, s["accent"], yaw=0, collision=False)


def _p_tank(m, x, y, z, rng, f, s):
    m.block((x, y, z + 160), (180, 180, 320), "Tank", f, "Metal", shape="cylinder")
    m.block((x, y, z + 330), (120, 120, 30), "TankLid", f, s["accent"], shape="cylinder")


def _p_arch(m, x, y, z, rng, f, s):
    yaw = rng.choice([0, 90])
    for d in (-160, 160):
        m.block((x + (d if yaw == 0 else 0), y + (0 if yaw == 0 else d), z + 250), (60, 60, 500), "ArchPost", f, s["wall"])
    m.block((x, y, z + 530), (400 if yaw == 0 else 60, 60 if yaw == 0 else 400, 60), "ArchLintel", f, s["accent"])


def _p_bench(m, x, y, z, rng, f, s):
    yaw = rng.choice([0, 90])
    m.block((x, y, z + 45), (200, 50, 10), "Bench", f, "Wood", yaw=yaw)
    for d in (-80, 80):
        m.block((x + (d if yaw == 0 else 0), y + (0 if yaw == 0 else d), z + 22), (10, 40, 44), "BenchLeg", f, "Dark", yaw=yaw)


def _p_clock(m, x, y, z, rng, f, s):
    m.block((x, y, z + 260), (240, 240, 20), "ClockFace", f, "Marking", shape="cylinder", pitch=90, yaw=rng.uniform(0, 180))
    m.block((x, y, z + 130), (30, 30, 260), "ClockStand", f, "Metal", shape="cylinder")


def _p_drone(m, x, y, z, rng, f, s):
    m.block((x, y, z + 300 + rng.uniform(0, 200)), (70, 70, 70), "Drone", f, s["accent"], shape="sphere", collision=False)


def _p_page(m, x, y, z, rng, f, s):
    for k in range(rng.randint(3, 6)):
        m.block((x + rng.uniform(-80, 80), y + rng.uniform(-80, 80), z + 4 + k * 2), (40, 30, 1), "Page", f, "Marking",
                yaw=rng.uniform(0, 360), collision=False)


def _p_bed(m, x, y, z, rng, f, s):
    m.block((x, y, z + 35), (200, 90, 40), "Bed", f, "Grey", yaw=rng.choice([0, 90]))
    m.block((x, y, z + 62), (190, 80, 12), "Mattress", f, s["accent"], collision=False)


def _p_coffin(m, x, y, z, rng, f, s):
    m.block((x, y, z + 40), (190, 70, 80), "Coffin", f, "Wood", yaw=rng.choice([0, 90, 45]))


def _p_press(m, x, y, z, rng, f, s):
    m.block((x, y, z + 200), (260, 260, 60), "PressHead", f, "Metal")
    for d in (-110, 110):
        m.block((x + d, y, z + 100), (30, 30, 200), "PressColumn", f, "Dark")


def _p_table(m, x, y, z, rng, f, s):
    m.block((x, y, z + 70), (200, 200, 12), "Table", f, "Wood", shape="cylinder")
    m.block((x, y, z + 35), (30, 30, 70), "TableLeg", f, "Dark", shape="cylinder")
    for k in range(4):
        m.block((x + 130 * math.cos(k * 1.57), y + 130 * math.sin(k * 1.57), z + 35), (40, 40, 70), "Stool", f, s["accent"], shape="cylinder")


PROPS = {
    "city": [_p_crates, _p_lamp, _p_bench, _p_arch, _p_tank, _p_banner, _p_drone],
    "office": [_p_desk, _p_shelf, _p_crates, _p_lamp, _p_rack, _p_clock],
    "court": [_p_bench, _p_desk, _p_shelf, _p_banner, _p_pillar, _p_statue],
    "lab": [_p_tank, _p_rack, _p_cage, _p_tree, _p_desk, _p_crates],
    "carnival": [_p_tent, _p_banner, _p_table, _p_lamp, _p_statue, _p_crates],
    "casino": [_p_table, _p_banner, _p_lamp, _p_bench, _p_statue, _p_drone],
    "library": [_p_shelf, _p_page, _p_desk, _p_lamp, _p_clock, _p_pillar],
    "cathedral": [_p_gear, _p_clock, _p_pillar, _p_banner, _p_statue, _p_bench],
    "range": [_p_crates, _p_drone, _p_banner, _p_rack, _p_tank, _p_lamp],
    "tunnel": [_p_crates, _p_lamp, _p_tank, _p_rack, _p_drone, _p_banner],
    "vault": [_p_pillar, _p_coffin, _p_statue, _p_page, _p_lamp, _p_bench],
    "gallery": [_p_pillar, _p_statue, _p_page, _p_clock, _p_arch, _p_drone],
    "war": [_p_crates, _p_tank, _p_banner, _p_drone, _p_cage, _p_lamp],
    "village": [_p_tree, _p_lamp, _p_bench, _p_arch, _p_banner, _p_tent],
    "factory": [_p_press, _p_gear, _p_rack, _p_tank, _p_crates, _p_drone],
    "field": [_p_tree, _p_tank, _p_drone, _p_rack, _p_banner, _p_lamp],
    "tower": [_p_desk, _p_rack, _p_drone, _p_banner, _p_lamp, _p_crates],
}


# ------------------------------------------------------------------ landmarks (boss section)
def _l_tower(m, c, f, s):
    for k in range(6):
        m.block((c[0], c[1], c[2] + 500 + k * 1000), (900 - k * 110, 900 - k * 110, 1000), "Tower", f, s["wall"], shape="cylinder")
    m.block((c[0], c[1], c[2] + 6200), (1100, 1100, 120), "TowerRing", f, s["accent"], shape="cylinder", collision=False)


def _l_dome(m, c, f, s):
    m.dome((c[0], c[1], c[2]), 1500.0, f, n_az=16, n_el=5, thickness=80.0, material=s["wall"], gap=lambda az, band: 150 < az < 210 and band < 2)


def _l_machine(m, c, f, s):
    for k in range(5):
        m.block((c[0] + (k - 2) * 420, c[1], c[2] + 500), (260, 260, 1000), "MachineCol", f, "Metal", shape="cylinder")
    m.block((c[0], c[1], c[2] + 1100), (2000, 400, 200), "MachineBeam", f, s["accent"], collision=False)
    m.block((c[0], c[1], c[2] + 250), (2200, 500, 500), "MachineBase", f, "Dark")


def _l_vault(m, c, f, s):
    m.block((c[0], c[1], c[2] + 800), (1600, 120, 1600), "VaultDoor", f, "Metal", shape="cylinder", pitch=90)
    m.block((c[0], c[1], c[2] + 800), (400, 140, 400), "VaultWheel", f, s["accent"], shape="cylinder", pitch=90, collision=False)
    m.box(c[0] - 1000, c[1] - 90, c[2], c[0] + 1000, c[1] + 90, c[2] + 1800, "VaultFrame", f, s["wall"])


def _l_arch(m, c, f, s):
    for d in (-1200, 1200):
        m.block((c[0], c[1] + d, c[2] + 1100), (260, 260, 2200), "GreatArchPost", f, s["wall"])
    m.block((c[0], c[1], c[2] + 2300), (260, 2900, 260), "GreatArchLintel", f, s["accent"])


def _l_stack(m, c, f, s):
    for k in range(6):
        m.block((c[0] + (k % 3 - 1) * 500, c[1] + (k // 3 - 0.5) * 700, c[2] + 700), (360, 480, 1400), "StackRack", f, "Dark")
        m.block((c[0] + (k % 3 - 1) * 500, c[1] + (k // 3 - 0.5) * 700, c[2] + 1450), (330, 440, 40), "StackLight", f, s["accent"], collision=False)


def _l_bench(m, c, f, s):
    m.block((c[0], c[1], c[2] + 250), (900, 300, 500), "JudgeBench", f, "Wood")
    for k in range(4):
        m.block((c[0] - 600, c[1] + (k - 1.5) * 380, c[2] + 40 + k * 0), (200, 360, 80), "BenchStep", f, s["wall"])
    m.block((c[0], c[1], c[2] + 620), (60, 60, 240), "Gavel", f, s["accent"], collision=False)


def _l_table(m, c, f, s):
    m.block((c[0], c[1], c[2] + 350), (1800, 1800, 90), "BigTable", f, "Wood", shape="cylinder")
    m.block((c[0], c[1], c[2] + 170), (300, 300, 340), "BigTableLeg", f, "Dark", shape="cylinder")
    for k in range(8):
        a = k * 45
        m.block((c[0] + 700 * math.cos(math.radians(a)), c[1] + 700 * math.sin(math.radians(a)), c[2] + 420), (140, 140, 20), "Chip", f,
                s["accent"], shape="cylinder", collision=False)


def _l_clock(m, c, f, s):
    m.block((c[0], c[1], c[2] + 1800), (2800, 2800, 160), "GreatClock", f, "Marking", shape="cylinder", pitch=90)
    m.block((c[0], c[1] - 100, c[2] + 2400), (80, 40, 900), "MinuteHand", f, s["accent"], collision=False)
    m.block((c[0], c[1], c[2] + 800), (200, 200, 1600), "ClockMast", f, "Metal", shape="cylinder")


def _l_bigtop(m, c, f, s):
    m.block((c[0], c[1], c[2] + 700), (3200, 3200, 1400), "BigTop", f, s["accent"], shape="cone", collision=False)
    for k in range(6):
        a = k * 60
        m.block((c[0] + 1500 * math.cos(math.radians(a)), c[1] + 1500 * math.sin(math.radians(a)), c[2] + 500), (120, 120, 1000), "TentPole", f,
                "Metal", shape="cylinder")


def _l_dais(m, c, f, s):
    for k in range(3):
        m.block((c[0], c[1], c[2] + 60 + k * 120), (2600 - k * 700, 2600 - k * 700, 120), "DaisTier", f, s["wall"], shape="cylinder")
    for k in range(8):
        a = k * 45
        m.block((c[0] + 1250 * math.cos(math.radians(a)), c[1] + 1250 * math.sin(math.radians(a)), c[2] + 700), (120, 120, 1400),
                "DaisPillar", f, s["wall"], shape="cylinder")


def _l_stage(m, c, f, s):
    m.block((c[0], c[1], c[2] + 100), (2400, 1400, 200), "Stage", f, "Wood")
    for d in (-1300, 1300):
        m.block((c[0], c[1] + d, c[2] + 800), (80, 80, 1600), "CurtainPost", f, "Metal")
    m.block((c[0] - 700, c[1], c[2] + 900), (60, 2600, 1400), "Curtain", f, s["accent"], collision=False)


def _l_statue(m, c, f, s):
    m.block((c[0], c[1], c[2] + 100), (900, 900, 200), "GiantPlinth", f, "Stone")
    m.block((c[0], c[1], c[2] + 1000), (500, 360, 1600), "GiantBody", f, "Stone")
    m.block((c[0], c[1], c[2] + 2000), (420, 420, 420), "GiantHead", f, "Stone", shape="sphere")
    for d in (-1, 1):
        m.block((c[0], c[1] + d * 380, c[2] + 1200), (160, 160, 1100), "GiantArm", f, "Stone")


def _l_colossus(m, c, f, s):
    _l_statue(m, c, f, s)
    for d in (-1, 1):
        m.block((c[0], c[1] + d * 600, c[2] + 1600), (260, 260, 260), "ColossusShoulderGun", f, s["accent"], shape="sphere", collision=False)


def _l_train(m, c, f, s):
    for k in range(4):
        m.block((c[0] + (k - 1.5) * 1100, c[1], c[2] + 300), (1000, 340, 460), "TrainCar", f, "Metal")
        m.block((c[0] + (k - 1.5) * 1100, c[1], c[2] + 560), (900, 300, 40), "TrainRoof", f, s["accent"], collision=False)


def _l_monolith(m, c, f, s):
    m.block((c[0], c[1], c[2] + 2500), (600, 300, 5000), "Monolith", f, "Monolith")
    for k in range(20):
        m.block((c[0] + 151, c[1] + ((k % 4) - 1.5) * 90, c[2] + 600 + (k // 4) * 800), (4, 40, 220), "RedGlyph", f, "RedGlyph", collision=False)


LANDMARKS = {
    "tower": _l_tower, "dome": _l_dome, "machine": _l_machine, "vault": _l_vault, "arch": _l_arch, "stack": _l_stack, "bench": _l_bench,
    "table": _l_table, "clock": _l_clock, "bigtop": _l_bigtop, "dais": _l_dais, "stage": _l_stage, "statue": _l_statue,
    "colossus": _l_colossus, "train": _l_train, "monolith": _l_monolith,
}


# ------------------------------------------------------------------ layout and map
def _centres(lay, spacing=15500.0):
    pts = []
    for i in range(4):
        if lay == "line":
            pts.append((-1.5 * spacing + i * spacing, 0.0, 0.0))
        elif lay == "zig":
            pts.append((-1.5 * spacing + i * spacing, 5500.0 * (1 if i % 2 else -1), 0.0))
        elif lay == "ring":
            a = math.radians(225 + i * 90)
            pts.append((11500 * math.cos(a), 11500 * math.sin(a), 0.0))
        elif lay == "up":
            a = math.radians(180 + i * 90)
            pts.append((10500 * math.cos(a), 10500 * math.sin(a), i * 3200.0))
        else:   # spiral
            a = math.radians(200 + i * 80)
            pts.append(((9000 + i * 1800) * math.cos(a), (9000 + i * 1800) * math.sin(a), i * 1500.0))
    return pts


def _route(m, a, b, folder, material, spec, rng):
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    ground = math.hypot(dx, dy)
    ux, uy = dx / ground, dy / ground
    start = (a[0] + ux * SECTION_R, a[1] + uy * SECTION_R, a[2])
    end = (b[0] - ux * SECTION_R, b[1] - uy * SECTION_R, b[2])
    length = math.sqrt((end[0] - start[0]) ** 2 + (end[1] - start[1]) ** 2 + (end[2] - start[2]) ** 2)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(end[2] - start[2], math.hypot(end[0] - start[0], end[1] - start[1])))
    mid = ((start[0] + end[0]) / 2, (start[1] + end[1]) / 2, (start[2] + end[2]) / 2 - 25)
    m.block(mid, (length, 700, 50), "Route", folder, material, yaw=yaw, pitch=pitch)
    for side in (-1, 1):   # rails
        o = (-uy * 350 * side, ux * 350 * side)
        m.railing((start[0] + o[0], start[1] + o[1], start[2]), (end[0] + o[0], end[1] + o[1], end[2]), folder, height=100, spacing=500,
                  rail=False, material="Metal")
    # floor markers every 7 m in the chapter's accent
    steps = int(length // 700)
    for k in range(1, steps):
        t = k / float(steps)
        m.block((start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t, start[2] + (end[2] - start[2]) * t + 3), (120, 40, 4),
                "RouteMark", folder, spec["accent"], yaw=yaw, collision=False)
    if spec["movers"]:
        for k in (1, 2):
            t = k / 3.0
            p = (start[0] + (end[0] - start[0]) * t, start[1] + (end[1] - start[1]) * t, start[2] + (end[2] - start[2]) * t + 40)
            m.mover(p, (500, 500, 60), "MovingPlatform", folder, "Climb", offset=(ux * 1200, uy * 1200, 0), period=5.0, phase=k * 0.3)
    return start, end


def _section_dress(m, rng, i, c, nxt, prv, spec, folder, boss):
    f = "%s/Section%d" % (folder, i + 1)
    H = 1500.0 if spec["indoor"] else 500.0
    exit_az = _bearing(c, nxt) if nxt else None
    entry_az = _bearing(c, prv) if prv else 180.0
    m.section(i + 1, c, f, radius=SECTION_R, boss=boss, floor=spec["floor"], gate_yaw=exit_az)
    gates = [entry_az] + ([exit_az] if exit_az is not None else [])

    def near_gate(az, tol=22.0):
        return any(abs(((az - g) + 180) % 360 - 180) < tol for g in gates)

    # perimeter walls (gaps at entry and exit), pillars, cover
    for k in range(20):
        az = k * 18.0
        if near_gate(az):
            continue
        p = _az(c, SECTION_R + 150, az, c[2])
        m.block((p[0], p[1], c[2] + H / 2), (120, 2 * math.pi * SECTION_R / 20 * 0.92, H), "Wall", f, spec["wall"], yaw=az)
        if k % 3 == 0:
            m.block((p[0], p[1], c[2] + H * 0.8), (140, 400, 40), "WallStrip", f, spec["accent"], yaw=az, collision=False)
    for k in range(6):
        p = _az(c, SECTION_R * 0.72, 30 + k * 60, c[2])
        m.block((p[0], p[1], c[2] + H / 2), (200, 200, H), "Pillar", f, spec["wall"], shape="cylinder")
    for k in range(8):
        az = rng.uniform(0, 360)
        p = _az(c, rng.uniform(1500, SECTION_R * 0.8), az, c[2])
        m.block((p[0], p[1], c[2] + 55), (220, 100, 110), "Cover", f, "Concrete", yaw=rng.uniform(0, 180))
    # props: the fight centre stays clear
    pool = PROPS[spec["theme"]]
    for k in range(34):
        az = rng.uniform(0, 360)
        if near_gate(az, 15.0):
            continue
        r = rng.uniform(1100.0, SECTION_R * 0.88)
        p = _az(c, r, az, c[2])
        rng.choice(pool)(m, p[0], p[1], c[2], rng, f, spec)
    _detail(m, rng, c, spec, f)
    # traversal: a raised platform with jump-up steps on one side and a climb wall on the other (orange / teal)
    az = exit_az + 90 if exit_az is not None else entry_az + 90
    plat = _az(c, SECTION_R * 0.62, az, c[2])
    m.box(plat[0] - 450, plat[1] - 450, c[2] + 300, plat[0] + 450, plat[1] + 450, c[2] + 400, "RaisedPlatform", f, "Concrete")
    m.jump_steps((plat[0] - 450 - 1300, plat[1], c[2]), 0.0, 4, f, rise=80.0, run=300.0, width=420.0)
    m.climb_wall((plat[0] + 450, plat[1], c[2]), 0.0, 700.0, 400.0, f)
    # lights
    for k in range(3):
        p = _az(c, SECTION_R * 0.5, k * 120 + 40, c[2] + H * 0.8)
        m.point_light(p, "Lighting", intensity=35000.0, radius=6000.0)
    if boss:
        lm = LANDMARKS[spec["land"]]
        base = _az(c, SECTION_R * 0.72, entry_az + 180, c[2])
        lm(m, base, f, spec)


def _detail(m, rng, c, spec, folder):
    """Floor tiles and debris so a section reads as a place (owner 2026-10-07: 8,000+ pieces a chapter map)."""
    f = folder + "/Detail"
    tile_mats = [spec["floor"], "Grey", "Concrete", spec["floor"]]
    step = 220.0
    n = int(SECTION_R * 0.96 // step)
    for gx in range(-n, n + 1):
        for gy in range(-n, n + 1):
            x, y = gx * step, gy * step
            if x * x + y * y > (SECTION_R * 0.96) ** 2:
                continue
            m.block((c[0] + x, c[1] + y, c[2] + 3), (step - 14, step - 14, 6), "Tile", f, tile_mats[(gx * 7 + gy * 3) % 4], collision=False)
    mats = [spec["wall"], "Rubble", "Metal", "Dark", spec["accent"]]
    for _ in range(700):
        az, r = rng.uniform(0, 360), SECTION_R * math.sqrt(rng.uniform(0.02, 0.95))
        p = _az(c, r, az, c[2])
        s = rng.uniform(15, 70)
        m.block((p[0], p[1], c[2] + s / 2), (s, s * rng.uniform(0.5, 1.5), s), "Debris", f, rng.choice(mats), yaw=rng.uniform(0, 360),
                collision=False)


def _backup(log, path):
    name = path.rsplit("/", 1)[-1]
    dst = "/Game/Maps/_Backup/" + name + "_old"
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(path) and not eal.does_asset_exist(dst):
        eal.duplicate_asset(path, dst)
        log("backup: %s -> %s" % (path, dst))


def build_map(n, spec, materials, log):
    path = MAPS + "/L_Ch%02d_%s" % (n, spec["short"])
    _backup(log, path)
    m = GreyboxMap(path, materials, log)
    rng = random.Random(7000 + n)
    folder = "Ch%02d" % n
    sun = spec["sun"]
    m.lighting(sun=3.0, sun_colour=tuple(sun), pitch=-35, fog=spec["fog"], fog_colour=(0.35, 0.35, 0.45), volumetric=spec["indoor"])
    pts = _centres(spec["lay"])
    half = 36000.0
    water = bool(spec["water"])
    if spec["theme"] != "gallery":   # gallery maps float over the void
        m.box(-half, -half, (-1500 if water else -200) - 200, half, half, -1500 if water else -200, "Ground", "Ground", spec["floor"])
    if water:
        m.water(-half, -half, -1500, half, half, -150, "Sea", "Water")
    sections = []
    for i, c in enumerate(pts):
        nxt = pts[i + 1] if i + 1 < 4 else None
        prv = pts[i - 1] if i > 0 else None
        _section_dress(m, rng, i, c, nxt, prv, spec, folder, boss=i == 3)
        if water:   # a way back up out of the water
            m.climb_wall((c[0] + SECTION_R + 400, c[1], -1500.0), 0.0, 800.0, 1500.0, "%s/Section%d" % (folder, i + 1))
        sections.append((i + 1, c, SECTION_R, i == 3))
    for i in range(3):
        _route(m, pts[i], pts[i + 1], folder + "/Routes", spec["floor"], spec, rng)
    first = pts[0]
    story_markers(m, n, spec, sections, folder="Story", start=(first[0] - SECTION_R * 0.8, first[1], first[2]), log=log)
    m.world_settings(unreal.GameModeBase.static_class())
    total = m.save(anime=True)
    log("Ch %02d %s: %d actors, boss %s" % (n, spec["short"], total, spec["boss"]))
    return total


def overlay(m, n, log):
    """Story markers on a reworked chapter map (2, 4-10, 12) built by greybox_chapters.py, on its own sections."""
    spec = load().get(n)
    if not spec or not m.sections:
        log("story overlay skipped for %d" % n)
        return
    sections = sorted([(s[0], s[1], s[2], s[3]) for s in m.sections])
    # the existing maps have four sections; section 4 is the boss section
    story_markers(m, n, spec, sections, folder="Story", start=None, log=log)
    log("story overlay: Ch %02d %s (boss %s)" % (n, spec["short"], spec["boss"]))


def build(materials, log, only=None):
    """Chapters 14-40: the bespoke levels (greybox_levels.py / greybox_levels2.py); the template builder is the fallback."""
    try:
        import world_plans
    except ImportError as error:
        log("world plans unavailable (%s); using the template builder" % error)
    else:
        return world_plans.build(materials, log, only)
    data = load()
    built = {}
    for n in sorted(data):
        if n < 14 or n in LOCKED:
            continue
        if only and n not in only:
            continue
        built[n] = build_map(n, data[n], materials, log)
    return built
