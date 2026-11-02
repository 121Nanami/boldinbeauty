"""L_Ark greybox (owner request 2026-10-07): the underground arc where humanity survives, one map with its areas laid out
along +X, west to east:

1. Operators' area: a large dome full of identical operator rooms (distinguished only by wear and tear) on four tiers
   around a central roll-call area. The only way out: two catwalks with minimal railing across a massive hole.
2. Town square: everything a city needs (shops, restaurants, gyms ...), the striking government building in the middle,
   the armoury, the unmarked "task" room with many chutes up to the surface (how operators reach it), ladders along the
   edges down to the Underworld, and an unmarked building with a small red stripe: the resistance meeting point.
   Underworld: under the town square, where the criminals are and the "trade" is done.
3. Civilian area: a 2050s utopia with fake lush grass and trees, mansion-like houses (only the ultra rich survived).
4. Secret dome: a long catwalk out into the sky and a single long staircase up to a large dome where the Secrets live.

What the owner did not specify is the assistant's greybox reading (PROPOSED, OPEN): sizes, where the civilian area and
the secret catwalk connect (a gate east of the town square; the catwalk leaves the far end of the civilian area), the shop
mix, the cavern ceiling at 150 m, the fake-sky ceiling panel and the wear-and-tear props.
"""

import math
import random

import unreal

import greybox_ark_hub
import greybox_bundle
import greybox_detail2
from greybox_common import NEON, MAPS, GreyboxMap, try_set

K1 = "/Game/Greybox/Kits/K1"
K2 = "/Game/Greybox/Kits/K2/Buildings"

# Region bounds along X (cm). The cavern is 420 m wide (Y -21000..21000).
DOME_R = 12000.0
HOLE = (13000.0, 33000.0)
TOWN = (33000.0, 73000.0)
CIV = (75000.0, 115000.0)
HALF_Y = 21000.0
CEILING = 15000.0
UNDER_FLOOR = -4500.0
TOWN_SLAB = -500.0

# Operator rooms.
ROOMS_PER_TIER = 80
TIERS = 4
TIER_H = 500.0
ROOM_H = 450.0
R_WALK = 9000.0
R_IN = 10000.0
R_OUT = 10800.0
EXIT_HALF_ANGLE = 9.0
CATWALK_Y = 1500.0


def build(materials, log):
    rng = random.Random(2002)   # the catastrophe year in MAIN; only seeds the layout
    m = GreyboxMap(MAPS + "/L_Ark", materials, log)
    _lighting(m, log)
    _cavern(m)
    _operators_dome(m, rng, log)
    _hole_and_catwalks(m, rng)
    specials = _town_square(m, rng, log)
    _underworld(m, rng)
    # The Ark is the hub (owner request 2026-10-07): stations, factory, stalls, arcade ... (greybox_ark_hub.py).
    greybox_ark_hub.build(m, rng, log, specials)
    _civilian(m, rng)
    _secret_dome(m)
    m.spawn(unreal.PlayerStart, (0, 0, 120), label="PlayerStart_RollCall", folder="Gameplay")
    # Second detail pass (owner 2026-10-07: the Ark with more details): furniture, planters, kiosks, crates, lights.
    greybox_detail2.ark(m, log, TOWN, CIV, HALF_Y, UNDER_FLOOR, -DOME_R - 3000,
                        keep_rects=list(greybox_ark_hub.RESERVED.values()) + [greybox_ark_hub.FACTORY],
                        keep_circles=[(x, y, 900) for x, y, _ in greybox_ark_hub.ANCHOR_POS] + [(greybox_ark_hub.TOWN_CX, 0, 4500)],
                        dome=(0.0, 0.0, R_WALK - 500, R_OUT + 700))
    # Owner 2026-10-08: fill the Ark with the Elite_Bundle assets (themed per area: market food, library books, Underworld crates ...).
    greybox_bundle.ark(m, log, TOWN, CIV, HALF_Y, UNDER_FLOOR, -DOME_R - 3000, greybox_ark_hub.RESERVED, greybox_ark_hub.FACTORY,
                       greybox_ark_hub.ANCHOR_POS)
    for name, pos in (("Operators", (0, 0, 200)), ("TownSquare", (TOWN[0] + 3000, 0, 200)), ("Underworld", (TOWN[0] + 4000, 0, UNDER_FLOOR + 200)),
                      ("Civilian", (CIV[0] + 2000, 0, 200)), ("SecretDome", (163000, 0, 20200))):
        m.spawn(unreal.TargetPoint, pos, label="Marker_" + name, folder="Gameplay", tags=["AreaMarker", name])
    # The Ark is the default level (owner 2026-10-11): it must carry the game's own game mode, not only get it from the Hub's
    # travel option, because the game and the editor now start in this map.
    mode = None
    try:
        mode = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BIB/Blueprints/BP_BIBGameMode")
    except Exception:
        mode = None
    m.world_settings(mode or unreal.BIBGameMode.static_class())
    return m.save()


# ============================================================================ shell

def _lighting(m, log):
    # Underground, so the "sun" casts no shadows: it only keeps the greybox readable everywhere.
    sun = m.spawn(unreal.DirectionalLight, (0, 0, 30000), yaw=30, pitch=-60, label="FillLight_NoShadows", folder="Lighting")
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    try_set(sun.light_component, "intensity", 1.2, log)
    try_set(sun.light_component, "cast_shadows", False, log)
    try_set(sun.light_component, "atmosphere_sun_light", True, log)
    sky = m.spawn(unreal.SkyLight, (0, 0, 30000), label="SkyLight", folder="Lighting")
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    try_set(sky.light_component, "real_time_capture", True, log)
    m.spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="Lighting")
    fog = m.spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeightFog", folder="Lighting")
    try_set(fog.component, "fog_density", 0.01, log)


def _cavern(m):
    f = "Cavern"
    x0, x1 = -DOME_R - 2000, CIV[1]
    # Floors: operators' side up to the hole, civilian area; the town square has its own slab over the Underworld.
    m.box(x0, -HALF_Y, -200, HOLE[0], HALF_Y, 0, "Floor_Operators", f, "Concrete")
    m.box(TOWN[1], -HALF_Y, -200, CIV[1], HALF_Y, 0, "Floor_Gate", f, "Concrete")
    # Rock ceiling and walls (the hole's walls go all the way down).
    m.box(x0, -HALF_Y - 1000, CEILING, x1, HALF_Y + 1000, CEILING + 1000, "RockCeiling", f, "Dark")
    for side in (-1, 1):
        m.box(x0, side * HALF_Y, -32000, x1, side * (HALF_Y + 1000), CEILING, "RockWall", f, "Dark")
    m.box(x0 - 1000, -HALF_Y, -200, x0, HALF_Y, CEILING, "RockWall_West", f, "Dark")
    # East wall with the opening the secret catwalk leaves through.
    m.box(x1, -HALF_Y, -200, x1 + 1000, -250, CEILING, "RockWall_East", f, "Dark")
    m.box(x1, 250, -200, x1 + 1000, HALF_Y, CEILING, "RockWall_East", f, "Dark")
    m.box(x1, -250, 600, x1 + 1000, 250, CEILING, "RockWall_East", f, "Dark")
    # Wall between the town square and the civilian area with the civilian gate.
    gx0, gx1 = TOWN[1], CIV[0]
    m.box(gx0, -HALF_Y, 0, gx1, -1200, CEILING, "Wall_CivilianGate", f, "Concrete")
    m.box(gx0, 1200, 0, gx1, HALF_Y, CEILING, "Wall_CivilianGate", f, "Concrete")
    m.box(gx0, -1200, 1200, gx1, 1200, CEILING, "Wall_CivilianGate", f, "Concrete")
    m.box(gx0 + 800, -1200, 0, gx1 - 800, -900, 300, "GateBooth", f, "Metal")
    m.box(gx0 + 900, -900, 80, gx0 + 1000, 900, 120, "GateBarrier", f, "NeonYellow", collision=False)
    m.label("CIVILIAN GATE (access rules OPEN)", ((gx0 + gx1) / 2, 0, 1400), "Labels", 220, yaw=180)


# ============================================================================ 1. operators' dome

def _operators_dome(m, rng, log):
    f = "1_Operators"
    centre = (0.0, 0.0, 0.0)

    def exit_gap(az, band):
        a = (az + 180) % 360 - 180
        return band == 0 and abs(a) < EXIT_HALF_ANGLE

    m.dome(centre, DOME_R, f + "/DomeShell", n_az=48, n_el=10, gap=exit_gap)
    step = 360.0 / ROOMS_PER_TIER
    rooms = 0
    stair_segments = {(t, s) for t in range(1, TIERS) for s in (ROOMS_PER_TIER // 4, ROOMS_PER_TIER // 2, 3 * ROOMS_PER_TIER // 4)}
    for tier in range(TIERS):
        z0 = tier * TIER_H
        for s in range(ROOMS_PER_TIER):
            az = s * step
            signed = (az + 180) % 360 - 180
            folder = "%s/Rooms/Tier%d" % (f, tier)
            exit_zone = tier == 0 and abs(signed) < EXIT_HALF_ANGLE + step
            if not exit_zone:
                _room(m, rng, az, step, z0, folder)
                rooms += 1
            # Floor slab above this segment: room ceiling + the next tier's walkway (left open where stairs come up).
            slab_z = z0 + ROOM_H
            mid_r = (R_IN + R_OUT) / 2
            m.block(m.polar(centre, mid_r, az, slab_z + 25), (R_OUT - R_IN + 40, _chord(R_OUT, step) + 30, 50), "RoomCeiling",
                    f + "/Floors", "Concrete", yaw=az)
            if (tier + 1, s) not in stair_segments:
                walk_r = (R_WALK + R_IN) / 2
                m.block(m.polar(centre, walk_r, az, slab_z + 25), (R_IN - R_WALK + 20, _chord(R_IN, step) + 30, 50), "Walkway",
                        f + "/Floors", "Concrete", yaw=az)
                if tier + 1 < TIERS:
                    m.block(m.polar(centre, R_WALK + 20, az, slab_z + 50 + 50), (8, _chord(R_WALK, step), 100), "WalkwayRail",
                            f + "/Floors", "Metal", yaw=az)
            if (tier + 1, s) in stair_segments:
                # Stairs climbing tangentially along the walkway from this tier up to the next one.
                # 12 steps x 45 cm fit inside this segment's opening (about 7.5 m of walkway).
                start = m.polar(centre, (R_WALK + R_IN) / 2, az - math.degrees(270.0 / ((R_WALK + R_IN) / 2)), z0)
                m.stairs(start, az + 90, 12, TIER_H / 12, 45, 400, f + "/Stairs", tags=["Stairs"])
    log("ark: %d operator rooms on %d tiers" % (rooms, TIERS))

    # Roll-call area in the middle.
    rc = f + "/RollCall"
    m.block((0, 0, 20), (7000, 7000, 40), "RollCallPlatform", rc, "Concrete", shape="cylinder")
    m.block((-2800, 0, 140), (600, 1200, 200), "RollCallPodium", rc, "Metal")
    m.stairs((-2200, 0, 40), 180, 4, 50, 50, 400, rc)
    for row in range(-5, 6):
        m.block((row * 400, 0, 41), (40, 4800, 2), "AssemblyLine", rc, "Marking", collision=False)
    m.block((-3200, 0, 1500), (30, 30, 2600), "FlagPole_Placeholder", rc, "Metal", shape="cylinder")
    m.label("ROLL CALL", (0, 0, 1200), "Labels", 500)
    m.label("OPERATORS' DOME", (0, 0, 6500), "Labels", 900)
    m.point_light((0, 0, 7000), "Lighting/Operators", 300000, 16000, (1.0, 0.95, 0.85), shadows=True)
    for k in range(8):
        m.point_light(m.polar(centre, 8500, k * 45, 1200), "Lighting/Operators", 30000, 4000)


def _chord(r, step_deg):
    return 2 * r * math.sin(math.radians(step_deg) / 2)


def _room(m, rng, az, step, z0, folder):
    """One operator room. Every room is the same; only the wear-and-tear props differ (Wear_0 .. Wear_3)."""
    c = (0.0, 0.0, 0.0)
    wear = rng.choices([0, 1, 2, 3], weights=[2, 4, 3, 2])[0]
    tags = ["OperatorRoom", "Wear_%d" % wear]
    zc = z0 + ROOM_H / 2
    # Back wall, side partition, front wall with a doorway and lintel.
    m.block(m.polar(c, R_OUT, az, zc), (30, _chord(R_OUT, step) + 30, ROOM_H), "RoomBack", folder, "Grey", yaw=az, tags=tags)
    m.block(m.polar(c, (R_IN + R_OUT) / 2, az - step / 2, zc), (R_OUT - R_IN, 30, ROOM_H), "RoomPartition", folder, "Grey",
            yaw=az - step / 2, tags=tags)
    front = _chord(R_IN, step)
    piece = (front - 130) / 2
    tangent = math.radians(az + 90)
    base = m.polar(c, R_IN, az, zc)
    for side in (-1, 1):
        off = side * (65 + piece / 2)
        m.block((base[0] + math.cos(tangent) * off, base[1] + math.sin(tangent) * off, zc), (30, piece, ROOM_H), "RoomFront",
                folder, "Grey", yaw=az, tags=tags)
    broken_lintel = wear == 3 and rng.random() < 0.5
    if not broken_lintel:
        m.block(m.polar(c, R_IN, az, z0 + 375), (30, 130, 150), "RoomLintel", folder, "Grey", yaw=az, tags=tags)
    # Same furniture in every room: bunk, locker, desk.
    m.block(m.polar(c, R_OUT - 120, az + step * 0.18, z0 + 30), (180, 90, 60), "Bunk", folder, "Metal",
            yaw=az + 90 + (8 if wear == 3 else 0), tags=tags)
    m.block(m.polar(c, R_OUT - 60, az - step * 0.3, z0 + 100), (50, 60, 200), "Locker", folder, "Metal", yaw=az, tags=tags)
    m.block(m.polar(c, R_IN + 120, az - step * 0.25, z0 + 38), (60, 100, 76), "Desk", folder, "Metal", yaw=az, tags=tags)
    # Wear and tear (placeholders for decals / damaged variants).
    wf = folder + "/Wear"
    for _ in range(wear):
        p = m.polar(c, rng.uniform(R_IN + 150, R_OUT - 150), az + rng.uniform(-step * 0.35, step * 0.35), z0 + 1)
        m.block(p, (rng.uniform(60, 160), rng.uniform(60, 160), 2), "WEAR_Stain", wf, "Wear", yaw=rng.uniform(0, 180),
                collision=False, tags=["WearPlaceholder"])
    if wear >= 2:
        m.block(m.polar(c, R_OUT - 20, az + rng.uniform(-step * 0.3, step * 0.3), z0 + rng.uniform(120, 380)),
                (6, 15, rng.uniform(80, 200)), "WEAR_WallCrack", wf, "Wear", yaw=az, roll=rng.uniform(-30, 30), collision=False,
                tags=["WearPlaceholder"])
        for _ in range(rng.randint(2, 4)):
            m.block(m.polar(c, rng.uniform(R_IN + 100, R_OUT - 100), az + rng.uniform(-step * 0.4, step * 0.4), z0 + 8),
                    (rng.uniform(10, 30),) * 2 + (15,), "WEAR_Debris", wf, "Wear", yaw=rng.uniform(0, 90), collision=False,
                    tags=["WearPlaceholder"])
    if wear == 3:
        m.block(m.polar(c, R_IN - 16, az + step * 0.3, z0 + rng.uniform(100, 300)), (2, 90, 70), "WEAR_ScorchMark", wf, "Wear",
                yaw=az, collision=False, tags=["WearPlaceholder"])


# ============================================================================ the hole and the two catwalks

def _hole_and_catwalks(m, rng):
    f = "1_Operators/ExitCatwalks"
    hx0, hx1 = HOLE
    bottom = -30000.0
    m.box(hx0, -HALF_Y, bottom - 200, hx1, HALF_Y, bottom, "HoleBottom", "Cavern/Hole", "Void")
    m.box(hx0 - 1000, -HALF_Y, bottom, hx0, HALF_Y, -200, "HoleWall_West", "Cavern/Hole", "Dark")
    m.box(hx1, -HALF_Y, bottom, hx1 + 200, HALF_Y, UNDER_FLOOR - 100, "HoleWall_East", "Cavern/Hole", "Dark")
    m.label("MASSIVE HOLE", ((hx0 + hx1) / 2, 0, 3000), "Labels", 700)
    start, end = DOME_R - 1200, hx1 + 400
    for side in (-1, 1):
        y = side * CATWALK_Y
        m.box(start, y - 100, -20, end, y + 100, 0, "Catwalk", f, "Metal", tags=["Catwalk"])
        # Minimal railing: sparse posts, a single thin rail on one side only, some posts missing.
        m.railing((hx0, y - side * 100, 0), (hx1, y - side * 100, 0), f, height=95, spacing=1000, rail=True,
                  skip=lambda i: rng.random() < 0.25)
    # Exit corridor from the roll-call area to both catwalks.
    m.box(start - 600, -CATWALK_Y - 300, -20, hx0, CATWALK_Y + 300, 0, "ExitApron", f, "Concrete")


# ============================================================================ 2. town square

# The arcade and the library are dedicated stations now (greybox_ark_hub.py), so they are not generic town buildings.
TOWN_FUNCTIONS = ["SHOP", "RESTAURANT", "GYM", "CLINIC", "BAR", "MARKET", "CINEMA", "HOTEL", "BANK", "SCHOOL", "PHARMACY",
                  "NOODLE BAR", "CAFE", "BARBER", "LAUNDRY", "TAILOR", "ELECTRONICS", "BAKERY"]
TOWN_MESHES = (["Corporation/SM_K1_Corporation_%d" % i for i in range(1, 11)] + ["HighRise/SM_K1_HighRise_%d" % i for i in range(5, 15)]
               + ["Factory/SM_K1_Factory_%d" % i for i in (1, 2, 3, 7, 8)] + ["Cottage/SM_K1_Cottage_%d" % i for i in range(1, 21)])


def _town_square(m, rng, log):
    f = "2_TownSquare"
    tx0, tx1 = TOWN
    cx = (tx0 + tx1) / 2
    strip = 2000.0
    # Main slab (the Underworld's roof) and the two edge strips with ladder shafts.
    m.box(tx0, -HALF_Y + strip, TOWN_SLAB, tx1, HALF_Y - strip, 0, "TownSlab", f + "/Ground", "Concrete")
    ladders_x = [tx0 + 5000, tx0 + 15000, tx0 + 25000, tx0 + 35000]
    for side in (-1, 1):
        y_in, y_out = side * (HALF_Y - strip), side * HALF_Y
        xs = [tx0] + [v for lx in ladders_x for v in (lx - 150, lx + 150)] + [tx1]
        for a, b in zip(xs[0::2], xs[1::2]):
            m.box(a, min(y_in, y_out), TOWN_SLAB, b, max(y_in, y_out), 0, "TownEdgeStrip", f + "/Ground", "Concrete")
        for lx in ladders_x:
            # The shaft between strip pieces is 3 m wide and runs the strip's width; the ladder is on its wall-side face.
            ly = side * (HALF_Y - 40)
            m.box(lx - 150, side * (HALF_Y - strip), 0, lx + 150, side * (HALF_Y - strip + 20), 100, "LadderHatchLip", f + "/Ladders",
                  "Ladder")
            m.box(lx - 30, ly - 15, UNDER_FLOOR, lx + 30, ly + 15, 150, "LadderToUnderworld", f + "/Ladders", "Ladder",
                  tags=["Ladder", "Climbable", "UnderworldLadder"])
            m.box(lx - 150, side * (HALF_Y - strip), TOWN_SLAB, lx + 150, side * HALF_Y, TOWN_SLAB - 20, "ShaftMarker",
                  f + "/Ladders", "Ladder", collision=False)
    m.label("TOWN SQUARE", (cx, 0, 9000), "Labels", 900)
    m.label("LADDERS DOWN TO THE UNDERWORLD", (cx, HALF_Y - 1000, 900), "Labels", 250, yaw=-90)
    m.label("LADDERS DOWN TO THE UNDERWORLD", (cx, -HALF_Y + 1000, 900), "Labels", 250, yaw=90)

    # Government building, striking in the middle, on a stepped plaza.
    m.block((cx, 0, 60), (12000, 12000, 120), "GovernmentPlaza", f + "/Government", "Concrete", shape="cylinder")
    m.block((cx, 0, 160), (8000, 8000, 80), "GovernmentPlaza", f + "/Government", "Concrete", shape="cylinder")
    gov = m.place_kit(K2 + "/SM_K2_60", cx, 0, 200, 0, 1.6, "GOVERNMENT_BUILDING", f + "/Government", ["Government", "Landmark"])
    m.label("GOVERNMENT", (cx - 3000, 0, 3500), "Labels", 600, yaw=180, in_game=True, colour=(255, 220, 120))
    m.point_light((cx, 0, 6000), "Lighting/Town", 400000, 16000, (1.0, 0.85, 0.6), shadows=True)
    log("ark: government building %.0f m tall" % ((gov["top"] - 200) / 100))

    # Buildings on a 35 m grid around the plaza ring road. Special slots first.
    pitch, plaza_r = 3500.0, 9500.0
    slots = []
    x = tx0 + 2500
    while x < tx1 - 1500:
        y = -HALF_Y + strip + 2500
        while y < HALF_Y - strip - 1500:
            if math.hypot(x - cx, y) > plaza_r and greybox_ark_hub.slot_is_free(x, y):
                slots.append((x, y))
            y += pitch
        x += pitch
    rng.shuffle(slots)
    specials = {
        "ARMOURY": min(slots, key=lambda s: math.hypot(s[0] - (cx + 12000), s[1] - 2000)),
        "TASK ROOM": min(slots, key=lambda s: math.hypot(s[0] - (cx - 12000), s[1] + 4000)),
        "RESISTANCE": min(slots, key=lambda s: math.hypot(s[0] - (tx1 - 2500), s[1] - (HALF_Y - strip - 2500))),
    }
    for s in specials.values():
        slots.remove(s)
    armoury = _armoury(m, specials["ARMOURY"], f + "/Armoury")
    task_door = _task_room(m, specials["TASK ROOM"], f + "/TaskRoom")
    resistance = _resistance(m, specials["RESISTANCE"], f + "/Resistance")
    for k, (x, y) in enumerate(slots):
        name = TOWN_FUNCTIONS[k % len(TOWN_FUNCTIONS)]
        path = "%s/%s" % (K1, rng.choice(TOWN_MESHES))
        yaw = rng.choice([0, 90, 180, 270])
        w, d, h = m.kit_footprint(path, 1.0, yaw)
        scale = min(1.3, 2600 / max(w, d), 11000 / h)
        folder = "%s/Buildings/%02d_%s" % (f, k, name.replace(" ", "_"))
        b = m.place_kit(path, x, y, 0, yaw, scale, name.replace(" ", "_"), folder, ["TownBuilding", name])
        x0, y0, x1, y1 = b["rect"]
        # Shop sign over the street-facing side nearest the plaza.
        face_x = x0 - 40 if x > cx else x1 + 40
        m.block((face_x, y, 450), (20, min(900, y1 - y0), 160), "SIGN_" + name.replace(" ", "_"), folder, rng.choice(NEON),
                collision=False, tags=["ShopSignPlaceholder"])
        m.label(name, (face_x + (-30 if x > cx else 30), y, 650), folder, 140, yaw=180 if x > cx else 0, in_game=True)
    log("ark: %d town buildings + armoury, task room, resistance" % len(slots))
    for k in range(6):
        m.point_light((tx0 + 3500 + k * 6600, rng.uniform(-12000, 12000), 5000), "Lighting/Town", 150000, 10000, shadows=True)
    # Station anchors (x, y, yaw the player faces): in front of each special building's west face.
    return {"armoury": (armoury[0] - 700, armoury[1], 0), "taskroom": (task_door[0] - 600, task_door[1], 0),
            "resistance": (resistance[0] - 700, resistance[1], 0)}


def _armoury(m, slot, f):
    x, y = slot
    b = m.place_kit(K1 + "/Factory/SM_K1_Factory_5", x, y, 0, 0, min(1.0, 3000 / 2892), "ARMOURY", f, ["Armoury"])
    m.label("ARMOURY", (b["rect"][0] - 40, y, 1200), f, 300, yaw=180, in_game=True, colour=(255, 120, 60))
    return b["rect"][0], y


def _task_room(m, slot, f):
    """Unmarked: a plain box with no sign, and a field of chutes going up through the cavern ceiling to the surface."""
    x, y = slot
    s, h = 1500.0, 1200.0
    m.box(x - s, y - s, 0, x + s, y - s + 50, h, "TaskRoomWall", f, "Grey")
    m.box(x - s, y + s - 50, 0, x + s, y + s, h, "TaskRoomWall", f, "Grey")
    m.box(x + s - 50, y - s, 0, x + s, y + s, h, "TaskRoomWall", f, "Grey")
    m.box(x - s, y - s, 0, x - s + 50, y - 150, h, "TaskRoomWall", f, "Grey")   # unmarked door on the west face
    m.box(x - s, y + 150, 0, x - s + 50, y + s, h, "TaskRoomWall", f, "Grey")
    m.box(x - s, y - 150, 300, x - s + 50, y + 150, h, "TaskRoomWall", f, "Grey")
    m.box(x - s, y - s, h, x + s, y + s, h + 50, "TaskRoomRoof", f, "Grey")
    top = CEILING + 15000   # past the rock ceiling: the surface is up there
    for i in range(4):
        for j in range(4):
            cx, cy = x - 900 + i * 600, y - 900 + j * 600
            m.block((cx, cy, 25), (220, 220, 50), "ChuteLaunchPad", f, "Metal", shape="cylinder", tags=["Chute"])
            m.block((cx, cy, 300 + (top - 300) / 2), (180, 180, top - 300), "ChuteToSurface", f, "Chute", shape="cylinder",
                    collision=False, tags=["Chute", "ToSurface"])
    m.label("TASK ROOM (unmarked in game) - chutes to the surface", (x, y, h + 600), "Labels", 200)
    return x - s, y


def _resistance(m, slot, f):
    """Unmarked building; the only tell is a small red stripe on its front."""
    x, y = slot
    b = m.place_kit(K1 + "/Cottage/SM_K1_Cottage_17", x, y, 0, 0, 1.4, "ResistanceMeetingPoint", f, ["Resistance"])
    x0, y0, x1, y1 = b["rect"]
    m.box(x0 - 12, y - 250, 260, x0 - 2, y + 250, 290, "SmallRedStripe", f, "RedStripe", collision=False, tags=["ResistanceMark"])
    m.label("RESISTANCE MEETING POINT (unmarked in game)", (x, y, b["top"] + 500), "Labels", 160)
    # Inside: the planning table where the Outer World is rebuilt (the resistance station; nothing marks it from outside).
    m.block((x, y, 90), (900, 500, 180), "PlanningTable", f, "Dark")
    m.block((x, y, 182), (860, 460, 4), "OuterWorldMap", f, "Holo", collision=False)
    return x0, y


# ============================================================================ Underworld (under the town square)

def _underworld(m, rng):
    f = "2_TownSquare/Underworld"
    tx0, tx1 = TOWN
    cx = (tx0 + tx1) / 2
    m.box(tx0, -HALF_Y, UNDER_FLOOR - 100, tx1, HALF_Y, UNDER_FLOOR, "UnderworldFloor", f + "/Shell", "Dark")
    m.box(tx1 - 100, -HALF_Y, UNDER_FLOOR, tx1, HALF_Y, TOWN_SLAB, "UnderworldWall", f + "/Shell", "Dark")
    m.box(tx0, -HALF_Y, UNDER_FLOOR, tx0 + 100, HALF_Y, TOWN_SLAB, "UnderworldWall", f + "/Shell", "Dark")
    x = tx0 + 2500
    while x < tx1:
        y = -HALF_Y + 2500
        while y < HALF_Y:
            m.block((x, y, (UNDER_FLOOR + TOWN_SLAB) / 2), (300, 300, TOWN_SLAB - UNDER_FLOOR), "SupportPillar", f + "/Pillars",
                    "Concrete", shape="cylinder")
            y += 5000
        x += 5000
    # A cramped shanty maze of shacks and stalls; an open trade floor under the government plaza.
    x = tx0 + 1200
    shacks = 0
    while x < tx1 - 800:
        y = -HALF_Y + 1500
        while y < HALF_Y - 1500:
            if math.hypot(x - cx, y) < 6000:
                if rng.random() < 0.35:
                    m.block((x, y, UNDER_FLOOR + 110), (300, 160, 20), "TRADE_STALL", f + "/TradeFloor", "Metal", tags=["TradeStall"])
                    m.block((x, y, UNDER_FLOOR + 260), (340, 220, 10), "StallAwning", f + "/TradeFloor", rng.choice(NEON), collision=False)
            elif rng.random() < 0.7:
                h = rng.choice([350, 450, 600])
                w, d = rng.uniform(900, 1600), rng.uniform(900, 1600)
                m.block((x, y, UNDER_FLOOR + h / 2), (w, d, h), "Shack", f + "/Shacks", "Concrete", yaw=rng.choice([0, 3, -4, 90]))
                if rng.random() < 0.4:
                    h2 = rng.choice([300, 400])
                    m.block((x + rng.uniform(-150, 150), y, UNDER_FLOOR + h + h2 / 2), (w * 0.8, d * 0.8, h2), "ShackUpper", f + "/Shacks",
                            "Grey", yaw=rng.choice([0, 5, -5]))
                if rng.random() < 0.4:
                    m.block((x - w / 2 - 15, y, UNDER_FLOOR + rng.uniform(200, 400)), (20, rng.uniform(150, 400), 80), "NEON_SIGN_PLACEHOLDER",
                            f + "/Neon", rng.choice(["NeonRed", "NeonPurple", "NeonPink"]), collision=False, tags=["NeonSignPlaceholder"])
                shacks += 1
            y += 2000
        x += 2000
    m.label("UNDERWORLD - criminals and \"trade\"", (cx, 0, UNDER_FLOOR + 2000), "Labels", 600)
    m.label("TRADE FLOOR", (cx, 0, UNDER_FLOOR + 800), "Labels", 300)
    for k in range(10):
        m.point_light((tx0 + 2000 + k * 4000, rng.uniform(-15000, 15000), UNDER_FLOOR + 1500), "Lighting/Underworld", 60000, 6000,
                      (1.0, 0.3, 0.4))


# ============================================================================ 3. civilian area

def _civilian(m, rng):
    f = "3_Civilian"
    x0, x1 = CIV
    m.box(x0, -HALF_Y, -2, x1, HALF_Y, 2, "FakeGrass", f + "/Ground", "Grass", tags=["FakeGrass"])
    m.box(x0, -1000, 2, x1, 1000, 6, "Boulevard", f + "/Ground", "Sidewalk")
    m.box(x0, -HALF_Y, 12000, x1, HALF_Y, 12100, "FAKE_SKY_PANEL", f + "/Sky", "FakeSky", collision=False, tags=["FakeSky"])
    m.label("CIVILIAN AREA - 2050s utopia (fake grass, fake sky)", ((x0 + x1) / 2, 0, 5000), "Labels", 600)
    cottages = ["Cottage/SM_K1_Cottage_%d" % i for i in range(1, 21)]
    plot = 6000.0
    mansions = 0
    for col in range(int((x1 - x0) // plot)):
        px = x0 + plot * (col + 0.5)
        for row in range(3):
            for side in (-1, 1):
                py = side * (1000 + plot * (row + 0.5))
                folder = "%s/Mansions/Plot_%d_%d_%s" % (f, col, row, "N" if side > 0 else "S")
                yaw = 270 if side > 0 else 90   # front door towards the boulevard
                path = "%s/%s" % (K1, rng.choice(cottages))
                m.place_kit(path, px, py + side * 600, 6, yaw, 2.2, "Mansion", folder, ["Mansion"])
                # Hedge around the plot with a driveway gap on the boulevard side.
                e = plot / 2 - 200
                near = py - side * e
                m.box(px - e, py + side * e - 60, 0, px + e, py + side * e + 60, 180, "Hedge", folder, "Hedge")
                m.box(px - e - 60, py - e, 0, px - e + 60, py + e, 180, "Hedge", folder, "Hedge")
                m.box(px + e - 60, py - e, 0, px + e + 60, py + e, 180, "Hedge", folder, "Hedge")
                m.box(px - e, near - 60, 0, px - 400, near + 60, 180, "Hedge", folder, "Hedge")
                m.box(px + 400, near - 60, 0, px + e, near + 60, 180, "Hedge", folder, "Hedge")
                m.box(px - 350, min(near, py), 2, px + 350, max(near, py), 8, "Driveway", folder, "Sidewalk")
                for _ in range(rng.randint(3, 6)):
                    _tree(m, (px + rng.uniform(-e + 400, e - 400), py + side * rng.uniform(-e + 400, -e + 1800), 0), folder, rng)
                mansions += 1
    for k in range(int((x1 - x0) // 1500)):
        for side in (-1, 1):
            _tree(m, (x0 + 750 + k * 1500, side * 1300, 0), f + "/BoulevardTrees", rng)
    for k in range(6):
        m.point_light((x0 + 3000 + k * 7000, 0, 6000), "Lighting/Civilian", 250000, 12000, (1.0, 0.97, 0.9), shadows=True)
    m.label("%d MANSIONS" % mansions, ((x0 + x1) / 2, 0, 4000), "Labels", 300)


def _tree(m, base, folder, rng):
    h = rng.uniform(450, 800)
    m.block((base[0], base[1], h / 2), (40, 40, h), "FakeTreeTrunk", folder, "Trunk", shape="cylinder", tags=["FakeTree"])
    r = rng.uniform(300, 500)
    m.block((base[0], base[1], h + r * 0.3), (r, r, r * 0.9), "FakeTreeCanopy", folder, "Tree", shape="sphere", collision=False,
            tags=["FakeTree"])


# ============================================================================ 4. secret dome

def _secret_dome(m):
    f = "4_SecretDome"
    x0 = CIV[1]
    cat_end = x0 + 8000
    m.box(x0 - 1000, -150, -20, cat_end, 150, 0, "SecretCatwalk", f, "Metal", tags=["Catwalk"])
    m.railing((x0 + 1000, -150, 0), (cat_end, -150, 0), f, spacing=1000)
    m.railing((x0 + 1000, 150, 0), (cat_end, 150, 0), f, spacing=1000)
    # One long staircase into the sky: 500 steps, 40 cm rise, 60 cm run -> 200 m up over 300 m.
    top = m.stairs((cat_end, 0, 0), 0, 500, 40, 60, 300, f + "/Staircase", material="Metal", tags=["SecretStaircase"])
    m.railing((cat_end, -150, 0), (top[0], -150, top[2]), f + "/Staircase", spacing=1200)
    m.railing((cat_end, 150, 0), (top[0], 150, top[2]), f + "/Staircase", spacing=1200)
    radius = 5000.0
    dcx = top[0] + 1500 + radius
    m.box(top[0], -1000, top[2] - 40, dcx - radius + 200, 1000, top[2], "SecretLanding", f, "Metal")
    m.block((dcx, 0, top[2] - 100), (radius * 2 + 400, radius * 2 + 400, 200), "SecretDomeFloor", f, "Concrete", shape="cylinder")
    m.block((dcx, 0, top[2] - 1500), (2000, 2000, 2800), "SecretDomeSupport", f, "Concrete", shape="cone", roll=180)

    def entrance(az, band):   # doorway facing the staircase (west, azimuth 180)
        return band == 0 and abs((az % 360) - 180) < 8

    m.dome((dcx, 0, top[2]), radius, f + "/Dome", n_az=32, n_el=8, material="Glass", gap=entrance)
    m.label("SECRET DOME - the Secrets live here", (dcx, 0, top[2] + 2500), "Labels", 400)
    m.label("SECRET CATWALK + STAIRCASE", (cat_end, 0, 1500), "Labels", 300)
    m.point_light((dcx, 0, top[2] + 2000), "Lighting/SecretDome", 300000, 8000, (0.9, 0.9, 1.0))
