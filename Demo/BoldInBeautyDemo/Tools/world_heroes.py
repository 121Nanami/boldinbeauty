"""Hero set pieces for the large story maps (greybox_world.py): big, enterable, themed structures. Greybox shapes only.

Every function takes (w, c, ...) where w is the World and c the centre (x, y, z), and returns nothing; they add geometry,
reserve space so districts keep clear, and use the chapter's palette (w.spec)."""

import math

import greybox_levels as L
import greybox_story as gs


def _f(w, name):
    return "%s/Hero_%s" % (w.f, name)


def tower(w, c, r=4200.0, floors=24, floor_h=1000.0, mast=True):
    m, s = w.m, w.spec
    f = _f(w, "Tower")
    L.tower_stack(m, c, r, floors, floor_h, f, s["wall"], s["accent"])
    ring = L.tiers  # podium of terraces at the base
    ring(m, c, r + 600, 3, 300.0, 700.0, f, "Concrete", seg=36, z0=c[2], skip=lambda az, i: 170 < az < 190)
    if mast:
        L.lattice(m, (c[0], c[1], c[2] + floors * floor_h), 900.0, 7000.0, f, "Metal")
    w.reserve(c[0], c[1], r + 3500)


def hall(w, c, width=24000.0, depth=14000.0, h=4200.0, style="court", gap_sides=("S", "N")):
    """A big hall (width along x, depth along y) with colonnades and style-specific contents."""
    m, s, rng = w.m, w.spec, w.rng
    f = _f(w, "Hall_" + style)
    x0, x1 = c[0] - width / 2, c[0] + width / 2
    y0, y1 = c[1] - depth / 2, c[1] + depth / 2
    gaps = {side: [(0.2, 0.3), (0.7, 0.8)] for side in gap_sides}
    L.room(m, x0, y0, x1, y1, c[2], h, f, s["wall"], floor=s["floor"], gaps=gaps, roof=s["wall"])
    n = int(width // 1800)
    L.colonnade(m, x0 + 900, y0 + 900, x1 - 900, y0 + 900, n, c[2], h, f, "Stone", r=150, cap=s["accent"])
    L.colonnade(m, x0 + 900, y1 - 900, x1 - 900, y1 - 900, n, c[2], h, f, "Stone", r=150, cap=s["accent"])
    if style == "court":
        L.tiers(m, (c[0], y1 - 600, c[2]), 2800, 4, 100.0, 650.0, f, "Wood", arc=(200.0, 340.0), seg=28, z0=c[2])
        m.box(c[0] - 2200, y1 - 1500, c[2], c[0] + 2200, y1 - 700, c[2] + 480, "Bench", f, "Wood")
        for r_ in range(5):
            m.box(c[0] - 5500, c[1] - 3500 + r_ * 1100, c[2], c[0] + 5500, c[1] - 3200 + r_ * 1100, c[2] + 100, "Pew", f, "Wood")
    elif style == "library":
        for k in range(int(width // 1500)):
            m.block((x0 + 1500 + k * 1500, c[1] - 2500, c[2] + h * 0.4), (400, 6000, h * 0.8), "Shelf", f, "Wood")
            m.block((x0 + 1500 + k * 1500, c[1] + 3500, c[2] + h * 0.4), (400, 4000, h * 0.8), "Shelf", f, "Wood")
    elif style == "cathedral":
        for k in range(int(width // 3000)):
            m.block((x0 + 1500 + k * 3000, c[1], c[2] + h - 200), (140, depth - 1800, 160), "Rib", f, "Stone")
        for k in range(4):
            m.block((c[0] - 6000 + k * 4000, y0 + 2500, c[2] + 1900), (2600, 2600, 140), "GreatGear", f, "Metal", shape="cylinder", pitch=90)
    elif style == "casino":
        for ring_i, rad in enumerate((2500, 4200, 5800)):
            for k in range(8 + ring_i * 4):
                a = k * 360.0 / (8 + ring_i * 4)
                p = gs._az(c, rad, a, c[2])
                m.block((p[0], p[1], c[2] + 190), (560, 560, 380), "TableBase", f, "Wood", shape="cylinder")
                m.block((p[0], p[1], c[2] + 400), (900, 900, 40), "TableFelt", f, "Grass" if ring_i % 2 == 0 else s["accent"], shape="cylinder")
    elif style == "canteen":
        for r_ in range(6):
            m.box(x0 + 2500, c[1] - 4500 + r_ * 1800, c[2], x1 - 2500, c[1] - 4100 + r_ * 1800, c[2] + 120, "LongTable", f, "Wood")
    elif style == "gallery":
        for k in range(int(width // 1300)):
            m.block((x0 + 900 + k * 1300, y0 + 450, c[2] + h * 0.4), (900, 240, h * 0.8), "DrawerBank", f, "Wood")
            m.block((x0 + 900 + k * 1300, y1 - 450, c[2] + h * 0.4), (900, 240, h * 0.8), "DrawerBank", f, "Wood")
    elif style == "mall":
        L.tiers(m, c, 5200, 1, 1800.0, 900.0, f, "Concrete", arc=(20.0, 340.0), seg=34, z0=c[2])
        for k in range(14):
            p = gs._az(c, 6800, 25 + k * 22.5, c[2])
            m.block((p[0], p[1], c[2] + 900), (300, 1100, 1800), "ShopFront", f, rng.choice(["Magenta", "Turquoise", "Coral", "Lime"]), yaw=25 + k * 22.5)
    elif style == "stage":
        m.box(x1 - 6000, c[1] - 5500, c[2], x1 - 700, c[1] + 5500, c[2] + 1100, "Stage", f, "Wood")
        for r_ in range(10):
            for k in range(14):
                m.block((x0 + 2500 + r_ * 900, c[1] - 5400 + k * 800, c[2] + 200), (500, 600, 400), "Seat", f, "NeonRed")
    elif style == "factory":
        for lane in range(4):
            for k in range(12):
                m.mover((x0 + 2500 + k * 1700, c[1] - 4500 + lane * 3000, c[2] + 120), (1500, 900, 220), "ConveyorSegment", f, "Dark",
                        offset=(900, 0, 0), period=3.0, phase=k * 0.1)
    elif style == "white":
        for k in range(int(width // 2600)):
            m.block((x0 + 1300 + k * 2600, c[1], c[2] + h / 2), (260, 260, h), "WhiteColumn", f, "Cloud", shape="cylinder")
    elif style == "council":
        L.tiers(m, c, 5200, 1, 100.0, 1000.0, f, "Wood", seg=36, z0=c[2], skip=lambda az, i: 170 < az < 190)
        m.block((c[0], c[1], c[2] + 380), (5200, 5200, 120), "CouncilTable", f, "Wood", shape="cylinder")
        for k in range(7):
            p = gs._az(c, 3400, 180 + k * 360 / 7, c[2])
            m.block((p[0], p[1], c[2] + 300), (320, 320, 600), "CommanderChair", f, s["accent"], shape="cylinder")
    w.reserve(c[0], c[1], max(width, depth) / 2 + 2500)


def dome(w, c, radius=12000.0, inner=True):
    m, s = w.m, w.spec
    f = _f(w, "Dome")
    m.dome(c, radius, f, n_az=40, n_el=9, thickness=100.0, material=s["wall"], gap=lambda az, b: 170 < az < 190 and b < 4)
    L.slab(m, c[0] - radius, c[1] - radius, c[0] + radius, c[1] + radius, c[2], f, s["floor"])
    if inner:
        L.tiers(m, c, radius * 0.45, 3, 300.0, 900.0, f, "Metal", seg=44, z0=c[2], skip=lambda az, i: 170 < az < 190)
    w.reserve(c[0], c[1], radius + 2500)


def tunnel(w, a, b, width=7000.0, h=2600.0, train=False, label="Tunnel"):
    """A long straight tunnel along x or y (a and b share one coordinate)."""
    m, s = w.m, w.spec
    f = _f(w, label)
    x0, y0 = min(a[0], b[0]), min(a[1], b[1])
    x1, y1 = max(a[0], b[0]), max(a[1], b[1])
    horizontal = abs(b[0] - a[0]) >= abs(b[1] - a[1])
    z = a[2]
    if horizontal:
        y0, y1 = a[1] - width / 2, a[1] + width / 2
    else:
        x0, x1 = a[0] - width / 2, a[0] + width / 2
    L.slab(m, x0, y0, x1, y1, z, f, "Dark")
    L.slab(m, x0, y0, x1, y1, z + h + 40, f, s["wall"], t=40, label="Roof")
    if horizontal:
        L.wall_run(m, x0, y0, x1, y0, z, h, f, s["wall"])
        L.wall_run(m, x0, y1, x1, y1, z, h, f, s["wall"])
        for k in range(int((x1 - x0) // 1500)):
            xx = x0 + k * 1500
            for ry in (a[1] - 800, a[1] + 800):
                m.block((xx + 750, ry, z + 14), (1500, 14, 14), "Rail", f, "Metal", collision=False)
            if k % 4 == 0:
                m.block((xx, y0 + 80, z + h - 200), (200, 80, 200), "TunnelLamp", f, s["accent"], collision=False)
                m.point_light((xx, a[1], z + h - 250), "Lighting", intensity=24000.0, radius=3200.0)
        if train:
            for car in range(8):
                m.mover((x0 + 6000 + car * 1200, a[1], z + 700), (1100, 1200, 1400), "TrainCarriage", f, "Metal", offset=(12000, 0, 0), period=24.0,
                        tags=["Train"])
    else:
        L.wall_run(m, x0, y0, x0, y1, z, h, f, s["wall"])
        L.wall_run(m, x1, y0, x1, y1, z, h, f, s["wall"])
        for k in range(int((y1 - y0) // 1500)):
            yy = y0 + k * 1500
            if k % 4 == 0:
                m.point_light((a[0], yy, z + h - 250), "Lighting", intensity=24000.0, radius=3200.0)
    for k in range(int(max(x1 - x0, y1 - y0) // 12000)):   # cross-cuts so the tunnel is not a single line
        pass
    w.reserve((x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0) / 2 + 800)


def terraces(w, c, steps=4, dz=1800.0, width=18000.0, depth=9000.0, mat="Concrete"):
    """Stepped terraces climbing along +y, each with a retaining wall; returns the centre of each terrace."""
    m = w.m
    f = _f(w, "Terraces")
    out = []
    for i in range(steps):
        y0 = c[1] + i * depth
        z = c[2] + i * dz
        m.box(c[0] - width / 2, y0, c[2] - 200, c[0] + width / 2, y0 + depth, z, "Terrace", f, mat)
        m.box(c[0] - width / 2, y0, z, c[0] + width / 2, y0 + 140, z + 300, "TerraceWall", f, "Stone")
        if i:
            L.stairs_to(m, (c[0], y0 - 2000, z - dz), (c[0], y0 + 200, z), f, mat, 1800.0, 80.0)
        out.append((c[0], y0 + depth / 2, z))
    w.reserve(c[0], c[1] + steps * depth / 2, max(width, steps * depth) / 2 + 1500)
    return out


def maze(w, c, size=30000.0, cells=18, h=2600.0, mat="Wood"):
    m = w.m
    f = _f(w, "Maze")
    L.maze(m, c[0] - size / 2, c[1] - size / 2, c[0] + size / 2, c[1] + size / 2, cells, cells, c[2], h, f, mat, w.rng, gap_pct=0.05)
    w.reserve(c[0], c[1], size * 0.72)


def bigtop(w, c, r=9000.0):
    m, s = w.m, w.spec
    f = _f(w, "BigTop")
    m.block((c[0], c[1], c[2] + 2800), (r * 2, r * 2, 5600), "BigTop", f, s["accent"], shape="cone", collision=False)
    for k in range(10):
        p = gs._az(c, r * 0.85, k * 36, c[2])
        m.block((p[0], p[1], c[2] + 2200), (260, 260, 4400), "TentPole", f, "Metal", shape="cylinder")
    m.block((c[0], c[1], c[2] + 40), (r * 1.5, r * 1.5, 80), "Sawdust", f, "Sand", shape="cylinder")
    w.reserve(c[0], c[1], r + 1500)


def monolith(w, c, plaza=9000.0):
    m = w.m
    f = _f(w, "Monolith")
    m.block((c[0], c[1], c[2] - 20), (plaza * 2, plaza * 2, 40), "Plaza", f, "Stone", shape="cylinder")
    m.block((c[0], c[1], c[2] + 2500), (600, 300, 5000), "Monolith", f, "Monolith")
    for k in range(24):
        m.block((c[0] - 151, c[1] + ((k % 4) - 1.5) * 90, c[2] + 500 + (k // 4) * 700), (4, 40, 220), "RedGlyph", f, "RedGlyph", collision=False)
    w.reserve(c[0], c[1], plaza + 1500)


def lighthouse(w, c):
    m, s = w.m, w.spec
    f = _f(w, "Lighthouse")
    L.tower_stack(m, c, 1100, 10, 1000, f, "Stone", s["accent"])
    L.spiral_ramp(m, c, 1900.0, c[2], c[2] + 10000.0, 3.0, f, "Stone", 500.0)
    m.block((c[0], c[1], c[2] + 10400), (900, 900, 900), "Lamp", f, s["accent"], shape="sphere", collision=False)
    w.reserve(c[0], c[1], 3800)


def colosseum(w, c, r=7000.0, steps=6):
    f = _f(w, "Colosseum")
    L.tiers(w.m, c, r, steps, 220.0, 800.0, f, "Concrete", seg=48, z0=c[2], skip=lambda az, i: i == 0 and (80 < az < 100 or 260 < az < 280))
    w.reserve(c[0], c[1], r + steps * 800 + 1500)


def lanes(w, c, length=36000.0, count=6):
    m = w.m
    f = _f(w, "Lanes")
    kinds = ("hurdles", "wall", "net", "beam", "crates", "hurdles")
    for lane in range(count + 1):
        y = c[1] - count * 1500 + lane * 3000
        L.wall_run(m, c[0] - length / 2, y, c[0] + length / 2, y, c[2], 320, f, "Metal", t=40)
    for lane in range(count):
        y = c[1] - count * 1500 + 1500 + lane * 3000
        kind = kinds[lane % len(kinds)]
        for k in range(int(length // 3000)):
            x = c[0] - length / 2 + 1500 + k * 3000
            if kind == "hurdles":
                m.block((x, y, c[2] + 450), (60, 1800, 900), "Hurdle", f, "Hazard")
            elif kind == "wall":
                m.block((x, y, c[2] + 800), (200, 1800, 1600), "ClimbObstacle", f, "Climb", tags=["Climbable"])
            elif kind == "net":
                m.block((x, y, c[2] + 1100), (40, 1800, 2200), "RopeNet", f, "Climb", tags=["Climbable"], collision=False)
            elif kind == "beam":
                m.block((x, y, c[2] + 650), (1200, 260, 1300), "BalanceBeamStand", f, "Jump", tags=["JumpUp"])
            else:
                m.block((x, y, c[2] + 400), (500, 500, 800), "CratePyramid", f, "Wood")
    w.reserve(c[0], c[1], length / 2 + 1500)


def machine(w, c, size=16000.0):
    m, s = w.m, w.spec
    f = _f(w, "Machine")
    for k in range(7):
        m.block((c[0] - size / 2 + k * size / 6, c[1], c[2] + 2200), (900, 900, 4400), "MachineColumn", f, "Metal", shape="cylinder")
    m.block((c[0], c[1], c[2] + 4600), (size, 1600, 400), "MachineBeam", f, s["accent"], collision=False)
    m.block((c[0], c[1], c[2] + 800), (size, 2400, 1600), "MachineBase", f, "Dark")
    for k in range(6):
        m.mover((c[0] - size / 2 + 1500 + k * 2600, c[1] + 2200, c[2] + 900), (240, 240, 2200), "RobotArm", f, "Hazard", swing=True, rotation=(0, 0, 30),
                period=4.0 + k * 0.3)
    w.reserve(c[0], c[1], size / 2 + 1500)


def counted(w, c, rows=16, cols=40, step=640.0):
    m = w.m
    f = _f(w, "Counted")
    for r in range(rows):
        for k in range(cols):
            m.block((c[0] - cols * step / 2 + k * step, c[1] - rows * step / 2 + r * step, c[2] + 90), (130, 130, 180), "CountedFigure", f, "Cloud",
                    shape="cylinder", collision=False)
    w.reserve(c[0], c[1], max(rows, cols) * step / 2 + 1200)


def vault(w, c, size=14000.0):
    m, s = w.m, w.spec
    f = _f(w, "Vault")
    L.room(m, c[0] - size / 2, c[1] - size / 2, c[0] + size / 2, c[1] + size / 2, c[2], 3600, f, "Metal", floor="Dark",
           gaps={"S": [(0.4, 0.6)]}, roof="Metal", t=120)
    m.block((c[0], c[1] - size / 2, c[2] + 1600), (4800, 400, 3200), "VaultDoorRing", f, "Metal", collision=False)
    for k in range(int(size // 900) - 2):
        m.block((c[0] - size / 2 + 700, c[1] - size / 2 + 1500 + k * 900, c[2] + 900), (300, 700, 1800), "DrawerBank", f, "Metal")
        m.block((c[0] + size / 2 - 700, c[1] - size / 2 + 1500 + k * 900, c[2] + 900), (300, 700, 1800), "DrawerBank", f, "Metal")
    w.reserve(c[0], c[1], size / 2 + 1800)


HEROES = {"tower": tower, "hall": hall, "dome": dome, "tunnel": tunnel, "terraces": terraces, "maze": maze, "bigtop": bigtop,
          "monolith": monolith, "lighthouse": lighthouse, "colosseum": colosseum, "lanes": lanes, "machine": machine,
          "counted": counted, "vault": vault}
