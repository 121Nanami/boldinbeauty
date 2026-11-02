"""Chapter maps 2-13 (owner level ideas, FINAL, 2026-10-07; HANDOFF.md and docs/chapter-maps.md). Chapter 1 is L_NeoTokyo.

Every map is a greybox: the owner's idea is the brief, the layout is the assistant's reading (PROPOSED). Shared language:
    orange  = climbable (walls with ledges every 4 m, pipes, chains, vines)      tag Climbable
    teal    = a block you can jump up onto (80 cm rises; the demo jump clears ~90 cm)  tag JumpUp
    blue    = swimmable water (a water PhysicsVolume + surface)                  tag Swim
    white   = section edge ring, section gate (how the player moves between sections)  tags Section / SectionGate
    movers  = ABIBGreyboxMover: swinging / sliding platforms, waves, conveyors, presses   tag Mover
Each map has four combat sections (MAIN section 7: 2-5 sections a level, ~8 s centre to wall), the fourth is the boss
section, joined by routes that use the chapter's traversal. Chapter 11 has none: one long walk, no combat.
Every map gets the BB anime look (ABIBAnimeStyleVolume) except Chapter 11 (owner: no post-processing).
"""

import math
import os
import random

import unreal

import greybox_bundle
import greybox_detail
import greybox_detail2
import greybox_story
from greybox_common import MAPS, NEON, GreyboxMap, try_set

K1 = "/Game/Greybox/Kits/K1"
K2 = "/Game/Greybox/Kits/K2/Buildings"
HIGHRISE = ["HighRise/SM_K1_HighRise_%d" % i for i in range(1, 15)]
CORP = ["Corporation/SM_K1_Corporation_%d" % i for i in range(1, 11)]
COTTAGE = ["Cottage/SM_K1_Cottage_%d" % i for i in range(1, 21)]
CITYSCAPE = ["Cityscape/SM_K1_Cityscape_%d" % i for i in range(1, 5)]
FACTORY = ["Factory/SM_K1_Factory_%d" % i for i in range(1, 9)]

MAP_NAMES = {
    2: "L_Ch02_Ruins", 3: "L_Ch03_Docks", 4: "L_Ch04_HangingCity", 5: "L_Ch05_ArcTech", 6: "L_Ch06_MoonKingdom",
    7: "L_Ch07_TOMBS", 8: "L_Ch08_Intersection", 9: "L_Ch09_RemnantRemains", 10: "L_Ch10_Hallucinations",
    11: "L_Ch11_ReturnToNowhere", 12: "L_Ch12_Paradise", 13: "L_Ch13_SecretiveSoul",
}

# Owner texts, verbatim (HANDOFF.md). Do not correct spelling or the Morse.
CH9_SCREEN = ("Angel//V1 Success Line down Operator//V! Faillllllllllllllllll ...---... ////// "
              ".- -.--. . .-.. ... / .-- .. .-.. .-.. / -.- .. .-.. .-.. / ..- ... / .- .-.. .-..")
CH10_STORY = ["The angels bring mercy for all", "Not those stupid pure ones you call angels", "but TRUE ANGELS THE ONE",
              "The rest are all filth compared to its existence"]
CH13_VOICE = "They wree purity, Impure sin, Thats all"

MORSE = {"A": ".-", "B": "-...", "C": "-.-.", "D": "-..", "E": ".", "F": "..-.", "G": "--.", "H": "....", "I": "..", "J": ".---",
         "K": "-.-", "L": ".-..", "M": "--", "N": "-.", "O": "---", "P": ".--.", "Q": "--.-", "R": ".-.", "S": "...", "T": "-",
         "U": "..-", "V": "...-", "W": ".--", "X": "-..-", "Y": "-.--", "Z": "--.."}


def morse(text):
    return " / ".join(" ".join(MORSE[c] for c in word.upper() if c in MORSE) for word in text.split())


def build(materials, log, only=None):
    built = {}
    for n, fn in BUILDERS.items():
        if only and n not in only:
            continue
        if n in greybox_story.LOCKED and not os.environ.get("BIB_BUILD_LOCKED"):
            log("chapter %d is LOCKED (owner): not built, not touched" % n)   # Ch 3, 11, 13; set BIB_BUILD_LOCKED=1 to override
            continue
        greybox_story._backup(log, MAPS + "/" + MAP_NAMES[n])   # owner rule: back up a map before it is rebuilt
        m = GreyboxMap(MAPS + "/" + MAP_NAMES[n], materials, log)
        anime = fn(m, random.Random(2000 + n), log)
        greybox_detail.DETAIL[n](m, random.Random(3000 + n), log)   # owner 2026-10-07: 8,000-12,000 pieces a map
        greybox_detail2.chapter(m, n, log)   # owner 2026-10-07: more detail where the player fights and travels
        greybox_bundle.chapter(m, n, log)    # owner 2026-10-08: fill the maps with the Elite_Bundle assets
        greybox_story.overlay(m, n, log)     # owner 2026-10-11: story placeholders, episode beats and level end
        m.world_settings(unreal.GameModeBase.static_class())
        built[n] = m.save(anime=anime is not False)
    return built


def _start(m, pos, yaw=0.0):
    m.spawn(unreal.PlayerStart, (pos[0], pos[1], pos[2] + 120), yaw=yaw, label="PlayerStart", folder="Gameplay")


def _route(m, a, b, folder, material="Concrete", width=600.0, tags=None):
    """A flat walkway between two points (any slope)."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return m.block(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2 - 25), (length, width, 50), "Route", folder, material,
                   yaw=yaw, pitch=pitch, tags=tags)


def _ground(m, half, material, z=0.0, thickness=200.0, label="Ground"):
    m.box(-half, -half, z - thickness, half, half, z, label, "Ground", material)


def _house(m, x, y, z, w, d, h, yaw, folder, wall="Wood", roof="Roof", tags=None):
    """Simple pitched-roof house from basic shapes (Japanese countryside, shacks)."""
    a = math.radians(yaw)
    m.block((x, y, z + h / 2), (w, d, h), "House", folder, wall, yaw=yaw, tags=tags)
    for side in (-1, 1):
        ox, oy = -math.sin(a) * side * d * 0.27, math.cos(a) * side * d * 0.27
        m.block((x + ox, y + oy, z + h + d * 0.14), (w * 1.15, d * 0.62, 30), "Roof", folder, roof, yaw=yaw, roll=-side * 28)


# ============================================================================ 2. The Ruins (Outer World, on top of the arc)

def ch02_ruins(m, rng, log):
    """The surface above the Ark: the Outer World's building types (school, police, fire station, hospital, gym) in ruins.
    Start at the Ark's chute hatches (the task room's chutes reach the surface here)."""
    m.lighting(sun=3.0, sun_colour=(255, 225, 190), pitch=-25, fog=0.035, fog_colour=(0.45, 0.38, 0.3), volumetric=True)
    half = 30000
    _ground(m, half + 5000, "Rubble")
    f = "Start/ChuteHatches"
    m.box(-26000, -3000, 0, -20000, 3000, 40, "HatchPad", f, "Concrete")
    for k in range(16):   # same count as the Ark's task-room chutes
        x, y = -25000 + (k % 4) * 1500, -2200 + (k // 4) * 1500
        m.block((x, y, 80), (300, 300, 80), "SurfaceHatch", f, "Metal", shape="cylinder", tags=["SurfaceHatch"])
    _start(m, (-23000, 0, 40))
    sections = [((-12000, -12000, 0), "SCHOOL"), ((6000, -14000, 0), "POLICE + FIRE STATION"), ((-4000, 12000, 0), "HOSPITAL"),
                ((18000, 10000, 0), "GYM")]
    for n, (c, name) in enumerate(sections, 1):
        fs = "Section%d_%s" % (n, name.split()[0])
        m.section(n, c, fs, boss=n == 4, floor="Concrete", gate_yaw=200 if n == 1 else 180)
        m.label(name + " (ruin)", (c[0], c[1], 2500), fs, 400)
        _ruined_buildings(m, rng, c, fs, CORP if name == "HOSPITAL" else (FACTORY if "POLICE" in name else CITYSCAPE + COTTAGE))
    # Gym: a broken stadium bowl around the boss section, climbable stands.
    gx, gy, _ = sections[3][0]
    for k in range(20):
        if k in (9, 10):
            continue   # the entrance gap faces the hospital
        az = k * 18.0
        p = m.polar((gx, gy, 0), 6200, az, 0)
        m.jump_steps((p[0], p[1], 0), az, 5, "Section4_GYM/Stands", run=300, width=1900, material="Concrete", tags=["Stand"])
    # Routes: rubble mounds to jump up, a toppled tower bridging a flooded crater, broken facades to climb.
    f = "Traversal"
    for a, b in ((sections[0][0], sections[1][0]), (sections[1][0], sections[3][0]), (sections[0][0], sections[2][0]),
                 (sections[2][0], sections[3][0])):
        for t in (0.35, 0.65):
            x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
            yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
            top = m.jump_steps((x - 600 * math.cos(math.radians(yaw)), y - 600 * math.sin(math.radians(yaw)), 0), yaw, 4, f + "/RubbleMounds",
                               material="Jump")
            m.block((top[0], top[1], top[2] - 100), (500, 500, 200), "RubbleTop", f + "/RubbleMounds", "Rubble")
    # Flooded crater with a toppled tower lying across it (walk the tower or swim the crater).
    cx, cy = -1000, -2000
    m.box(cx - 4000, cy - 3000, -1600, cx + 4000, cy + 3000, -1400, "CraterBed", f + "/Crater", "Rock")
    m.water(cx - 4000, cy - 3000, -1400, cx + 4000, cy + 3000, -60, "CraterWater", f + "/Crater")
    m.box(cx - 4200, cy - 3200, -1400, cx - 4000, cy + 3200, 0, "CraterWall", f + "/Crater", "Rock")
    m.box(cx + 4000, cy - 3200, -1400, cx + 4200, cy + 3200, 0, "CraterWall", f + "/Crater", "Rock")
    m.block((cx, cy, 300), (11000, 1400, 1400), "ToppledTower", f + "/Crater", "Concrete", yaw=8, roll=4, tags=["ToppledTower"])
    m.climb_wall((cx - 5600, cy, 0), 180, 1400, 700, f + "/Crater")
    m.label("FLOODED CRATER (swim) + TOPPLED TOWER", (cx, cy, 2500), f, 300)


def _ruined_buildings(m, rng, c, folder, pool):
    for k in range(6):
        az = k * 60.0 + rng.uniform(-15, 15)
        p = m.polar(c, rng.uniform(6200, 8500), az, 0)
        path = "%s/%s" % (K1, rng.choice(pool))
        b = m.place_kit(path, p[0], p[1], -rng.uniform(200, 900), rng.choice([0, 90, 180, 270]), 1.0, "Ruin", folder,
                        ["Building", "Ruin", "Climbable"], material="Rubble")
        b["actor"].set_actor_rotation(unreal.Rotator(roll=rng.uniform(-6, 6), pitch=rng.uniform(-6, 6), yaw=b["actor"].get_actor_rotation().yaw), False)
        if rng.random() < 0.6:   # a broken facade route up the side facing the section
            face = m.polar(c, 5600, az, 0)
            m.climb_wall(face, az + 180, 600, min(1600, max(800, b["top"] - 400)), folder + "/Climb")
    for _ in range(10):
        p = m.polar(c, rng.uniform(1500, 4000), rng.uniform(0, 360), 0)
        m.block((p[0], p[1], 60), (rng.uniform(200, 700), rng.uniform(200, 700), rng.uniform(80, 160)), "RubbleChunk", folder, "Rubble",
                yaw=rng.uniform(0, 360), tags=["Cover"])


# ============================================================================ 3. Docks

def ch03_docks(m, rng, log):
    """Semi-ruined docks, crashing waves, heavy fog; the only structure is a lighthouse."""
    m.lighting(sun=0.8, sun_colour=(200, 210, 230), pitch=-20, sky=0.8, fog=0.25, fog_colour=(0.35, 0.38, 0.42), volumetric=True)
    f = "Shore"
    m.box(-30000, -25000, -400, 0, 25000, 0, "Shore", f, "Concrete")
    m.box(-30000, -25000, -6000, 70000, 25000, -3000, "Seabed", "Sea", "Rock")
    m.water(0, -25000, -3000, 70000, 25000, -150, "Sea", "Sea")
    # Broken quay edge: slabs tilted into the water.
    for k in range(12):
        y = -22000 + k * 4000
        if rng.random() < 0.5:
            m.block((300, y, -500), (1200, 3000, 300), "BrokenQuaySlab", f, "Concrete", pitch=-rng.uniform(10, 25), yaw=rng.uniform(-8, 8))
    _start(m, (-20000, 0, 0))
    m.section(1, (-8000, 0, 0), "Section1_Quay", gate_yaw=180)
    # Three broken piers; the middle one leads on. Missing planks = jump gaps, collapsed parts slope into the sea.
    f = "Piers"
    for py, main in ((-9000, False), (0, True), (9000, False)):
        x = 0
        while x < (24000 if main else 14000):
            seg = rng.uniform(1200, 2400)
            if rng.random() < 0.18 and x > 2000:
                m.block((x + seg / 2, py, -700), (seg, 900, 60), "CollapsedPier", f, "Wood", pitch=-14)   # slopes into the water
            else:
                m.block((x + seg / 2, py, -20), (seg, 900, 40), "PierDeck", f, "Wood", tags=["Pier"])
            for side in (-1, 1):
                m.block((x + seg / 2, py + side * 400, -1500), (60, 60, 3000), "PierPile", f, "Wood")
            x += seg + (rng.uniform(250, 450) if rng.random() < 0.3 else 0)   # gap to jump
    m.section(2, (28500, 0, 0), "Section2_PierHead", floor="Wood", gate_yaw=180)
    # Stepping rocks to the lighthouse island: jump between them, or swim.
    f = "Causeway"
    for k in range(14):
        x = 34000 + k * 900 + rng.uniform(-100, 100)
        y = rng.uniform(-900, 900)
        m.block((x, y, -900 + (k % 3) * 80), (rng.uniform(500, 750), rng.uniform(500, 750), 1800), "SteppingRock", f, "Rock",
                yaw=rng.uniform(0, 90), tags=["JumpUp"])
    m.section(3, (48000, 0, 0), "Section3_Rocks", floor="Rock", gate_yaw=180)
    for x in (53000, 53800):
        m.block((x, 0, -840), (650, 650, 1800), "SteppingRock", f, "Rock", tags=["JumpUp"])
    # Lighthouse island (boss section) and the lighthouse: outside spiral stairs, gallery, rotating beam.
    f = "Lighthouse"
    ix = 62000
    m.block((ix, 0, -1500), (16000, 16000, 3000), "Island", f, "Rock", shape="cylinder")
    m.section(4, (ix, 0, 0), "Section4_Island", boss=True, gate_yaw=180)
    m.block((ix, 0, 3500), (1800, 1800, 7000), "LighthouseTower", f, "Grey", shape="cylinder", tags=["Lighthouse", "Climbable"])
    for k in range(84):
        az = k * 13.5
        p = m.polar((ix, 0, 0), 1150, az, 60 + k * 83)
        m.block(p, (500, 260, 40), "SpiralStep", f, "Metal", yaw=az, tags=["LighthouseStair"])
    m.block((ix, 0, 6950), (2800, 2800, 100), "Gallery", f, "Metal", shape="cylinder")
    m.block((ix, 0, 7600), (1300, 1300, 1000), "LampRoom", f, "Glass", shape="cylinder", collision=False)
    m.block((ix, 0, 7600), (300, 300, 300), "Lamp", f, "Lamp", shape="sphere", collision=False)
    m.mover((ix, 0, 7600), (12000, 120, 120), "LighthouseBeam", f, "Lamp", rotation=(0, 30, 0), collision=False, tags=["LighthouseBeam"])
    m.point_light((ix, 0, 7600), f, 400000, 12000, (1.0, 0.95, 0.8))
    # Crashing waves: foam ridges rolling in toward the quay, piers and rocks (visual, no collision).
    f = "Waves"
    for k in range(18):
        x0 = rng.uniform(8000, 60000)
        y0 = rng.uniform(-22000, 22000)
        m.mover((x0, y0, -150), (600, rng.uniform(3000, 7000), 220), "WaveCrest", f, "Foam", offset=(-3500, 0, 180),
                period=rng.uniform(5, 8), phase=rng.random(), collision=False, tags=["Wave"])
    for y in range(-22000, 22001, 4000):
        m.mover((900, y, -300), (500, 3500, 200), "WaveCrash", f, "Foam", offset=(-200, 0, 700), period=6.5,
                phase=rng.random(), collision=False, tags=["Wave", "WaveCrash"])


# ============================================================================ 4. The Hanging City

def ch04_hanging(m, rng, log):
    """A city over an endless void, held up by one massive chain rising into the sky (top never visible)."""
    m.lighting(sun=1.5, sun_colour=(220, 220, 255), pitch=-35, fog=0.06, fog_colour=(0.2, 0.22, 0.3), volumetric=True)
    f = "MainDisc"
    R = 20000
    m.block((0, 0, -750), (R * 2, R * 2, 1500), "CityDisc", f, "Concrete", shape="cylinder")
    m.block((0, 0, -4000), (R * 1.2, R * 1.2, 5000), "DiscUnderside", f, "Dark", shape="cone", pitch=180)
    # The chain: links alternate orientation, climbable, up to 3 km into the fog.
    f = "Chain"
    for k in range(150):
        z = 1200 + k * 2000
        yaw = 0 if k % 2 == 0 else 90
        for side in (-1, 1):
            a = math.radians(yaw)
            ox, oy = math.cos(a) * side * 700, math.sin(a) * side * 700
            m.block((ox, oy, z), (260, 260, 2400), "ChainLinkSide", f, "Chain", tags=["Chain", "Climbable"])
        m.block((0, 0, z + 1100), (1660, 260, 260), "ChainLinkEnd", f, "Chain", yaw=yaw, tags=["Chain", "Climbable"])
        m.block((0, 0, z - 1100), (1660, 260, 260), "ChainLinkEnd", f, "Chain", yaw=yaw, tags=["Chain", "Climbable"])
    m.block((0, 0, 600), (4000, 4000, 1200), "ChainAnchor", f, "Metal", shape="cylinder", tags=["ChainAnchor"])
    m.label("THE CHAIN (top never visible)", (0, 0, 9000), f, 500)
    # City on the disc.
    f = "MainDisc/Buildings"
    for k in range(26):
        r = rng.uniform(6500, R - 2500)
        az = rng.uniform(0, 360)
        if 30 < (az % 90) < 60 and r > 12000:
            continue   # keep avenues clear
        p = m.polar((0, 0, 0), r, az, 0)
        path = "%s/%s" % (K1, rng.choice(HIGHRISE + CORP))
        m.place_kit(path, p[0], p[1], 0, rng.choice([0, 90, 180, 270]), 0.8, "Building", f, ["Building", "Climbable"])
    m.section(3, (0, -11000, 0), "Section3_DiscStreets", gate_yaw=270)
    m.section(4, (0, 0, 0), "Section4_ChainAnchor", radius=4000, boss=True, gate_yaw=270)
    # Four hanging districts below the rim, hung on cables; swinging bridges and lifts link them.
    districts = []
    for k in range(4):
        az = 45 + k * 90
        c = m.polar((0, 0, 0), R + 7500, az, -6000 - k * 300)
        fd = "Districts/District_%d" % (k + 1)
        m.block((c[0], c[1], c[2] - 400), (12000, 12000, 800), "DistrictPlatform", fd, "Concrete", shape="cylinder")
        for j in range(4):
            q = m.polar(c, 5000, j * 90 + 45, c[2])
            top = m.polar((0, 0, 0), R - 500, az + (j - 1.5) * 8, 0)
            _route(m, q, top, fd + "/Cables", "Chain", width=60)
        for j in range(5):
            p = m.polar(c, rng.uniform(1000, 3500), rng.uniform(0, 360), c[2])
            m.place_kit("%s/%s" % (K1, rng.choice(COTTAGE + CITYSCAPE)), p[0], p[1], c[2], rng.choice([0, 90]), 0.6, "Shack", fd,
                        ["Building", "Climbable"])
        districts.append((c, az))
    m.section(1, districts[0][0], "Districts/District_1", radius=4500, gate_yaw=districts[0][1])
    m.section(2, districts[1][0], "Districts/District_2", radius=4500, gate_yaw=districts[1][1] + 180)
    _start(m, districts[0][0])
    f = "Traversal"
    for k in range(4):
        (a, aza), (b, azb) = districts[k], districts[(k + 1) % 4]
        mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2)
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        dist = math.hypot(b[0] - a[0], b[1] - a[1])
        t0 = 6000.0 / dist
        n = max(3, int((dist - 12000) // 2600))
        for j in range(n):   # swinging plank segments across the gap: jump from one to the next
            t = t0 + (1 - 2 * t0) * (j + 0.5) / n
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)
            m.mover(p, (1800, 700, 60), "SwingingPlank", f + "/SwingBridges", "Climb", yaw=yaw, rotation=(0, 0, 10), swing=True,
                    period=4 + j % 3, phase=j * 0.23, tags=["SwingBridge"])
        # A lift from each district up to the disc rim.
        lift = m.polar((0, 0, 0), R + 800, aza, a[2])
        m.mover(lift, (1500, 1500, 100), "Lift", f + "/Lifts", "Climb", offset=(0, 0, -a[2] + 100), period=12, phase=k * 0.25,
                tags=["Lift"])
    # Rim climb walls where the lifts arrive.
    for k in range(4):
        m.climb_wall(m.polar((0, 0, 0), R, 45 + k * 90, -1500), 45 + k * 90, 1200, 1500, "Traversal/RimClimb")
    m.label("ENDLESS VOID", (0, 0, -20000), "Labels", 800)


# ============================================================================ 5. A.R.C TECH INCORPORATED

def ch05_arctech(m, rng, log):
    """A massive, undamaged, fully automatic factory with no workers, making tiny cubes that glow insanely bright yellow."""
    m.lighting(sun=0.5, pitch=-60, sky=0.6, fog=0.01, fog_colour=(0.3, 0.28, 0.2), atmosphere=True)
    X0, X1, Y0, Y1, H = -30000, 30000, -15000, 15000, 7000
    f = "Hall"
    m.box(X0, Y0, -200, X1, Y1, 0, "FactoryFloor", f, "Concrete")
    m.box(X0, Y0, H, X1, Y1, H + 300, "Roof", f, "Metal")
    for y in (Y0, Y1):
        m.box(X0, y - 100, 0, X1, y + 100, H, "Wall", f, "Metal")
    m.box(X1, Y0, 0, X1 + 200, Y1, H, "Wall", f, "Metal")
    m.box(X0 - 200, Y0, 1200, X0, Y1, H, "Wall_Intake", f, "Metal")   # the intake end is open at ground level
    for k in range(-25, 26, 5):
        m.block((k * 1000, 0, H - 200), (400, 28000, 300), "RoofLightStrip", f, "Lamp", collision=False)
    m.label("A.R.C TECH INCORPORATED", (X0 - 400, 0, 3000), f, 600, yaw=180, in_game=True, colour=(255, 220, 30))
    _start(m, (X0 - 3000, 0, 0), yaw=0)
    m.box(X0 - 6000, Y0, -200, X0, Y1, 0, "LoadingApron", f, "Concrete")
    m.section(1, (-24000, 0, 0), "Section1_Intake", gate_yaw=180)
    m.section(2, (-8000, 0, 0), "Section2_AssemblyLines", gate_yaw=180)
    m.section(3, (8000, 0, 0), "Section3_PressHall", gate_yaw=180)
    m.section(4, (24000, 0, 0), "Section4_CubeCore", boss=True, gate_yaw=180)
    # Conveyor lines along the hall with glowing cubes riding them; no workers anywhere.
    f = "Lines"
    for y in (-11000, -6500, 6500, 11000):
        m.box(X0 + 2000, y - 300, 0, X1 - 2000, y + 300, 90, "Conveyor", f, "Dark", tags=["Conveyor", "JumpUp"])
        for k in range(30):
            x = X0 + 2500 + k * 1800
            m.mover((x, y, 110), (30, 30, 30), "Cube", f + "/Cubes", "CubeGlow", offset=(1800, 0, 0), period=3.0,
                    collision=False, tags=["Cube"])
        for x in range(X0 + 4000, X1 - 3000, 6000):   # robot arms (swinging placeholders), climbable bases
            m.block((x, y + 700, 600), (500, 500, 1200), "ArmBase", f + "/Arms", "Hazard", tags=["JumpUp"])
            m.mover((x, y + 700, 1500), (2200, 200, 200), "RobotArm", f + "/Arms", "Metal", rotation=(0, 70, 0), swing=True,
                    period=3.5, phase=rng.random(), tags=["RobotArm"])
    # Presses in the press hall: vertical stampers to time a run under.
    f = "Presses"
    for k in range(10):
        x, y = 4000 + (k % 5) * 2000, -2400 + (k // 5) * 4800
        m.block((x, y, 50), (1200, 1200, 100), "PressBed", f, "Hazard")
        m.mover((x, y, 2200), (1100, 1100, 800), "Press", f, "Metal", offset=(0, 0, -2000), period=2.5, phase=k * 0.17, tags=["Press"])
    # Overhead catwalks over the lines, reached by ladders / climb walls; coolant pools to swim through.
    f = "Catwalks"
    for y in (-8800, 8800):
        m.box(X0 + 2000, y - 300, 3500, X1 - 2000, y + 300, 3550, "Catwalk", f, "Metal", tags=["Catwalk"])
        for x in range(X0 + 3000, X1 - 2000, 9000):
            m.climb_wall((x, y + 320, 0), 90, 400, 3500, f, material="Ladder")
    f = "Coolant"
    for x in (-16000, 0, 16000):
        m.box(x - 2500, -4200, -1800, x + 2500, 4200, -1600, "PoolBed", f, "Dark")
        for sx in (-1, 1):
            m.box(x + sx * 2500, -4200, -1800, x + sx * 2700, 4200, 0, "PoolWall", f, "Metal")
        m.water(x - 2500, -4200, -1600, x + 2500, 4200, -100, "CoolantPool", f)
    # Cube core: a pillar of stacked cubes behind the boss section.
    f = "CubeCore"
    m.block((28000, 0, 3000), (2000, 2000, 6000), "CubeCorePillar", f, "Glass", shape="cylinder", collision=False)
    for k in range(400):
        m.block((28000 + rng.uniform(-800, 800), rng.uniform(-800, 800), rng.uniform(100, 5900)), (30, 30, 30), "Cube", f, "CubeGlow",
                collision=False, tags=["Cube"])
    for k in range(12):   # pallets of finished cubes
        p = (rng.uniform(X0 + 2000, X1 - 2000), rng.choice([-13500, 13500]), 0)
        m.block((p[0], p[1], 60), (1200, 1000, 120), "Pallet", f + "/Pallets", "Wood", tags=["JumpUp"])
        for j in range(40):
            m.block((p[0] + rng.uniform(-500, 500), p[1] + rng.uniform(-400, 400), 140), (30, 30, 30), "Cube", f + "/Pallets", "CubeGlow",
                    collision=False, tags=["Cube"])
    m.point_light((28000, 0, 3000), f, 800000, 15000, (1.0, 0.85, 0.1))


# ============================================================================ 6. The Moon Kingdom

def ch06_moon(m, rng, log):
    """A second arc, built out and liveable, but empty of life and overgrown, with a massive Moon inside casting darkness."""
    m.lighting(sun=0.05, sun_colour=(150, 160, 210), pitch=-80, sky=0.15, fog=0.03, fog_colour=(0.03, 0.035, 0.06), shadows=False)
    HX, HY, CEIL = 40000, 30000, 40000
    f = "Cavern"
    m.box(-HX, -HY, -200, HX, HY, 0, "CavernFloor", f, "Flora")
    m.box(-HX, -HY, CEIL, HX, HY, CEIL + 1000, "RockCeiling", f, "Dark")
    for s in (-1, 1):
        m.box(-HX, s * HY, 0, HX, s * (HY + 1000), CEIL, "RockWall", f, "Dark")
        m.box(s * HX, -HY, 0, s * (HX + 1000), HY, CEIL, "RockWall", f, "Dark")
    # The Moon: a massive sphere hanging inside the arc; dim, so it reads as a source of darkness rather than light.
    m.block((0, 0, 26000), (24000, 24000, 24000), "TheMoon", "Moon", "Moon", shape="sphere", collision=False, tags=["Moon"])
    m.label("THE MOON (casts deep darkness)", (0, 0, 40000 - 1500), "Moon", 600)
    # The lake under the Moon, with an island: boss section.
    f = "Lake"
    m.box(-9000, -9000, -2200, 9000, 9000, -2000, "LakeBed", f, "Rock")
    m.water(-9000, -9000, -2000, 9000, 9000, -80, "MoonLake", f)
    for s in (-1, 1):
        m.box(-9200, s * 9000, -2000, 9200, s * 9200, 0, "LakeWall", f, "Rock")
        m.box(s * 9000, -9000, -2000, s * 9200, 9000, 0, "LakeWall", f, "Rock")
    m.block((0, 0, -1000), (9000, 9000, 2000), "MoonIsland", f, "Flora", shape="cylinder")
    m.section(4, (0, 0, 0), "Section4_UnderTheMoon", radius=4000, boss=True, gate_yaw=180)
    # Town of the second arc (gate plaza, market streets, residential terraces), all overgrown.
    secs = [(-30000, 0, 0), (-18000, 18000, 0), (18000, 16000, 0)]
    _start(m, (-37000, 0, 0))
    m.block((-38500, 0, 1500), (400, 4000, 3000), "ArcGate", "Section1_GatePlaza", "Metal", tags=["JumpUp"])
    for n, c in enumerate(secs[:2], 1):
        m.section(n, c, ("Section1_GatePlaza", "Section2_Market")[n - 1], floor="Concrete", gate_yaw=180)
    f = "Town"
    for k in range(70):
        x, y = rng.uniform(-HX + 3000, HX - 3000), rng.uniform(-HY + 3000, HY - 3000)
        if math.hypot(x, y) < 11000 or any(math.hypot(x - c[0], y - c[1]) < 5500 for c in secs):
            continue
        path = "%s/%s" % (K1, rng.choice(COTTAGE + CITYSCAPE + HIGHRISE[:6]))
        b = m.place_kit(path, x, y, 0, rng.choice([0, 90, 180, 270]), 0.9, "Building", f, ["Building", "Climbable"])
        _overgrow(m, rng, b, f + "/Flora")
    # Residential terraces: jump-up tiers to section 3.
    tx, ty, _ = secs[2]
    top = m.jump_steps((tx - 9000, ty - 9000, 0), 45, 8, "Traversal/Terraces", run=500, width=2400, material="Jump")
    m.box(tx - 6000, ty - 6000, 0, tx + 6000, ty + 6000, top[2], "TerracePlateau", "Traversal/Terraces", "Concrete")
    m.section(3, (tx, ty, top[2]), "Section3_Terraces", gate_yaw=225)
    # Lily stepping stones across the lake to the island.
    for k in range(10):
        p = m.polar((0, 0, 0), 8500 - k * 500, 200 + k * 4, -60)
        m.block(p, (500, 500, 120), "LilyPad", "Lake/Pads", "Jump", shape="cylinder", tags=["JumpUp"])


def _overgrow(m, rng, b, folder):
    x0, y0, x1, y1 = b["rect"]
    m.block(((x0 + x1) / 2, (y0 + y1) / 2, b["top"] + 100), (x1 - x0, y1 - y0, 200), "RoofFlora", folder, "Flora", collision=False)
    for _ in range(2):   # hanging vines, climbable
        side = rng.choice([0, 1])
        x = rng.uniform(x0, x1) if side else rng.choice([x0 - 30, x1 + 30])
        y = rng.choice([y0 - 30, y1 + 30]) if side else rng.uniform(y0, y1)
        m.block((x, y, b["top"] / 2), (60, 60, b["top"]), "Vine", folder, "Climb", tags=["Vine", "Climbable"])


# ============================================================================ 7. T.O.M.B.S

def ch07_tombs(m, rng, log):
    """The Old Man's Battings Site: a structure larger than anything man-made; inside, a regular office holding Angel secrets."""
    m.lighting(sun=2.0, pitch=-30, fog=0.02, fog_colour=(0.35, 0.37, 0.42))
    W, H = 30000, 250000
    f = "Exterior"
    _ground(m, 60000, "Concrete")
    for s in (-1, 1):   # the colossal shell; the office inside is a slice near the bottom
        m.box(-W, s * W, 0, W, s * (W + 1000), H, "MegaWall", f, "Dark")
    m.box(W, -W, 0, W + 1000, W, H, "MegaWall", f, "Dark")
    m.box(-W - 1000, -W, 0, -W, -1500, H, "MegaWall", f, "Dark")   # west wall with the only entrance
    m.box(-W - 1000, 1500, 0, -W, W, H, "MegaWall", f, "Dark")
    m.box(-W - 1000, -1500, 1500, -W, 1500, H, "MegaWall", f, "Dark")
    m.box(-W, -W, H, W, W, H + 2000, "MegaRoof", f, "Dark")
    m.label("T.O.M.B.S", (-W - 1100, 0, 6000), f, 3000, yaw=180, in_game=True)
    _start(m, (-W - 8000, 0, 0), yaw=0)
    # Inside: ten office floors around an atrium. Lobby at the bottom.
    f = "Office"
    OX0, OX1, OY = -W + 2000, -W + 26000, 12000
    FLOOR = 450
    m.section(1, (OX0 + 6000, 0, 0), "Section1_Lobby", radius=4500, gate_yaw=180)
    m.block((OX0 + 6000, 0, 60), (1600, 600, 120), "ReceptionDesk", f + "/Lobby", "Wood", tags=["JumpUp"])
    for fl in range(1, 11):
        z = fl * FLOOR
        ff = "%s/Floor_%02d" % (f, fl)
        hole = (OX0 + 9000, OX0 + 15000)   # atrium shaft
        m.box(OX0, -OY, z - 30, hole[0], OY, z, "FloorSlab", ff, "Concrete")
        m.box(hole[1], -OY, z - 30, OX1, OY, z, "FloorSlab", ff, "Concrete")
        m.box(hole[0], -OY, z - 30, hole[1], -3000, z, "FloorSlab", ff, "Concrete")
        m.box(hole[0], 3000, z - 30, hole[1], OY, z, "FloorSlab", ff, "Concrete")
        if fl in (3, 6, 9):
            continue   # section floors stay open
        for k in range(40):   # cubicles: desks and low partitions you can vault
            x, y = rng.uniform(OX0 + 500, OX1 - 500), rng.uniform(-OY + 500, OY - 500)
            if hole[0] - 200 < x < hole[1] + 200 and -3200 < y < 3200:
                continue
            m.block((x, y, z + 40), (160, 80, 80), "Desk", ff, "Wood", tags=["Desk", "JumpUp"])
            m.block((x, y + 90, z + 70), (180, 10, 140), "Partition", ff, "Grey")
    # Atrium jump-ups: broken slabs and stacked cabinets going up floor to floor.
    f = "Traversal/Atrium"
    for fl in range(0, 10):
        z = fl * FLOOR
        x = OX0 + 9600 + (fl % 2) * 4800
        m.jump_steps((x, -3200 if fl % 2 else 3200, z), 270 if fl % 2 == 0 else 90, 5, f, rise=90, run=250, width=600,
                     material="Jump", tags=["Atrium"])
    # Elevator shafts (climbable) and stair cores.
    for y in (-OY + 1000, OY - 1000):
        m.climb_wall((OX1 + 300, y, 0), 180, 600, FLOOR * 10, "Traversal/ElevatorShaft")
    for n, fl in ((2, 3), (3, 6), (4, 9)):
        m.section(n, (OX0 + 19000, 0, fl * FLOOR), "Section%d_Floor%02d" % (n, fl), radius=4500, boss=n == 4, gate_yaw=180)
    # Angel secrets: unmarked rooms behind office walls (placeholders, glowing).
    f = "AngelSecrets"
    for fl in (2, 4, 6, 8, 10):
        z = fl * FLOOR
        x, y = rng.uniform(OX0 + 2000, OX1 - 2000), rng.choice([-OY + 600, OY - 600])
        inward = -1 if y > 0 else 1
        for sx in (-1, 1):
            m.box(x + sx * 600 - 20, y - 500, z, x + sx * 600 + 20, y + 500, z + 400, "SecretRoomWall", f, "Grey")
        m.box(x - 600, y - inward * 500 - 20, z, x + 600, y - inward * 500 + 20, z + 400, "SecretRoomWall", f, "Grey")
        m.box(x - 600, y + inward * 500 - 20, z, x + 600, y + inward * 500 + 20, z + 400, "SECRET_DOOR_PLACEHOLDER", f, "Grey",
              tags=["SecretDoor"], collision=False)   # looks like a plain office wall
        m.box(x - 620, y - 520, z + 400, x + 620, y + 520, z + 420, "SecretRoomCeiling", f, "Grey")
        m.block((x, y, z + 150), (200, 200, 200), "ANGEL_SECRET_PLACEHOLDER", f, "Angel", collision=False, tags=["AngelSecret"])


# ============================================================================ 8. Intersection

def ch08_intersection(m, rng, log):
    """A tunnel drenched in rain and flooding, full of angels: their main transport path from their assumed headquarters."""
    m.lighting(sun=0.3, pitch=-70, sky=0.3, fog=0.08, fog_colour=(0.12, 0.14, 0.18), volumetric=True, shadows=False)
    L, HW, HT = 150000, 3000, 2600
    f = "Tunnel"
    cx = 70000
    m.box(0, -HW, -200, 92000, HW, 0, "TunnelFloor", f, "Concrete")
    m.box(104000, -HW, -200, L, HW, 0, "TunnelFloor", f, "Concrete")
    for s in (-1, 1):   # walls, open where the cross tunnel meets
        m.box(0, s * HW, 0, cx - HW, s * (HW + 300), HT, "TunnelWall", f, "Concrete")
        m.box(cx + HW, s * HW, 0, L, s * (HW + 300), HT, "TunnelWall", f, "Concrete")
    for x in range(0, L, 5000):   # roof with broken grates: rain falls through the gaps
        m.box(x, -HW, HT, x + 4200, HW, HT + 300, "TunnelRoof", f, "Concrete")
        m.block((x + 4600, 0, HT + 150), (800, HW * 2, 300), "RainGrate", f, "Metal", collision=False, tags=["RainGrate"])
    m.rain_placeholder(L, text="RAIN PLACEHOLDER - rain pours in through every roof grate")
    # The cross tunnel that gives the place its name.
    m.box(cx - HW, -40000, -200, cx + HW, 40000, 0, "CrossTunnelFloor", f + "/Cross", "Concrete")
    for s in (-1, 1):
        m.box(cx - HW - 300, s * HW, 0, cx - HW, s * 40000, HT, "CrossWall", f + "/Cross", "Concrete")
        m.box(cx + HW, s * HW, 0, cx + HW + 300, s * 40000, HT, "CrossWall", f + "/Cross", "Concrete")
        m.box(cx - HW, s * HW, HT, cx + HW, s * 40000, HT + 300, "CrossRoof", f + "/Cross", "Concrete")
    _start(m, (2000, 0, 0), yaw=0)
    # Flooding: the water rises along the tunnel (ankle, waist, then swimming); a pump room is fully flooded.
    f = "Flood"
    for x0, x1, depth in ((20000, 45000, 40), (45000, 65000, 120), (80000, 110000, 400), (110000, 125000, 2200)):
        m.water(x0, -HW, 0, x1, HW, depth, "Floodwater_%d" % depth, f)
    m.box(92000, -HW, -2700, 104000, HW, -2500, "PumpRoomFloor", f, "Concrete")   # the floor drops into a flooded pump room
    for s in (-1, 1):
        m.box(92000, s * HW, -2500, 104000, s * (HW + 300), 0, "PumpRoomWall", f, "Concrete")
    for x in (91800, 104000):
        m.box(x, -HW, -2500, x + 200, HW, -200, "PumpRoomWall", f, "Concrete")
    m.water(92000, -HW, -2500, 104000, HW, 0, "PumpRoomWater", f, surface=False)
    # Side maintenance ledges (jump up) and overhead pipes (climb) to stay above the flood.
    f = "Traversal"
    for s in (-1, 1):
        m.box(15000, s * (HW - 500), 0, L, s * HW, 600, "MaintenanceLedge", f, "Jump", tags=["JumpUp"])
        for x in range(15000, L, 3000):
            m.block((x, s * (HW - 700), 300), (300, 400, 300), "LedgeStep", f, "Jump", tags=["JumpUp"])
        m.block((L / 2, s * (HW - 200), HT - 300), (120, 120, L), "OverheadPipe", f, "Climb", shape="cylinder", pitch=90,
                tags=["Climbable"])
    # Angels everywhere: standing, and flying along the tunnel (movers).
    f = "Angels"
    for k in range(160):
        x, y = rng.uniform(4000, L - 2000), rng.uniform(-HW + 600, HW - 600)
        if rng.random() < 0.5:
            m.mover((x, y, rng.uniform(900, 2000)), (120, 120, 260), "ANGEL_PLACEHOLDER", f, "Angel", offset=(rng.choice([-1, 1]) * 9000, 0, 0),
                    period=rng.uniform(6, 14), phase=rng.random(), collision=False, tags=["AngelPlaceholder"], shape="sphere")
        else:
            m.block((x, y, 200), (90, 90, 400), "ANGEL_PLACEHOLDER", f, "Angel", collision=False, tags=["AngelPlaceholder"], shape="cylinder")
    for k, x in enumerate(range(0, L, 3000)):
        m.block((x, 0, HT - 50), (200, 600, 40), "TunnelLamp", "Tunnel/Lamps", "Lamp", collision=False)
    m.section(1, (10000, 0, 0), "Section1_Entrance", radius=2900, gate_yaw=180)
    m.section(2, (cx, 0, 0), "Section2_Intersection", radius=2900, gate_yaw=180)
    m.section(3, (98000, 0, -2500), "Section3_PumpRoom", radius=2900, gate_yaw=180)
    m.section(4, (140000, 0, 0), "Section4_TowardHQ", radius=2900, boss=True, gate_yaw=180)
    m.label("TOWARD THE ANGELS' ASSUMED HEADQUARTERS", (L - 500, 0, 1800), "Labels", 250, yaw=180)


# ============================================================================ 9. Remnant Remains

def ch09_remnant(m, rng, log):
    """An empty desert, violent wind and constant downpour; a massive data centre in the middle with a small screen."""
    m.lighting(sun=0.7, sun_colour=(200, 190, 170), pitch=-30, sky=0.6, fog=0.09, fog_colour=(0.35, 0.32, 0.28), volumetric=True)
    half = 45000
    _ground(m, half + 10000, "Sand")
    m.rain_placeholder(half, text="DOWNPOUR + VIOLENT WIND PLACEHOLDER")
    wind = m.box(-half, -half, 0, half, half, 8000, "WIND_VOLUME_PLACEHOLDER", "FX", "Sand", tags=["WindPlaceholder"], collision=False,
                 hidden_in_game=True)
    wind.static_mesh_component.set_editor_property("visible", False)
    f = "Dunes"
    for _ in range(45):
        x, y = rng.uniform(-half, half), rng.uniform(-half, half)
        if math.hypot(x, y) < 16000:
            continue
        m.block((x, y, -rng.uniform(300, 900)), (rng.uniform(5000, 12000), rng.uniform(3000, 7000), rng.uniform(1600, 3000)), "Dune", f,
                "Sand", shape="sphere", yaw=rng.uniform(0, 180))
    # Downpour pools in dune hollows (swim).
    for k in range(4):
        p = m.polar((0, 0, 0), 26000, k * 90 + 20, 0)
        m.box(p[0] - 2500, p[1] - 2500, -1500, p[0] + 2500, p[1] + 2500, -1300, "HollowBed", f + "/Pools", "Sand")
        m.water(p[0] - 2500, p[1] - 2500, -1300, p[0] + 2500, p[1] + 2500, -50, "RainPool", f + "/Pools")
    _start(m, (-40000, 0, 0), yaw=0)
    m.section(1, (-30000, 0, 0), "Section1_DuneField", gate_yaw=180)
    # Cable canyon: huge cable trunks running out from the data centre over the sand (climb over, balance along).
    f = "CableCanyon"
    for k in range(8):
        az = k * 45 + 10
        a, b = m.polar((0, 0, 0), 9000, az, 800), m.polar((0, 0, 0), 42000, az, 300)
        _route(m, a, b, f, "Climb", width=500, tags=["Cable", "Climbable"])
    for k in range(6):
        x = -22000 + k * 1600
        m.block((x, 0, 600), (800, 2400, 1200 + (k % 3) * 300), "ServerRackFallen", f, "Metal", yaw=rng.uniform(-20, 20), tags=["JumpUp"])
    m.section(2, (-17000, 0, 0), "Section2_CableCanyon", gate_yaw=180)
    # The data centre: tiered blocks, cooling towers, a perimeter yard and the server hall with the screen.
    f = "DataCentre"
    m.box(-8000, -8000, 0, 8000, 8000, 300, "Plinth", f, "Concrete")
    m.box(5000, -7000, 300, 7000, 7000, 9000, "DataCentreMass", f, "Dark", tags=["DataCentre"])   # around the server hall
    for s in (-1, 1):
        m.box(-7000, s * 5000, 300, 5000, s * 7000, 9000, "DataCentreMass", f, "Dark", tags=["DataCentre"])
        m.box(-7000, s * 2500, 300, -6800, s * 5000, 2500, "DataCentreMass", f, "Dark", tags=["DataCentre"])
    m.box(-7000, -5000, 2500, 5000, 5000, 9000, "DataCentreMass", f, "Dark", tags=["DataCentre"])
    m.box(-4000, -4000, 9000, 4000, 4000, 15000, "DataCentreTier", f, "Dark")
    for k in range(4):
        p = m.polar((0, 0, 0), 11500, k * 90 + 45, 0)
        m.block((p[0], p[1], 3000), (3500, 3500, 6000), "CoolingTower", f, "Concrete", shape="cylinder", tags=["Climbable"])
        m.climb_wall(m.polar((0, 0, 0), 9750, k * 90 + 45, 0), k * 90 + 225, 800, 6000, f + "/Climb")
    for x in range(-6500, 6501, 1000):
        m.block((x, -7050, 4500), (500, 40, 8000), "StatusLights", f, rng.choice(["NeonGreen", "NeonCyan"]), collision=False)
    m.section(3, (-12500, 0, 300), "Section3_Perimeter", radius=4000, gate_yaw=180)
    # Server hall (cut into the mass) with the small screen.
    m.box(-6800, -5000, 300, 5000, 5000, 320, "HallFloor", f, "Metal")
    for y in range(-4000, 4001, 1600):
        m.box(-2000, y - 200, 320, 4500, y + 200, 2300, "ServerRack", f + "/Hall", "Dark", tags=["JumpUp"])
    m.block((4700, 0, 1600), (40, 200, 120), "SmallScreen", f + "/Hall", "Screen", tags=["Screen"])
    m.label(CH9_SCREEN, (4660, 0, 1600), f + "/Hall", 6, yaw=180, in_game=True, colour=(80, 255, 110))
    m.section(4, (-4500, 0, 320), "Section4_ServerHall", radius=2300, boss=True, gate_yaw=180)


# ============================================================================ 10. Hallucinations

def ch10_hallucinations(m, rng, log):
    """A nonsensical pastel dreamscape (drawing look), with hyper-realistic terrifying things appearing and Morse messages."""
    m.lighting(sun=3.0, sun_colour=(255, 220, 240), pitch=-50, sky=2.0, fog=0.02, fog_colour=(0.9, 0.75, 0.9))
    pastel = ["PastelPink", "PastelBlue", "PastelMint", "PastelLemon", "PastelLilac"]
    f = "MilkSea"
    m.box(-60000, -60000, -4000, 60000, 60000, -3800, "SeaBed", f, "PastelBlue")
    m.water(-60000, -60000, -3800, 60000, 60000, -1500, "PastelSea", f)
    islands = [(-30000, 0, 0), (-8000, 14000, 2500), (14000, -6000, 5000), (32000, 10000, 7500)]
    names = ["Section1_Island", "Section2_UpsideDown", "Section3_Clockwork", "Section4_TrueAngel"]
    for n, (c, name) in enumerate(zip(islands, names), 1):
        m.block((c[0], c[1], c[2] - 1500), (11000, 11000, 3000), "FloatingIsland", name, pastel[n % 5], shape="cylinder")
        m.block((c[0], c[1], c[2] - 5000), (8000, 8000, 4000), "IslandRoot", name, pastel[(n + 2) % 5], shape="cone", pitch=180,
                collision=False)
        m.section(n, c, name, radius=4500, boss=n == 4, gate_yaw=200)
        # Morse message n (in game), with the plain text as an editor label.
        m.label(morse(CH10_STORY[n - 1]), (c[0], c[1] + 5200, c[2] + 1200), name + "/Morse", 90, yaw=270, in_game=True,
                colour=(60, 40, 90))
        m.label("MORSE: " + CH10_STORY[n - 1], (c[0], c[1] + 5200, c[2] + 2000), name + "/Morse", 120, yaw=270)
        m.spawn(unreal.TargetPoint, (c[0], c[1] + 5200, c[2] + 1200), label="MorseMessage_%d" % n, folder=name + "/Morse",
                tags=["MorseMessage", "Morse_%d" % n])
    _start(m, (-34000, 0, 0), yaw=0)
    # Floating staircases between islands (some pieces drift), and impossible stairs going nowhere.
    f = "Traversal"
    for k in range(3):
        a, b = islands[k], islands[k + 1]
        steps = int((b[2] - a[2]) / 80) + 6
        for j in range(steps):
            t = (j + 0.5) / steps
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)
            if j % 5 == 2:
                m.mover(p, (900, 900, 120), "DriftingStep", f + "/Stairs", pastel[j % 5], offset=(0, 0, 400), period=5, phase=j * 0.1,
                        tags=["JumpUp"])
            else:
                m.block(p, (900, 900, 120), "FloatingStep", f + "/Stairs", pastel[j % 5], tags=["JumpUp"])
    for k in range(10):   # impossible stairs (upside down / sideways), set dressing
        p = (rng.uniform(-40000, 40000), rng.uniform(-30000, 30000), rng.uniform(2000, 14000))
        m.stairs(p, rng.uniform(0, 360), 12, 80, 120, 400, "Dressing/ImpossibleStairs", rng.choice(pastel))
    # Giant everyday objects to climb: chairs, teacups, a clock face; one per island.
    f = "Dressing/GiantObjects"
    for n, c in enumerate(islands):
        p = m.polar(c, 3000, 60 + n * 70, c[2])
        kind = n % 3
        if kind == 0:   # giant chair
            m.block((p[0], p[1], p[2] + 600), (1200, 1200, 150), "GiantChairSeat", f, "PastelLilac", tags=["JumpUp"])
            m.block((p[0] - 550, p[1], p[2] + 1400), (150, 1200, 1600), "GiantChairBack", f, "Climb", tags=["Climbable"])
            for dx in (-500, 500):
                for dy in (-500, 500):
                    m.block((p[0] + dx, p[1] + dy, p[2] + 270), (120, 120, 540), "GiantChairLeg", f, "PastelLilac")
        elif kind == 1:   # giant teacup
            m.block((p[0], p[1], p[2] + 700), (1800, 1800, 1400), "GiantTeacup", f, "PastelPink", shape="cylinder", tags=["Climbable"])
        else:   # melting clock
            m.block((p[0], p[1], p[2] + 1500), (2400, 200, 2400), "GiantClock", f, "PastelLemon", shape="cylinder", roll=90, pitch=15)
    # Hyper-realistic terrifying things: appear at random (spawn points) plus a few placed placeholders.
    f = "Horror"
    for k in range(24):
        c = islands[k % 4]
        p = m.polar(c, rng.uniform(1500, 4800), rng.uniform(0, 360), c[2])
        m.spawn(unreal.TargetPoint, p, label="ApparitionSpawn", folder=f, tags=["ApparitionSpawn"])
        if k % 4 == 0:
            m.block((p[0], p[1], p[2] + 300), (160, 120, 600), "HORROR_APPARITION_PLACEHOLDER", f, "Horror", collision=False,
                    tags=["HorrorApparitionPlaceholder"])
    m.label("PASTEL DRAWING LOOK: tune MI_BIBAnimeStyle for this map (OPEN)", (0, 0, 16000), "Labels", 500)


# ============================================================================ 11. Return to Nowhere

def ch11_nowhere(m, rng, log):
    """Pure grey, no post-processing, no texture, white noise; a ~5 minute single walk through all ten past maps pasted together.
    No levels, no bosses, no enemies; nothing happens. Returns False: no anime style volume."""
    m.lighting(sun=1.0, sun_colour=(255, 255, 255), pitch=-89, sky=1.0, fog=0.04, fog_colour=(0.5, 0.5, 0.5), shadows=False, atmosphere=False)
    ppv = m.spawn(unreal.PostProcessVolume, (0, 0, 0), label="NoPostProcess", folder="Lighting", tags=["NoPostProcess"])
    try_set(ppv, "unbound", True, log)
    s = ppv.get_editor_property("settings")
    for prop, value in (("bloom_intensity", 0.0), ("vignette_intensity", 0.0), ("film_grain_intensity", 0.0), ("motion_blur_amount", 0.0),
                        ("lens_flare_intensity", 0.0), ("scene_fringe_intensity", 0.0), ("ambient_occlusion_intensity", 0.0),
                        ("auto_exposure_min_brightness", 1.0), ("auto_exposure_max_brightness", 1.0)):
        try_set(s, "override_" + prop, True, log)
        try_set(s, prop, value, log)
    ppv.set_editor_property("settings", s)
    m.spawn(unreal.TargetPoint, (0, 0, 300), label="WHITE_NOISE_AMBIENCE_PLACEHOLDER", folder="Audio", tags=["WhiteNoisePlaceholder"])
    g = "PureGrey"
    # One walk: ten legs of 140 m (14 m wide), about 1.4 km: ~5 min at the demo walking speed of 450 cm/s (BIBTuning).
    legs = []
    x, y, heading = 0.0, 0.0, 0.0
    for k in range(10):
        length = 14000.0
        nx, ny = x + length * math.cos(math.radians(heading)), y + length * math.sin(math.radians(heading))
        legs.append(((x, y), (nx, ny), heading))
        x, y = nx, ny
        heading += rng.choice([-50, -35, 35, 50])
    for k, (a, b, h) in enumerate(legs):
        _route(m, (a[0], a[1], 0), (b[0], b[1], 0), "Path", g, width=1400, tags=["NowherePath"])
        m.block((b[0], b[1], -25), (1400, 1400, 50), "PathJoint", "Path", g, shape="cylinder")
        _fragment(m, rng, k, a, b, h, "Fragments/%02d" % (k + 1))
    m.legs = legs   # the bundle pass dresses each leg with the props of the map it echoes
    _start(m, (0, 0, 0), yaw=legs[0][2])
    end = legs[-1][1]
    m.block((end[0], end[1], 600), (100, 900, 1200), "ExitFrame", "Exit", g)
    m.spawn(unreal.TargetPoint, (end[0], end[1], 100), label="NowhereExit", folder="Exit", tags=["NowhereExit"])
    m.label("WALK OUT. NOTHING HAPPENS.", (end[0], end[1], 2000), "Labels", 300)
    total = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b, _ in legs)
    log("ch11: walk %.0f m, about %.1f min at 450 cm/s" % (total / 100, total / 450 / 60))
    return False


def _fragment(m, rng, k, a, b, heading, folder):
    """A grey piece of map k+1 pasted across the path, slightly wrong (tilted, sunk, cut off)."""
    g = "PureGrey"
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    side = m.polar((mx, my, 0), 2500, heading + 90, 0)
    other = m.polar((mx, my, 0), 2500, heading - 90, 0)
    tags = ["NowhereFragment", "Fragment_%02d" % (k + 1)]
    if k == 0:    # Neo Tokyo: towers, a crossing
        for p in (side, other):
            m.place_kit("%s/%s" % (K1, rng.choice(HIGHRISE)), p[0], p[1], 0, 0, 0.7, "Tower", folder, tags, material=g)
        for j in range(-4, 5):
            q = m.polar((mx, my, 0), j * 220, heading, 1)
            m.block(q, (110, 900, 2), "CrossingStripe", folder, g, yaw=heading, collision=False)
    elif k == 1:  # Ruins: rubble and a toppled tower over the path
        m.block((mx, my, 900), (1200, 9000, 1200), "ToppledTower", folder, g, yaw=heading + 70, roll=5, tags=tags)
    elif k == 2:  # Docks: a pier stub and the lighthouse, sunk
        m.block((side[0], side[1], 2000), (1500, 1500, 7000), "Lighthouse", folder, g, shape="cylinder", tags=tags)
        m.block((other[0], other[1], -20), (6000, 900, 40), "Pier", folder, g, yaw=heading + 90)
    elif k == 3:  # Hanging City: chain links rising out of the ground into the fog
        for j in range(40):
            m.block((side[0], side[1], j * 2000 + 1000), (260, 1400 if j % 2 else 260, 2400), "ChainLink", folder, g, tags=tags)
    elif k == 4:  # A.R.C TECH: a dead conveyor with grey cubes
        m.block((mx, my, 45), (9000, 600, 90), "Conveyor", folder, g, yaw=heading + 25, tags=tags)
        for j in range(30):
            q = m.polar((mx, my, 0), -4000 + j * 280, heading + 25, 110)
            m.block(q, (30, 30, 30), "Cube", folder, g, collision=False)
    elif k == 5:  # Moon Kingdom: a house and a grey moon low over the path
        m.place_kit("%s/%s" % (K1, rng.choice(COTTAGE)), side[0], side[1], -200, 90, 0.9, "House", folder, tags, material=g)
        m.block((mx, my, 9000), (9000, 9000, 9000), "Moon", folder, g, shape="sphere", collision=False)
    elif k == 6:  # T.O.M.B.S: office desks on the path, a wall too tall to see over
        for j in range(16):
            q = m.polar((mx, my, 0), rng.uniform(-5000, 5000), heading, 0)
            q = m.polar(q, rng.uniform(-500, 500), heading + 90, 40)
            m.block(q, (160, 80, 80), "Desk", folder, g, yaw=heading, tags=tags)
        m.block((other[0], other[1], 25000), (400, 8000, 50000), "MegaWall", folder, g, yaw=heading)
    elif k == 7:  # Intersection: a tunnel segment the path walks through
        for s in (-1, 1):
            q = m.polar((mx, my, 0), 800 * s, heading + 90, 1300)
            m.block(q, (8000, 100, 2600), "TunnelWall", folder, g, yaw=heading, tags=tags)
        m.block((mx, my, 2650), (8000, 1700, 100), "TunnelRoof", folder, g, yaw=heading)
    elif k == 8:  # Remnant Remains: a dune and the data centre block
        m.block((side[0], side[1], -400), (8000, 5000, 2000), "Dune", folder, g, shape="sphere", tags=tags)
        m.block((other[0], other[1], 4000), (5000, 5000, 8000), "DataCentre", folder, g)
    else:         # Hallucinations: a floating island and a giant teacup, hanging still
        m.block((side[0], side[1], 4000), (6000, 6000, 1500), "FloatingIsland", folder, g, shape="cylinder", tags=tags)
        m.block((other[0], other[1], 700), (1800, 1800, 1400), "GiantTeacup", folder, g, shape="cylinder")


# ============================================================================ 12. Paradise

def ch12_paradise(m, rng, log):
    """A beautiful neo-tropical city, not ruined, bright vibrant colours, constant good vibes, quite heavy rainfall."""
    m.lighting(sun=6.0, sun_colour=(255, 240, 210), pitch=-55, sky=1.5, fog=0.015, fog_colour=(0.5, 0.65, 0.75))
    bright = ["Coral", "Turquoise", "Lime", "Sunflower", "Magenta"]
    half = 35000
    _ground(m, half, "Sidewalk")
    m.rain_placeholder(half, text="HEAVY RAINFALL PLACEHOLDER (bright sun stays)")
    # Canals in a cross; swim them or take the bridges.
    f = "Canals"
    for horizontal in (True, False):
        for off in (-12000, 12000):
            x0, y0, x1, y1 = (-half, off - 1500, half, off + 1500) if horizontal else (off - 1500, -half, off + 1500, half)
            m.box(x0, y0, -1200, x1, y1, -1000, "CanalBed", f, "Turquoise")
            m.water(x0, y0, -1000, x1, y1, -80, "Canal", f)
            for along in range(-half + 4000, half, 8000):
                bx, by = (along, off) if horizontal else (off, along)
                m.block((bx, by, 60), (700, 3600, 60) if horizontal else (3600, 700, 60), "CanalBridge", f + "/Bridges", "Sunflower")
    # Lagoon and beach in one corner (boss section on the beach).
    f = "Lagoon"
    m.box(14000, 14000, -2200, half, half, -2000, "LagoonBed", f, "Turquoise")
    m.water(14000, 14000, -2000, half, half, -80, "Lagoon", f)
    m.box(14000, 14000, -150, 24000, 24000, 0, "Beach", f, "Sand")
    m.section(4, (19000, 19000, 0), "Section4_Beach", radius=4500, boss=True, gate_yaw=225)
    # City blocks in bright colours with rooftop gardens, waterfalls off the roofs, palms everywhere.
    f = "City"
    gardens = []
    for i in range(-3, 4):
        for j in range(-3, 4):
            x, y = i * 9000 + 1500, j * 9000 + 1500
            if (x > 12000 and y > 12000) or abs(abs(x) - 12000) < 2600 or abs(abs(y) - 12000) < 2600:
                continue
            if (i, j) in ((-3, 0), (-1, -2), (1, 0)):
                continue   # section plazas
            path = "%s/%s" % (K1, rng.choice(HIGHRISE + CORP + CITYSCAPE))
            b = m.place_kit(path, x, y, 0, rng.choice([0, 90, 180, 270]), 0.7, "Building", f, ["Building", "Climbable"],
                            material=rng.choice(bright))
            if b["top"] < 6000 and rng.random() < 0.6:
                gardens.append(b)
                x0, y0, x1, y1 = b["rect"]
                m.block(((x0 + x1) / 2, (y0 + y1) / 2, b["top"] + 50), (x1 - x0, y1 - y0, 100), "RooftopGarden", f + "/Gardens", "Palm",
                        tags=["RooftopGarden"])
                m.block((x1 + 60, (y0 + y1) / 2, b["top"] / 2), (100, 600, b["top"]), "Waterfall", f + "/Waterfalls", "Water",
                        collision=False, tags=["Waterfall"])
                m.climb_wall((x0, (y0 + y1) / 2, 0), 180, 500, b["top"], f + "/Climb")
    for _ in range(140):
        x, y = rng.uniform(-half + 1000, half - 1000), rng.uniform(-half + 1000, half - 1000)
        h = rng.uniform(600, 1200)
        m.block((x, y, h / 2), (60, 60, h), "PalmTrunk", "Palms", "Trunk", roll=rng.uniform(-8, 8))
        m.block((x, y, h + 100), (600, 600, 150), "PalmCrown", "Palms", "Palm", shape="sphere", collision=False)
    # Sky bridges between rooftop gardens (jump-up planters lead up to them).
    for k in range(min(8, len(gardens) - 1)):
        a, b = gardens[k], gardens[k + 1]
        ca = ((a["rect"][0] + a["rect"][2]) / 2, (a["rect"][1] + a["rect"][3]) / 2, a["top"] + 100)
        cb = ((b["rect"][0] + b["rect"][2]) / 2, (b["rect"][1] + b["rect"][3]) / 2, b["top"] + 100)
        if math.hypot(cb[0] - ca[0], cb[1] - ca[1]) < 20000:
            _route(m, ca, cb, "Traversal/SkyBridges", "Magenta", width=400, tags=["SkyBridge"])
    for k in range(6):
        p = (rng.uniform(-20000, 10000), rng.uniform(-20000, 10000), 0)
        m.jump_steps(p, rng.uniform(0, 360), 6, "Traversal/Planters", material="Jump")
    m.section(1, (-25500, 1500, 0), "Section1_HarbourPlaza", gate_yaw=180)
    m.section(2, (-7500, -16500, 0), "Section2_CanalDistrict", gate_yaw=180)
    m.section(3, (10500, 1500, 0), "Section3_GardenSquare", gate_yaw=180)
    _start(m, (-32000, 1500, 0), yaw=0)
    for k in range(30):
        p = (rng.uniform(-half, half), rng.uniform(-half, half), rng.uniform(800, 3000))
        m.block(p, (40, 300, 600), "NEON_SIGN_PLACEHOLDER", "Dressing/Neon", rng.choice(NEON), collision=False, tags=["NeonSignPlaceholder"])


# ============================================================================ 13. A Secretive Soul

def ch13_soul(m, rng, log):
    """A small Japanese countryside town in low to medium rain; in the middle a pure black monolith with red hieroglyphics.
    Getting close plays the voice of the angel god (trigger + line placeholder)."""
    m.lighting(sun=1.2, sun_colour=(210, 215, 225), pitch=-35, sky=0.9, fog=0.04, fog_colour=(0.4, 0.43, 0.45), volumetric=True)
    half = 30000
    _ground(m, half + 8000, "Grass")
    m.rain_placeholder(half, text="LOW-MEDIUM RAIN PLACEHOLDER")
    # Hills ring the valley.
    for k in range(14):
        p = m.polar((0, 0, 0), rng.uniform(30000, 36000), k * 360 / 14, 0)
        m.block((p[0], p[1], -1500), (rng.uniform(15000, 22000), 12000, rng.uniform(8000, 14000)), "Hill", "Hills", "Grass",
                shape="sphere", yaw=k * 360 / 14)
    # The monolith in the middle (boss section around it).
    f = "Monolith"
    m.block((0, 0, 2500), (600, 1600, 5000), "Monolith", f, "Monolith", tags=["Monolith"])
    for k in range(60):
        face = rng.choice([-1, 1])
        m.block((face * 305, rng.uniform(-700, 700), rng.uniform(300, 4700)), (10, rng.uniform(60, 180), rng.uniform(60, 220)),
                "RedGlyph", f + "/Glyphs", "RedGlyph", collision=False, tags=["RedGlyph"])
    voice = m.spawn(unreal.TriggerBox, (0, 0, 300), label="AngelGodVoice_Trigger", folder=f, tags=["AngelGodVoice"])
    try:
        voice.get_editor_property("collision_component").set_editor_property("box_extent", unreal.Vector(2500, 2500, 600))
    except Exception as error:
        log("warn: monolith trigger extent not set (%s)" % error)
    m.label("VOICE OF THE ANGEL GOD: " + CH13_VOICE, (0, 0, 6000), f, 220)
    m.section(4, (0, 0, 0), "Section4_Monolith", radius=4500, boss=True, gate_yaw=180)
    # Rice paddies (shallow water) and a river with a deep pool to swim.
    f = "Paddies"
    for i in range(4):
        for j in range(3):
            x, y = -24000 + i * 3500, -6000 + j * 3500
            m.box(x - 1600, y - 1600, -60, x + 1600, y + 1600, -40, "PaddyBed", f, "Paddy")
            m.water(x - 1600, y - 1600, -40, x + 1600, y + 1600, 0, "Paddy", f)
            m.box(x - 1750, y - 1750, 0, x + 1750, y - 1600, 60, "PaddyBank", f, "Grass", tags=["JumpUp"])
    f = "River"
    m.box(-half, 13000, -1600, half, 16000, -1400, "RiverBed", f, "Rock")
    m.water(-half, 13000, -1400, half, 16000, -100, "River", f)
    for x in (-15000, 0, 15000):
        m.block((x, 14500, 120), (600, 3600, 80), "WoodBridge", f, "Wood", tags=["Bridge"])
    m.section(1, (-19000, -2500, 0), "Section1_RiceTerraces", gate_yaw=180)
    _start(m, (-27000, -12000, 0), yaw=45)
    # Village street of wooden houses, stone walls to jump, a torii and the shrine up a long stair.
    f = "Village"
    for k in range(22):
        x, y = rng.uniform(-6000, 20000), rng.choice([-1, 1]) * rng.uniform(5500, 11000)
        if math.hypot(x, y) < 6000:
            continue
        _house(m, x, y, 0, rng.uniform(800, 1400), rng.uniform(700, 1000), rng.uniform(350, 600), rng.choice([0, 90]), f,
               tags=["Climbable"])
    for k in range(10):
        m.block((rng.uniform(-6000, 20000), rng.uniform(-11000, 11000), 60), (rng.uniform(500, 1500), 150, 120), "StoneWall",
                f + "/Walls", "Rock", yaw=rng.choice([0, 90]), tags=["JumpUp"])
    m.section(2, (10000, -8000, 0), "Section2_VillageStreet", gate_yaw=180)
    f = "Shrine"
    sx, sy = 22000, 6000
    for side in (-1, 1):
        m.block((sx - 3000, sy + side * 450, 450), (80, 80, 900), "ToriiPost", f, "NeonRed", tags=["Torii"])
    m.block((sx - 3000, sy, 950), (100, 1300, 80), "ToriiBeam", f, "NeonRed", tags=["Torii"])
    top = m.stairs((sx - 2800, sy, 0), 0, 50, 40, 60, 500, f)
    m.box(top[0], sy - 4500, 0, top[0] + 9000, sy + 4500, top[2], "ShrineHill", f, "Grass")
    _house(m, top[0] + 6500, sy, top[2], 1600, 1200, 600, 90, f, wall="Wood", roof="NeonRed", tags=["Shrine"])
    m.climb_wall((top[0], sy + 3000, 0), 180, 1500, top[2], f + "/Climb")
    m.section(3, (top[0] + 3500, sy, top[2]), "Section3_Shrine", radius=3500, gate_yaw=180)
    for _ in range(80):
        p = m.polar((0, 0, 0), rng.uniform(20000, 28000), rng.uniform(0, 360), 0)
        h = rng.uniform(800, 1500)
        m.block((p[0], p[1], h / 2), (80, 80, h), "TreeTrunk", "Forest", "Trunk")
        m.block((p[0], p[1], h + 200), (700, 700, 900), "Canopy", "Forest", "Tree", shape="cone", collision=False)


BUILDERS = {
    2: ch02_ruins, 3: ch03_docks, 4: ch04_hanging, 5: ch05_arctech, 6: ch06_moon, 7: ch07_tombs, 8: ch08_intersection,
    9: ch09_remnant, 10: ch10_hallucinations, 11: ch11_nowhere, 12: ch12_paradise, 13: ch13_soul,
}
