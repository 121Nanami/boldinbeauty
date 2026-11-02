"""Second detail pass for the greybox maps (owner, 2026-10-07: "continue greyboxing all 13 chapters + the arc with more
details"). The first pass (greybox_detail.py) filled the maps around the fights; the combat sections themselves still held
only their marker ring and a few traversal pieces. This pass dresses where the player actually plays:

- Every combat section of chapters 2-10, 12 and 13 gets cover (low walls, pillars, themed props), two or three raised
  platforms reached by teal jump-up steps with an orange climb face on another side, broken-up ground and lights, all in the
  chapter's palette and built from its own kind of props (ruins, docks, factory, office, shrine ...).
- Between consecutive sections: a lit, marked route on the floor and a lookout tower (jump up, climb, look over the next
  section), so moving between sections reads at a glance.
- Neo Tokyo (Chapter 1): fire escapes up the street facades (orange: climbable), balconies, vertical neon signs and street
  clutter on the pavements.
- The Ark: the plaza, the operators' platform, the Underworld and the civilian boulevard get furniture, planters, kiosks,
  crates, lockers and lights.
- Chapter 11 (Return to Nowhere) is left alone: the owner wants "nothing in it".

Pieces are placed on whatever floor is really there (an editor line trace from just above the section's height), so they sit on
the floating discs, the office floors and the terraces, and never in the water or over a hole. Nothing here moves a section, a
gate or an existing route. Every placement is the assistant's greybox reading (PROPOSED); the owner's level ideas are unchanged.
"""

import math

import unreal

import greybox_props as P
from greybox_common import NEON

FOLDER = "Detail2"


# ============================================================================ floor tracing

class Floor:
    """Finds the walkable floor under a point in the editor world (pieces spawned earlier in the build have collision)."""

    def __init__(self, m, up=250.0, min_normal=0.7):
        self.m = m
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.ok = True
        self.misses = 0
        self.up = up
        self.min_normal = min_normal

    def at(self, x, y, z_ref, up=None, down=400.0):
        up = self.up if up is None else up
        """The floor height near z_ref at (x, y), or None (water, a hole, a wall top far above)."""
        if not self.ok:
            return z_ref
        try:
            hit = unreal.SystemLibrary.line_trace_single(self.world, unreal.Vector(x, y, z_ref + up), unreal.Vector(x, y, z_ref - down),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
        except Exception as error:   # no tracing in this editor build: fall back to the section height
            self.ok = False
            self.m.log("detail2: floor traces unavailable (%s); pieces use the section height" % error)
            return z_ref
        if isinstance(hit, tuple):   # (bool, HitResult) on some engine versions; HitResult or None on others
            hit = hit[1] if len(hit) > 1 and hit[0] else None
        if not hit:
            self.misses += 1
            return None
        parts = hit.to_tuple()   # the native break: (blocking_hit, initial_overlap, time, distance, location, impact_point, normal, impact_normal, ...)
        blocking, impact, normal = parts[0], parts[5], parts[7]
        if not blocking or normal.z < self.min_normal:
            self.misses += 1
            return None
        if impact.z > z_ref + up - 20:   # started inside or on top of something tall (a roof): not this section's floor
            self.misses += 1
            return None
        # (Water surfaces have no collision, so a pool or the sea gives no hit within the trace and is skipped above.)
        if False:
            self.misses += 1
            return None
        return impact.z


class Spots:
    """Keeps this pass's own pieces apart: a new piece needs `gap` cm of air from the others (and from kit buildings)."""

    def __init__(self, m):
        self.m = m
        self.taken = []

    def free(self, x, y, r):
        if self.m.blocked(x, y, r * 0.5):
            return False
        return all(math.hypot(x - a, y - b) > r + c for a, b, c in self.taken)

    def take(self, x, y, r):
        self.taken.append((x, y, r))


# ============================================================================ chapter themes

def _cover_wall(m, x, y, z, yaw, t, rng, f):
    length = rng.choice([300, 400, 500])
    m.block((x, y, z + 60), (length, 60, 120), "CoverWall", f, t["cover"], yaw=yaw, tags=["Cover"])
    m.block((x, y, z + 125), (length + 20, 80, 10), "CoverCap", f, t["trim"], yaw=yaw, collision=False)
    if rng.random() < 0.5:
        ex, ey = P._off(x, y, yaw, length / 2 + 40, 0)
        m.block((ex, ey, z + 90), (60, 60, 180), "CoverPost", f, t["cover"], yaw=yaw, tags=["Cover"])


def _pillar(m, x, y, z, yaw, t, rng, f):
    h = rng.choice([260, 340, 420])
    m.block((x, y, z + h / 2), (90, 90, h), "CoverPillar", f, t["cover"], yaw=yaw, tags=["Cover"], shape=rng.choice(["cube", "cylinder"]))
    m.block((x, y, z + h + 10), (120, 120, 20), "PillarCap", f, t["trim"], yaw=yaw, collision=False)


def _platform(m, x, y, z, yaw, t, rng, f):
    """A raised block to fight from: teal jump-up steps on one side, an orange climb face on the opposite side, a railing."""
    h = t.get("plat_h", 320) * rng.choice([1.0, 1.0, 1.5])
    w, d = rng.choice([(600, 600), (800, 500), (700, 700)])
    m.block((x, y, z + h / 2), (w, d, h), "Platform", f, t["plat"], yaw=yaw, tags=["Platform", "JumpUp"])
    m.block((x, y, z + h + 8), (w + 30, d + 30, 16), "PlatformLip", f, t["trim"], yaw=yaw, collision=False)
    steps = int(math.ceil(h / 80.0))
    sx, sy = P._off(x, y, yaw, -w / 2 - 130 - 260 * (steps - 1), 0)
    m.jump_steps((sx - 130 * math.cos(math.radians(yaw)), sy - 130 * math.sin(math.radians(yaw)), z), yaw, steps, f + "/JumpUp",
                 rise=h / steps, run=260, width=min(d, 320))
    cx, cy = P._off(x, y, yaw, w / 2, 0)
    m.climb_wall((cx, cy, z), yaw, min(d, 400), h, f + "/Climb")
    for side in (-1, 1):
        a = P._off(x, y, yaw, -w / 2 + 40, side * (d / 2 - 15))
        b = P._off(x, y, yaw, w / 2 - 40, side * (d / 2 - 15))
        m.railing((a[0], a[1], z + h), (b[0], b[1], z + h), f, height=90, spacing=200, material=t["trim"])


def _ground_tile(m, x, y, z, rng, t, f):
    s = rng.uniform(150, 420)
    m.block((x, y, z + 2), (s, s * rng.uniform(0.5, 1.0), 4), "GroundTile", f, rng.choice(t["ground"]), yaw=rng.uniform(0, 90),
            collision=False)


def _light(m, x, y, z, t, rng, f):
    P.lamp(m, x, y, z, rng.uniform(0, 360), f, h=rng.choice([450, 550]), head=t["light"])


def _props(kind):
    """Themed set dressing (several shapes each) by the chapter's kind of place."""
    def ruins(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.rubble_pile(m, x, y, z, f, rng), lambda: P.car(m, x, y, z, yaw, f, "Rust", wrecked=True, rng=rng),
                    lambda: P.sandbags(m, x, y, z, yaw, f), lambda: P.rebar(m, x, y, z, f, rng), lambda: P.barrier(m, x, y, z, yaw, f)])()

    def docks(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.crate_stack(m, x, y, z, yaw, f, rng), lambda: P.barrel(m, x, y, z, f, "Rust", fallen=rng.random() < 0.3, yaw=yaw),
                    lambda: P.crate(m, x, y, z, yaw, f, 140, "Rust"),
                    lambda: m.block((x, y, z + 45), (50, 50, 90), "Bollard", f, "Dark", shape="cylinder", tags=["Cover"]),
                    lambda: m.block((x, y, z + 8), (300, 220, 16), "NetPile", f, "Rope" if "Rope" in m.mat else "Wood", yaw=yaw,
                                    collision=False)])()

    def city(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.bench(m, x, y, z, yaw, f, "Metal"), lambda: P.bin_(m, x, y, z, f),
                    lambda: P.vending(m, x, y, z, yaw, f, rng, NEON), lambda: P.crate_stack(m, x, y, z, yaw, f, rng, "Metal"),
                    lambda: P.stall(m, x, y, z, yaw, f, rng, rng.choice(NEON))])()

    def factory(m, x, y, z, yaw, rng, f):
        def cube_pallet():
            m.block((x, y, z + 10), (200, 200, 20), "Pallet", f, "Wood", yaw=yaw)
            for i in range(3):
                for j in range(3):
                    cx, cy = P._off(x, y, yaw, -60 + i * 60, -60 + j * 60)
                    m.block((cx, cy, z + 40), (30, 30, 30), "GlowCube", f, "CubeGlow", yaw=yaw, collision=False)
        rng.choice([lambda: P.crate_stack(m, x, y, z, yaw, f, rng, "Hazard"), lambda: P.console(m, x, y, z, yaw, f, rng),
                    lambda: P.barrel(m, x, y, z, f, "Hazard"), cube_pallet,
                    lambda: P.tank(m, x, y, z, f, r=120, h=300, mat="Metal")])()

    def flora(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.bush(m, x, y, z, f, rng, "Flora"), lambda: P.tree(m, x, y, z, f, rng, crown="Flora"),
                    lambda: P.flower_patch(m, x, y, z, f, rng, ["Flora", "Moss", "PastelLilac"]),
                    lambda: P.stall(m, x, y, z, yaw, f, rng, "Moss"), lambda: P.bench(m, x, y, z, yaw, f, "Stone")])()

    def office(m, x, y, z, yaw, rng, f):
        def cubicle():
            for side in (-1, 1):
                px, py = P._off(x, y, yaw, 0, side * 90)
                m.block((px, py, z + 70), (200, 6, 140), "CubiclePanel", f, "Grey", yaw=yaw, tags=["Cover"])
            P.desk_set(m, x, y, z, yaw, f, rng)
        rng.choice([cubicle, lambda: P.cabinet(m, x, y, z, yaw, f), lambda: P.plant(m, x, y, z, f, rng),
                    lambda: P.server_rack(m, x, y, z, yaw, f, rng), lambda: P.console(m, x, y, z, yaw, f, rng)])()

    def tunnel(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.car(m, x, y, z, yaw, f, rng.choice(["Dark", "Grey", "Rust"]), wrecked=True, rng=rng),
                    lambda: P.barrier(m, x, y, z, yaw, f), lambda: P.crate(m, x, y, z, yaw, f, 120, "Hazard"),
                    lambda: P.barrel(m, x, y, z, f, "Hazard", fallen=True, yaw=yaw),
                    lambda: m.block((x, y, z + 50), (400, 50, 100), "JerseyBarrier", f, "Concrete", yaw=yaw, tags=["Cover"])])()

    def desert(m, x, y, z, yaw, rng, f):
        rng.choice([lambda: P.rock(m, x, y, z, f, rng, mat="Sand"), lambda: P.rock(m, x, y, z, f, rng),
                    lambda: P.server_rack(m, x, y, z, yaw, f, rng),
                    lambda: P.pipe(m, (x, y, z + 40), P._off(x, y, yaw, 600, 0) + (z + 40,), f, r=35, mat="Dark"),
                    lambda: m.block((x, y, z + 60), (700, 400, 120), "SandDrift", f, "Sand", yaw=yaw, shape="sphere", tags=["Cover"])])()

    def dream(m, x, y, z, yaw, rng, f):
        pastel = ["PastelPink", "PastelBlue", "PastelMint", "PastelLemon", "PastelLilac"]

        def toy_stack():
            r = rng.uniform(80, 140)
            for k in range(3):
                m.block((x, y, z + r * (0.9 + 1.6 * k)), (r * 2 * (1 - k * 0.2),) * 3, "DreamToy", f, rng.choice(pastel), shape="sphere",
                        tags=["Cover"] if k == 0 else None)
        rng.choice([toy_stack,
                    lambda: m.block((x, y, z + 150), (240, 240, 300), "DreamCone", f, rng.choice(pastel), shape="cone", tags=["Cover"]),
                    lambda: m.block((x, y, z + 90), (300, 300, 180), "DreamCube", f, rng.choice(pastel), yaw=yaw, roll=rng.uniform(-25, 25),
                                    tags=["Cover"]),
                    lambda: m.block((x, y, z + 60), (60, 60, 120), "HORROR_PLACEHOLDER", f, "Horror", shape="cylinder",
                                    tags=["HorrorPlaceholder"])])()

    def tropical(m, x, y, z, yaw, rng, f):
        bright = ["Coral", "Turquoise", "Lime", "Sunflower", "Magenta"]
        rng.choice([lambda: P.palm(m, x, y, z, f, rng), lambda: P.bench(m, x, y, z, yaw, f, rng.choice(bright)),
                    lambda: P.stall(m, x, y, z, yaw, f, rng, rng.choice(bright)),
                    lambda: P.flower_patch(m, x, y, z, f, rng, bright), lambda: P.vending(m, x, y, z, yaw, f, rng, bright)])()

    def village(m, x, y, z, yaw, rng, f):
        def lantern():
            m.block((x, y, z + 40), (60, 60, 80), "StoneLantern_Base", f, "Stone", shape="cylinder", tags=["Cover"])
            m.block((x, y, z + 110), (50, 50, 60), "StoneLantern_Light", f, "Lamp", collision=False)
            m.block((x, y, z + 150), (90, 90, 25), "StoneLantern_Roof", f, "Stone", yaw=45)

        def fence():
            for k in range(5):
                px, py = P._off(x, y, yaw, -200 + k * 100, 0)
                m.block((px, py, z + 60), (12, 12, 120), "BambooPost", f, "Bamboo")
            m.block((x, y, z + 90), (420, 8, 8), "BambooRail", f, "Bamboo", yaw=yaw)
        rng.choice([lantern, fence, lambda: P.rock(m, x, y, z, f, rng, mat="Stone"), lambda: P.bush(m, x, y, z, f, rng, "Moss"),
                    lambda: P.crate_stack(m, x, y, z, yaw, f, rng, "Wood")])()

    return {"ruins": ruins, "docks": docks, "city": city, "factory": factory, "flora": flora, "office": office, "tunnel": tunnel,
            "desert": desert, "dream": dream, "tropical": tropical, "village": village}[kind]


THEMES = {
    2: dict(kind="ruins", cover="Rubble", trim="Rust", plat="Concrete", ground=["Rubble", "Concrete", "Asphalt"], light="Dark"),
    3: dict(kind="docks", cover="Wood", trim="Rust", plat="Concrete", ground=["Wood", "Concrete", "Rust"], light="Lamp"),
    4: dict(kind="city", cover="Metal", trim="Chain", plat="Concrete", ground=["Concrete", "Metal"], light="NeonCyan", plat_h=240),
    5: dict(kind="factory", cover="Metal", trim="Hazard", plat="Metal", ground=["Metal", "Hazard", "Dark"], light="Lamp"),
    6: dict(kind="flora", cover="Stone", trim="Moss", plat="Stone", ground=["Moss", "Stone", "Flora"], light="Moon"),
    7: dict(kind="office", cover="Grey", trim="Dark", plat="Grey", ground=["Grey", "Dark"], light="Lamp", plat_h=160, props_only=True),
    8: dict(kind="tunnel", cover="Concrete", trim="Hazard", plat="Concrete", ground=["Asphalt", "Concrete"], light="Angel", plat_h=240),
    9: dict(kind="desert", cover="Sand", trim="Rust", plat="Rock", ground=["Sand", "Rock"], light="Screen", floor_up=600.0,
            floor_normal=0.45),   # dunes: sloped, rolling ground
    10: dict(kind="dream", cover="PastelBlue", trim="PastelPink", plat="PastelMint", ground=["PastelLemon", "PastelLilac", "PastelBlue"],
             light="PastelLemon"),
    12: dict(kind="tropical", cover="Coral", trim="Turquoise", plat="Sunflower", ground=["Turquoise", "Coral", "Lime"], light="Lamp"),
    13: dict(kind="village", cover="Wood", trim="Roof", plat="Stone", ground=["Stone", "Moss", "Wood"], light="Lamp"),
}


# ============================================================================ sections and routes

def _section(m, rng, floor, spots, n, centre, radius, boss, t):
    f = "%s/Section%d" % (FOLDER, n)
    cx, cy, cz = centre
    props = _props(t["kind"])
    placed = 0

    def spot(rmin, rmax, r):
        for _ in range(14):
            az = rng.uniform(0, 360)
            d = rng.uniform(rmin, rmax)
            x, y = cx + d * math.cos(math.radians(az)), cy + d * math.sin(math.radians(az))
            if not spots.free(x, y, r):
                continue
            z = floor.at(x, y, cz)
            if z is None:
                continue
            spots.take(x, y, r)
            return x, y, z, az
        return None

    # The middle stays open for the fight (and the boss); cover rings it.
    inner = radius * 0.28
    if not t.get("props_only"):
        for _ in range(3 if boss else 2):
            s = spot(radius * 0.45, radius * 0.75, 650)
            if s:
                _platform(m, s[0], s[1], s[2], s[3] + 180, t, rng, f + "/Platforms")
                placed += 1
    for _ in range(9 if boss else 7):
        s = spot(inner, radius * 0.85, 300)
        if s:
            _cover_wall(m, s[0], s[1], s[2], s[3] + 90 + rng.uniform(-20, 20), t, rng, f + "/Cover")
            placed += 1
    for _ in range(6):
        s = spot(inner, radius * 0.9, 120)
        if s:
            _pillar(m, s[0], s[1], s[2], rng.uniform(0, 90), t, rng, f + "/Cover")
            placed += 1
    for _ in range(16 if boss else 12):
        s = spot(inner * 0.8, radius * 0.95, 260)
        if s:
            props(m, s[0], s[1], s[2], rng.uniform(0, 360), rng, f + "/Props")
            placed += 1
    for _ in range(46):
        s = spot(0, radius * 0.95, 60)
        if s:
            _ground_tile(m, s[0], s[1], s[2], rng, t, f + "/Ground")
    for k in range(8):
        s = spot(radius * 0.88, radius * 0.98, 100)
        if s:
            _light(m, s[0], s[1], s[2], t, rng, f + "/Lights")
    return placed


def _route(m, rng, floor, spots, a, b, t, k):
    """Lit markers on the floor from section a to section b, and a lookout tower part-way."""
    f = "%s/Routes/Route%d" % (FOLDER, k)
    (ax, ay, az), (bx, by, bz) = a[1], b[1]
    ra, rb = a[2], b[2]
    dx, dy = bx - ax, by - ay
    length = math.hypot(dx, dy)
    if length < ra + rb + 1500:
        return 0
    yaw = math.degrees(math.atan2(dy, dx))
    ux, uy = dx / length, dy / length
    n = 0
    d = ra + 400
    while d < length - rb - 400:
        x, y = ax + ux * d, ay + uy * d
        zr = az + (bz - az) * d / length
        z = floor.at(x, y, zr)
        if z is not None and spots.free(x, y, 80):
            spots.take(x, y, 80)
            m.block((x, y, z + 3), (220, 60, 6), "RouteMarker", f, t["light"] if t["light"] != "Dark" else "Hazard", yaw=yaw,
                    collision=False, tags=["RouteMarker"])
            n += 1
        d += 700
    # Lookout tower at the middle, off to one side of the route.
    for side in (1, -1):
        mx, my = ax + ux * length * 0.5 - uy * 900 * side, ay + uy * length * 0.5 + ux * 900 * side
        zr = (az + bz) / 2
        z = floor.at(mx, my, zr)
        if z is None or not spots.free(mx, my, 600):
            continue
        spots.take(mx, my, 600)
        # Two tiers: jump up four teal steps onto the lower deck (3.2 m), climb the orange face onto the lookout (6.4 m).
        h = 640.0
        m.block((mx, my, z + 160), (700, 700, 320), "Lookout_Deck", f, t["plat"], tags=["Lookout", "JumpUp"])
        m.block((mx, my, z + 480), (320, 320, 320), "Lookout_Top", f, t["plat"], tags=["Lookout"])
        m.block((mx, my, z + h + 8), (380, 380, 16), "LookoutLip", f, t["trim"], collision=False)
        m.jump_steps(P._off(mx, my, yaw + 180, 350 + 4 * 260, 0) + (z,), yaw, 4, f + "/JumpUp", rise=80, run=260, width=300)
        m.climb_wall(P._off(mx, my, yaw, 160, 0) + (z + 320,), yaw, 300, 320, f + "/Climb")
        _light(m, mx, my, z + h, t, rng, f)
        n += 1
        break
    return n


def chapter(m, n, log):
    """Runs after the chapter's own builder and first detail pass (greybox_chapters.build)."""
    t = THEMES.get(n)
    if not t:
        log("detail2: chapter %d left as is (owner: Return to Nowhere has nothing in it)" % n if n == 11 else "detail2: no theme for %d" % n)
        return
    rng = __import__("random").Random(5000 + n)
    floor = Floor(m, t.get("floor_up", 250.0), t.get("floor_normal", 0.7))
    spots = Spots(m)
    before = sum(m.counts.values())
    placed = 0
    for sec in m.sections:
        placed += _section(m, rng, floor, spots, sec[0], sec[1], sec[2], sec[3], t)
    secs = sorted(m.sections, key=lambda s: s[0])
    routes = 0
    for k in range(len(secs) - 1):
        routes += _route(m, rng, floor, spots, secs[k], secs[k + 1], t, k + 1)
    log("detail2: chapter %d: %d sections dressed (%d cover / platform / prop groups), %d route pieces, +%d actors, %d floor misses"
        % (n, len(m.sections), placed, routes, sum(m.counts.values()) - before, floor.misses))


# ============================================================================ Neo Tokyo (Chapter 1)

def neotokyo(m, log):
    rng = __import__("random").Random(5001)
    f = FOLDER + "/NeoTokyo"
    before = sum(m.counts.values())
    escapes = signs = 0
    for b in m.kits:
        x0, y0, x1, y1 = b["rect"]
        top, ground = b["top"], b["ground"]
        if top - ground < 1500 or rng.random() < 0.35:
            continue
        # Fire escape on one street face: landings every 4 m with zig-zag stair flights (all orange: climbable).
        side = rng.choice(["s", "n", "w", "e"])
        if side in ("s", "n"):
            fy = y0 - 120 if side == "s" else y1 + 120
            cx = (x0 + x1) / 2 + rng.uniform(-(x1 - x0) * 0.2, (x1 - x0) * 0.2)
            along, nyaw = 0.0, (270.0 if side == "s" else 90.0)
            pos = lambda u, z: (cx + u, fy, z)
        else:
            fx = x0 - 120 if side == "w" else x1 + 120
            cy = (y0 + y1) / 2 + rng.uniform(-(y1 - y0) * 0.2, (y1 - y0) * 0.2)
            along, nyaw = 90.0, (180.0 if side == "w" else 0.0)
            pos = lambda u, z: (fx, cy + u, z)
        z = ground + 400
        flip = 1
        height = min(top - 200, ground + 2800)
        while z < height:
            p = pos(0, z)
            m.block(p, (500, 200, 15), "FireEscape_Landing", f + "/FireEscapes", "Climb", yaw=along, tags=["Climbable", "FireEscape"])
            ox, oy = P._off(p[0], p[1], nyaw, 95, 0)
            m.block((ox, oy, z + 50), (500 if along == 0.0 else 10, 10 if along == 0.0 else 500, 100), "FireEscape_Rail",
                    f + "/FireEscapes", "Metal", collision=False)
            if z + 400 < height:
                s = pos(flip * 60, z + 200)
                m.block(s, (420, 90, 10), "FireEscape_Stair", f + "/FireEscapes", "Climb", yaw=along, pitch=-flip * 43,
                        tags=["Climbable", "FireEscape"])
            flip = -flip
            z += 400
        escapes += 1
        # Vertical neon sign and a few balconies on the other faces.
        if rng.random() < 0.7:
            sx, sy = ((x0 + x1) / 2, y0 - 80) if side != "s" else (x0 - 80, (y0 + y1) / 2)
            sh = rng.uniform(600, 1400)
            m.block((sx, sy, ground + 900 + sh / 2), (40, 160, sh), "VerticalNeonSign", f + "/Signs", rng.choice(NEON),
                    yaw=0 if side != "s" else 90, collision=False, tags=["NeonPlaceholder"])
            signs += 1
        for _ in range(rng.randint(2, 5)):
            bz = ground + rng.choice([800, 1200, 1600, 2000])
            if bz > top - 300:
                continue
            if rng.random() < 0.5:
                bx, by, yaw = rng.uniform(x0 + 200, x1 - 200), rng.choice([y0 - 90, y1 + 90]), 0
            else:
                bx, by, yaw = rng.choice([x0 - 90, x1 + 90]), rng.uniform(y0 + 200, y1 - 200), 90
            m.block((bx, by, bz), (300, 160, 15), "Balcony", f + "/Balconies", "Jump", yaw=yaw, tags=["JumpUp"])
            m.block((bx, by, bz + 55), (300, 170, 100), "Balcony_Rail", f + "/Balconies", "Metal", yaw=yaw, collision=False)
            P.ac_unit(m, bx, by, bz + 8, yaw, f + "/Balconies")
    # Street clutter on the pavements: crates, bins, vending, bikes (frames), A-board signs.
    clutter = 0
    for _ in range(1400):
        x, y = rng.uniform(-30000, 30000), rng.uniform(-30000, 30000)
        if not m.blocked(x, y, 60) and m.blocked(x, y, 450):   # just outside a building: the pavement
            k = rng.random()
            if k < 0.3:
                P.bin_(m, x, y, 0, f + "/Street")
            elif k < 0.5:
                P.crate_stack(m, x, y, 0, rng.uniform(0, 360), f + "/Street", rng, "Wood")
            elif k < 0.65:
                P.vending(m, x, y, 0, rng.uniform(0, 360), f + "/Street", rng, NEON)
            elif k < 0.85:
                m.block((x, y, 45), (160, 8, 90), "Bike_Frame", f + "/Street", "Metal", yaw=rng.uniform(0, 360), collision=False)
                m.block((x, y, 35), (70, 70, 6), "Bike_Wheel", f + "/Street", "Dark", pitch=90, yaw=rng.uniform(0, 360), shape="cylinder",
                        collision=False)
            else:
                m.block((x, y, 60), (70, 50, 120), "ABoard", f + "/Street", rng.choice(NEON), yaw=rng.uniform(0, 360), pitch=10,
                        collision=False)
            clutter += 1
            if clutter >= 700:
                break
    log("detail2: Neo Tokyo: %d fire escapes, %d vertical signs, %d street props, +%d actors"
        % (escapes, signs, clutter, sum(m.counts.values()) - before))


# ============================================================================ the Ark

def ark(m, log, town, civ, half_y, under_floor, platform_x0, keep_rects=(), keep_circles=(), dome=(0.0, 0.0, 8500.0, 11500.0)):
    """keep_rects (x0, y0, x1, y1) and keep_circles (x, y, r): the hub's structures and station rings stay clear.
    dome = (x, y, inner, outer): nothing between the walkway ring and the outside of the operator rooms."""
    rng = __import__("random").Random(5002)
    f = FOLDER + "/Ark"
    before = sum(m.counts.values())
    floor = Floor(m)
    spots = Spots(m)
    for x0, y0, x1, y1 in keep_rects:
        m.keepout.append((x0, y0, x1, y1))
    for x, y, r in keep_circles:
        spots.take(x, y, r)

    def scatter(n, area, z_ref, r, place, tries=8):
        done = 0
        for _ in range(n * tries):
            if done >= n:
                break
            x, y = rng.uniform(area[0], area[2]), rng.uniform(area[1], area[3])
            dd = math.hypot(x - dome[0], y - dome[1])
            if z_ref == 0 and dome[2] - r < dd < dome[3] + r:
                continue
            if not spots.free(x, y, r):
                continue
            z = floor.at(x, y, z_ref)
            if z is None:
                continue
            spots.take(x, y, r)
            place(x, y, z)
            done += 1
        return done

    tx0, tx1 = town
    strip = 2000.0
    town_area = (tx0 + 1500, -half_y + strip + 800, tx1 - 1500, half_y - strip - 800)
    # Town square: benches, planters with trees, kiosks, bins, street lamps, holo-ad pillars.
    scatter(160, town_area, 0, 200, lambda x, y, z: P.bench(m, x, y, z, rng.choice([0, 90, 180, 270]), f + "/Town/Benches", "Metal"))
    scatter(120, town_area, 0, 260, lambda x, y, z: (m.block((x, y, z + 40), (240, 240, 80), "Planter", f + "/Town/Planters", "Concrete",
                                                              tags=["JumpUp"]),
                                                      P.tree(m, x, y, z + 80, f + "/Town/Planters", rng, h=rng.uniform(350, 550))))
    scatter(70, town_area, 0, 300, lambda x, y, z: P.stall(m, x, y, z, rng.uniform(0, 360), f + "/Town/Kiosks", rng, rng.choice(NEON)))
    scatter(150, town_area, 0, 80, lambda x, y, z: P.bin_(m, x, y, z, f + "/Town/Bins"))
    scatter(140, town_area, 0, 150, lambda x, y, z: P.lamp(m, x, y, z, rng.uniform(0, 360), f + "/Town/Lamps", h=600, head="Lamp"))
    scatter(40, town_area, 0, 200, lambda x, y, z: (m.block((x, y, z + 300), (120, 120, 600), "HoloAdPillar", f + "/Town/HoloAds", "Dark",
                                                             shape="cylinder"),
                                                     m.block((x, y, z + 450), (130, 130, 250), "HoloAd", f + "/Town/HoloAds", "Holo",
                                                             shape="cylinder", collision=False, tags=["HoloAdPlaceholder"])))
    scatter(60, town_area, 0, 200, lambda x, y, z: P.vending(m, x, y, z, rng.choice([0, 90, 180, 270]), f + "/Town/Vending", rng, NEON))
    # Operators' platform (west of the hole): supply crates, lockers by the dome, roll-call line markings.
    plat = (platform_x0 + 1500, -half_y + 1500, town[0] - 21500, half_y - 1500)
    scatter(110, plat, 0, 260, lambda x, y, z: P.crate_stack(m, x, y, z, rng.choice([0, 90]), f + "/Operators/Crates", rng, "Metal"))
    scatter(90, plat, 0, 120, lambda x, y, z: m.block((x, y, z + 100), (60, 50, 200), "Locker", f + "/Operators/Lockers",
                                                      rng.choice(["Metal", "Grey", "Dark"]), yaw=rng.choice([0, 90]), tags=["JumpUp"]))
    scatter(80, plat, 0, 150, lambda x, y, z: P.lamp(m, x, y, z, rng.uniform(0, 360), f + "/Operators/Lamps", h=550, head="Lamp"))
    for k in range(-6, 7):   # roll-call lines on the floor in front of the dome
        m.block((-500 + 0, k * 500, 3), (2000, 30, 4), "RollCallLine", f + "/Operators/RollCall", "Marking", collision=False)
    # The Underworld: barrels, crates, hanging wires, scrap piles, dim lamps.
    under = (tx0 + 800, -half_y + 1200, tx1 - 800, half_y - 1200)
    scatter(160, under, under_floor, 200, lambda x, y, z: P.barrel(m, x, y, z, f + "/Underworld/Barrels", rng.choice(["Rust", "Hazard"]),
                                                                   fallen=rng.random() < 0.3, yaw=rng.uniform(0, 360)))
    scatter(140, under, under_floor, 260, lambda x, y, z: P.crate_stack(m, x, y, z, rng.uniform(0, 360), f + "/Underworld/Crates", rng, "Wood"))
    scatter(120, under, under_floor, 260, lambda x, y, z: P.rubble_pile(m, x, y, z, f + "/Underworld/Scrap", rng, mat="Rust"))
    scatter(90, under, under_floor, 150, lambda x, y, z: P.lamp(m, x, y, z, rng.uniform(0, 360), f + "/Underworld/Lamps", h=350,
                                                                head=rng.choice(NEON)))
    for _ in range(90):
        a = (rng.uniform(under[0], under[2]), rng.uniform(under[1], under[3]), under_floor + rng.uniform(2600, 3600))
        b = (a[0] + rng.uniform(-2500, 2500), a[1] + rng.uniform(-2500, 2500), a[2] + rng.uniform(-300, 300))
        P.wire(m, a, b, f + "/Underworld/Wires", sag=rng.uniform(80, 300))
    # Civilian boulevard: fountains, street lamps, benches and flower beds along the middle.
    cx0, cx1 = civ
    x = cx0 + 3000
    while x < cx1 - 2000:
        for side in (-1, 1):
            P.lamp(m, x, side * 800, 6, 90 * side, f + "/Civilian/Lamps", h=650, head="Lamp")
            P.bench(m, x + 800, side * 700, 6, 90 * side + 90, f + "/Civilian/Benches", "Wood")
            P.flower_patch(m, x + 1600, side * 600, 6, f + "/Civilian/Flowers", rng, ["Coral", "Sunflower", "Magenta", "PastelPink"])
        if int((x - cx0) // 3000) % 4 == 0:
            m.block((x, 0, 40), (900, 900, 80), "Fountain_Basin", f + "/Civilian/Fountains", "Stone", shape="cylinder", tags=["JumpUp"])
            m.block((x, 0, 82), (820, 820, 6), "Fountain_Water", f + "/Civilian/Fountains", "Water", shape="cylinder", collision=False)
            m.block((x, 0, 250), (80, 80, 340), "Fountain_Spout", f + "/Civilian/Fountains", "Stone", shape="cylinder")
            m.block((x, 0, 440), (260, 260, 40), "Fountain_Bowl", f + "/Civilian/Fountains", "Stone", shape="cylinder")
        x += 3000
    log("detail2: Ark: town, operators' platform, Underworld and civilian boulevard dressed, +%d actors, %d floor misses"
        % (sum(m.counts.values()) - before, floor.misses))
