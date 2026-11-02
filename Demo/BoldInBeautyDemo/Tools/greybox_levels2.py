"""Bespoke story levels, chapters 28-40, and the driver for all bespoke chapters (14-40). See greybox_levels.py."""

import math
import random

import unreal

import greybox_levels as L
import greybox_story as gs
from greybox_common import MAPS, GreyboxMap


# ================================================================== chapters 28-40
def ch28(m, rng, spec):
    """The Commanders' Table: tower interior. Lobby and mirrored lift, office floor, logo corridor, round chamber."""
    f = "Ch28"
    S = []
    # S1 lobby (marble hall, wing relief)
    L.room(m, -9000, -5000, -1500, 5000, 0, 3200, f + "/Lobby", "Concrete", floor="Marking", gaps={"E": [(0.4, 0.6)]}, roof="Concrete")
    L.colonnade(m, -8500, -4200, -2000, -4200, 6, 0, 3200, f + "/Lobby", "Stone", r=150, cap="NeonYellow")
    L.colonnade(m, -8500, 4200, -2000, 4200, 6, 0, 3200, f + "/Lobby", "Stone", r=150, cap="NeonYellow")
    m.box(-9000, -1000, 0, -8000, 1000, 1200, "WingRelief", f + "/Lobby", "NeonYellow")
    S.append(L.arena(m, 1, (-5500.0, 0.0, 0.0), 3800, f + "/S1"))
    # S2 office floor: grid of glass offices with framed logos
    L.slab(m, -1500, -6000, 9000, 6000, 0, f + "/Office", "Concrete")
    for gx in range(3):
        for gy in range(3):
            L.room(m, -1000 + gx * 3300, -5500 + gy * 3800, 1800 + gx * 3300 - 300, -2200 + gy * 3800 - 200, 0, 2400, f + "/Office", "Glass",
                   gaps={"S": [(0.4, 0.6)], "N": [(0.4, 0.6)]}, roof=None)
    S.append(L.arena(m, 2, (3750.0, 0.0, 0.0), 4200, f + "/S2"))
    # S3 logo corridor: a long hall of framed wings
    L.room(m, 9000, -1800, 24000, 1800, 0, 2800, f + "/Corridor", "Concrete", floor="Wood", gaps={"W": [(0.3, 0.7)], "E": [(0.3, 0.7)]}, roof="Concrete")
    for k in range(20):
        m.block((9600 + k * 700, 1700, 1500), (400, 20, 600), "FramedLogo", f + "/Corridor", spec["accent"], collision=False)
        m.block((9600 + k * 700, -1700, 1500), (400, 20, 600), "FramedLogo", f + "/Corridor", spec["accent"], collision=False)
    S.append(L.arena(m, 3, (16500.0, 0.0, 0.0), 2300, f + "/S3"))
    # S4 the round council chamber: a huge round table, seven chairs, a fan-of-wings ceiling
    cc = (31500.0, 0.0, 0.0)
    L.tiers(m, cc, 3700, 1, 100.0, 700.0, f + "/Chamber", "Wood", seg=36, skip=lambda az, i: 170 < az < 190)
    m.block((cc[0], cc[1], 380), (3400, 3400, 120), "CouncilTable", f + "/Chamber", "Wood", shape="cylinder")
    for k in range(7):
        p = gs._az(cc, 2300, 180 + k * 360 / 7)
        m.block((p[0], p[1], 300), (260, 260, 600), "CommanderChair", f + "/Chamber", "NeonYellow", shape="cylinder")
    for k in range(12):
        p = gs._az(cc, 3900, k * 30)
        m.block((p[0], p[1], 3000), (60, 1200, 3000), "WingFin", f + "/Chamber", "NeonYellow", yaw=k * 30, collision=False)
    S.append(L.arena(m, 4, cc, 3900, f + "/S4", boss=True))
    L.walkway(m, (-1500, 0, 0), (-1500 + 300, 0, 0), f + "/Links", "Concrete", 500.0, rail=False)
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90, f + "/S%d" % (i + 1))
        L.props(m, rng, c, S[i][2], 0, "tower", 14, f + "/S%d" % (i + 1), spec, keep=1100)
    return S, (-8200.0, 0.0, 0.0)


def ch29(m, rng, spec):
    """Sponsor Night: backstage maze, under-stage rig (low), cage vault (rows), gala stage and house."""
    f = "Ch29"
    S = []
    L.maze(m, -14000, -7000, -3000, 7000, 8, 8, 0, 1700, f + "/Backstage", "Wood", rng)
    S.append(L.arena(m, 1, (-8500.0, 0.0, 0.0), 3600, f + "/S1"))
    # under-stage rig: beams and a low ceiling
    L.slab(m, -2500, -6500, 8500, 6500, -2400, f + "/Rig", "Concrete")
    for k in range(10):
        m.box(-2200 + k * 1050, -6400, -2400, -1900 + k * 1050, 6400, -2100, "RigBeam", f + "/Rig", "Metal")
    for k in range(8):
        m.block((-1500 + k * 1200, rng.uniform(-4000, 4000), -1700), (160, 160, 1400), "RigPost", f + "/Rig", "Metal", shape="cylinder")
    L.stairs_to(m, (-3500, 0, 0), (-2500, 0, -2400), f + "/Rig", "Concrete", 900.0, 80.0)
    S.append(L.arena(m, 2, (3000.0, 0.0, -2400.0), 3400, f + "/S2"))
    # cage vault: rows of cages
    for r in range(4):
        for c in range(7):
            x, y = 12500 + c * 1100, -3000 + r * 2000
            m.block((x, y, -2400 + 140), (700, 700, 280), "CageBase", f + "/Vault", "Metal")
            for dx, dy in ((-300, -300), (300, -300), (-300, 300), (300, 300)):
                m.block((x + dx, y + dy, -2400 + 400), (30, 30, 520), "CageBar", f + "/Vault", "Metal")
            m.block((x, y, -2400 + 700), (720, 720, 30), "CageTop", f + "/Vault", "Metal")
    L.slab(m, 10500, -5500, 21000, 5500, -2400, f + "/Vault", "Dark")
    S.append(L.arena(m, 3, (15500.0, 0.0, -2400.0), 3800, f + "/S3"))
    # gala stage and house (ground level, east): stage, curtains, seat rows
    L.stairs_to(m, (21000, 0, -2400), (23500, 0, 0), f + "/Links", "Concrete", 1000.0, 80.0)
    L.slab(m, 23500, -8000, 38000, 8000, 0, f + "/Hall", "Wood")
    m.box(33500, -6500, 0, 38000, 6500, 1100, "GalaStage", f + "/Stage", "Wood")
    for r in range(8):
        for c in range(10):
            m.block((25500 + r * 900, -4500 + c * 900, 200), (500, 600, 400), "Seat", f + "/House", "NeonRed")
    for k in range(6):
        m.block((33500, -5600 + k * 2200, 2500), (80, 400, 1900), "Curtain", f + "/Stage", spec["accent"], collision=False)
    S.append(L.arena(m, 4, (33500.0, 0.0, 1100.0), 4000, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90 + 50 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, 3200, c[2], "casino", 12, f + "/S%d" % (i + 1), spec, keep=1200)
    return S, (-12500.0, 0.0, 0.0)


def ch30(m, rng, spec):
    """The Line Restarts: factory halls with conveyors as moving belts, a control spine, the cradle hall."""
    f = "Ch30"
    S = []
    L.slab(m, -30000, -9000, 30000, 9000, 0, f + "/Floor", "Metal")
    S.append(L.arena(m, 1, (-22500.0, 0.0, 0.0), 3800, f + "/S1"))
    # S2: three long conveyor lines (movers) with robot arms between
    for lane in range(3):
        y = -4000 + lane * 4000
        for k in range(8):
            m.mover((-14000 + k * 1700, y, 120), (1500, 900, 220), "ConveyorSegment", f + "/Line%d" % lane, "Dark", offset=(900, 0, 0), period=3.0, phase=k * 0.1)
            m.block((-14000 + k * 1700, y, 340), (200, 200, 220), "Cube", f + "/Line%d" % lane, "CubeGlow", collision=False)
        for k in range(4):
            m.mover((-13000 + k * 3400, y + 1700, 900), (200, 200, 1800), "RobotArm", f + "/Line%d" % lane, "Hazard", swing=True, rotation=(0, 0, 25),
                    period=4.0 + k * 0.4)
    S.append(L.arena(m, 2, (-8000.0, 0.0, 0.0), 4400, f + "/S2"))
    # S3: control spine (tall column with three node consoles around it)
    cs = (6000.0, 0.0, 0.0)
    m.block((cs[0], cs[1], 3500), (1400, 1400, 7000), "ControlSpine", f + "/Spine", "Metal", shape="cylinder")
    for k in range(3):
        p = gs._az(cs, 2400, k * 120)
        m.block((p[0], p[1], 400), (500, 500, 800), "NodeConsole", f + "/Spine", "NeonYellow")
    S.append(L.arena(m, 3, cs, 3600, f + "/S3"))
    # S4: cradle hall (rows of cradles) with the single empty polished one at the end
    L.room(m, 12000, -6000, 28500, 6000, 0, 3400, f + "/Cradle", "Concrete", floor="Marking", gaps={"W": [(0.4, 0.6)]}, roof="Concrete")
    for r in range(3):
        for c in range(6):
            m.box(13000 + c * 2000, -4500 + r * 4500 - 250, 0, 14000 + c * 2000, -4500 + r * 4500 + 250, 500, "Cradle", f + "/Cradle", "Metal")
    m.box(27200, -600, 0, 28200, 600, 700, "CradleV1", f + "/Cradle", "Marking")
    S.append(L.arena(m, 4, (20000.0, 0.0, 0.0), 4400, f + "/S4", boss=True))
    L.walkway(m, (-18000, 0, 0), (-12400, 0, 0), f + "/Links", "Metal", 800.0)
    L.walkway(m, (-3600, 0, 0), (2400, 0, 0), f + "/Links", "Metal", 800.0)
    L.walkway(m, (9600, 0, 0), (12000, 0, 0), f + "/Links", "Metal", 800.0)
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90, f + "/S%d" % (i + 1))
        L.props(m, rng, c, S[i][2], 0, "factory", 12, f + "/S%d" % (i + 1), spec, keep=1200)
    return S, (-26000.0, 0.0, 0.0)


def ch31(m, rng, spec):
    """Fracture: a rail line with three stations and a moving train of carriages (movers) between them."""
    f = "Ch31"
    S = []
    L.slab(m, -3000, -3500, 74000, 3500, 0, f + "/Track", "Dark")
    L.wall_run(m, -3000, -3500, 74000, -3500, 0, 2400, f + "/Tunnel", "Concrete")
    L.wall_run(m, -3000, 3500, 74000, 3500, 0, 2400, f + "/Tunnel", "Concrete")
    L.slab(m, -3000, -3500, 74000, 3500, 2440, f + "/Tunnel", "Concrete", t=40, label="Roof")
    for k in range(70):
        x = k * 1060
        for ry in (-800, 800):
            m.block((x + 530, ry, 14), (1060, 14, 14), "Rail", f + "/Rails", "Metal", collision=False)
        m.block((x + 530, 0, 6), (160, 2000, 10), "Sleeper", f + "/Rails", "Wood", collision=False)
    xs = [4000.0, 26000.0, 48000.0, 68000.0]
    for i, x in enumerate(xs):
        # stations are widened with a platform
        m.box(x - 3000, 1700, 0, x + 3000, 3300, 160, "Platform", f + "/Station%d" % (i + 1), "Concrete")
        m.block((x, 0, 1300), (6000, 40, 40), "StationBeam", f + "/Station%d" % (i + 1), "Metal", collision=False)
        S.append(L.arena(m, i + 1, (x, 0.0, 0.0), 3000, f + "/S%d" % (i + 1), boss=i == 3))
    # the train: eight carriages as movers sliding along the track between the stations
    for car in range(8):
        m.mover((8000 + car * 1200, 0, 700), (1100, 1200, 1400), "TrainCarriage", f + "/Train", "Metal", offset=(10000, 0, 0), period=20.0,
                phase=0.0, tags=["Train"])
        m.block((8000 + car * 1200, 0, 1450), (1000, 1100, 60), "CarriageRoof", f + "/Train", spec["accent"], collision=False)
    # hardliner locomotive (boss set piece) at station four
    m.block((xs[3] + 1500, 0, 800), (3600, 1500, 1600), "HardlinerLocomotive", f + "/Boss", "Dark")
    for k in range(4):
        m.block((xs[3] + 400 + k * 900, 0, 1750), (500, 500, 300), "Turret", f + "/Boss", "NeonRed", shape="cylinder")
    for i, x in enumerate(xs):
        L.jump_and_climb(m, (x, 0.0, 0.0), 90, f + "/S%d" % (i + 1), 3)
        L.props(m, rng, (x, 0.0, 0.0), 2800, 0, "tunnel", 12, f + "/Dress", spec, keep=700)
    return S, (-1500.0, 0.0, 0.0)


def ch32(m, rng, spec):
    """The Charter: a central signature chamber ringed by three trial rooms (barracks, stage, court) off a vault corridor."""
    f = "Ch32"
    S = []
    # entrance and archive halls (S1) on the south
    L.room(m, -5000, -17000, 5000, -9000, 0, 3000, f + "/Archive", "Concrete", floor="Concrete", gaps={"N": [(0.4, 0.6)]}, roof="Concrete")
    for k in range(8):
        m.block((-4600, -16500 + k * 900, 1100), (300, 700, 2200), "ArchiveCabinet", f + "/Archive", "Metal")
        m.block((4600, -16500 + k * 900, 1100), (300, 700, 2200), "ArchiveCabinet", f + "/Archive", "Metal")
    m.block((0, -17000, 1400), (4600, 400, 2800), "VaultDoorRing", f + "/Archive", "Metal", collision=False)
    S.append(L.arena(m, 1, (0.0, -13000.0, 0.0), 3600, f + "/S1"))
    # the signature chamber at the centre (boss), trial rooms to W / E / N around it, linked by short halls
    cc = (0.0, 0.0, 0.0)
    L.tiers(m, cc, 3600, 2, 120.0, 600.0, f + "/Chamber", "Stone", seg=36, skip=lambda az, i: 70 < az < 110 or 250 < az < 290 or az < 20 or az > 340 or 160 < az < 200)
    m.block((0, 0, 300), (1600, 3600, 60), "SignatureTable", f + "/Chamber", "Wood")
    for k in range(7):
        m.block((-1500 + k * 500, 0, 700), (240, 700, 80), "SignatureLine", f + "/Chamber", "Marking" if k < 6 else "NeonRed", collision=False)
    m.block((0, 3000, 900), (700, 700, 1800), "GuardianPlinth", f + "/Chamber", "Stone")
    S.append(L.arena(m, 4, cc, 3800, f + "/S4", boss=True))
    # trial rooms (S2 west barracks, S3 east theatre) and a north court (decor / extra fight)
    L.room(m, -17000, -4000, -7000, 4000, 0, 2600, f + "/Barracks", "Concrete", floor="Wood", gaps={"E": [(0.4, 0.6)]}, roof="Concrete")
    for r in range(3):
        for c in range(6):
            m.block((-15500 + c * 1300, -2800 + r * 2700, 300), (500, 1000, 600), "Bunk", f + "/Barracks", "Metal")
    S.append(L.arena(m, 2, (-12000.0, 0.0, 0.0), 3600, f + "/S2"))
    L.room(m, 7000, -4000, 17000, 4000, 0, 3000, f + "/Theatre", "Concrete", floor="Wood", gaps={"W": [(0.4, 0.6)]}, roof="Concrete")
    m.box(14000, -3500, 0, 16800, 3500, 900, "RehearsalStage", f + "/Theatre", "Wood")
    for r in range(5):
        for c in range(7):
            m.block((8000 + r * 900, -2800 + c * 900, 200), (500, 600, 400), "TheatreSeat", f + "/Theatre", "NeonRed")
    S.append(L.arena(m, 3, (12000.0, 0.0, 0.0), 3600, f + "/S3"))
    L.room(m, -5000, 7000, 5000, 15000, 0, 3400, f + "/Court", "Concrete", floor="Marking", gaps={"S": [(0.4, 0.6)]}, roof="Concrete")
    m.box(-2000, 13800, 0, 2000, 14700, 400, "CourtBench", f + "/Court", "Wood")
    # halls linking everything
    L.walkway(m, (0, -9000, 0), (0, -3600, 0), f + "/Links", "Stone", 800.0, rail=False)
    L.walkway(m, (-7000, 0, 0), (-3600, 0, 0), f + "/Links", "Stone", 800.0, rail=False)
    L.walkway(m, (3600, 0, 0), (7000, 0, 0), f + "/Links", "Stone", 800.0, rail=False)
    L.walkway(m, (0, 3600, 0), (0, 7000, 0), f + "/Links", "Stone", 800.0, rail=False)
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, 3000, 0, "vault", 12, f + "/S%d" % (i + 1), spec, keep=1200)
    return S, (0.0, -15500.0, 0.0)


def ch33(m, rng, spec):
    """The White Room: white stone corridors, a hall of ranks, the honour-guard hall, the white room with a table."""
    f = "Ch33"
    S = []
    L.slab(m, -3000, -34000, 3000, 6000, 0, f + "/Corridor", "Cloud")
    L.wall_run(m, -3000, -34000, -3000, 6000, 0, 3600, f + "/Corridor", "Cloud")
    L.wall_run(m, 3000, -34000, 3000, 6000, 0, 3600, f + "/Corridor", "Cloud")
    L.colonnade(m, -2200, -33000, -2200, 5000, 20, 0, 3600, f + "/Corridor", "PastelMint", r=100)
    L.colonnade(m, 2200, -33000, 2200, 5000, 20, 0, 3600, f + "/Corridor", "PastelMint", r=100)
    S.append(L.arena(m, 1, (0.0, -30000.0, 0.0), 2800, f + "/S1"))
    # S2 hall of silence: ranks of standing figures
    L.room(m, -9000, -26000, 9000, -14000, 0, 4400, f + "/Hall", "Cloud", floor="Cloud", gaps={"N": [(0.45, 0.55)], "S": [(0.45, 0.55)]}, roof=None)
    for r in range(5):
        for c in range(9):
            m.block((-7500 + c * 1900, -24500 + r * 2200, 90), (140, 140, 180), "RankFigure", f + "/Hall", "Cloud", shape="cylinder")
    S.append(L.arena(m, 2, (0.0, -20000.0, 0.0), 4200, f + "/S2"))
    # S3 honour-guard hall: two long ranks with a central aisle
    L.room(m, -7000, -12000, 7000, -3000, 0, 4000, f + "/Guard", "Cloud", floor="Cloud", gaps={"N": [(0.45, 0.55)], "S": [(0.45, 0.55)]}, roof=None)
    for k in range(12):
        m.block((-4500, -11000 + k * 700, 90), (140, 140, 180), "GuardFigure", f + "/Guard", "Cloud", shape="cylinder")
        m.block((4500, -11000 + k * 700, 90), (140, 140, 180), "GuardFigure", f + "/Guard", "Cloud", shape="cylinder")
    S.append(L.arena(m, 3, (0.0, -7500.0, 0.0), 3200, f + "/S3"))
    # S4 the white room: a table, two chairs, a vase
    L.room(m, -3000, -2000, 3000, 5000, 0, 3000, f + "/Room", "Cloud", floor="PastelMint", gaps={"S": [(0.4, 0.6)]}, roof="Cloud")
    m.block((0, 1500, 380), (900, 1400, 40), "WhiteTable", f + "/Room", "Cloud")
    m.block((0, 1500 + 1100, 220), (400, 400, 440), "WhiteChair", f + "/Room", "Cloud")
    m.block((0, 1500 - 1100, 220), (400, 400, 440), "WhiteChair", f + "/Room", "Cloud")
    m.block((0, 1500, 560), (120, 120, 260), "WhiteVase", f + "/Room", "Cloud", shape="cylinder")
    S.append(L.arena(m, 4, (0.0, 1500.0, 0.0), 2600, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        L.props(m, rng, c, 2500, 0, "gallery", 8, f + "/S%d" % (i + 1), spec, keep=1300)
        L.jump_and_climb(m, c, 90 if i % 2 else 270, f + "/S%d" % (i + 1), 3)
    return S, (0.0, -33000.0, 0.0)


def ch34(m, rng, spec):
    """The Night Before: the Operators' dome: party floor, kitchens, rooftop, printing hall below."""
    f = "Ch34"
    S = []
    c0 = (0.0, 0.0, 0.0)
    m.dome(c0, 9000.0, f + "/Dome", n_az=36, n_el=8, thickness=100.0, material="Concrete", gap=lambda az, b: 170 < az < 190 and b < 3)
    L.slab(m, -9500, -9500, 9500, 9500, 0, f + "/Floor", "Concrete")
    # S1 party floor: tables, bunting poles, a cake table, a stage
    for k in range(8):
        p = gs._az(c0, 4200, k * 45)
        m.block((p[0], p[1], 190), (500, 500, 380), "PartyTable", f + "/Party", "Wood", shape="cylinder")
    m.block((0, 0, 420), (1800, 900, 120), "CakeTable", f + "/Party", "Marking")
    m.block((0, 0, 700), (600, 400, 400), "Cake", f + "/Party", "NeonPink")
    for k in range(16):
        p = gs._az(c0, 7000, k * 22.5)
        m.block((p[0], p[1], 1700), (50, 50, 3400), "BuntingPole", f + "/Party", "Metal", shape="cylinder")
    S.append(L.arena(m, 1, c0, 4600, f + "/S1"))
    # S2 kitchens: a block off the east side
    L.room(m, 10500, -6000, 20000, 6000, 0, 2600, f + "/Kitchens", "Concrete", floor="Marking", gaps={"W": [(0.4, 0.6)]}, roof="Concrete")
    for k in range(6):
        m.box(11500, -5000 + k * 1700, 0, 19000, -4500 + k * 1700, 130, "KitchenCounter", f + "/Kitchens", "Metal")
    S.append(L.arena(m, 2, (15250.0, 0.0, 0.0), 3600, f + "/S2"))
    # S3 rooftop: a raised deck above the dome (z=6500) reached by a spiral ramp
    L.spiral_ramp(m, (0.0, 12000.0, 0.0), 3600.0, 0.0, 6500.0, 1.5, f + "/Roof", "Concrete", 700.0)
    L.slab(m, -4500, 8000, 4500, 16500, 6500, f + "/Roof", "Concrete")
    L.wall_run(m, -4500, 8000, 4500, 8000, 6500, 110, f + "/Roof", "Metal")
    L.wall_run(m, -4500, 16500, 4500, 16500, 6500, 110, f + "/Roof", "Metal")
    S.append(L.arena(m, 3, (0.0, 12250.0, 6500.0), 3500, f + "/S3"))
    # S4 printing hall below: a long hall of presses, reached by a ramp west
    L.slab(m, -26000, -6000, -12000, 6000, -3000, f + "/Print", "Dark")
    L.stairs_to(m, (-9500, 0, 0), (-12000, 0, -3000), f + "/Print", "Concrete", 900.0, 80.0)
    for k in range(7):
        m.block((-24000 + k * 2000, -3600, -3000 + 700), (900, 1400, 1400), "Press", f + "/Print", "Metal")
        m.block((-24000 + k * 2000, 3600, -3000 + 700), (900, 1400, 1400), "Press", f + "/Print", "Metal")
    S.append(L.arena(m, 4, (-19000.0, 0.0, -3000.0), 4200, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90 + 70 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, 3300, c[2], "city", 12, f + "/S%d" % (i + 1), spec, keep=1200)
        L.lights(m, c, c[2], 2600, 4, 30000)
    return S, (-3500.0, 0.0, 0.0)


def ch35(m, rng, spec):
    """The Ledger: the Long Gallery. Four narrow year rooms in a row, then the ledger desk hall."""
    f = "Ch35"
    S = []
    x = 0.0
    centres = []
    for i in range(4):
        w = 14000.0
        L.room(m, x, -2400, x + w, 2400, 0, 3000, f + "/Room%d" % (i + 1), "Concrete", floor="Stone",
               gaps={"W": [(0.35, 0.65)] if i else [], "E": [(0.35, 0.65)]}, roof="Concrete")
        for k in range(10):   # drawer banks along the long walls
            m.block((x + 1000 + k * 1200, 2250, 900), (900, 220, 1800), "DrawerBank", f + "/Room%d" % (i + 1), "Wood")
            m.block((x + 1000 + k * 1200, -2250, 900), (900, 220, 1800), "DrawerBank", f + "/Room%d" % (i + 1), "Wood")
        for k in range(8):   # the counted: faded figures in rows
            for r in (-1, 1):
                m.block((x + 1500 + k * 1400, r * 700, 90), (140, 140, 180), "CountedFigure", f + "/Room%d" % (i + 1), "Cloud", shape="cylinder",
                        collision=False)
        centres.append((x + w / 2, 0.0, 0.0))
        S.append(L.arena(m, i + 1, centres[-1], 2300, f + "/S%d" % (i + 1), boss=i == 3))
        x += w
    # the ledger desk at the east end with a lamp
    m.block((x - 1800, 0, 450), (1500, 900, 900), "LedgerDesk", f + "/Room4", "Wood")
    m.block((x - 1800, 0, 1300), (140, 140, 700), "DeskLamp", f + "/Room4", "NeonYellow", shape="cylinder")
    for i, c in enumerate(centres):
        L.props(m, rng, c, 2200, 0, "vault", 8, f + "/S%d" % (i + 1), spec, keep=1100)
        L.lights(m, c, 0, 1500, 2, 18000)
        L.jump_and_climb(m, c, 90, f + "/S%d" % (i + 1), 3)
    return S, (800.0, 0.0, 0.0)


def ch36(m, rng, spec):
    """Unity: the Great Hall of the arc: a long hall with galleries, a dais, huge doors, and Charter Square beyond."""
    f = "Ch36"
    S = []
    L.room(m, -9000, -16000, 9000, 16000, 0, 7500, f + "/Hall", "Concrete", floor="Marking", gaps={"S": [(0.4, 0.6)], "N": [(0.4, 0.6)]}, roof=None)
    L.colonnade(m, -7500, -15000, -7500, 15000, 14, 0, 7500, f + "/Hall", "Stone", r=200, cap="NeonYellow")
    L.colonnade(m, 7500, -15000, 7500, 15000, 14, 0, 7500, f + "/Hall", "Stone", r=200, cap="NeonYellow")
    # galleries on both sides at +3200
    m.box(-9000, -14000, 3100, -6500, 14000, 3200, "WestGallery", f + "/Gallery", "Concrete")
    m.box(6500, -14000, 3100, 9000, 14000, 3200, "EastGallery", f + "/Gallery", "Concrete")
    wall_run = L.wall_run
    wall_run(m, -6500, -14000, -6500, 14000, 3200, 110, f + "/Gallery", "Metal")
    wall_run(m, 6500, -14000, 6500, 14000, 3200, 110, f + "/Gallery", "Metal")
    L.stairs_to(m, (-8000, -15000, 0), (-8000, -11000, 3200), f + "/Gallery", "Concrete", 700.0, 80.0)
    L.stairs_to(m, (8000, -15000, 0), (8000, -11000, 3200), f + "/Gallery", "Concrete", 700.0, 80.0)
    # the dais at the north end with a long signature table and chairs
    for k in range(3):
        m.box(-3500 + k * 400, 10000 + k * 600, k * 120, 3500 - k * 400, 10600 + k * 600 + 0, 120 * (k + 1), "DaisStep", f + "/Dais", "Stone")
    m.box(-2800, 12000, 360, 2800, 12800, 460, "SignatureTable", f + "/Dais", "Wood")
    for k in range(7):
        m.block((-2400 + k * 800, 11300, 540), (400, 400, 160), "SignatoryChair", f + "/Dais", "NeonYellow", shape="cylinder")
    # the great doors at the south end (forty feet = 1200 cm)
    m.block((-2300, -16000, 1200), (2200, 200, 2400), "GreatDoorLeaf", f + "/Doors", "Wood")
    m.block((2300, -16000, 1200), (2200, 200, 2400), "GreatDoorLeaf", f + "/Doors", "Wood")
    # charter square beyond the north doors
    L.slab(m, -12000, 16000, 12000, 34000, 0, f + "/Square", "Concrete")
    m.block((0, 25000, 300), (3000, 3000, 600), "Fountain", f + "/Square", "Water", shape="cylinder")
    S.append(L.arena(m, 1, (0.0, -9500.0, 0.0), 4200, f + "/S1"))
    S.append(L.arena(m, 2, (-5000.0, 1500.0, 3200.0), 2400, f + "/S2"))
    S.append(L.arena(m, 3, (0.0, 5000.0, 0.0), 4000, f + "/S3"))
    S.append(L.arena(m, 4, (0.0, 13500.0, 400.0), 3300, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        L.props(m, rng, c, S[i][2], c[2], "court", 10, f + "/S%d" % (i + 1), spec, keep=1200)
        L.jump_and_climb(m, c, 90 + 50 * i, f + "/S%d" % (i + 1), 3)
        L.lights(m, c, c[2], 2600, 3, 36000)
    return S, (0.0, -14500.0, 0.0)


def ch37(m, rng, spec):
    """The Alliance Breaks: four districts of the Ark around a central stair: hall ruin, burning dome, garrison, government quarter."""
    f = "Ch37"
    S = []
    c0 = (0.0, 0.0, 0.0)
    L.slab(m, -22000, -22000, 22000, 22000, 0, f + "/Ground", "Dark")
    for k in range(4):   # central stair: a stepped plaza
        m.block((0, 0, 100 + k * 100), (4200 - k * 800, 4200 - k * 800, 200), "CentralStair", f + "/Centre", "Stone", shape="cylinder")
    cs = [(-13000.0, 0.0, 0.0), (0.0, -13000.0, 0.0), (13000.0, 0.0, 0.0), (0.0, 13000.0, 0.0)]
    for i, c in enumerate(cs):
        S.append(L.arena(m, i + 1, c, 4300, f + "/S%d" % (i + 1), boss=i == 3))
    # S1 wrecked hall: broken columns and trampled paper
    for k in range(8):
        p = gs._az(cs[0], 3200, k * 45)
        m.block((p[0], p[1], 700 - (k % 3) * 300), (300, 300, 1400 - (k % 3) * 600), "BrokenColumn", f + "/S1", "Stone", shape="cylinder",
                roll=(k % 3) * 8)
    # S2 the burning dome: dome shell and fire columns
    m.dome(cs[1], 3600.0, f + "/S2", n_az=20, n_el=6, thickness=80.0, material="Concrete", gap=lambda az, b: 60 < az < 120 and b < 3)
    for k in range(10):
        p = gs._az(cs[1], rng.uniform(800, 3200), rng.uniform(0, 360))
        m.block((p[0], p[1], 600), (240, 240, 1200 + rng.uniform(0, 900)), "FireColumn", f + "/S2", "NeonRed", shape="cylinder", collision=False)
    # S3 secrets' garrison: whitewashed walls and gates
    L.room(m, cs[2][0] - 4000, -4000, cs[2][0] + 4000, 4000, 0, 2800, f + "/S3", "Cloud", floor="Concrete", gaps={"W": [(0.4, 0.6)]}, roof=None)
    L.room(m, cs[2][0] - 1800, -1800, cs[2][0] + 1800, 1800, 0, 2200, f + "/S3", "Cloud", gaps={"S": [(0.4, 0.6)]})
    # S4 government quarter: the colossus on a plinth
    m.block((cs[3][0], cs[3][1] + 1500, 150), (2600, 2600, 300), "ColossusPlinth", f + "/S4", "Concrete")
    L.tower_stack(m, (cs[3][0] - 3500, cs[3][1] + 2500, 0), 800, 6, 900, f + "/S4", "Concrete", "NeonRed")
    for i, c in enumerate(cs):
        L.walkway(m, (c[0] * 0.28, c[1] * 0.28, 0), (c[0] * 0.72, c[1] * 0.72, 0), f + "/Links", "Stone", 1200.0, rail=False)
        L.jump_and_climb(m, c, 45 + 90 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, 4000, 0, "war", 16, f + "/S%d" % (i + 1), spec, keep=1200)
    return S, (-17000.0, 0.0, 0.0)


def ch38(m, rng, spec):
    """Who Is Strongest: three island fronts over a sea, linked by bridges, and the lighthouse stair arena."""
    f = "Ch38"
    S = []
    L.slab(m, -26000, -26000, 26000, 26000, -1500, f + "/SeaBed", "Sand")
    m.water(-26000, -26000, -1500, 26000, 26000, -100, "Sea", f + "/Water")
    # three islands: docks (S1), moon fortress (S2), hanging-city platform (S3), lighthouse stair (S4)
    cs = [(-15000.0, -8000.0, 0.0), (0.0, 12000.0, 0.0), (15000.0, -4000.0, 1600.0), (3000.0, -17000.0, 0.0)]
    for i, c in enumerate(cs):
        m.block((c[0], c[1], c[2] - 400), (9400, 9400, 800), "Island", f + "/S%d" % (i + 1), "Sand" if i != 2 else "Concrete", shape="cylinder")
        S.append(L.arena(m, i + 1, c, 4300, f + "/S%d" % (i + 1), boss=i == 3))
    # bridges: planks between the islands
    for a, b in ((0, 1), (1, 2), (0, 3), (3, 2)):
        pa, pb = cs[a], cs[b]
        d = math.hypot(pb[0] - pa[0], pb[1] - pa[1])
        ux, uy = (pb[0] - pa[0]) / d, (pb[1] - pa[1]) / d
        L.walkway(m, (pa[0] + ux * 4300, pa[1] + uy * 4300, pa[2]), (pb[0] - ux * 4300, pb[1] - uy * 4300, pb[2]), f + "/Bridges", "Wood", 700.0)
    # S1 barricade lines; S2 vine walls; S3 tilted plank city; S4 the lighthouse and its stair
    for k in range(6):
        m.block((cs[0][0] - 3000 + k * 1100, cs[0][1] + 2600, 350), (700, 200, 700), "DockBarricade", f + "/S1", "Wood")
    for k in range(8):
        p = gs._az(cs[1], 3600, k * 45)
        m.block((p[0], p[1], 500), (300, 900, 1000), "VineBarricade", f + "/S2", "Flora", yaw=k * 45)
    for k in range(5):
        m.block((cs[2][0] - 2400 + k * 1200, cs[2][1], cs[2][2] + 200), (900, 3600, 80), "TiltedPlank", f + "/S3", "Wood", roll=4 + k)
    L.tower_stack(m, (cs[3][0], cs[3][1] - 1500, 0), 700, 9, 1000, f + "/S4", "Stone", "NeonYellow")
    L.spiral_ramp(m, (cs[3][0], cs[3][1] - 1500, 0), 1700.0, 0.0, 9000.0, 3.0, f + "/S4", "Stone", 500.0)
    m.block((cs[3][0], cs[3][1] - 1500, 9300), (900, 900, 900), "Lamp", f + "/S4", "NeonYellow", shape="sphere", collision=False)
    for i, c in enumerate(cs):
        L.jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, 3800, c[2], "war", 14, f + "/S%d" % (i + 1), spec, keep=1200)
    return S, (cs[0][0] - 3500, cs[0][1], 0.0)


def ch39(m, rng, spec):
    """Nowhere, Inhabited: grey echoes of the whole story laid on a long path, a field of the counted, Death's gallery."""
    f = "Ch39"
    S = []
    L.slab(m, -3000, -3000, 70000, 3000, 0, f + "/Path", "PureGrey")
    xs = [3000.0, 20000.0, 37000.0, 54000.0]
    # echoes along the path: crossing, quay, chain, data hall, with the counted field before the gallery
    for k in range(6):   # S1 crossing: signal poles and a zebra
        m.block((xs[0] - 2500 + k * 1000, 3300, 1500), (120, 120, 3000), "EchoSignalPole", f + "/S1", "PureGrey", shape="cylinder")
        m.block((xs[0] - 2500 + k * 1000, 0, 4), (600, 5200, 8), "EchoZebra", f + "/S1", "Marking", collision=False)
    for k in range(10):   # S2 quay and lighthouse
        m.block((xs[1] - 4000 + k * 800, -2800, 300), (700, 160, 600), "EchoPierPost", f + "/S2", "PureGrey")
    m.block((xs[1] + 2500, 3500, 3000), (1100, 1100, 6000), "EchoLighthouse", f + "/S2", "PureGrey", shape="cylinder")
    for k in range(12):   # S3 chain links rising from the ground, the child on a lift
        m.block((xs[2] - 3000 + k * 550, 2800, 300 + k * 450), (500, 200, 300), "EchoChainLink", f + "/S3", "PureGrey", roll=(k % 2) * 90)
    # the counted: rows of faded figures
    for r in range(10):
        for c in range(26):
            m.block((xs[2] + 2500 + c * 650, -2400 + r * 540, 90), (130, 130, 180), "CountedFigure", f + "/Counted", "Cloud", shape="cylinder", collision=False)
    # S4 Death's gallery: low room with a desk and a single lamp
    L.room(m, xs[3] - 4000, -3000, xs[3] + 7000, 3000, 0, 2400, f + "/Gallery", "PureGrey", floor="PureGrey", gaps={"W": [(0.35, 0.65)]}, roof="PureGrey")
    m.block((xs[3] + 5200, 0, 450), (1500, 900, 900), "DeathDesk", f + "/Gallery", "PureGrey")
    m.block((xs[3] + 5200, 0, 1300), (140, 140, 700), "DeskLamp", f + "/Gallery", "NeonYellow", shape="cylinder")
    for i, x in enumerate(xs):
        S.append(L.arena(m, i + 1, (x, 0.0, 0.0), 2800, f + "/S%d" % (i + 1), boss=i == 3))
        L.props(m, rng, (x, 0.0, 0.0), 2600, 0, "gallery", 8, f + "/S%d" % (i + 1), spec, keep=900)
        L.jump_and_climb(m, (x, 0.0, 0.0), 90, f + "/S%d" % (i + 1), 3)
    return S, (-1500.0, 0.0, 0.0)


def ch40(m, rng, spec):
    """They Were Purity: the valley. Torii, paddies, river and bridge, the shrine stair, the monolith plaza."""
    f = "Ch40"
    S = []
    L.slab(m, -26000, -26000, -1800, 26000, -50, f + "/Ground", "Moss")
    L.slab(m, 1800, -26000, 26000, 26000, -50, f + "/Ground", "Moss")
    L.slab(m, -1800, -26000, 1800, 26000, -1500, f + "/Riverbed", "Sand")
    # paddies: a grid of shallow water beds in the south-west
    for gx in range(6):
        for gy in range(4):
            m.water(-21000 + gx * 3200, -14000 + gy * 3000, -150, -18400 + gx * 3200, -11600 + gy * 3000, -10, "Paddy%d_%d" % (gx, gy), f + "/Paddies",
                    surface=True)
    # the river along x=0 with a wooden bridge
    m.water(-1800, -26000, -1500, 1800, 26000, -50, "River", f + "/River")
    m.box(-2400, -1200, -40, 2400, 1200, 40, "Bridge", f + "/Bridge", "Wood")
    L.wall_run(m, -2400, -1200, 2400, -1200, 40, 110, f + "/Bridge", "Wood")
    L.wall_run(m, -2400, 1200, 2400, 1200, 40, 110, f + "/Bridge", "Wood")
    # torii at the start; the village street; the shrine on a 50-step stair; the monolith plaza on the far bank
    for dx in (-400, 400):
        m.block((-19500 + 0, dx, 1100), (160, 160, 2200), "ToriiPost", f + "/Torii", "NeonRed", shape="cylinder")
    m.block((-19500, 0, 2300), (200, 1300, 160), "ToriiBeam", f + "/Torii", "NeonRed")
    for k in range(8):
        m.block((-14000 + k * 1700, -20000 + (k % 2) * 2500, 500), (1200, 1200, 1000), "VillageHouse", f + "/Village", "Wood")
        m.block((-14000 + k * 1700, -20000 + (k % 2) * 2500, 1200), (1500, 1400, 40), "VillageRoof", f + "/Village", "Roof", roll=12)
    L.stairs_to(m, (-12000, 15000, 0), (-12000, 18000, 4000), f + "/Shrine", "Stone", 700.0, 80.0)
    m.box(-13200, 18000, 3960, -10800, 20500, 4000, "ShrineFloor", f + "/Shrine", "Wood")
    m.block((-12000, 19200, 4600), (1400, 1200, 1200), "ShrineHouse", f + "/Shrine", "Wood")
    mc = (14000.0, 0.0, 0.0)
    m.block((mc[0], mc[1], 2500), (600, 300, 5000), "Monolith", f + "/Monolith", "Monolith")
    for k in range(24):
        m.block((mc[0] - 151, ((k % 4) - 1.5) * 90, 500 + (k // 4) * 700), (4, 40, 220), "RedGlyph", f + "/Monolith", "RedGlyph", collision=False)
    m.block((mc[0], mc[1], -20), (9000, 9000, 40), "MonolithPlaza", f + "/Monolith", "Stone", shape="cylinder")
    S.append(L.arena(m, 1, (-16000.0, -18000.0, 0.0), 3800, f + "/S1"))
    S.append(L.arena(m, 2, (-10000.0, -2500.0, 0.0), 4400, f + "/S2"))
    S.append(L.arena(m, 3, (0.0, 0.0, 0.0), 2800, f + "/S3"))
    S.append(L.arena(m, 4, mc, 4400, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        L.jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        L.props(m, rng, c, S[i][2], 0, "village", 12, f + "/S%d" % (i + 1), spec, keep=1100)
    return S, (-19000.0, -2000.0, 0.0)


LEVELS = dict(L.LEVELS)
LEVELS.update({28: ch28, 29: ch29, 30: ch30, 31: ch31, 32: ch32, 33: ch33, 34: ch34, 35: ch35, 36: ch36, 37: ch37, 38: ch38, 39: ch39, 40: ch40})


# ================================================================== driver
def build_map(n, spec, materials, log):
    path = MAPS + "/L_Ch%02d_%s" % (n, spec["short"])
    gs._backup(log, path)
    m = GreyboxMap(path, materials, log)
    rng = random.Random(9000 + n)
    m.lighting(sun=3.0, sun_colour=tuple(spec["sun"]), pitch=-35, fog=spec["fog"], fog_colour=(0.35, 0.35, 0.45), volumetric=spec["indoor"])
    sections, start = LEVELS[n](m, rng, spec)
    placed = L.place_assets(m, n, sections, log)
    L.context(m, rng, spec, sections)
    spec = dict(spec)
    gs.story_markers(m, n, spec, sections, folder="Story", start=start, log=log)
    L.fill_detail(m, rng, sections, spec, [spec["floor"], "Grey", "Concrete", spec["floor"]])
    m.world_settings(unreal.GameModeBase.static_class())
    total = m.save(anime=True)
    log("Ch %02d %s: %d actors, %d asset stand-ins, boss %s" % (n, spec["short"], total, placed, spec["boss"]))
    return total


def build(materials, log, only=None):
    data = gs.load()
    built = {}
    for n in sorted(LEVELS):
        if n in gs.LOCKED or (only and n not in only):
            continue
        built[n] = build_map(n, data[n], materials, log)
    return built
