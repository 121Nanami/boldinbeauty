"""World generator for the large story maps (chapters 14-40), owner request 2026-10-11: "most are too small".

A map is about 1 km across (90,000-120,000 cm): four combat sections (section radius ~45-55 m, MAIN section 7: ~8 s centre to wall)
joined by long avenues; districts of enterable buildings filling the space between; five or more discovery areas off the critical
path (a landmark, an archive pickup and an optional elite); a hero set piece per chapter; plateaus, water and an edge that frames
the space. Greybox only: basic engine shapes and palette colours. Reuses the helpers of greybox_levels.py.

    World(m, rng, spec, extent).ground() / .road() / .district() / .section() / .discovery() / .edge() / .plateau() / .lake()
"""

import math
import random

import unreal

import greybox_levels as L
import greybox_story as gs

KINDS = {
    # kind: walls, height range, width range, street width, density, roof, accent strip, hollow share, extras
    "city": dict(walls=["Concrete", "Roof", "Dark", "Rock"], h=(1800, 11000), w=(1000, 2600), street=900, density=0.78, accent=0.45, hollow=0.10),
    "market": dict(walls=["Wood", "Rubble", "Metal", "Coral", "Sunflower"], h=(350, 1100), w=(600, 1500), street=700, density=0.85, accent=0.0, hollow=0.12, awning=True),
    "ruin": dict(walls=["Rubble", "Concrete", "Rock"], h=(600, 4200), w=(900, 2400), street=1000, density=0.55, accent=0.0, hollow=0.10, broken=True),
    "village": dict(walls=["Wood"], h=(500, 900), w=(900, 1500), street=1100, density=0.8, accent=0.0, hollow=0.14, roofs=True, trees=True),
    "industrial": dict(walls=["Metal", "Dark", "Concrete"], h=(1500, 5500), w=(1800, 4500), street=1300, density=0.6, accent=0.2, hollow=0.08, tanks=True),
    "campus": dict(walls=["Concrete", "Marking", "Roof"], h=(1200, 3600), w=(1500, 3200), street=1300, density=0.55, accent=0.1, hollow=0.12),
    "tents": dict(walls=["Magenta", "Coral", "Turquoise", "Sunflower"], h=(500, 1300), w=(800, 1600), street=1100, density=0.6, accent=0.0, hollow=0.08, cones=True),
    "stacks": dict(walls=["Wood", "Dark"], h=(1500, 3600), w=(500, 900), street=500, density=0.9, accent=0.0, hollow=0.0, rows=True),
    "crypt": dict(walls=["Stone", "Rock", "Concrete"], h=(600, 2200), w=(700, 1500), street=900, density=0.6, accent=0.1, hollow=0.2),
    "white": dict(walls=["Cloud", "PastelMint", "Marking"], h=(800, 3800), w=(500, 1200), street=1400, density=0.4, accent=0.0, hollow=0.0, pillars=True),
    "wild": dict(walls=["Moss", "Rock"], h=(400, 1400), w=(500, 1500), street=1000, density=0.92, accent=0.0, hollow=0.0, trees=True, rocks=True),
}


class World:
    def __init__(self, m, rng, spec, extent=100000.0, folder="World"):
        self.m, self.rng, self.spec, self.extent, self.f = m, rng, spec, extent, folder
        self.half = extent / 2.0
        self.reserved = []     # (x, y, r): keep-out circles for districts
        self.cells = {}        # spatial hash of the reserved circles (cell = 3000 cm)
        self.water = []        # (x0, y0, x1, y1)
        self.sections = []
        self.discoveries = []
        self.counter = 0

    # -------------------------------------------------------------- bookkeeping
    def reserve(self, x, y, r):
        entry = (x, y, r)
        self.reserved.append(entry)
        reach = r + 3000.0
        for cx in range(int((x - reach) // 3000), int((x + reach) // 3000) + 1):
            for cy in range(int((y - reach) // 3000), int((y + reach) // 3000) + 1):
                self.cells.setdefault((cx, cy), []).append(entry)

    def free(self, x, y, pad=0.0):
        for rx, ry, rr in self.cells.get((int(x // 3000), int(y // 3000)), ()):
            if (x - rx) ** 2 + (y - ry) ** 2 < (rr + pad) ** 2:
                return False
        for x0, y0, x1, y1 in self.water:
            if x0 - pad < x < x1 + pad and y0 - pad < y < y1 + pad:
                return False
        return True

    # -------------------------------------------------------------- ground, water, roads
    def ground(self, mat="Dark", tile=6000.0, z=0.0):
        m, h = self.m, self.half
        n = int(math.ceil(self.extent / tile))
        for gx in range(n):
            for gy in range(n):
                x0, y0 = -h + gx * tile, -h + gy * tile
                x1, y1 = x0 + tile, y0 + tile
                if any(not (x1 <= w[0] or x0 >= w[2] or y1 <= w[1] or y0 >= w[3]) for w in self.water):
                    continue
                m.box(x0, y0, z - 300, x1, y1, z, "Ground", self.f + "/Ground", mat)

    def lake(self, x0, y0, x1, y1, depth=1300.0, climb_sides=("N", "S", "E", "W")):
        """A swimmable lake: sunken bed, water volume, jump-step exits on each side."""
        m = self.m
        self.water.append((x0, y0, x1, y1))
        m.box(x0, y0, -depth - 200, x1, y1, -depth, "LakeBed", self.f + "/Water", "Sand")
        m.water(x0, y0, -depth, x1, y1, -80.0, "Lake", self.f + "/Water")
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        for side in climb_sides:
            if side == "N":
                m.jump_steps((cx, y1 - 600, -depth), 90.0, 6, self.f + "/Water", rise=depth / 6.0 - 5, run=300.0, width=500.0, gap=0.0)
            elif side == "S":
                m.jump_steps((cx, y0 + 600, -depth), 270.0, 6, self.f + "/Water", rise=depth / 6.0 - 5, run=300.0, width=500.0, gap=0.0)
            elif side == "E":
                m.jump_steps((x1 - 600, cy, -depth), 0.0, 6, self.f + "/Water", rise=depth / 6.0 - 5, run=300.0, width=500.0, gap=0.0)
            else:
                m.jump_steps((x0 + 600, cy, -depth), 180.0, 6, self.f + "/Water", rise=depth / 6.0 - 5, run=300.0, width=500.0, gap=0.0)

    def road(self, a, b, width=1500.0, mat="Asphalt", lamps=True, z=None):
        """A road from a to b (z follows the end points); reserves a corridor so districts leave it open."""
        m = self.m
        dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        length = math.sqrt(dx * dx + dy * dy)
        if length < 100:
            return
        ux, uy = dx / length, dy / length
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, length))
        steps = max(1, int(length // 6000))
        for k in range(steps):
            t0, t1 = k / steps, (k + 1) / steps
            p0 = (a[0] + dx * t0, a[1] + dy * t0, a[2] + dz * t0)
            p1 = (a[0] + dx * t1, a[1] + dy * t1, a[2] + dz * t1)
            seg = math.sqrt((p1[0] - p0[0]) ** 2 + (p1[1] - p0[1]) ** 2 + (p1[2] - p0[2]) ** 2)
            m.block(((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2 - 25), (seg + 20, width, 50), "Road", self.f + "/Roads", mat,
                    yaw=yaw, pitch=pitch)
        for k in range(0, int(length // 1500)):
            t = (k * 1500.0 + 750.0) / length
            p = (a[0] + dx * t, a[1] + dy * t, a[2] + dz * t)
            m.block((p[0], p[1], p[2] + 4), (500, 60, 6), "RoadMark", self.f + "/Roads", "Marking", yaw=yaw, collision=False)
            self.reserve(p[0], p[1], width * 0.9)
            if lamps and k % 2 == 0:
                for s in (-1, 1):
                    lx, ly = p[0] - uy * (width / 2 + 160) * s, p[1] + ux * (width / 2 + 160) * s
                    m.block((lx, ly, p[2] + 330), (30, 30, 660), "RoadLamp", self.f + "/Roads", "Metal", shape="cylinder")
                    m.block((lx, ly, p[2] + 690), (110, 110, 40), "RoadLampHead", self.f + "/Roads", self.spec["accent"], collision=False)

    def plateau(self, x0, y0, x1, y1, z, mat="Concrete", ramp_side="S"):
        m = self.m
        m.box(x0, y0, -200, x1, y1, z, "Plateau", self.f + "/Plateaus", mat)
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        if ramp_side == "S":
            L.stairs_to(m, (cx, y0 - 3000, 0), (cx, y0, z), self.f + "/Plateaus", mat, 1400.0, 80.0)
        elif ramp_side == "N":
            L.stairs_to(m, (cx, y1 + 3000, 0), (cx, y1, z), self.f + "/Plateaus", mat, 1400.0, 80.0)
        elif ramp_side == "E":
            L.stairs_to(m, (x1 + 3000, cy, 0), (x1, cy, z), self.f + "/Plateaus", mat, 1400.0, 80.0)
        else:
            L.stairs_to(m, (x0 - 3000, cy, 0), (x0, cy, z), self.f + "/Plateaus", mat, 1400.0, 80.0)
        for rx, ry in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
            self.reserve(rx, ry, 800)

    # -------------------------------------------------------------- districts
    def building(self, x, y, w, d, h, k, z=0.0):
        m, rng, f = self.m, self.rng, self.f + "/Districts"
        self.reserve(x, y, max(w, d) * 0.6)
        mat = rng.choice(k["walls"])
        yaw = rng.choice([0, 0, 0, 90])
        if k.get("rows"):
            m.block((x, y, z + h / 2), (w, d * 3.2, h), "Shelf", f, mat)
            return
        if k.get("pillars"):
            m.block((x, y, z + h / 2), (w, w, h), "WhitePillar", f, mat, shape="cylinder")
            return
        if k.get("cones") and rng.random() < 0.7:
            m.block((x, y, z + h * 0.45), (w * 1.4, w * 1.4, h * 0.9), "Tent", f, mat, shape="cone")
            return
        if k.get("rocks") and rng.random() < 0.3:
            m.block((x, y, z + h * 0.3), (w * 1.2, w * 0.9, h * 0.6), "Boulder", f, rng.choice(["Rock", "Moss", "Stone"]), shape="sphere", yaw=rng.uniform(0, 180))
            return
        if k.get("trees") and rng.random() < 0.7:
            gs._p_tree(m, x, y, z, rng, f, self.spec)
            if k.get("roofs") is None:
                return
        if rng.random() < k.get("hollow", 0.0) and w > 900:
            # an enterable building: floor, four walls with a door, a roof, a lamp and a small stand-in loot crate
            L.room(m, x - w / 2, y - d / 2, x + w / 2, y + d / 2, z, min(h, 1100), f, mat, floor="Concrete",
                   gaps={"S": [(0.4, 0.6)]}, roof=mat if not k.get("broken") else None)
            m.block((x, y, z + 80), (120, 120, 120), "LootCrate", f, "Wood")
            m.point_light((x, y, z + 700), "Lighting", intensity=12000.0, radius=1800.0)
            return
        bw, bd = (w, d) if yaw == 0 else (d, w)
        if k.get("broken"):
            m.block((x, y, z + h * 0.35), (bw, bd, h * 0.7), "RuinWall", f, mat, roll=self.rng.uniform(-6, 6))
            m.block((x + bw * 0.2, y, z + h * 0.7), (bw * 0.4, bd * 0.8, h * 0.6), "RuinTop", f, "Rubble", yaw=self.rng.uniform(0, 180))
            return
        m.block((x, y, z + h / 2), (bw, bd, h), "Building", f, mat, yaw=0)
        if k.get("roofs"):
            m.block((x, y, z + h + bd * 0.12), (bw * 1.15, bd * 0.65, 30), "Roof", f, "Roof", roll=24)
        if k.get("awning"):
            m.block((x, y - bd / 2 - 150, z + h * 0.8), (bw * 1.1, 400, 30), "Awning", f, self.spec["accent"], collision=False)
        if k.get("tanks") and rng.random() < 0.4:
            m.block((x + bw * 0.6 + 300, y, z + 700), (600, 600, 1400), "Tank", f, "Metal", shape="cylinder")
        if rng.random() < k.get("accent", 0.0):
            m.block((x, y, z + h * rng.uniform(0.3, 0.9)), (bw * 1.04, bd * 1.04, 110), "NeonBand", f, self.spec["accent"], collision=False)

    def district(self, x0, y0, x1, y1, kind, z=0.0, pad=1800.0, climb_share=0.12):
        k = KINDS[kind]
        rng = self.rng
        wmid = (k["w"][0] + k["w"][1]) / 2
        step = wmid + k["street"]
        gx = x0
        count = 0
        while gx < x1:
            gy = y0
            while gy < y1:
                x = gx + rng.uniform(-0.15, 0.15) * step
                y = gy + rng.uniform(-0.15, 0.15) * step
                if rng.random() < k["density"] and self.free(x, y, pad):
                    w = rng.uniform(*k["w"])
                    d = rng.uniform(*k["w"])
                    h = rng.uniform(*k["h"])
                    self.building(x, y, w, d, h, k, z)
                    count += 1
                    if rng.random() < climb_share and k["h"][1] > 2000 and not k.get("rows"):   # climbable facade ledges
                        self.m.climb_wall((x + w / 2, y, z), 0.0, min(d, 900.0), min(h, 2400.0), self.f + "/Districts")
                gy += step
            gx += step
        return count

    # -------------------------------------------------------------- sections and discoveries
    def section(self, idx, c, r=5000.0, boss=False, ring=True):
        m = self.m
        if c[2] > 50.0:   # an elevated section stands on a raised pad
            m.block((c[0], c[1], c[2] / 2 - 30), (r * 2 + 1000, r * 2 + 1000, c[2]), "SectionPad", "%s/Section%d" % (self.f, idx), "Concrete", shape="cylinder")
        sec = L.arena(m, idx, c, r, "%s/Section%d" % (self.f, idx), boss=boss, floor=self.spec["floor"])
        self.reserve(c[0], c[1], r + 1500.0)
        self.sections.append(sec)
        # edge posts and cover so the circle reads as an arena without being a closed box
        if ring:
            for k in range(12):
                p = gs._az(c, r + 200, k * 30, c[2])
                m.block((p[0], p[1], c[2] + 700), (300, 300, 1400), "ArenaPost", "%s/Section%d" % (self.f, idx), self.spec["wall"], shape="cylinder")
                m.block((p[0], p[1], c[2] + 1450), (420, 420, 60), "ArenaPostCap", "%s/Section%d" % (self.f, idx), self.spec["accent"], collision=False)
            for k in range(10):
                p = gs._az(c, self.rng.uniform(1500, r * 0.85), self.rng.uniform(0, 360), c[2])
                m.block((p[0], p[1], c[2] + 60), (240, 110, 120), "Cover", "%s/Section%d" % (self.f, idx), "Concrete", yaw=self.rng.uniform(0, 180))
        L.jump_and_climb(m, c, 40 + 90 * idx, "%s/Section%d" % (self.f, idx))
        return sec

    def discovery(self, c, name, doc, landmark="arch", elite=None, kind="plaza"):
        """An off-path area: a ring wall with two gaps, a landmark, an archive pickup (YELLOW) and an optional elite (RED)."""
        m, rng = self.m, self.rng
        self.counter += 1
        n = self.counter
        f = "%s/Discovery%d" % (self.f, n)
        r = 2300.0
        m.block((c[0], c[1], c[2] - 25), (r * 2, r * 2, 50), "DiscoveryFloor", f, self.spec["floor"], shape="cylinder")
        for k in range(16):
            az = k * 22.5
            if 80 < az < 100 or 260 < az < 280:
                continue
            p = gs._az(c, r, az, c[2])
            m.block((p[0], p[1], c[2] + 450), (200, 2 * math.pi * r / 16 * 0.9, 900), "DiscoveryWall", f, self.spec["wall"], yaw=az)
        lm = gs.LANDMARKS.get(landmark)
        if lm:
            lm(m, (c[0] + 400, c[1] + 400, c[2]), f, self.spec)
        self.reserve(c[0], c[1], r + 1500.0)
        gs._marker(m, "yellow", "ARCHIVE: %s" % doc, (c[0] - 700, c[1] - 600, c[2]), f, size=0.9)
        gs._marker(m, "purple", "DISCOVERY %d: %s" % (n, name), (c[0], c[1] - 1500, c[2]), f, size=1.0, trigger="Discovery_%d" % n)
        if elite:
            gs._marker(m, "red", "OPTIONAL ELITE: %s" % elite, (c[0] + 900, c[1] - 700, c[2]), f, scale=1.4, size=0.9)
        L.props(m, rng, c, r, c[2], self.spec["theme"], 4, f, self.spec, keep=700)
        self.discoveries.append((n, c, name))

    def dress(self, target=12000, theme=None):
        """Scatters themed props over the free ground until the map holds `target` pieces (owner: 8-12k pieces minimum)."""
        m, rng = self.m, self.rng
        pool = gs.PROPS[theme or self.spec["theme"]]
        f = self.f + "/Dressing"
        tries = 0
        centres = [(s_[1], s_[2]) for s_ in self.sections] + [(c, 2300.0) for _, c, _ in self.discoveries]
        sea = bool(self.water) and self.water[0][2] - self.water[0][0] >= self.extent - 10
        while sum(m.counts.values()) < target and tries < 80000:
            tries += 1
            if sea:
                c, r = rng.choice(centres)
                a, rr = rng.uniform(0, 360), r * math.sqrt(rng.uniform(0.1, 1.0)) * 1.1
                x, y, z = c[0] + rr * math.cos(math.radians(a)), c[1] + rr * math.sin(math.radians(a)), c[2]
            else:
                x = rng.uniform(-self.half + 4000, self.half - 4000)
                y = rng.uniform(-self.half + 4000, self.half - 4000)
                z = 0.0
                if not self.free(x, y, 500.0):
                    continue
            rng.choice(pool)(m, x, y, z, rng, f, self.spec)

    def edge(self, kind="skyline", inset=2500.0):
        """The map boundary: walls of buildings / cliffs / a high wall, so the space is framed."""
        m, rng, f = self.m, self.rng, self.f + "/Edge"
        h = self.half - inset
        n = int(self.extent // 1800)
        for k in range(n):
            t = -h + k * 1800.0
            for (x, y) in ((t, -h), (t, h), (-h, t), (h, t)):
                if kind == "hills":
                    w = rng.uniform(3500, 7500)
                    ht = rng.uniform(3000, 8500)
                    m.block((x, y, ht / 2), (w, w, ht), "Hill", f, rng.choice(["Moss", "Rock", "Grass"]), shape="cone", collision=False)
                elif kind == "wall":
                    m.block((x, y, 3000), (1900, 1900, 6000), "EdgeWall", f, self.spec["wall"])
                else:
                    w, d, ht = rng.uniform(1200, 2800), rng.uniform(1200, 2800), rng.uniform(6000, 17000)
                    m.block((x, y, ht / 2), (w, d, ht), "Skyline", f, rng.choice(["Dark", "Concrete", "Roof", "Rock"]))
                    if rng.random() < 0.5:
                        m.block((x, y, ht * rng.uniform(0.3, 0.9)), (w * 1.04, d * 1.04, 130), "SkylineBand", f, self.spec["accent"], collision=False)
