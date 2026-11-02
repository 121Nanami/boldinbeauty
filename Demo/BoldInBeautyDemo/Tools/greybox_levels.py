"""Bespoke story levels, chapters 14-27 (owner request 2026-10-11: "proper high quality" levels, one layout per chapter).

Chapters 28-40 are in greybox_levels2.py. Every chapter has its own macro layout built from the architecture helpers below
(rooms, corridors, colonnades, tiers, mazes, spirals, towers, bridges), its own landmark set pieces, one labelled greybox
stand-in per asset of the chapter (Tools/story_assets.json, from docs/Assets/TripoAssetList.md), the story placeholders and
episode beats (greybox_story.story_markers) and a floor-detail pass. Greybox only: basic engine shapes and palette colours.

A level function takes (m, rng, spec) and returns (sections, start): sections = [(index 1-4, centre, radius, is_boss)].
"""

import json
import math
import os
import random

import unreal

import greybox_story as gs
from greybox_common import MAPS, GreyboxMap

HERE = os.path.dirname(os.path.abspath(__file__))
SIZES = {"XS": (35, 35, 35), "S": (110, 110, 110), "M": (260, 260, 200), "L": (650, 450, 380), "XL": (1300, 800, 650),
         "H": (60, 60, 180), "HH": (200, 200, 520), "V": (620, 250, 230), "P": (220, 12, 160)}


# ================================================================== architecture helpers
def slab(m, x0, y0, x1, y1, z, f, mat, t=60.0, label="Floor"):
    return m.box(min(x0, x1), min(y0, y1), z - t, max(x0, x1), max(y0, y1), z, label, f, mat)


def wall_run(m, x0, y0, x1, y1, z, h, f, mat, gaps=(), t=40.0, label="Wall"):
    """A straight wall from (x0,y0) to (x1,y1); `gaps` = [(t0, t1)] fractions of its length left open (doors)."""
    length = math.hypot(x1 - x0, y1 - y0)
    if length < 1:
        return
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    edges = [0.0]
    for a, b in sorted(gaps):
        edges += [a, b]
    edges.append(1.0)
    for i in range(0, len(edges), 2):
        a, b = edges[i], edges[i + 1]
        if b - a <= 0.001:
            continue
        mid = (a + b) / 2
        m.block((x0 + (x1 - x0) * mid, y0 + (y1 - y0) * mid, z + h / 2), (length * (b - a), t, h), label, f, mat, yaw=yaw)


def room(m, x0, y0, x1, y1, z, h, f, wall, floor=None, gaps=None, roof=None, t=40.0):
    gaps = gaps or {}
    if floor:
        slab(m, x0, y0, x1, y1, z, f, floor)
    wall_run(m, x0, y0, x1, y0, z, h, f, wall, gaps.get("S", ()), t)
    wall_run(m, x1, y0, x1, y1, z, h, f, wall, gaps.get("E", ()), t)
    wall_run(m, x1, y1, x0, y1, z, h, f, wall, gaps.get("N", ()), t)
    wall_run(m, x0, y1, x0, y0, z, h, f, wall, gaps.get("W", ()), t)
    if roof:
        slab(m, x0, y0, x1, y1, z + h + 40, f, roof, t=40, label="Roof")


def colonnade(m, x0, y0, x1, y1, n, z, h, f, mat, r=90.0, cap=None):
    for i in range(n):
        t = i / max(1, n - 1)
        x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
        m.block((x, y, z + h / 2), (r * 2, r * 2, h), "Column", f, mat, shape="cylinder")
        if cap:
            m.block((x, y, z + h + 20), (r * 2.6, r * 2.6, 40), "ColumnCap", f, cap)


def tiers(m, c, r0, steps, rise, depth, f, mat, arc=(0.0, 360.0), seg=40, skip=None, z0=0.0):
    """Concentric rising tiers (stands, terraces, galleries). Each ring is solid up to its height."""
    a0, a1 = arc
    for i in range(steps):
        r = r0 + i * depth
        for k in range(seg):
            az = a0 + (a1 - a0) * (k + 0.5) / seg
            if skip and skip(az, i):
                continue
            w = math.radians((a1 - a0) / seg) * r * 1.05
            p = gs._az(c, r, az, z0)
            m.block((p[0], p[1], z0 + rise * (i + 1) / 2), (depth, w, rise * (i + 1)), "Tier", f, mat, yaw=az)


def stairs_to(m, a, b, f, mat="Concrete", width=500.0, rise=70.0):
    """Straight stairs from point a up (or down) to point b."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    steps = max(2, int(abs(dz) // rise))
    run = math.hypot(dx, dy) / steps
    yaw = math.degrees(math.atan2(dy, dx))
    for k in range(steps):
        t = (k + 0.5) / steps
        top = a[2] + dz * (k + 1) / steps
        bottom = min(a[2], b[2]) - 40
        m.block((a[0] + dx * t, a[1] + dy * t, (top + bottom) / 2), (run * 1.02, width, top - bottom), "Step", f, mat, yaw=yaw)


def ramp(m, a, b, f, mat="Concrete", width=600.0):
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    m.block(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2 - 25), (length, width, 50), "Ramp", f, mat, yaw=yaw, pitch=pitch)


def walkway(m, a, b, f, mat="Metal", width=500.0, rail=True):
    ramp(m, a, b, f, mat, width)
    if rail:
        yaw = math.radians(math.atan2(b[1] - a[1], b[0] - a[0]) * 180 / math.pi)
        ux, uy = math.cos(yaw), math.sin(yaw)
        for s in (-1, 1):
            o = (-uy * width / 2 * s, ux * width / 2 * s)
            m.railing((a[0] + o[0], a[1] + o[1], a[2]), (b[0] + o[0], b[1] + o[1], b[2]), f, height=100, spacing=600, rail=True, material="Metal")


def maze(m, x0, y0, x1, y1, cells_x, cells_y, z, h, f, mat, rng, t=40.0, gap_pct=0.0):
    """A perfect maze (always connected) of walls; the start cell (0,0) and the far corner are open to the rect's outside."""
    cw, ch = (x1 - x0) / cells_x, (y1 - y0) / cells_y
    seen = {(0, 0)}
    open_edges = set()
    stack = [(0, 0)]
    while stack:
        cx, cy = stack[-1]
        nbrs = [(cx + dx, cy + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
                if 0 <= cx + dx < cells_x and 0 <= cy + dy < cells_y and (cx + dx, cy + dy) not in seen]
        if not nbrs:
            stack.pop()
            continue
        nx, ny = rng.choice(nbrs)
        open_edges.add(frozenset(((cx, cy), (nx, ny))))
        seen.add((nx, ny))
        stack.append((nx, ny))
    for cx in range(cells_x):
        for cy in range(cells_y):
            if cx + 1 < cells_x and frozenset(((cx, cy), (cx + 1, cy))) not in open_edges and rng.random() > gap_pct:
                wx = x0 + (cx + 1) * cw
                m.block((wx, y0 + (cy + 0.5) * ch, z + h / 2), (t, ch, h), "MazeWall", f, mat)
            if cy + 1 < cells_y and frozenset(((cx, cy), (cx, cy + 1))) not in open_edges and rng.random() > gap_pct:
                wy = y0 + (cy + 1) * ch
                m.block((x0 + (cx + 0.5) * cw, wy, z + h / 2), (cw, t, h), "MazeWall", f, mat)
    wall_run(m, x0, y0, x1, y0, z, h, f, mat, [(0.0, 1.0 / cells_x)])
    wall_run(m, x0, y1, x1, y1, z, h, f, mat, [(1 - 1.0 / cells_x, 1.0)])
    wall_run(m, x0, y0, x0, y1, z, h, f, mat)
    wall_run(m, x1, y0, x1, y1, z, h, f, mat)


def tower_stack(m, c, r, floors, floor_h, f, mat, accent):
    for i in range(floors):
        m.block((c[0], c[1], c[2] + i * floor_h + floor_h / 2), (r * 2, r * 2, floor_h - 20), "TowerFloor", f, mat, shape="cylinder")
        m.block((c[0], c[1], c[2] + (i + 1) * floor_h), (r * 2.15, r * 2.15, 30), "TowerBand", f, accent, shape="cylinder", collision=False)


def spiral_ramp(m, c, r, z0, z1, turns, f, mat="Metal", width=500.0, n=48):
    for k in range(n):
        t0, t1 = k / n, (k + 1) / n
        a0, a1 = math.radians(turns * 360 * t0), math.radians(turns * 360 * t1)
        p0 = (c[0] + r * math.cos(a0), c[1] + r * math.sin(a0), z0 + (z1 - z0) * t0)
        p1 = (c[0] + r * math.cos(a1), c[1] + r * math.sin(a1), z0 + (z1 - z0) * t1)
        ramp(m, p0, p1, f, mat, width)


def lattice(m, c, w, h, f, mat, climb=True):
    """A lattice mast: four legs and cross braces (orange legs are climbable)."""
    for sx in (-1, 1):
        for sy in (-1, 1):
            m.block((c[0] + sx * w / 2, c[1] + sy * w / 2, c[2] + h / 2), (40, 40, h), "MastLeg", f, "Climb" if climb else mat,
                    tags=["Climbable"] if climb else None)
    n = int(h // 400)
    for i in range(n):
        z = c[2] + 200 + i * 400
        m.block((c[0], c[1] - w / 2, z), (w, 14, 14), "MastBrace", f, mat)
        m.block((c[0], c[1] + w / 2, z), (w, 14, 14), "MastBrace", f, mat)
        m.block((c[0] - w / 2, c[1], z), (14, w, 14), "MastBrace", f, mat)
        m.block((c[0] + w / 2, c[1], z), (14, w, 14), "MastBrace", f, mat)


def arena(m, idx, c, r, f, boss=False, floor=None, mat="Concrete"):
    sec = m.section(idx, c, f, radius=r, boss=boss)
    if floor:
        m.block((c[0], c[1], c[2] - 30), (r * 2, r * 2, 60), "ArenaFloor", f, floor, shape="cylinder")
    return (idx, c, r, boss)


def jump_and_climb(m, c, yaw_deg, f, rise_steps=4):
    """Traversal pair beside a section: teal jump-up steps to a ledge and an orange climb wall."""
    p = gs._az(c, 3400, yaw_deg, c[2])
    m.box(p[0] - 400, p[1] - 400, c[2] + 300, p[0] + 400, p[1] + 400, c[2] + 380, "Ledge", f, "Concrete")
    m.jump_steps((p[0] - 400 - 1250, p[1], c[2]), 0.0, rise_steps, f, rise=80.0, run=300.0, width=420.0)
    m.climb_wall((p[0] + 400, p[1], c[2]), 0.0, 700.0, 380.0, f)


def props(m, rng, c, r, z, theme, count, f, spec, keep=900.0):
    pool = gs.PROPS[theme]
    for _ in range(int(count * 3)):   # themed set dressing: three props per call count
        az = rng.uniform(0, 360)
        rr = rng.uniform(keep, r * 0.9)
        p = gs._az(c, rr, az, z)
        rng.choice(pool)(m, p[0], p[1], z, rng, f, spec)


def lights(m, c, z, r=3000.0, n=4, intensity=30000.0):
    for k in range(n):
        p = gs._az(c, r, k * 360.0 / n + 20, z + 900)
        m.point_light(p, "Lighting", intensity=intensity, radius=6500.0)


def context(m, rng, spec, sections):
    """Gives the map a setting: a ground plane and a ring of skyline blocks (city themes) or hills and trees (open themes).
    Interior-only themes (gallery, vault) stay in the void on purpose."""
    theme = spec["theme"]
    if theme in ("gallery", "vault") or spec["water"]:
        return 0
    xs = [s_[1][0] for s_ in sections]
    ys = [s_[1][1] for s_ in sections]
    zs = [s_[1][2] for s_ in sections]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    half = max(max(xs) - min(xs), max(ys) - min(ys)) / 2 + 14000.0
    zmin = min(zs) - 400.0
    f = "Context"
    placed = 0
    if not spec["water"]:
        m.box(cx - half, cy - half, zmin - 300, cx + half, cy + half, zmin, "ContextGround", f,
              spec["floor"] if theme in ("village", "field", "lab") else "Dark")
        placed += 1

    def clear(x, y, pad):
        return all(math.hypot(x - c[0], y - c[1]) > r + pad for _, c, r, _ in sections)

    open_theme = theme in ("village", "field", "lab")
    if not open_theme and theme not in ("city", "tower", "factory", "casino", "carnival", "range", "war"):
        return placed   # interiors (library, court, cathedral, office, tunnel): ground only, no skyline
    for _ in range(90):
        a = rng.uniform(0, 360)
        rr = rng.uniform(half * 0.5, half * 0.97)
        x, y = cx + rr * math.cos(math.radians(a)), cy + rr * math.sin(math.radians(a))
        if not clear(x, y, 3500.0):
            continue
        if open_theme:
            w = rng.uniform(4000, 9000)
            h = rng.uniform(2500, 7000)
            m.block((x, y, zmin + h / 2), (w, w, h), "Hill", f, rng.choice(["Moss", "Rock", "Grass"]), shape="cone", collision=False)
        else:
            w, d, h = rng.uniform(900, 2600), rng.uniform(900, 2600), rng.uniform(2500, 13000)
            m.block((x, y, zmin + h / 2), (w, d, h), "Skyline", f, rng.choice(["Dark", "Concrete", "Roof", "Rock"]), yaw=rng.uniform(0, 90))
            if rng.random() < 0.45:
                m.block((x, y, zmin + h * rng.uniform(0.3, 0.9)), (w * 1.04, d * 1.04, 120), "SkylineBand", f, spec["accent"], collision=False)
        placed += 1
    return placed


# ================================================================== asset stand-ins (one per asset of the chapter)
def place_assets(m, n, sections, log):
    path = os.path.join(HERE, "story_assets.json")
    if not os.path.exists(path):
        return 0
    data = json.load(open(path)).get(str(n), [])
    placed = 0
    counters = {}
    for a in data:
        zone = max(1, min(4, a["zone"]))
        sec = sections[zone - 1]
        c, r = sec[1], sec[2]
        k = counters.get(zone, 0)
        counters[zone] = k + 1
        golden = 137.5 * k
        big = a["size"] in ("L", "XL", "V", "HH")
        rr = r * (0.7 + 0.2 * ((k % 5) / 5.0)) if big else r * (0.3 + 0.55 * (((k * 7) % 11) / 11.0))
        p = gs._az(c, rr, golden, c[2])
        w, d, h = SIZES.get(a["size"], SIZES["M"])
        kind = a["kind"]
        mat = {"boss": "Dark", "enemy": "Rock", "char": "Grey", "npc": "Grey", "secret": "Concrete", "weapon": "Metal", "prop": "Wood",
               "vehicle": "Metal", "env": "Grey", "symbol": "Concrete"}.get(kind, "Grey")
        shape = "cylinder" if kind in ("char", "npc", "secret", "enemy", "boss") else "cube"
        m.block((p[0], p[1], p[2] + h / 2), (w, d, h), "ASSET_" + a["name"][:40], "Assets/Section%d" % zone, mat, yaw=golden, shape=shape,
                tags=["AssetStandIn", kind.upper()])
        if kind not in ("env",) or a["size"] in ("L", "XL"):
            m.label("ASSET: " + a["name"], (p[0], p[1], p[2] + h + 90), "Assets/Section%d" % zone, size=70.0, in_game=True, colour=(200, 200, 200))
        placed += 1
    return placed


def fill_detail(m, rng, sections, spec, floor_mats, target=8200):
    """Floor tiles and debris on every section until the map reaches the detail floor (owner 2026-10-07: 8,000+ pieces)."""
    f = "Detail"
    for idx, c, r, boss in sections:
        if sum(m.counts.values()) >= target:
            break
        step = 240.0
        n = int(min(r, 4200) * 0.94 // step)
        for gx in range(-n, n + 1):
            for gy in range(-n, n + 1):
                x, y = gx * step, gy * step
                if x * x + y * y > (n * step) ** 2:
                    continue
                m.block((c[0] + x, c[1] + y, c[2] + 3), (step - 16, step - 16, 6), "Tile", f, floor_mats[(gx * 5 + gy * 3) % len(floor_mats)],
                        collision=False)
    mats = [spec["wall"], "Rubble", "Metal", "Dark", spec["accent"]]
    guard = 0
    while sum(m.counts.values()) < target and guard < 20000:
        guard += 1
        idx, c, r, boss = sections[guard % len(sections)]
        az, rr = rng.uniform(0, 360), r * math.sqrt(rng.uniform(0.03, 0.95))
        p = gs._az(c, rr, az, c[2])
        s = rng.uniform(15, 75)
        m.block((p[0], p[1], p[2] + s / 2), (s, s * rng.uniform(0.5, 1.5), s), "Debris", f, rng.choice(mats), yaw=rng.uniform(0, 360),
                collision=False)


# ================================================================== chapters 14-27
def ch14(m, rng, spec):
    """Declaration: the broadcast spire. A core tower with a spiral of terraces; the roof mast is the boss section."""
    f = "Ch14"
    core = (0.0, 0.0, 0.0)
    tower_stack(m, core, 2400, 14, 900, f + "/Tower", "Glass", "NeonRed")
    S = []
    c1, c2, c3, c4 = (-10500.0, 0.0, 0.0), (0.0, -10500.0, 3600.0), (10500.0, 0.0, 7200.0), (0.0, 10500.0, 10800.0)
    for i, c in enumerate((c1, c2, c3, c4)):
        S.append(arena(m, i + 1, c, 4300, f + "/S%d" % (i + 1), boss=i == 3, floor="Concrete"))
    # S1: street plaza with the lobby building, tour rope, kiosks and the OB van row
    room(m, -14200, -2600, -9800, 2600, 0, 1800, f + "/Lobby", "Glass", floor="Concrete", gaps={"E": [(0.35, 0.65)], "W": [(0.4, 0.6)]}, roof="Concrete")
    colonnade(m, -9600, -2400, -9600, 2400, 7, 0, 1800, f + "/Lobby", "Metal", cap="NeonRed")
    # S2: studio floor with three quiz sets on raised stages
    for k, (dx, dy) in enumerate(((-2400, -1800), (0, 1800), (2400, -1800))):
        m.box(c2[0] + dx - 900, c2[1] + dy - 700, 3600, c2[0] + dx + 900, c2[1] + dy + 700, 3750, "SetStage", f + "/S2", "Magenta")
        for j in range(3):
            m.block((c2[0] + dx - 500 + j * 500, c2[1] + dy, 3750 + 60), (200, 160, 120), "Podium", f + "/S2", spec["accent"])
        m.block((c2[0] + dx, c2[1] + dy + 600, 3750 + 500), (1800, 40, 1000), "SetBackdrop", f + "/S2", "NeonPurple", collision=False)
    # S3: control floor: rows of consoles and a wall of screens
    for row in range(5):
        for col in range(6):
            m.block((c3[0] - 2200 + col * 880, c3[1] - 1600 + row * 800, 7200 + 60), (500, 260, 120), "Console", f + "/S3", "Dark")
            m.block((c3[0] - 2200 + col * 880, c3[1] - 1600 + row * 800 + 90, 7200 + 160), (460, 20, 120), "ConsoleScreen", f + "/S3", "Screen", collision=False)
    m.block((c3[0], c3[1] + 3200, 7200 + 900), (5000, 60, 1700), "ScreenWall", f + "/S3", "Holo", collision=False)
    # S4: transmitter room and the antenna mast
    room(m, c4[0] - 1200, c4[1] - 1000, c4[0] + 1200, c4[1] + 1000, 10800, 900, f + "/S4", "Concrete", floor="Metal", gaps={"S": [(0.4, 0.6)]}, roof="Metal")
    lattice(m, (c4[0], c4[1] + 2400, 10800), 600, 5200, f + "/S4", "Metal")
    # connections: stairs and walkways between terraces, lifts as movers
    for a, b in ((c1, c2), (c2, c3), (c3, c4)):
        ux, uy = b[0] - a[0], b[1] - a[1]
        d = math.hypot(ux, uy)
        ux, uy = ux / d, uy / d
        pa = (a[0] + ux * 4300, a[1] + uy * 4300, a[2])
        pb = (b[0] - ux * 4300, b[1] - uy * 4300, b[2])
        stairs_to(m, pa, pb, f + "/Routes", "Concrete", 700.0, 80.0)
        mid = ((pa[0] + pb[0]) / 2 + uy * 700, (pa[1] + pb[1]) / 2 - ux * 700, (pa[2] + pb[2]) / 2)
        m.mover((mid[0], mid[1], mid[2] - 1500), (600, 600, 60), "LiftCar", f + "/Routes", "Climb", offset=(0, 0, 3000), period=7.0, tags=["Lift"])
    for i, c in enumerate((c1, c2, c3, c4)):
        jump_and_climb(m, c, 120 + 40 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 4300, c[2], "tower", 26, f + "/S%d" % (i + 1), spec)
        lights(m, c, c[2])
    return S, (c1[0] - 3200, c1[1], 0.0)


def ch15(m, rng, spec):
    """Dead Air: a subway line. Platform, tunnel, signal junction, studio; rails, trains and cable trays."""
    f = "Ch15"
    S = []
    xs = [-27000.0, -9000.0, 9000.0, 27000.0]
    z = 0.0
    # continuous tunnel shell along X
    wall_run(m, -33000, -3000, 33000, -3000, z, 1800, f + "/Tunnel", "Concrete")
    wall_run(m, -33000, 3000, 33000, 3000, z, 1800, f + "/Tunnel", "Concrete")
    slab(m, -33000, -3000, 33000, 3000, z, f + "/Tunnel", "Dark")
    slab(m, -33000, -3000, 33000, 3000, z + 1840, f + "/Tunnel", "Concrete", t=40, label="Roof")
    for k in range(34):
        x = -32000 + k * 1960
        for rail_y in (-700, 700):
            m.block((x + 980, rail_y, 14), (1960, 14, 14), "Rail", f + "/Rails", "Metal", collision=False)
        m.block((x + 980, 0, 6), (140, 1900, 10), "Sleeper", f + "/Rails", "Wood", collision=False)
    # S1: platform with a subway car, benches and a ticket gate
    S.append(arena(m, 1, (xs[0], 0.0, 0.0), 3200, f + "/S1"))
    m.box(xs[0] - 3000, 1400, 0, xs[0] + 3000, 2900, 120, "Platform", f + "/S1", "Concrete")
    m.block((xs[0], -400, 130 + 100), (5600, 1300, 1000), "SubwayCar", f + "/S1", "Metal")
    # S2: a long open tunnel section with cable trays and fan housings
    S.append(arena(m, 2, (xs[1], 0.0, 0.0), 3200, f + "/S2"))
    for k in range(8):
        m.block((xs[1] - 2800 + k * 800, 2850, 1500), (700, 120, 60), "CableTray", f + "/S2", "Metal")
        m.block((xs[1] - 2800 + k * 800, -2850, 1500), (700, 120, 60), "CableTray", f + "/S2", "Metal")
    # S3: signal junction: a cross-shaped hub with a side tunnel and a patch room
    S.append(arena(m, 3, (xs[2], 0.0, 0.0), 3200, f + "/S3"))
    room(m, xs[2] - 800, 3000, xs[2] + 800, 7600, 0, 1700, f + "/S3", "Concrete", floor="Dark", gaps={"S": [(0.3, 0.7)]}, roof="Concrete")
    room(m, xs[2] - 800, -7600, xs[2] + 800, -3000, 0, 1700, f + "/S3", "Concrete", floor="Dark", gaps={"N": [(0.3, 0.7)]}, roof="Concrete")
    # S4: Studio Static: the mixing desk, tape racks, a dish on the roof
    S.append(arena(m, 4, (xs[3], 0.0, 0.0), 3200, f + "/S4", boss=True))
    room(m, xs[3] - 2600, -2600, xs[3] + 2600, 2600, 0, 1600, f + "/S4", "Concrete", floor="Wood", gaps={"W": [(0.35, 0.65)]}, roof=None)
    m.block((xs[3] + 1500, 0, 900), (1200, 3600, 120), "MixingDesk", f + "/S4", "Dark")
    for k in range(6):
        m.block((xs[3] - 2300, -2000 + k * 800, 700), (300, 600, 1400), "TapeRack", f + "/S4", "Metal")
    m.block((xs[3] + 600, 0, 4200), (3200, 3200, 200), "RelayDish", f + "/S4", "Metal", shape="cylinder", pitch=25)
    # side platforms and cable-spans to climb/jump
    for c in xs:
        jump_and_climb(m, (c, 0.0, 0.0), 90, f + "/Traversal", 3)
    lights(m, (xs[0], 0, 0), 0, 2800, 4)
    for c in xs:
        props(m, rng, (c, 0.0, 0.0), 3000, 0, "tunnel", 18, f + "/Dress", spec, keep=700)
    return S, (xs[0] - 2600, 0.0, 0.0)


def ch16(m, rng, spec):
    """Recruitment Drive: the Underworld cavern. A market square with four wings: table, briefing hall, bunks, arcade ruin."""
    f = "Ch16"
    S = []
    R = 15000.0
    for k in range(40):   # cavern wall of rock columns
        a = k * 9.0
        p = gs._az((0, 0, 0), R, a)
        m.block((p[0], p[1], 2500), (1500, 1500, 5000 + rng.uniform(0, 2000)), "CavernRock", f + "/Cavern", "Rock", shape="cylinder")
    slab(m, -R, -R, R, R, 0, f + "/Cavern", "Concrete")
    # central market square with a grid of stalls
    for gx in range(-3, 4):
        for gy in range(-3, 4):
            if (gx, gy) == (0, 0) or abs(gx) + abs(gy) > 5:
                continue
            x, y = gx * 1300, gy * 1300
            m.block((x, y, 130), (800, 800, 260), "StallBase", f + "/Market", "Metal")
            m.block((x, y, 400), (1000, 1000, 40), "StallRoof", f + "/Market", spec["accent"] if (gx + gy) % 2 else "Coral", collision=False)
    centres = [(-9500.0, 0.0, 0.0), (0.0, -9500.0, 0.0), (9500.0, 0.0, 0.0), (0.0, 9500.0, 0.0)]
    for i, c in enumerate(centres):
        S.append(arena(m, i + 1, c, 3600, f + "/S%d" % (i + 1), boss=i == 3))
    # S1 the recruiting table; S2 briefing hall; S3 bunks; S4 collapsed arcade
    m.box(-10300, -300, 0, -8700, 300, 110, "RecruitTable", f + "/S1", "Wood")
    room(m, -2800, -12000, 2800, -7600, 0, 1500, f + "/S2", "Concrete", floor="Wood", gaps={"N": [(0.4, 0.6)]}, roof="Concrete")
    m.box(-1500, -10600, 0, 1500, -9400, 120, "MapTable", f + "/S2", "Wood")
    for r in range(4):
        for c in range(6):
            m.block((7000 + c * 650, -1700 + r * 1100, 420), (500, 900, 840), "Bunk", f + "/S3", "Metal")
    for k in range(10):   # arcade: collapsed shutters and a fallen roof
        m.block((rng.uniform(-3000, 3000), 9500 + rng.uniform(-2500, 2500), 250 + k * 40), (1200, 800, 100), "FallenShutter", f + "/S4", "Rubble",
                roll=rng.uniform(-20, 20))
    for i, c in enumerate(centres):
        jump_and_climb(m, c, 45 + 90 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 3600, 0, "city", 20, f + "/S%d" % (i + 1), spec, keep=700)
    return S, (-12800.0, 0.0, 0.0)


def ch17(m, rng, spec):
    """Court in Session: a cross-shaped courthouse: atrium, cell wing, second courtroom, grand courtroom."""
    f = "Ch17"
    S = []
    # central atrium + four wings
    room(m, -4500, -4500, 4500, 4500, 0, 2600, f + "/Atrium", "Concrete", floor="Concrete", gaps={"N": [(0.4, 0.6)], "S": [(0.4, 0.6)], "E": [(0.4, 0.6)],
         "W": [(0.4, 0.6)]})
    colonnade(m, -3600, -3600, -3600, 3600, 6, 0, 2600, f + "/Atrium", "Marble" if False else "Stone", cap="NeonYellow")
    colonnade(m, 3600, -3600, 3600, 3600, 6, 0, 2600, f + "/Atrium", "Stone", cap="NeonYellow")
    S.append(arena(m, 1, (0.0, 0.0, 0.0), 3800, f + "/S1", floor="Concrete"))
    # west wing: holding cells in a row
    room(m, -15000, -3200, -4500, 3200, 0, 2200, f + "/Cells", "Concrete", floor="Dark", gaps={"E": [(0.35, 0.65)]}, roof="Concrete")
    for k in range(8):
        wall_run(m, -14500 + k * 1250, -3200, -14500 + k * 1250, -800, 0, 2000, f + "/Cells", "Metal", t=30)
    S.append(arena(m, 2, (-9800.0, 0.0, 0.0), 2900, f + "/S2"))
    # east wing: courtroom 2 with a witness stand and pews
    room(m, 4500, -3200, 13000, 3200, 0, 2400, f + "/Court2", "Concrete", floor="Wood", gaps={"W": [(0.35, 0.65)]}, roof="Concrete")
    m.box(11800, -900, 0, 12800, 900, 260, "Bench2", f + "/Court2", "Wood")
    for r in range(4):
        m.box(6500 + r * 900, -2200, 0, 6700 + r * 900, 2200, 90, "Pew", f + "/Court2", "Wood")
    S.append(arena(m, 3, (8800.0, 0.0, 0.0), 2900, f + "/S3"))
    # north wing: the grand courtroom with tiered gallery, the bench, the dock
    room(m, -5500, 4500, 5500, 16500, 0, 3800, f + "/Court1", "Concrete", floor="Wood", gaps={"S": [(0.4, 0.6)]}, roof="Concrete")
    tiers(m, (0.0, 14500.0, 0.0), 2600, 4, 90.0, 520.0, f + "/Court1", "Wood", arc=(200.0, 340.0), seg=24)
    m.box(-1800, 15600, 0, 1800, 16300, 420, "JudgeBench", f + "/Court1", "Wood")
    m.box(-400, 13300, 0, 400, 14100, 140, "Dock", f + "/Court1", "Wood")
    S.append(arena(m, 4, (0.0, 10500.0, 0.0), 3700, f + "/S4", boss=True))
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 30 + 80 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, S[i][2], 0, "court", 18, f + "/S%d" % (i + 1), spec, keep=800)
        lights(m, c, 0, 2000, 3, 24000)
    return S, (0.0, -3600.0, 0.0)


def ch18(m, rng, spec):
    """Backend Deals: a stepped cavern market: gate plaza, market tier, ledger vault below, broker's gantries above."""
    f = "Ch18"
    S = []
    c1, c2, c3, c4 = (-12000.0, 0.0, 0.0), (-2000.0, 0.0, -1800.0), (8500.0, 0.0, -3600.0), (19000.0, 0.0, -1800.0)
    for k in range(3):   # step-down terraces
        m.box(-17000 + k * 9000, -7000, -4200, -8500 + k * 9000, 7000, -k * 1800 - 60, "Terrace", f + "/Terraces", "Concrete")
    m.box(13500, -7000, -4200, 24500, 7000, -1860, "Terrace", f + "/Terraces", "Concrete")
    # stall grids on each terrace
    for t, cx in enumerate((-12500.0, -3500.0, 5500.0)):
        zz = -t * 1800.0
        for gx in range(5):
            for gy in range(-2, 3):
                m.block((cx + gx * 1500 - 3000, gy * 2200, zz + 150), (900, 900, 300), "StallBase", f + "/Stalls", "Metal")
                m.block((cx + gx * 1500 - 3000, gy * 2200, zz + 520), (1200, 1200, 40), "StallCanopy", f + "/Stalls", rng.choice(["Coral", "Lime", "Magenta", "Sunflower"]), collision=False)
    for i, c in enumerate((c1, c2, c3, c4)):
        S.append(arena(m, i + 1, c, 3900, f + "/S%d" % (i + 1), boss=i == 3))
    # vault: a room with a wardrobe-sized door, drawers
    room(m, 5800, -4200, 11200, 4200, -3600, 2400, f + "/Vault", "Metal", floor="Dark", gaps={"W": [(0.4, 0.6)]}, roof="Metal")
    for k in range(10):
        m.block((10600, -3600 + k * 720, -3600 + 600), (300, 600, 1200), "DrawerBank", f + "/Vault", "Metal")
    # gantries over the broker's floor
    for k in range(4):
        walkway(m, (16000, -4500 + k * 3000, -1000), (22000, -3000 + k * 2000, -1000), f + "/S4", "Metal", 500)
    for i, c in enumerate((c1, c2, c3, c4)):
        stairs_to(m, (c[0] + 3900, 0, c[2]), (c[0] + 5400, 0, c[2] - 400), f + "/Links", "Concrete", 700.0, 70.0) if i < 3 else None
        props(m, rng, c, 3900, c[2], "city", 22, f + "/S%d" % (i + 1), spec, keep=900)
        jump_and_climb(m, c, 70 + 90 * i, f + "/S%d" % (i + 1))
    return S, (c1[0] - 3000, 0.0, 0.0)


def ch19(m, rng, spec):
    """House Rules: the Deck casino: octagonal floor with table rings, a ring corridor, vault, rooftop terrace."""
    f = "Ch19"
    S = []
    c1 = (0.0, 0.0, 0.0)
    # octagonal gaming floor with concentric roulette rings
    for ring, rad in enumerate((1800, 3000, 4200)):
        for k in range(8 + ring * 4):
            a = k * 360.0 / (8 + ring * 4)
            p = gs._az(c1, rad, a)
            m.block((p[0], p[1], 190), (520, 520, 380), "TableBase", f + "/Floor", "Wood", shape="cylinder")
            m.block((p[0], p[1], 400), (820, 820, 40), "TableFelt", f + "/Floor", "Grass" if ring % 2 == 0 else spec["accent"], shape="cylinder")
    S.append(arena(m, 1, c1, 5200, f + "/S1"))
    # ring corridor and four rooms: high-roller, vault, rooftop stair, backer's office
    c2, c3, c4 = (-15000.0, 0.0, 0.0), (0.0, 15000.0, 0.0), (15000.0, 0.0, 3200.0)
    room(m, -19000, -3500, -11000, 3500, 0, 2400, f + "/HighRoller", "Concrete", floor="Wood", gaps={"E": [(0.4, 0.6)]}, roof="Concrete")
    m.block((-15000, 0, 100), (2400, 2400, 200), "PrivateTable", f + "/HighRoller", "Wood", shape="cylinder")
    S.append(arena(m, 2, c2, 3300, f + "/S2"))
    room(m, -4200, 11000, 4200, 19000, 0, 2600, f + "/Vault", "Metal", floor="Dark", gaps={"S": [(0.4, 0.6)]}, roof="Metal")
    for k in range(12):
        m.block((-3800 + k * 700, 18700, 700), (500, 200, 1400), "DrawerBank", f + "/Vault", "Metal")
    S.append(arena(m, 3, c3, 3600, f + "/S3"))
    m.box(11000, -4500, 3100, 19000, 4500, 3200, "Rooftop", f + "/Rooftop", "Concrete")
    stairs_to(m, (5200, 0, 0), (11000, 0, 3200), f + "/Rooftop", "Concrete", 900.0, 80.0)
    wall_run(m, 11000, -4500, 19000, -4500, 3200, 120, f + "/Rooftop", "Metal")
    wall_run(m, 11000, 4500, 19000, 4500, 3200, 120, f + "/Rooftop", "Metal")
    S.append(arena(m, 4, c4, 4200, f + "/S4", boss=True))
    m.block((c4[0], c4[1], 3200 + 360), (2600, 2600, 70), "BossRoulette", f + "/Rooftop", "Wood", shape="cylinder")
    for a, b in ((c1, c2), (c1, c3)):
        ramp(m, (a[0] + (b[0] - a[0]) * 0.34, a[1] + (b[1] - a[1]) * 0.34, 0), (a[0] + (b[0] - a[0]) * 0.66, a[1] + (b[1] - a[1]) * 0.66, 0), f + "/Links", "Wood", 700.0)
    for i, c in enumerate((c1, c2, c3, c4)):
        jump_and_climb(m, c, 60 + 80 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 3500, c[2], "casino", 20, f + "/S%d" % (i + 1), spec, keep=1200)
        lights(m, c, c[2], 2600, 4, 36000)
    return S, (0.0, -4800.0, 0.0)


def ch20(m, rng, spec):
    """The Hour: Clock Cathedral. A long nave with gear halls to the sides and a belfry tower for the boss."""
    f = "Ch20"
    S = []
    # nave: two colonnades, a stone floor, a barrel roof
    slab(m, -3600, -24000, 3600, 12000, 0, f + "/Nave", "Stone")
    colonnade(m, -3000, -23000, -3000, 11000, 18, 0, 3600, f + "/Nave", "Stone", r=140, cap="NeonYellow")
    colonnade(m, 3000, -23000, 3000, 11000, 18, 0, 3600, f + "/Nave", "Stone", r=140, cap="NeonYellow")
    for k in range(10):
        m.block((0, -22000 + k * 3400, 3700), (6600, 140, 160), "Rib", f + "/Nave", "Stone")
    # S1 nave west, S2 gear halls (a side chamber with giant gears), S3 stopped hall (side chamber), S4 belfry
    S.append(arena(m, 1, (0.0, -19000.0, 0.0), 3300, f + "/S1"))
    room(m, 3600, -13000, 12000, -4000, 0, 3200, f + "/Gears", "Stone", floor="Dark", gaps={"W": [(0.4, 0.6)]})
    for k in range(5):
        m.block((5000 + k * 1500, -8500, 1700 + (k % 2) * 400), (1900 - k * 150, 1900 - k * 150, 120), "GreatGear", f + "/Gears", "Metal", shape="cylinder", pitch=90)
    S.append(arena(m, 2, (8000.0, -8500.0, 0.0), 3300, f + "/S2"))
    room(m, -12000, -9000, -3600, 0, 0, 2800, f + "/Stopped", "Stone", floor="Wood", gaps={"E": [(0.4, 0.6)]}, roof="Stone")
    for gx in range(4):
        for gy in range(5):
            m.block((-11000 + gx * 1900, -8000 + gy * 1600, 1900), (500, 40, 500), "StoppedClock", f + "/Stopped", "Marking", pitch=90, collision=False)
    S.append(arena(m, 3, (-8000.0, -4500.0, 0.0), 3200, f + "/S3"))
    # belfry: a tall tower at the east end of the nave: spiral ramp inside a ring wall, pendulum below
    bc = (0.0, 9000.0, 0.0)
    tower_stack(m, bc, 2300, 12, 1100, f + "/Belfry", "Stone", "NeonYellow")
    m.block((bc[0], bc[1], 5600), (600, 600, 9000), "Pendulum", f + "/Belfry", "Metal", shape="cylinder")
    m.block((bc[0], bc[1], 1200), (1800, 1800, 600), "PendulumBob", f + "/Belfry", "Metal", shape="sphere")
    spiral_ramp(m, bc, 3800, 0.0, 6000.0, 2.0, f + "/Belfry", "Stone", 600.0)
    S.append(arena(m, 4, (bc[0], bc[1], 6000.0), 3000, f + "/S4", boss=True))
    m.box(bc[0] - 2800, bc[1] - 2800, 5940, bc[0] + 2800, bc[1] + 2800, 6000, "BelfryFloor", f + "/Belfry", "Stone")
    m.block((bc[0], bc[1], 6000 + 1300), (4400, 4400, 160), "BelfryClockFace", f + "/Belfry", "Marking", shape="cylinder", pitch=90)
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 100 + 70 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 3000, c[2], "cathedral", 16, f + "/S%d" % (i + 1), spec, keep=1000)
    return S, (0.0, -22500.0, 0.0)


def ch21(m, rng, spec):
    """Fixed Dose: stepped terraces up a hillside: farm, greenhouse dome, lab block, reactor annex with a containment pit."""
    f = "Ch21"
    S = []
    cs = [(-14000.0, 0.0, 0.0), (-4500.0, 0.0, 1800.0), (5000.0, 0.0, 3600.0), (14500.0, 0.0, 5400.0)]
    for i, c in enumerate(cs):
        m.box(c[0] - 4900, -6500, c[2] - 1800, c[0] + 4900, 6500, c[2], "Terrace", f + "/Terraces", "Grass" if i == 0 else "Concrete")
        m.box(c[0] - 4900, -6500, c[2], c[0] - 4800, 6500, c[2] + 300, "TerraceWall", f + "/Terraces", "Stone")
        S.append(arena(m, i + 1, c, 4200, f + "/S%d" % (i + 1), boss=i == 3))
    # S1 farm rows; S2 glass dome; S3 lab rooms; S4 reactor with a pit
    for r in range(6):
        for k in range(10):
            m.block((cs[0][0] - 3600 + k * 700, -2800 + r * 1000, 130), (300, 700, 260), "CropBed", f + "/Farm", "Moss")
    m.dome((cs[1][0], cs[1][1], cs[1][2]), 3200.0, f + "/Dome", n_az=24, n_el=7, thickness=60.0, material="Glass", gap=lambda az, b: 160 < az < 200 and b < 3)
    for r in range(3):
        for k in range(6):
            m.block((cs[1][0] - 1800 + k * 700, -1200 + r * 1200, cs[1][2] + 160), (500, 800, 320), "PlanterRack", f + "/Dome", "Flora")
    room(m, cs[2][0] - 4200, -4200, cs[2][0] + 4200, 4200, cs[2][2], 1900, f + "/Lab", "Metal", floor="Concrete", gaps={"W": [(0.4, 0.6)]}, roof="Metal")
    for r in range(3):
        m.box(cs[2][0] - 2800, -2800 + r * 2000, cs[2][2], cs[2][0] + 2800, -2200 + r * 2000, cs[2][2] + 120, "LabBench", f + "/Lab", "Metal")
    pc = cs[3]
    m.box(pc[0] - 4200, -4200, pc[2] - 60, pc[0] + 4200, 4200, pc[2], "ReactorFloor", f + "/Reactor", "Metal")
    m.box(pc[0] - 1000, -1000, pc[2] - 1700, pc[0] + 1000, 1000, pc[2] - 60, "ContainmentPit", f + "/Reactor", "NeonGreen", collision=False)
    for a in range(0, 360, 60):
        p = gs._az(pc, 1400, a, pc[2] + 700)
        m.block(p, (160, 160, 1400), "ReactorPipe", f + "/Reactor", "Metal", shape="cylinder")
    for i in range(3):
        stairs_to(m, (cs[i][0] + 4300, 0, cs[i][2]), (cs[i + 1][0] - 4300, 0, cs[i + 1][2]), f + "/Links", "Concrete", 800.0, 75.0)
    for i, c in enumerate(cs):
        jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 4000, c[2], "lab", 18, f + "/S%d" % (i + 1), spec, keep=1100)
        lights(m, c, c[2], 2800, 4, 30000)
    return S, (cs[0][0] - 3500, 0.0, 0.0)


def ch22(m, rng, spec):
    """Narrator's Page: a library of concentric rings: entrance hall, maze of reading rooms, drafting hall, central reading room."""
    f = "Ch22"
    S = []
    c0 = (0.0, 0.0, 0.0)
    slab(m, -16000, -16000, 16000, 16000, 0, f + "/Floor", "Wood")
    # outer ring: shelf canyon
    for k in range(60):
        a = k * 6.0
        p = gs._az(c0, 15200, a)
        m.block((p[0], p[1], 2300), (1200, 700, 4600), "OuterShelf", f + "/Stacks", "Wood", yaw=a)
    # middle band: a maze of reading rooms (south-west quadrant as a rectangular maze)
    maze(m, -14000, -14000, -2000, -2000, 8, 8, 0, 1400, f + "/Maze", "Wood", rng)
    S.append(arena(m, 1, (-14500.0, -14500.0, 0.0), 2200, f + "/S1"))
    S.append(arena(m, 2, (-8000.0, -8000.0, 0.0), 3600, f + "/S2"))
    # S3 drafting hall: long tables with red-margin drafts
    room(m, 1500, -9000, 13000, -1500, 0, 2000, f + "/Drafting", "Wood", floor="Wood", gaps={"W": [(0.4, 0.6)]}, roof="Wood")
    for k in range(5):
        m.box(2500, -8200 + k * 1300, 0, 12000, -7700 + k * 1300, 100, "DraftingTable", f + "/Drafting", "Wood")
    S.append(arena(m, 3, (7500.0, -5250.0, 0.0), 3500, f + "/S3"))
    # S4 central reading room: a round room with a desk and three chairs, raised on a spiral stair
    cc = (0.0, 8500.0, 0.0)
    tiers(m, cc, 2400, 5, 70.0, 420.0, f + "/Central", "Wood", arc=(0.0, 360.0), seg=36, skip=lambda az, i: i == 0 and 80 < az < 100)
    m.block((cc[0], cc[1], 380), (900, 500, 70), "NarratorDesk", f + "/Central", "Wood")
    m.block((cc[0], cc[1] - 700, 380), (160, 160, 60), "Chair", f + "/Central", "Wood", shape="cylinder")
    S.append(arena(m, 4, (cc[0], cc[1], 350.0), 3200, f + "/S4", boss=True))
    # drifting pages: movers that sweep paths
    for k in range(12):
        m.mover((-6000 + k * 1100, 2000, 600), (900, 600, 10), "DriftingPage", f + "/Pages", "Marking", offset=(0, 4000, 300), period=5.0 + k * 0.3, collision=False)
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 20 + 100 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, S[i][2], c[2], "library", 18, f + "/S%d" % (i + 1), spec, keep=900)
    ramp(m, (-12500, -12500, 0), (-9000, -9200, 0), f + "/Links", "Wood", 600.0)
    return S, (-14800.0, -14800.0, 0.0)


def ch23(m, rng, spec):
    """Red Sky: a mountain-top observatory: hatch yard, instrument galleries inside the dome ring, map room, storm balcony."""
    f = "Ch23"
    S = []
    c0 = (0.0, 0.0, 0.0)
    m.block((0, 0, -1500), (36000, 36000, 3000), "MountainTop", f + "/Mountain", "Rock", shape="cylinder")
    # hatch yard (S1) with a hatch tower
    S.append(arena(m, 1, (-12000.0, 0.0, 0.0), 3800, f + "/S1", floor="Concrete"))
    tower_stack(m, (-12000.0, 4200.0, 0.0), 1100, 5, 1000, f + "/HatchTower", "Metal", "NeonRed")
    for k in range(6):
        p = gs._az((-12000.0, 0.0, 0.0), 2200, k * 60)
        m.block((p[0], p[1], 40), (300, 300, 80), "Hatch", f + "/S1", "Metal", shape="cylinder")
    # dome with a gallery ring (S2) and an inner map room (S3)
    m.dome(c0, 6500.0, f + "/Dome", n_az=32, n_el=8, thickness=80.0, material="Glass", gap=lambda az, b: 170 < az < 190 and b < 4)
    tiers(m, c0, 4200, 3, 300.0, 700.0, f + "/Gallery", "Metal", arc=(0.0, 360.0), seg=48, skip=lambda az, i: 170 < az < 190)
    S.append(arena(m, 2, (0.0, -1500.0, 0.0), 3600, f + "/S2"))
    room(m, 800, 600, 5200, 4400, 0, 1800, f + "/MapRoom", "Concrete", floor="Wood", gaps={"S": [(0.4, 0.6)]}, roof="Concrete")
    for k in range(6):
        m.block((1300 + k * 650, 4300, 900), (560, 20, 800), "WallMap", f + "/MapRoom", "Marking", collision=False)
    S.append(arena(m, 3, (3000.0, 2500.0, 0.0), 2300, f + "/S3"))
    # storm balcony: a cantilever far out over the void
    m.box(10000, -4000, -80, 20000, 4000, 0, "StormBalcony", f + "/Balcony", "Metal")
    wall_run(m, 10000, -4000, 20000, -4000, 0, 110, f + "/Balcony", "Metal")
    wall_run(m, 10000, 4000, 20000, 4000, 0, 110, f + "/Balcony", "Metal")
    walkway(m, (6500, 0, 0), (10000, 0, 0), f + "/Balcony", "Metal", 900.0)
    for k in range(4):
        m.block((11000 + k * 2400, 0, 800), (80, 80, 1600), "LightningRod", f + "/Balcony", "Metal", shape="cylinder")
    S.append(arena(m, 4, (15000.0, 0.0, 0.0), 4000, f + "/S4", boss=True))
    walkway(m, (-8200, 0, 0), (-5000, -1500, 0), f + "/Links", "Metal", 900.0)
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, S[i][2], 0, "field", 14, f + "/S%d" % (i + 1), spec, keep=800)
    return S, (-15000.0, 0.0, 0.0)


def ch24(m, rng, spec):
    """The Joke: a carnival under Neo Tokyo. A central ring with spoke avenues: midway, shooting gallery, mirror house."""
    f = "Ch24"
    S = []
    c0 = (0.0, 0.0, 0.0)
    slab(m, -17000, -17000, 17000, 17000, 0, f + "/Ground", "Dark")
    # central ring (boss) with a big top cone and trapeze poles
    m.block((0, 0, 1300), (7000, 7000, 2600), "BigTop", f + "/Ring", spec["accent"], shape="cone", collision=False)
    for k in range(8):
        p = gs._az(c0, 3300, k * 45)
        m.block((p[0], p[1], 1100), (160, 160, 2200), "TentPole", f + "/Ring", "Metal", shape="cylinder")
    m.block((0, 0, 40), (5400, 5400, 80), "Sawdust", f + "/Ring", "Sand", shape="cylinder")
    S_centre = (0.0, 0.0, 0.0)
    # spoke S1: midway street (west)
    for k in range(7):
        for s in (-1, 1):
            x = -15000 + k * 1700
            m.block((x, s * 1600, 450), (900, 900, 900), "MidwayStall", f + "/Midway", rng.choice(["Magenta", "Coral", "Sunflower", "Turquoise"]))
            m.block((x, s * 1600, 1000), (1100, 1100, 40), "StallAwning", f + "/Midway", spec["accent"], collision=False)
    S.append(arena(m, 1, (-11000.0, 0.0, 0.0), 3400, f + "/S1"))
    # spoke S2: shooting gallery (south): a long counter, tin ducks on rails
    m.box(-2500, -14000, 0, 2500, -13000, 130, "GalleryCounter", f + "/Gallery", "Wood")
    for k in range(10):
        m.mover((-4000 + k * 800, -9500, 500), (300, 40, 300), "TinDuck", f + "/Gallery", "Sunflower", offset=(2400, 0, 0), period=4.0 + k * 0.2, collision=False)
    S.append(arena(m, 2, (0.0, -9500.0, 0.0), 3400, f + "/S2"))
    # spoke S3: mirror house (north): a maze of glass walls
    maze(m, -4800, 6000, 4800, 15800, 8, 8, 0, 1300, f + "/MirrorHouse", "Glass", rng, gap_pct=0.1)
    S.append(arena(m, 3, (0.0, 10500.0, 0.0), 3400, f + "/S3"))
    # boss: the ring
    S.append(arena(m, 4, c0, 3300, f + "/S4", boss=True))
    # carousel on the east spoke (set piece, not a section)
    m.block((11000, 0, 200), (3600, 3600, 120), "CarouselBase", f + "/Carousel", "Wood", shape="cylinder")
    m.mover((11000, 0, 900), (3000, 3000, 60), "CarouselCanopy", f + "/Carousel", spec["accent"], rotation=(0, 20, 0), shape="cylinder", collision=False)
    for k in range(8):
        p = gs._az((11000, 0, 0), 1100, k * 45, 0)
        m.block((p[0], p[1], 500), (200, 200, 600), "CarouselHorse", f + "/Carousel", "Coral")
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 45 + 90 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 3200, 0, "carnival", 16, f + "/S%d" % (i + 1), spec, keep=1000)
    return S, (-15500.0, 0.0, 0.0)


def ch25(m, rng, spec):
    """Adapt: a range of parallel lanes. Training hall, live-fire lanes with obstacle sets, control tower, range arena."""
    f = "Ch25"
    S = []
    slab(m, -30000, -9000, 30000, 9000, 0, f + "/Floor", "Concrete")
    # five lanes divided by low walls, each with a different obstacle set
    for lane in range(6):
        y = -7500 + lane * 3000
        wall_run(m, -8000, y, 22000, y, 0, 300, f + "/Lanes", "Metal", t=40)
    kinds = ("hurdles", "wall", "net", "beam", "crates")
    for lane, kind in enumerate(kinds):
        y = -6000 + lane * 3000
        for k in range(8):
            x = -6500 + k * 3300
            if kind == "hurdles":
                m.block((x, y, 450), (60, 1800, 900), "Hurdle", f + "/Lanes", "Hazard")
            elif kind == "wall":
                m.block((x, y, 800), (200, 1800, 1600), "ClimbObstacle", f + "/Lanes", "Climb", tags=["Climbable"])
            elif kind == "net":
                m.block((x, y, 1100), (40, 1800, 2200), "RopeNet", f + "/Lanes", "Climb", tags=["Climbable"], collision=False)
            elif kind == "beam":
                m.block((x, y, 650), (1200, 260, 1300), "BalanceBeamStand", f + "/Lanes", "Jump", tags=["JumpUp"])
            else:
                m.block((x, y, 400), (500, 500, 800), "CratePyramid", f + "/Lanes", "Wood")
    # S1 training hall (west end): a room with mats; S2 course lanes; S3 control tower; S4 arena (east)
    room(m, -29000, -6000, -19000, 6000, 0, 2400, f + "/Hall", "Concrete", floor="Rubble", gaps={"E": [(0.35, 0.65)]}, roof="Concrete")
    S.append(arena(m, 1, (-24000.0, 0.0, 0.0), 4200, f + "/S1"))
    S.append(arena(m, 2, (-1500.0, 0.0, 0.0), 4400, f + "/S2"))
    tc = (13000.0, 11000.0, 0.0)
    tower_stack(m, tc, 1500, 6, 1100, f + "/Tower", "Metal", "NeonGreen")
    spiral_ramp(m, tc, 2600, 0.0, 6600.0, 1.5, f + "/Tower", "Metal", 600.0)
    S.append(arena(m, 3, (tc[0], tc[1], 6600.0), 3000, f + "/S3"))
    m.box(tc[0] - 2800, tc[1] - 2800, 6540, tc[0] + 2800, tc[1] + 2800, 6600, "TowerDeck", f + "/Tower", "Metal")
    S.append(arena(m, 4, (24000.0, 0.0, 0.0), 4400, f + "/S4", boss=True))
    for k in range(6):   # spectator stands along the north edge
        m.box(-12000 + k * 5000, 9100, 0, -8000 + k * 5000, 9800, 300 + k * 0, "Stand", f + "/Stands", "Metal")
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 90 + 40 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, 3600, c[2], "range", 14, f + "/S%d" % (i + 1), spec, keep=1200)
    ramp(m, (13000 - 2600, 11000 - 1200, 0), (21000, 3000, 0), f + "/Links", "Concrete", 700.0)
    return S, (-27500.0, 0.0, 0.0)


def ch26(m, rng, spec):
    """Death's Door: a descent. Four terraced tiers of the Mortuary, each lower than the last, ending in the hall of records."""
    f = "Ch26"
    S = []
    cs = [(0.0, 0.0, 0.0), (0.0, 11000.0, -3200.0), (0.0, 22000.0, -6400.0), (0.0, 33000.0, -9600.0)]
    for i, c in enumerate(cs):
        # circular terrace ringed by name walls with niches
        tiers(m, c, 3800, 2, 200.0, 500.0, f + "/Tier%d" % (i + 1), "Stone", arc=(0.0, 360.0), seg=40, skip=lambda az, k: 80 < az < 100 or 260 < az < 280, z0=c[2])
        for k in range(24):
            az = 15 * k
            if 80 < az < 100 or 260 < az < 280:
                continue
            p = gs._az(c, 4900, az, c[2])
            m.block((p[0], p[1], c[2] + 1200), (200, 700, 2400), "NameWall", f + "/Tier%d" % (i + 1), "Stone", yaw=az)
            m.block((p[0], p[1], c[2] + 1300), (30, 500, 500), "NameNiche", f + "/Tier%d" % (i + 1), "Dark", yaw=az, collision=False)
        S.append(arena(m, i + 1, c, 3700, f + "/S%d" % (i + 1), boss=i == 3, floor="Stone"))
    # long stairs between the tiers
    for i in range(3):
        stairs_to(m, (cs[i][0], cs[i][1] + 3700, cs[i][2]), (cs[i + 1][0], cs[i + 1][1] - 3700, cs[i + 1][2]), f + "/Stairs", "Stone", 900.0, 70.0)
    # tier 2: lantern stands in a row; tier 3: open crypts (a grid of niches in the floor); tier 4: the lectern and hall columns
    for k in range(3):
        m.block((-1500 + k * 1500, cs[1][1], cs[1][2] + 200), (160, 160, 400), "LanternStand", f + "/Tier2", "Stone", shape="cylinder")
        m.block((-1500 + k * 1500, cs[1][1], cs[1][2] + 460), (120, 120, 120), "Lantern", f + "/Tier2", "NeonYellow", shape="sphere", collision=False)
    for gx in range(-2, 3):
        for gy in range(-2, 3):
            m.box(gx * 1200 - 400, cs[2][1] + gy * 1200 - 400, cs[2][2] - 5, gx * 1200 + 400, cs[2][1] + gy * 1200 + 400, cs[2][2] + 60, "OpenCrypt", f + "/Tier3", "Dark", collision=False)
    colonnade(m, -3000, cs[3][1] - 3000, -3000, cs[3][1] + 3000, 6, cs[3][2], 2800, f + "/Tier4", "Stone", r=130)
    colonnade(m, 3000, cs[3][1] - 3000, 3000, cs[3][1] + 3000, 6, cs[3][2], 2800, f + "/Tier4", "Stone", r=130)
    m.block((0, cs[3][1] + 2600, cs[3][2] + 600), (900, 700, 1200), "Lectern", f + "/Tier4", "Wood")
    for i, c in enumerate(cs):
        jump_and_climb(m, c, 0 if i % 2 == 0 else 180, f + "/S%d" % (i + 1))
        props(m, rng, c, 3400, c[2], "vault", 16, f + "/S%d" % (i + 1), spec, keep=1100)
        lights(m, c, c[2], 2400, 3, 20000)
    return S, (0.0, -3400.0, 0.0)


def ch27(m, rng, spec):
    """Ordinary Hours: canteen, two-level mall atrium with escalators, a vault below, a loading dock."""
    f = "Ch27"
    S = []
    # S1 canteen: long hall with trays
    room(m, -22000, -3500, -11000, 3500, 0, 2400, f + "/Canteen", "Concrete", floor="Concrete", gaps={"E": [(0.4, 0.6)]}, roof="Concrete")
    for k in range(5):
        m.box(-21000, -3000 + k * 1500, 0, -12000, -2600 + k * 1500, 110, "CanteenTable", f + "/Canteen", "Wood")
    S.append(arena(m, 1, (-16500.0, 0.0, 0.0), 3300, f + "/S1"))
    # S2 mall atrium: ground floor + a balcony ring at +1800, shop fronts, escalators
    mc = (-2500.0, 0.0, 0.0)
    slab(m, -9500, -7000, 4500, 7000, 0, f + "/Mall", "Marking")
    tiers(m, mc, 4300, 1, 1800.0, 900.0, f + "/Mall", "Concrete", arc=(20.0, 340.0), seg=30)
    for k in range(10):
        a = 30 + k * 30
        p = gs._az(mc, 5600, a, 0)
        m.block((p[0], p[1], 900), (300, 900, 1800), "ShopFront", f + "/Mall", rng.choice(["Magenta", "Turquoise", "Coral", "Lime"]), yaw=a)
    stairs_to(m, (mc[0] - 4200, -2000, 0), (mc[0] - 4200, -2000 + 3200, 1800), f + "/Mall", "Concrete", 800.0, 80.0)
    S.append(arena(m, 2, mc, 4000, f + "/S2"))
    # S3 vault: a room under the mall (sub-level)
    room(m, 7000, -4000, 14500, 4000, -2400, 2200, f + "/Vault", "Metal", floor="Dark", gaps={"W": [(0.4, 0.6)]}, roof="Metal")
    stairs_to(m, (4500, 0, 0), (7000, 0, -2400), f + "/Vault", "Metal", 900.0, 80.0)
    for k in range(10):
        m.block((14000, -3500 + k * 780, -2400 + 700), (300, 600, 1400), "EnvelopeShelf", f + "/Vault", "Metal")
    S.append(arena(m, 3, (10750.0, 0.0, -2400.0), 3000, f + "/S3"))
    # S4 loading dock: a yard with lorry blocks and roller doors
    m.box(17000, -8000, -5, 31000, 8000, 0, "DockYard", f + "/Dock", "Concrete")
    for k in range(5):
        m.block((19000 + k * 2400, -6500, 600), (1800, 800, 1200), "Lorry", f + "/Dock", "Metal")
        m.block((19000 + k * 2400, 7800, 1000), (1800, 120, 2000), "RollerDoor", f + "/Dock", "Hazard")
    S.append(arena(m, 4, (24000.0, 0.0, 0.0), 4600, f + "/S4", boss=True))
    walkway(m, (14500, 0, 0), (17000, 0, 0), f + "/Links", "Concrete", 800.0)
    ramp(m, (-11000, 0, 0), (-9500, 0, 0), f + "/Links", "Concrete", 700.0)
    for i, c in enumerate([s[1] for s in S]):
        jump_and_climb(m, c, 90 + 60 * i, f + "/S%d" % (i + 1))
        props(m, rng, c, S[i][2], c[2], "city", 18, f + "/S%d" % (i + 1), spec, keep=1100)
    return S, (-21000.0, 0.0, 0.0)


LEVELS = {14: ch14, 15: ch15, 16: ch16, 17: ch17, 18: ch18, 19: ch19, 20: ch20, 21: ch21, 22: ch22, 23: ch23, 24: ch24, 25: ch25,
          26: ch26, 27: ch27}
