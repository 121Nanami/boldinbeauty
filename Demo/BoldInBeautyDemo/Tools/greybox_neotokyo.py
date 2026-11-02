"""L_NeoTokyo greybox (owner request 2026-10-07): a Neo Tokyo of many buildings and many streets in heavy rain and fog,
travelled mainly by climbing buildings and running the streets. Placeholders for neon signs and other dressing.

Layout (assistant's greybox reading, all PROPOSED): an 8 x 8 grid of 60 m city blocks; streets of three widths (alleys
14 m, streets 24 m, one 36 m avenue each way) meet at a scramble crossing in the centre, where the player starts. Kit 2
towers form the skyline ring; inner blocks hold Kit 2 mid-rises or clusters of four Kit 1 towers with alleys between them.
Orange = climbable: facade climb routes (pipe, ledges every 4 m, fire-escape landings) and rooftop bridges across
streets at 12 / 24 / 36 m. Rain is a marked placeholder volume (no Niagara system yet); fog is real height fog.
"""

import math
import random

import unreal

import greybox_bundle
import greybox_detail
import greybox_detail2
from greybox_common import NEON, MAPS, GreyboxMap, try_set

K1 = "/Game/Greybox/Kits/K1"
K2 = "/Game/Greybox/Kits/K2/Buildings"
BLOCK = 6000.0
GAPS = [1400, 2400, 1400, 3600, 1400, 2400, 1400]   # street widths between the 8 blocks; index 3 is the avenue
N = 8
SIDEWALK = 15.0
SKYLINE = [2, 3, 16, 17, 18, 23, 24, 60, 46]
MIDRISE = [4, 5, 6, 7, 8, 9, 10, 12, 15, 19, 31, 32, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59]
TILES = [33, 34, 35, 36, 11, 13, 14]
K1_TOWERS = (["HighRise/SM_K1_HighRise_%d" % i for i in range(1, 15)] + ["Skyscraper/SM_K1_Skyscraper_%d" % i for i in (1, 2, 3)]
             + ["Corporation/SM_K1_Corporation_%d" % i for i in range(1, 11)])


def _axis():
    """Block start coordinates along one axis, centred on 0."""
    total = N * BLOCK + sum(GAPS)
    starts, x = [], -total / 2
    for i in range(N):
        starts.append(x)
        x += BLOCK + (GAPS[i] if i < N - 1 else 0)
    return starts, total


def _fit(m, path, room, yaw):
    w, d, _ = m.kit_footprint(path, 1.0, yaw)
    return min(1.0, room / max(w, d))


def build(materials, log):
    rng = random.Random(2009)   # the Neo Tokyo year in MAIN; only seeds the layout
    m = GreyboxMap(MAPS + "/L_NeoTokyo", materials, log)
    starts, total = _axis()
    half = total / 2

    # ---------------------------------------------------------------- sky, fog, rain
    moon = m.spawn(unreal.DirectionalLight, (0, 0, 20000), yaw=40, pitch=-35, label="Moon", folder="Lighting")
    moon.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    try_set(moon.light_component, "intensity", 0.6, log)
    try_set(moon.light_component, "light_color", unreal.Color(r=150, g=170, b=255, a=255), log)
    try_set(moon.light_component, "atmosphere_sun_light", True, log)
    sky = m.spawn(unreal.SkyLight, (0, 0, 20000), label="SkyLight", folder="Lighting")
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    try_set(sky.light_component, "real_time_capture", True, log)
    try_set(sky.light_component, "intensity", 0.6, log)
    m.spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="Lighting")
    fog = m.spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeavyFog", folder="Lighting")
    fc = fog.component
    try_set(fc, "fog_density", 0.12, log)
    try_set(fc, "fog_height_falloff", 0.04, log)
    try_set(fc, "fog_inscattering_luminance", unreal.LinearColor(0.08, 0.07, 0.14, 1.0), log)
    try_set(fc, "enable_volumetric_fog", True, log)
    try_set(fc, "volumetric_fog_extinction_scale", 2.5, log)
    rain = m.box(-half, -half, 0, half, half, 30000, "RAIN_VOLUME_PLACEHOLDER", "FX", "NeonCyan", tags=["RainPlaceholder"],
                 collision=False, hidden_in_game=True)
    rain.static_mesh_component.set_editor_property("visible", False)
    m.label("RAIN PLACEHOLDER - heavy rain FX (Niagara) covers the whole map", (0, 0, 9000), "FX", 600, in_game=False,
            colour=(0, 220, 255))

    # ---------------------------------------------------------------- ground
    m.box(-half - 8000, -half - 8000, -200, half + 8000, half + 8000, 0, "WetRoad", "Ground", "WetRoad")
    for i, x in enumerate(starts):
        for j, y in enumerate(starts):
            m.box(x - 200, y - 200, 0, x + BLOCK + 200, y + BLOCK + 200, SIDEWALK, "Sidewalk", "Ground", "Sidewalk")
    edge = half + 2000
    for sx, sy, w, d in ((0, edge, total + 4000, 100), (0, -edge, total + 4000, 100), (edge, 0, 100, total + 4000), (-edge, 0, 100, total + 4000)):
        wall = m.block((sx, sy, 15000), (w, d, 30000), "BoundaryBlocker", "Ground/Blockers", hidden_in_game=True)
        wall.static_mesh_component.set_editor_property("visible", False)   # collision only

    # ---------------------------------------------------------------- blocks
    occupants = {}
    for i, x0 in enumerate(starts):
        for j, y0 in enumerate(starts):
            cx, cy = x0 + BLOCK / 2, y0 + BLOCK / 2
            folder = "Buildings/Block_%d_%d" % (i, j)
            outer = i in (0, N - 1) or j in (0, N - 1)
            occ = []
            kind = "skyline" if outer else rng.choice(["mid", "mid", "cluster", "cluster", "tile"])
            yaw = rng.choice([0, 90, 180, 270])
            if kind in ("skyline", "mid"):
                path = "%s/SM_K2_%d" % (K2, rng.choice(SKYLINE if kind == "skyline" else MIDRISE))
                scale = max(0.55, _fit(m, path, BLOCK - 300, yaw))
                occ.append(m.place_kit(path, cx, cy, SIDEWALK, yaw, scale, "Building_%d_%d" % (i, j), folder, ["Building", "Climbable"]))
            elif kind == "tile":
                path = "%s/SM_K2_%d" % (K2, rng.choice(TILES))
                occ.append(m.place_kit(path, cx, cy, SIDEWALK, yaw, _fit(m, path, BLOCK - 300, yaw), "LowRise_%d_%d" % (i, j), folder,
                                       ["Building", "Climbable"]))
            else:
                q = BLOCK / 4
                for k, (ox, oy) in enumerate(((-q, -q), (q, -q), (-q, q), (q, q))):
                    path = "%s/%s" % (K1, rng.choice(K1_TOWERS))
                    t_yaw = rng.choice([0, 90, 180, 270])
                    scale = min(1.7, (BLOCK / 2 - 500) / max(m.kit_footprint(path, 1.0, t_yaw)[:2]))
                    occ.append(m.place_kit(path, cx + ox, cy + oy, SIDEWALK, t_yaw, scale, "Tower_%d_%d_%d" % (i, j, k), folder,
                                           ["Building", "Climbable"]))
            occupants[(i, j)] = occ
            for o in occ:
                _dress_facades(m, rng, o, folder)
            # Back-alley clutter between cluster towers: overhead cables, bins, steam vents.
            if kind == "cluster":
                for _ in range(3):
                    z = rng.uniform(700, 1400)
                    m.block((cx + rng.uniform(-300, 300), cy, z), (20, BLOCK - 1200, 20), "OverheadCable", folder + "/Dressing", "Dark",
                            yaw=rng.choice([0, 90]))
                m.block((cx, cy, SIDEWALK + 60), (120, 80, 120), "PROP_Bin", folder + "/Dressing", "Dark")
                m.block((cx + 200, cy - 200, SIDEWALK + 20), (60, 60, 40), "PROP_SteamVent", folder + "/Dressing", "Metal",
                        shape="cylinder", tags=["SteamVentPlaceholder"])

    # ---------------------------------------------------------------- rooftop bridges across streets
    bridges = 0
    for (i, j), occ in occupants.items():
        for di, dj in ((1, 0), (0, 1)):
            other = occupants.get((i + di, j + dj))
            if not other or rng.random() > 0.65:
                continue
            bridges += _bridge(m, rng, occ, other, di == 1, "Traversal/RooftopBridges")
    log("neo tokyo: %d rooftop bridges" % bridges)

    # ---------------------------------------------------------------- streets: lamps, cars, vending machines, puddles
    centres = [s + BLOCK + GAPS[k] / 2 for k, s in enumerate(starts[:-1])]
    for k, c in enumerate(centres):
        width = GAPS[k]
        for along in range(int(-half), int(half), 4000):
            for side in (-1, 1):
                for horizontal in (True, False):
                    off = c + side * (width / 2 - 150)
                    x, y = (along, off) if horizontal else (off, along)   # on the kerb, so never inside a building
                    m.block((x, y, SIDEWALK + 300), (20, 20, 600), "LampPost", "Streets/Lamps", "Metal", shape="cylinder")
                    m.block((x, y, SIDEWALK + 620), (60, 60, 30), "LampHead", "Streets/Lamps", "Lamp", collision=False)
        for _ in range(10):
            along = rng.uniform(-half, half)
            horizontal = rng.random() < 0.5
            lane = c + rng.choice([-1, 1]) * (width / 2 - 450)
            x, y = (along, lane) if horizontal else (lane, along)
            if _in_block(starts, x, y):
                continue
            pick = rng.random()
            if pick < 0.5:
                m.block((x, y, 75), (450, 180, 150), "PROP_Car", "Streets/Cars", "Metal", yaw=0 if horizontal else 90,
                        tags=["CarPlaceholder"])
            elif pick < 0.75:
                m.block((x, y, 95), (90, 70, 190), "PROP_VendingMachine", "Streets/Props", rng.choice(NEON), yaw=0 if horizontal else 90)
            else:
                m.block((x, y, 1), (rng.uniform(200, 600), rng.uniform(150, 400), 2), "PROP_Puddle", "Streets/Puddles", "Glass",
                        collision=False)

    # ---------------------------------------------------------------- the scramble crossing (centre), player start
    av = GAPS[3]
    for k in range(-8, 9):
        for horizontal in (True, False):
            p = k * 220
            if horizontal:
                m.block((p, av / 2 - 300, 1), (110, 500, 2), "CrossingStripe", "Streets/Crossing", "Marking", collision=False)
                m.block((p, -av / 2 + 300, 1), (110, 500, 2), "CrossingStripe", "Streets/Crossing", "Marking", collision=False)
            else:
                m.block((av / 2 - 300, p, 1), (500, 110, 2), "CrossingStripe", "Streets/Crossing", "Marking", collision=False)
                m.block((-av / 2 + 300, p, 1), (500, 110, 2), "CrossingStripe", "Streets/Crossing", "Marking", collision=False)
    for k in range(-6, 7):   # the diagonal scramble
        p = k * 260
        m.block((p, p, 1), (110, 500, 2), "CrossingStripe", "Streets/Crossing", "Marking", yaw=45, collision=False)
        m.block((p, -p, 1), (110, 500, 2), "CrossingStripe", "Streets/Crossing", "Marking", yaw=-45, collision=False)
    # One giant screen on each corner block, facing the crossing across the avenue.
    face = av / 2 + 200 + 60
    for sx, sy in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
        m.block((sx * face, sy * (face + 1800), 2200), (40, 3000, 1700), "HOLO_AD_PLACEHOLDER", "Streets/Crossing", "Holo",
                tags=["HoloAdPlaceholder"], collision=False)
    m.label("SCRAMBLE CROSSING", (0, 0, 1500), "Labels", 400)
    m.spawn(unreal.PlayerStart, (0, 0, 120), label="PlayerStart", folder="Gameplay")
    m.point_light((0, 0, 1500), "Lighting", 200000, 6000, (0.6, 0.7, 1.0))
    # Detail pass (owner 2026-10-07: 8,000-12,000 pieces): rooftops, traffic lights, shop fronts, street furniture, power lines.
    greybox_detail.neotokyo(m, random.Random(2010), occupants, starts, GAPS, BLOCK, half, SIDEWALK)
    greybox_detail2.neotokyo(m, log)   # second detail pass (owner 2026-10-07): fire escapes, balconies, signs, street props
    greybox_bundle.neotokyo(m, log, half)   # owner 2026-10-08: street and rooftop props from the Elite_Bundle assets

    m.world_settings(unreal.GameModeBase.static_class())
    return m.save()


def _in_block(starts, x, y):
    def inside(v):
        return any(s - 200 <= v <= s + BLOCK + 200 for s in starts)
    return inside(x) and inside(y)


def _dress_facades(m, rng, occ, folder):
    """Neon signs on every side, a facade billboard on tall buildings, and one or two climb routes."""
    x0, y0, x1, y1 = occ["rect"]
    top = occ["top"]
    sides = [("x-", x0, (y0, y1)), ("x+", x1, (y0, y1)), ("y-", y0, (x0, x1)), ("y+", y1, (x0, x1))]
    for name, face, (a, b) in sides:
        normal = -1 if name.endswith("-") else 1
        along_x = name.startswith("y")
        for _ in range(rng.randint(1, 3)):
            t = rng.uniform(a + 150, b - 150) if b - a > 300 else (a + b) / 2
            z = rng.uniform(450, min(2200, max(500, top - 300)))
            h = rng.uniform(350, 900)
            pos = (t, face + normal * 70, z) if along_x else (face + normal * 70, t, z)
            # Vertical blade sign sticking out of the facade, the classic Tokyo look.
            m.block(pos, (140, 25, h) if not along_x else (25, 140, h), "NEON_SIGN_PLACEHOLDER", folder + "/Neon", rng.choice(NEON),
                    tags=["NeonSignPlaceholder"], collision=False)
        if top > 4500 and rng.random() < 0.35:
            t = (a + b) / 2
            w = min(2400, (b - a) * 0.7)
            pos = (t, face + normal * 30, rng.uniform(2200, min(4000, top - 800)))
            m.block(pos, (w, 20, w * 0.55) if along_x else (20, w, w * 0.55), "BILLBOARD_PLACEHOLDER", folder + "/Neon",
                    rng.choice(NEON + ["Holo"]), tags=["BillboardPlaceholder"], collision=False)
    for name, face, (a, b) in rng.sample(sides, rng.randint(1, 2)):
        normal = -1 if name.endswith("-") else 1
        along_x = name.startswith("y")
        t = rng.uniform(a + 200, b - 200) if b - a > 400 else (a + b) / 2
        _climb_route(m, rng, t, face, normal, along_x, min(top - 100, 4800), folder + "/ClimbRoutes")


def _climb_route(m, rng, t, face, normal, along_x, height, folder):
    """Drain pipe + hand ledges every 4 m + a fire-escape landing every 8 m, flush against a facade (orange = climbable)."""
    if height < 600:
        return

    def at(along, out, z):
        return (along, face + normal * out, z) if along_x else (face + normal * out, along, z)

    tags = ["Climbable", "ClimbRoute"]
    m.block(at(t, 20, height / 2), (24, 24, height), "ClimbPipe", folder, "Climb", shape="cylinder", tags=tags)
    z = 400.0
    k = 0
    while z < height:
        side = 1 if k % 2 else -1
        m.block(at(t + side * 120, 30, z), (220, 50, 20) if along_x else (50, 220, 20), "ClimbLedge", folder, "Climb", tags=tags)
        if k % 2 == 1:
            m.block(at(t + side * 150, 90, z - 10), (320, 160, 15) if along_x else (160, 320, 15), "FireEscapeLanding", folder,
                    "Climb", tags=tags)
        z += 400.0
        k += 1


def _bridge(m, rng, occ_a, occ_b, along_x, folder):
    """A plank across the street between the facing faces of the nearest buildings, at a height both can reach."""
    if along_x:
        a = max(occ_a, key=lambda o: o["rect"][2])
        b = min(occ_b, key=lambda o: o["rect"][0])
        start, end = a["rect"][2], b["rect"][0]
        lo, hi = max(a["rect"][1], b["rect"][1]), min(a["rect"][3], b["rect"][3])
    else:
        a = max(occ_a, key=lambda o: o["rect"][3])
        b = min(occ_b, key=lambda o: o["rect"][1])
        start, end = a["rect"][3], b["rect"][1]
        lo, hi = max(a["rect"][0], b["rect"][0]), min(a["rect"][2], b["rect"][2])
    if hi - lo < 300 or end <= start:
        return 0
    tiers = [z for z in (1200, 2400, 3600) if z < min(a["top"], b["top"]) - 300]
    if not tiers:
        return 0
    z = rng.choice(tiers)
    c = rng.uniform(lo + 150, hi - 150)
    length = end - start + 600   # 3 m landing on each side
    mid = (start + end) / 2
    tags = ["Climbable", "RooftopBridge"]
    if along_x:
        m.block((mid, c, z), (length, 200, 20), "RooftopBridge", folder, "Climb", tags=tags)
        m.railing((start - 300, c + 100, z), (end + 300, c + 100, z), folder, spacing=600)
    else:
        m.block((c, mid, z), (200, length, 20), "RooftopBridge", folder, "Climb", tags=tags)
        m.railing((c + 100, start - 300, z), (c + 100, end + 300, z), folder, spacing=600)
    return 1
