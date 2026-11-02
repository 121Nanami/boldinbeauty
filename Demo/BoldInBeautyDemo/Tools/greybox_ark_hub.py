"""The Ark as the hub (owner request 2026-10-07): the 2D lobby is gone, and every place the menu used to list is a real place in
L_Ark. This module greyboxes the new structures and drops one anchor per station (a TargetPoint tagged "BIBStation" + the station
id from BIBRules/BIBArk.cpp) where the player stands when the menu teleports them there; ABIBGameMode spawns the glowing station
(hologram sign, ring, prompt) on each anchor at runtime.

New structures (the assistant's greybox reading, PROPOSED / OPEN for the owner):
- Operator Generator Factory: a big factory hall on the empty north side of the operators' platform (between the dome and the
  cavern wall): generator core, assembly line with operator pods, banner screens, chimneys. Summoning happens here.
- Merchant Row (town square, north-west of the plaza): a covered market street with one stall per merchant (Trade Centre,
  Premium Store, Ark Shop, Event Shop, Gang Shop), each with a merchant stand-in behind the counter.
- Arcade hall (south-east of the plaza): neon facade and a row of cabinets, one per arcade game.
- Training Grounds (south-west of the plaza): the Endless Tower and the Arena.
- Library (north-east) and Gang Hall (south-east corner).
- Back Alley: a stall on the Underworld trade floor.
- Stations on existing buildings: Roll Call (operators' dome), Government Building (plaza), Armoury, Task Room (unmarked) and
  the resistance's unmarked building (the Outer World planning table).
"""

import math

import unreal

# Town square rectangles kept free of the generic town buildings (x0, y0, x1, y1), in cm. greybox_ark._town_square skips any
# building slot whose footprint would touch one of these.
TOWN_CX = 53000.0
RESERVED = {
    "merchant_row": (36000.0, 10000.0, 50000.0, 19000.0),
    "training": (36000.0, -19000.0, 49000.0, -10000.0),
    "arcade": (55000.0, -19000.0, 64000.0, -10500.0),
    "ganghall": (65500.0, -19000.0, 72500.0, -11500.0),
    "library": (55500.0, 10000.0, 62500.0, 19000.0),
}
SLOT_MARGIN = 1800.0   # half the largest generic building footprint (26 m) plus a little air

FACTORY = (-11000.0, 13500.0, 10000.0, 20500.0)   # x0, y0, x1, y1 on the operators' platform (north flank)
UNDER_FLOOR = -4500.0


def slot_is_free(x, y):
    for x0, y0, x1, y1 in RESERVED.values():
        if x0 - SLOT_MARGIN < x < x1 + SLOT_MARGIN and y0 - SLOT_MARGIN < y < y1 + SLOT_MARGIN:
            return False
    return True


ANCHORS = []
ANCHOR_POS = []   # (x, y, z) of every anchor: the detail pass keeps the station rings clear


def anchor(m, station, x, y, z, yaw):
    """Where the player stands (feet at z) when teleported to this station, facing `yaw`."""
    ANCHORS.append(station)
    ANCHOR_POS.append((x, y, z))
    m.spawn(unreal.TargetPoint, (x, y, z + 100), yaw=yaw, label="Station_" + station, folder="Gameplay/Stations",
            tags=["BIBStation", station])


def build(m, rng, log, specials):
    """`specials`: the town's special buildings placed by greybox_ark (armoury / task room / resistance rects and doors)."""
    del ANCHORS[:]
    del ANCHOR_POS[:]
    _factory(m)
    _roll_call(m)
    _government(m)
    _merchant_row(m)
    _arcade(m)
    _training(m)
    _library(m)
    _gang_hall(m)
    _back_alley(m)
    for station, (x, y, yaw) in specials.items():
        anchor(m, station, x, y, 0, yaw)
    # Where a player arrives in the Ark (sign-in, or no station remembered): the plaza in front of the government building.
    m.spawn(unreal.TargetPoint, (TOWN_CX - 9500, 0, 200), yaw=0, label="ArkSpawn", folder="Gameplay", tags=["BIBArkSpawn"])
    log("ark hub: factory, merchant row (5 stalls), arcade, training grounds, library, gang hall, back alley; %d station anchors (%s)"
        % (len(ANCHORS), ", ".join(sorted(ANCHORS))))


# ============================================================================ Operator Generator Factory

def _factory(m):
    f = "1_Operators/GeneratorFactory"
    x0, y0, x1, y1 = FACTORY
    h = 3500.0
    t = 60.0
    door0, door1 = 3000.0, 8000.0   # wide door on the south face, towards the dome's exit apron
    # Shell: north / west / east walls, south wall with the door, roof with a skylight strip.
    m.box(x0, y1 - t, 0, x1, y1, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(x0, y0, 0, x0 + t, y1, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(x1 - t, y0, 0, x1, y1, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(x0, y0, 0, door0, y0 + t, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(door1, y0, 0, x1, y0 + t, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(door0, y0, 1600, door1, y0 + t, h, "FactoryWall", f + "/Shell", "Metal")
    m.box(x0, y0, h, x1, y1, h + 80, "FactoryRoof", f + "/Shell", "Dark")
    m.box(door0, y0 - 40, 1500, door1, y0, 1620, "DoorFrame", f + "/Shell", "NeonYellow", collision=False)
    m.box(x0, y0 - 2500, -10, x1, y0, 0, "FactoryForecourt", f + "/Ground", "Concrete")
    # Painted walkway from the dome's exit apron, between the dome and the hole's edge, north to the forecourt.
    for k in range(11):
        wy = 2400 + k * 850
        wx = 12500 if wy < 6000 else 11500
        m.block((wx, wy, 4), (800, 120, 6), "WalkwayStripe", f + "/Ground", "NeonYellow", collision=False)
    # Saw-tooth roof lights and chimneys (readable from the town square across the hole).
    for k in range(7):
        cx = x0 + 1500 + k * 3000
        m.block((cx, (y0 + y1) / 2, h + 450), (2400, y1 - y0 - 400, 40), "SawtoothRoof", f + "/Roof", "Metal", pitch=0, roll=12)
    for k, cx in enumerate((x0 + 2000, x0 + 6000, x1 - 6000, x1 - 2000)):
        top = 7500 + (k % 2) * 1500
        m.block((cx, y1 - 1500, (h + top) / 2), (700, 700, top - h), "Chimney", f + "/Roof", "Dark", shape="cylinder")
        m.block((cx, y1 - 1500, top + 60), (760, 760, 120), "ChimneyGlow", f + "/Roof", "NeonYellow", shape="cylinder", collision=False)
    # Inside: the generator core, an assembly line of operator pods, banner screens on the north wall.
    core = (-2000.0, (y0 + y1) / 2, 0.0)
    m.block((core[0], core[1], 60), (2600, 2600, 120), "CorePlinth", f + "/Core", "Metal", shape="cylinder")
    m.block((core[0], core[1], 1300), (1300, 1300, 2400), "GeneratorCore", f + "/Core", "Holo", shape="cylinder", tags=["FactoryCore"])
    for ring in range(4):
        m.block((core[0], core[1], 500 + ring * 600), (1900, 1900, 60), "CoreRing", f + "/Core", "NeonYellow", shape="cylinder", collision=False)
    line_y = y0 + 1600
    # Two runs of belt with a crossing in front of the door, so the hall can be walked from the door to the core.
    for a, b in ((x0 + 800, door0 - 300), (door1 + 300, x1 - 800)):
        m.box(a, line_y - 250, 0, b, line_y + 250, 110, "AssemblyLine", f + "/Line", "Dark")
        m.box(a, line_y - 260, 110, b, line_y + 260, 118, "AssemblyBelt", f + "/Line", "NeonCyan", collision=False)
    x = x0 + 1500
    while x < x1 - 1200:
        if not (door0 - 300 < x < door1 + 300):
            m.block((x, line_y, 330), (300, 300, 420), "OperatorPod", f + "/Line", "Glass", shape="cylinder")
            m.block((x, line_y, 560), (320, 320, 40), "PodCap", f + "/Line", "NeonCyan", shape="cylinder", collision=False)
        x += 1100
    for k in range(4):
        sx = x0 + 3000 + k * 4500
        m.block((sx, y1 - t - 30, 2100), (3200, 40, 1700), "BannerScreen", f + "/Screens", "Holo", collision=False, tags=["BannerScreen"])
        m.label("BANNER %d" % (k + 1), (sx, y1 - t - 120, 3100), f + "/Screens", 220, yaw=-90, in_game=True, colour=(255, 230, 140))
    m.label("OPERATOR GENERATOR FACTORY", ((x0 + x1) / 2, y0 - 80, h + 900), f, 520, yaw=-90, in_game=True, colour=(255, 210, 90))
    for k in range(5):
        m.point_light((x0 + 2000 + k * 4200, (y0 + y1) / 2, h - 300), "Lighting/Factory", 120000, 9000, (1.0, 0.85, 0.55), shadows=False)
    m.point_light((core[0], core[1], 1500), "Lighting/Factory", 200000, 6000, (0.3, 0.7, 1.0))
    # The player stands in front of the core, facing the banner screens.
    anchor(m, "factory", core[0] + 3200, y0 + 3600, 0, 90)


# ============================================================================ stations on existing places

def _roll_call(m):
    rc = "1_Operators/RollCall"
    m.block((-600, 1800, 120), (500, 900, 240), "OperatorRecordsTerminal", rc, "Metal")
    m.block((-600, 1800, 330), (520, 920, 20), "TerminalScreen", rc, "Holo", collision=False)
    anchor(m, "rollcall", -1400, 0, 40, 180)   # facing the podium


def _government(m):
    f = "2_TownSquare/Government"
    x = TOWN_CX - 6200
    # Notice board kiosk on the plaza steps.
    for side in (-1, 1):
        m.block((x, side * 1400, 300), (120, 120, 600), "NoticePillar", f, "Metal")
    m.block((x, 0, 500), (60, 2800, 900), "NoticeBoard", f, "Holo", collision=False)
    m.label("NOTICES", (x - 40, 0, 1050), f, 160, yaw=180, in_game=True, colour=(255, 220, 120))
    anchor(m, "government", x - 1300, 0, 200, 0)


def _stall(m, folder, x, y, facing_y, colour, sign):
    """A market stall opening towards -facing_y * Y: counter, back wall, awning, sign, merchant stand-in."""
    d = facing_y
    m.block((x, y + d * 700, 500), (1700, 60, 1000), "StallBackWall", folder, "Concrete")
    for side in (-1, 1):
        m.block((x + side * 850, y + d * 250, 500), (60, 960, 1000), "StallSideWall", folder, "Concrete")
    m.block((x, y - d * 150, 55), (1500, 220, 110), "StallCounter", folder, "Metal")
    m.block((x, y - d * 150, 113), (1520, 240, 6), "CounterGlow", folder, colour, collision=False)
    m.block((x, y + d * 100, 1020), (1900, 1300, 40), "StallAwning", folder, colour, roll=d * 6, collision=False)
    m.block((x, y - d * 600, 1250), (1500, 40, 300), "StallSign", folder, colour, collision=False, tags=["ShopSign"])
    m.label(sign, (x, y - d * 640, 1180), folder, 130, yaw=-90 if d > 0 else 90, in_game=True)
    # Merchant stand-in behind the counter.
    m.block((x, y + d * 300, 90), (70, 70, 180), "MerchantBody", folder, "Grey", shape="cylinder")
    m.block((x, y + d * 300, 215), (55, 55, 55), "MerchantHead", folder, "Grey", shape="sphere")


def _merchant_row(m):
    f = "2_TownSquare/MerchantRow"
    x0, y0, x1, y1 = RESERVED["merchant_row"]
    street_y = y0 + 1500
    m.box(x0, y0, 0, x1, y1, 8, "MarketFloor", f + "/Ground", "Sidewalk")
    m.box(x0, street_y - 40, 8, x1, street_y + 40, 10, "MarketStripe", f + "/Ground", "NeonYellow", collision=False)
    # Entrance arch facing the plaza.
    for side in (-1, 1):
        m.block((x1 - 300, street_y + side * 1300, 900), (200, 200, 1800), "ArchPillar", f, "Metal")
    m.block((x1 - 300, street_y, 1900), (240, 2800, 220), "ArchBeam", f, "NeonPink", collision=False)
    m.label("MERCHANT ROW", (x1 - 420, street_y, 2050), f, 260, yaw=0, in_game=True, colour=(255, 200, 240))
    stalls = [("trade", "TRADE CENTRE", "NeonCyan"), ("premium", "PREMIUM STORE", "NeonPink"), ("arkshop", "ARK SHOP", "NeonPurple"),
              ("eventshop", "EVENT SHOP", "NeonYellow"), ("gangshop", "GANG SHOP", "NeonRed")]
    pitch = (x1 - x0 - 1500) / len(stalls)
    for k, (station, sign, colour) in enumerate(stalls):
        x = x0 + 1200 + pitch * (k + 0.5)
        y = street_y + 1700
        _stall(m, "%s/Stall_%s" % (f, station), x, y, 1, colour, sign)
        anchor(m, station, x, y - 900, 8, 90)
    for k in range(5):
        m.point_light((x0 + 1500 + k * 2800, street_y + 800, 1600), "Lighting/MerchantRow", 40000, 3500, (1.0, 0.7, 0.9))


def _arcade(m):
    f = "2_TownSquare/Arcade"
    x0, y0, x1, y1 = RESERVED["arcade"]
    h = 1600.0
    t = 50.0
    door = (x0 + x1) / 2
    m.box(x0, y0, 0, x1, y1, 8, "ArcadeFloor", f, "Dark")
    m.box(x0, y0, 0, x1, y0 + t, h, "ArcadeWall", f, "Concrete")
    m.box(x0, y0, 0, x0 + t, y1, h, "ArcadeWall", f, "Concrete")
    m.box(x1 - t, y0, 0, x1, y1, h, "ArcadeWall", f, "Concrete")
    m.box(x0, y1 - t, 0, door - 900, y1, h, "ArcadeFront", f, "Concrete")
    m.box(door + 900, y1 - t, 0, x1, y1, h, "ArcadeFront", f, "Concrete")
    m.box(door - 900, y1 - t, 500, door + 900, y1, h, "ArcadeFront", f, "Concrete")
    m.box(x0, y0, h, x1, y1, h + 60, "ArcadeRoof", f, "Dark")
    # Neon facade: stripes and the sign.
    for k, colour in enumerate(("NeonPink", "NeonCyan", "NeonPurple")):
        m.box(x0, y1, 900 + k * 220, x1, y1 + 30, 960 + k * 220, "FacadeNeon", f + "/Neon", colour, collision=False)
    m.label("ARCADE", (door, y1 + 60, 1300), f, 380, yaw=90, in_game=True, colour=(255, 120, 230))
    games = ["NIGHT SHIFT", "BOSS RUSH", "RHYTHM", "HALO FALL", "NEON MEMORY", "FIREWALL BREACH"]
    colours = ["NeonRed", "NeonYellow", "NeonPink", "NeonCyan", "NeonPurple", "NeonGreen"]
    for k, (game, colour) in enumerate(zip(games, colours)):
        cx = x0 + 900 + k * (x1 - x0 - 1800) / (len(games) - 1)
        cy = y0 + 900
        m.block((cx, cy, 100), (500, 400, 200), "CabinetBase", f + "/Cabinets", "Dark")
        m.block((cx, cy - 60, 330), (480, 300, 300), "CabinetBody", f + "/Cabinets", "Dark")
        m.block((cx, cy + 92, 340), (380, 10, 230), "CabinetScreen", f + "/Cabinets", colour, collision=False)
        m.block((cx, cy - 60, 520), (500, 320, 80), "CabinetMarquee", f + "/Cabinets", colour, collision=False)
        m.label(game, (cx, cy + 120, 600), f + "/Cabinets", 50, yaw=90, in_game=True)
    m.point_light((door, (y0 + y1) / 2, 1300), "Lighting/Arcade", 80000, 6000, (1.0, 0.3, 0.9))
    anchor(m, "arcade", door, y1 - 1600, 8, -90)


def _training(m):
    f = "2_TownSquare/TrainingGrounds"
    x0, y0, x1, y1 = RESERVED["training"]
    m.box(x0, y0, 0, x1, y1, 8, "TrainingGround", f, "Concrete")
    # Endless Tower: a tall ringed tower reaching for the cavern ceiling.
    tx, ty = x0 + 3500, (y0 + y1) / 2
    m.block((tx, ty, 6000), (2600, 2600, 12000), "EndlessTower", f + "/Tower", "Metal", shape="cylinder", tags=["EndlessTower"])
    for k in range(10):
        m.block((tx, ty, 900 + k * 1150), (2800, 2800, 60), "TowerRing", f + "/Tower", "NeonCyan" if k % 2 else "NeonPurple",
                shape="cylinder", collision=False)
    m.box(tx - 400, ty + 1290, 0, tx + 400, ty + 1320, 700, "TowerDoor", f + "/Tower", "Holo", collision=False)
    m.label("ENDLESS TOWER", (tx, ty + 1500, 1500), f + "/Tower", 220, yaw=90, in_game=True, colour=(170, 190, 255))
    anchor(m, "tower", tx, ty + 2600, 8, -90)
    # Arena: an open oval of stands around a sand floor.
    ax, ay = x1 - 4200, (y0 + y1) / 2
    m.block((ax, ay, 12), (6000, 6000, 10), "ArenaFloor", f + "/Arena", "Sidewalk", shape="cylinder", collision=False)
    seg = 24
    for k in range(seg):
        a = 360.0 * k / seg
        if abs((a + 180) % 360 - 180 - 90) < 16:
            continue   # the gate faces the plaza (+Y)
        px, py, _ = m.polar((ax, ay, 0), 3300, a, 0)
        m.block((px, py, 300), (500, 900, 600), "ArenaStand", f + "/Arena", "Concrete", yaw=a)
        px, py, _ = m.polar((ax, ay, 0), 3700, a, 0)
        m.block((px, py, 650), (400, 900, 1300), "ArenaStandUpper", f + "/Arena", "Grey", yaw=a)
    m.label("ARENA", (ax, ay + 3800, 1500), f + "/Arena", 240, yaw=90, in_game=True, colour=(255, 140, 120))
    m.point_light((ax, ay, 2500), "Lighting/Training", 120000, 7000, (1.0, 0.6, 0.5))
    anchor(m, "arena", ax, ay + 4600, 8, -90)


def _library(m):
    f = "2_TownSquare/Library"
    x0, y0, x1, y1 = RESERVED["library"]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 + 1000
    m.block((cx, cy, 700), (4500, 4000, 1400), "LibraryHall", f, "Concrete")
    for k in range(6):
        m.block((cx - 1875 + k * 750, cy - 2050, 600), (180, 180, 1200), "LibraryColumn", f, "Marking", shape="cylinder")
    m.block((cx, cy - 2100, 1300), (4700, 400, 200), "LibraryPortico", f, "Concrete")
    m.label("LIBRARY", (cx, cy - 2350, 1600), f, 220, yaw=-90, in_game=True, colour=(170, 255, 160))
    anchor(m, "library", cx, cy - 3200, 0, 90)


def _gang_hall(m):
    f = "2_TownSquare/GangHall"
    x0, y0, x1, y1 = RESERVED["ganghall"]
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 - 500
    m.block((cx, cy, 800), (5000, 4500, 1600), "GangHall", f, "Dark")
    m.block((cx, cy + 2260, 1200), (3000, 40, 400), "GangBanner", f, "NeonRed", collision=False)
    m.label("GANG HALL", (cx, cy + 2320, 1250), f, 200, yaw=90, in_game=True, colour=(255, 120, 120))
    anchor(m, "ganghall", cx, cy + 3300, 0, -90)


def _back_alley(m):
    f = "2_TownSquare/Underworld/BackAlley"
    x, y = 49000.0, 3000.0
    z = UNDER_FLOOR
    m.block((x, y + 700, z + 450), (1800, 60, 900), "AlleyBackWall", f, "Dark")
    m.block((x, y - 150, z + 55), (1500, 220, 110), "AlleyCounter", f, "Metal")
    m.block((x, y + 100, z + 920), (1900, 1300, 30), "AlleyTarp", f, "NeonPurple", roll=-5, collision=False)
    m.block((x, y + 300, z + 90), (70, 70, 180), "MerchantBody", f, "Grey", shape="cylinder")
    m.block((x, y + 300, z + 215), (55, 55, 55), "MerchantHead", f, "Grey", shape="sphere")
    m.label("BACK ALLEY", (x, y - 300, z + 1050), f, 120, yaw=-90, in_game=True, colour=(255, 90, 160))
    m.point_light((x, y - 400, z + 800), "Lighting/Underworld", 25000, 2500, (1.0, 0.25, 0.6))
    anchor(m, "alley", x, y - 1100, z, 90)
