"""Detail passes for the greybox maps (owner, 2026-10-07: more fleshed out, higher detail, 8,000-12,000 pieces a map, Neo
Tokyo included). Each pass runs after its map's layout (greybox_chapters.py / greybox_neotokyo.py) and adds props, street
furniture, structure, foliage and dressing that fit the owner's brief. Nothing here changes a section, a route or a
traversal piece; props avoid placed kit buildings (GreyboxMap.blocked) and keep-out rects. All placements are PROPOSED."""

import math

import unreal

import greybox_props as P
from greybox_common import NEON


def _scatter(m, rng, n, area, place, pad=200.0, tries=6):
    """Calls place(x, y) n times at random free points of area = (x0, y0, x1, y1) or a callable returning (x, y)."""
    done = 0
    for _ in range(n * tries):
        if done >= n:
            break
        if callable(area):
            pt = area()
            if pt is None:
                continue
            x, y = pt
        else:
            x, y = rng.uniform(area[0], area[2]), rng.uniform(area[1], area[3])
        if m.blocked(x, y, pad):
            continue
        place(x, y)
        done += 1
    return done


def _near_section(m, x, y, extra=0.0):
    return any(math.hypot(x - c[0], y - c[1]) < r + extra for _, c, r, _ in m.sections)


def _rooftop(m, rng, b, folder, tank=True):
    x0, y0, x1, y1 = b["rect"]
    top = b["top"]
    w, d = x1 - x0, y1 - y0
    if w < 600 or d < 600:
        return
    if tank and rng.random() < 0.7:
        P.water_tank(m, x0 + w * rng.uniform(0.25, 0.75), y0 + d * rng.uniform(0.25, 0.75), top, folder, r=rng.uniform(100, 170))
    for _ in range(rng.randint(2, 4)):
        P.ac_unit(m, x0 + w * rng.uniform(0.15, 0.85), y0 + d * rng.uniform(0.15, 0.85), top, rng.choice([0, 90]), folder)
    if rng.random() < 0.6:
        P.antenna(m, x0 + w * rng.uniform(0.2, 0.8), y0 + d * rng.uniform(0.2, 0.8), top, folder, h=rng.uniform(300, 800))
    # Roof-edge railing posts along two sides.
    for k in range(int(w // 300)):
        m.block((x0 + 150 + k * 300, y0 + 20, top + 50), (8, 8, 100), "RoofRail", folder, "Metal", collision=False)
    for k in range(int(d // 300)):
        m.block((x0 + 20, y0 + 150 + k * 300, top + 50), (8, 8, 100), "RoofRail", folder, "Metal", collision=False)


def _road_line(m, rng, a, b, folder, width=1200.0, seg=1500.0, broken=0.0, dashes=900.0, mat="Asphalt", z=0.0):
    """A road from a to b (axis aligned or not): slabs (some tilted or missing when broken) and centre dashes."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    n = max(1, int(length // seg))
    for k in range(n):
        t = (k + 0.5) / n
        x, y = a[0] + dx * t, a[1] + dy * t
        if broken and rng.random() < broken * 0.3:
            continue
        tilt = rng.uniform(-6, 6) if (broken and rng.random() < broken) else 0.0
        m.block((x, y, z + 4), (length / n - 20, width, 8), "RoadSlab", folder, mat, yaw=yaw, pitch=tilt, roll=tilt * 0.5,
                collision=False)
    if dashes:
        for k in range(int(length // dashes)):
            t = (k + 0.5) * dashes / length
            if broken and rng.random() < broken * 0.4:
                continue
            m.block((a[0] + dx * t, a[1] + dy * t, z + 9), (300, 25, 2), "LaneDash", folder, "Marking", yaw=yaw, collision=False)


# ============================================================================ Neo Tokyo

def neotokyo(m, rng, occupants, starts, gaps, block, half, sidewalk):
    f = "Detail"
    # Rooftop clutter on every building: tanks, AC units, antennas, roof rails.
    for occ in occupants.values():
        for b in occ:
            _rooftop(m, rng, b, f + "/Rooftops")
    # Traffic lights on the corners of every crossing, awnings and shop fronts on every building face.
    centres = [s + block + gaps[k] / 2 for k, s in enumerate(starts[:-1])]
    for i, cx in enumerate(centres):
        for j, cy in enumerate(centres):
            hw, hh = gaps[i] / 2, gaps[j] / 2
            for sx, sy, yaw in ((1, 1, 180), (-1, 1, 270), (-1, -1, 0), (1, -1, 90)):
                P.traffic_light(m, cx + sx * (hw + 150), cy + sy * (hh + 150), sidewalk, yaw, f + "/TrafficLights")
    for occ in occupants.values():
        for b in occ:
            x0, y0, x1, y1 = b["rect"]
            for (fx, fy, yaw, along) in (((x0 + x1) / 2, y0 - 120, 0, x1 - x0), ((x0 + x1) / 2, y1 + 120, 180, x1 - x0),
                                         (x0 - 120, (y0 + y1) / 2, 90, y1 - y0), (x1 + 120, (y0 + y1) / 2, 270, y1 - y0)):
                if along < 800:
                    continue
                for k in range(min(4, int(along // 900))):
                    off = (k - (min(4, int(along // 900)) - 1) / 2) * 900
                    ax, ay = P._off(fx, fy, yaw, off, 0)
                    m.block((ax, ay, sidewalk + 330), (600, 220, 12), "Awning", f + "/ShopFronts", rng.choice(NEON + ["Grey", "Dark"]),
                            yaw=yaw, pitch=0, roll=10, collision=False)
                    if rng.random() < 0.5:
                        P.vending(m, ax + rng.uniform(-200, 200) * (yaw in (0, 180)), ay + rng.uniform(-200, 200) * (yaw in (90, 270)),
                                  sidewalk, yaw + 180, f + "/Vending", rng, NEON)
    # Sidewalk furniture along every street: benches, bins, bollards, bikes, cars parked at the kerb.
    for k, c in enumerate(centres):
        width = gaps[k]
        for along in range(int(-half) + 600, int(half), 1100):
            for side in (-1, 1):
                for horizontal in (True, False):
                    off = c + side * (width / 2 - 350)
                    x, y = (along, off) if horizontal else (off, along)
                    if rng.random() > 0.45:
                        continue
                    yaw = (90 if side < 0 else 270) if horizontal else (0 if side < 0 else 180)
                    pick = rng.random()
                    if pick < 0.3:
                        P.bench(m, x, y, sidewalk, yaw + 180, f + "/Street")
                    elif pick < 0.55:
                        P.bin_(m, x, y, sidewalk, f + "/Street")
                    elif pick < 0.75:
                        for b2 in range(3):
                            bx, by = P._off(x, y, yaw, b2 * 80 - 80, 0)
                            m.block((bx, by, sidewalk + 45), (16, 16, 90), "Bollard", f + "/Street", "Hazard", shape="cylinder")
                    else:
                        lane = c + side * (width / 2 - 900)
                        cx2, cy2 = (along, lane) if horizontal else (lane, along)
                        P.car(m, cx2, cy2, 0, 0 if horizontal else 90, f + "/ParkedCars", rng.choice(["Metal", "Grey", "Dark"] + NEON[:2]))
    # Power poles and sagging cables down the alleys (the 14 m streets).
    for k, c in enumerate(centres):
        if gaps[k] > 1500:
            continue
        for horizontal in (True, False):
            prev = None
            for along in range(int(-half) + 1000, int(half), 3500):
                x, y = (along, c + gaps[k] / 2 - 120) if horizontal else (c + gaps[k] / 2 - 120, along)
                P.power_pole(m, x, y, sidewalk, f + "/PowerLines", h=1000)
                top = (x, y, sidewalk + 940)
                if prev:
                    P.wire(m, prev, top, f + "/PowerLines")
                    P.wire(m, (prev[0], prev[1], prev[2] - 40), (top[0], top[1], top[2] - 40), f + "/PowerLines")
                prev = top


# ============================================================================ 2. The Ruins

def ch02(m, rng, log):
    import greybox_neotokyo as NT
    half = 30000
    f = "Detail"
    secs = [c for _, c, _, _ in m.sections]
    m.keepout += [(-26500, -3500, -19500, 3500), (-5200, -5200, 3200, 1200)]   # hatch pad, crater
    # Broken road grid.
    for k in range(-5, 6):
        v = k * 6000
        _road_line(m, rng, (-half, v), (half, v), f + "/Roads", broken=0.6)
        _road_line(m, rng, (v, -half), (v, half), f + "/Roads", broken=0.6)
    # More ruined buildings filling the blocks between roads, each with a broken facade climb route and a rubble skirt.
    pool = ["%s/%s" % ("/Game/Greybox/Kits/K1", p) for p in
            ["HighRise/SM_K1_HighRise_%d" % i for i in range(1, 15)] + ["Corporation/SM_K1_Corporation_%d" % i for i in range(1, 11)]
            + ["Cottage/SM_K1_Cottage_%d" % i for i in range(1, 21)] + ["Cityscape/SM_K1_Cityscape_%d" % i for i in range(1, 5)]]
    for i in range(-5, 5):
        for j in range(-5, 5):
            x, y = i * 6000 + 3000, j * 6000 + 3000
            if _near_section(m, x, y, 3500) or m.blocked(x, y, 1500):
                continue
            b = m.place_kit(rng.choice(pool), x, y, -rng.uniform(200, 1200), rng.choice([0, 90, 180, 270]), 0.9, "Ruin", f + "/Ruins",
                            ["Building", "Ruin", "Climbable"], material="Rubble")
            b["actor"].set_actor_rotation(unreal.Rotator(roll=rng.uniform(-7, 7), pitch=rng.uniform(-7, 7),
                                                         yaw=b["actor"].get_actor_rotation().yaw), False)
            x0, y0, x1, y1 = b["rect"]
            NT._climb_route(m, rng, (x0 + x1) / 2, y0, -1, True, min(b["top"] - 200, 2400), f + "/ClimbRoutes")
            for _ in range(3):
                P.rubble_pile(m, rng.uniform(x0, x1), rng.choice([y0 - 300, y1 + 300]), 0, f + "/Rubble", rng)
    P_area = (-half, -half, half, half)
    _scatter(m, rng, 190, P_area, lambda x, y: P.car(m, x, y, 0, rng.uniform(0, 360), f + "/Wrecks", rng.choice(["Rust", "Dark", "Grey"]),
                                                     wrecked=True, rng=rng))
    _scatter(m, rng, 260, P_area, lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Lamps", head="Dark", broken=rng.random() < 0.6,
                                                      rng=rng))
    _scatter(m, rng, 380, P_area, lambda x, y: P.rubble_pile(m, x, y, 0, f + "/Rubble", rng))
    _scatter(m, rng, 170, P_area, lambda x, y: P.rebar(m, x, y, 0, f + "/Rebar", rng))
    _scatter(m, rng, 160, P_area, lambda x, y: P.tree(m, x, y, 0, f + "/DeadTrees", rng, dead=True))
    _scatter(m, rng, 45, P_area, lambda x, y: P.sandbags(m, x, y, 0, rng.uniform(0, 180), f + "/Barricades"))
    _scatter(m, rng, 90, P_area, lambda x, y: P.barrier(m, x, y, 0, rng.uniform(0, 180), f + "/Barricades"))
    # Section dressing: the Outer World building types.
    school, police, hospital, gym = secs[0], secs[1], secs[2], secs[3]
    for k in range(36):   # scattered classroom desks
        p = m.polar(school, rng.uniform(800, 4000), rng.uniform(0, 360), 0)
        P.desk_set(m, p[0], p[1], 0, rng.uniform(0, 360), f + "/School", rng)
    for k in range(60):   # schoolyard fence, many posts missing
        p = m.polar(school, 5000, k * 6, 0)
        if rng.random() < 0.7:
            m.block((p[0], p[1], 100), (10, 10, 200), "FencePost", f + "/School", "Metal", pitch=rng.uniform(-15, 15))
    m.box(school[0] + 1500, school[1] - 300, 0, school[0] + 1800, school[1] + 300, 250, "SwingFrame", f + "/School", "Metal")
    P.truck(m, police[0] + 2500, police[1] + 1500, 0, 30, f + "/FireStation", "NeonRed", wrecked=True, rng=rng)
    P.truck(m, police[0] + 3000, police[1] - 2000, 0, 200, f + "/FireStation", "NeonRed", wrecked=True, rng=rng)
    for k in range(8):
        p = m.polar(police, rng.uniform(1000, 3800), rng.uniform(0, 360), 0)
        P.car(m, p[0], p[1], 0, rng.uniform(0, 360), f + "/Police", "NeonCyan", wrecked=True, rng=rng)
    for k in range(24):
        p = m.polar(police, rng.uniform(1500, 4200), rng.uniform(0, 360), 0)
        P.barrier(m, p[0], p[1], 0, rng.uniform(0, 180), f + "/Police")
    for k in range(30):   # hospital beds
        p = m.polar(hospital, rng.uniform(800, 4000), rng.uniform(0, 360), 0)
        m.block((p[0], p[1], 50), (200, 90, 15), "Bed", f + "/Hospital", "Marking", yaw=rng.uniform(0, 360), roll=rng.uniform(-10, 10))
        m.block((p[0], p[1], 25), (190, 80, 50), "BedFrame", f + "/Hospital", "Metal", yaw=rng.uniform(0, 360), collision=False)
    P.truck(m, hospital[0] - 2000, hospital[1] + 2500, 0, 120, f + "/Hospital", "Marking", wrecked=True, rng=rng)
    for k in range(80):   # stadium seats on the gym stands
        az = rng.uniform(0, 360)
        if 160 < az < 200:
            continue
        p = m.polar(gym, rng.uniform(6400, 7600), az, 0)
        m.block((p[0], p[1], 120 + (math.hypot(p[0] - gym[0], p[1] - gym[1]) - 6200) / 300 * 80), (50, 50, 40), "StadiumSeat",
                f + "/Gym", rng.choice(["NeonRed", "Grey"]), yaw=az, collision=False)


# ============================================================================ 3. Docks

def ch03(m, rng, log):
    f = "Detail"
    # Quay edge stones (two courses) and cracked paving slabs near the shore.
    for row in range(2):
        for k in range(250):
            y = -25000 + k * 200 + row * 100
            if rng.random() < 0.08:
                continue
            m.block((-100 - row * 210, y, -40 + row * 2), (200, 195, 80), "QuayStone", f + "/Quay", "Stone", yaw=rng.uniform(-3, 3),
                    pitch=rng.uniform(-3, 3))
    for _ in range(1400):
        x, y = rng.uniform(-14000, -500), rng.uniform(-24500, 24500)
        m.block((x, y, 3), (rng.uniform(250, 420), rng.uniform(250, 420), 6), "PavingSlab", f + "/Quay", rng.choice(["Concrete", "Stone"]),
                yaw=rng.uniform(-5, 5), pitch=rng.uniform(-2, 2), collision=False)
    for k in range(64):
        y = -25000 + k * 800
        m.block((-700, y, 50), (50, 50, 100), "Bollard", f + "/Quay", "Dark", shape="cylinder", tags=["JumpUp"])
        m.block((-700, y, 105), (70, 70, 14), "BollardCap", f + "/Quay", "Dark", shape="cylinder", collision=False)
        if rng.random() < 0.4:
            m.block((-850, y + 60, 8), (90, 90, 16), "RopeCoil", f + "/Quay", "Sand", shape="cylinder", collision=False)
    # Pier planks and posts on top of every pier deck (each deck is tagged Pier by the layout).
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for deck in [a for a in eas.get_all_level_actors() if unreal.Name("Pier") in a.tags]:
        origin, extent = deck.get_actor_bounds(False)
        x0, x1 = origin.x - extent.x, origin.x + extent.x
        x = x0 + 25
        while x < x1 - 20:
            if rng.random() > 0.06:   # a few planks missing
                m.block((x, origin.y, 3), (40, 880, 6), "Plank", f + "/PierPlanks", "Wood", yaw=rng.uniform(-1.5, 1.5), collision=False)
            x += 46
        for side in (-1, 1):
            for k in range(int((x1 - x0) // 300)):
                if rng.random() < 0.3:
                    continue
                m.block((x0 + 150 + k * 300, origin.y + side * 430, 60), (14, 14, 120), "PierPost", f + "/PierPlanks", "Wood",
                        pitch=rng.uniform(-10, 10), collision=False)
    # Shore and sea rocks, sea stacks, kelp, driftwood, washed-up crates and barrels, wrecked boats, broken lamps, nets.
    _scatter(m, rng, 1500, (-29000, -24500, -1000, 24500), lambda x, y: P.rock(m, x, y, 0, f + "/ShoreRocks", rng, s=rng.uniform(60, 260)),
             pad=0)
    for _ in range(750):
        x, y = rng.uniform(2000, 69000), rng.uniform(-24500, 24500)
        P.rock(m, x, y, -3100, f + "/SeaRocks", rng, s=rng.uniform(200, 900))
    for _ in range(30):
        x, y = rng.uniform(8000, 68000), rng.uniform(-23000, 23000)
        if abs(y) < 2000 and x > 30000:
            continue
        m.block((x, y, -1000), (rng.uniform(900, 1800), rng.uniform(900, 1800), rng.uniform(3500, 5500)), "SeaStack", f + "/SeaStacks",
                "Rock", yaw=rng.uniform(0, 90), tags=["Climbable"])
        for k in range(4):
            P.rock(m, x + rng.uniform(-1200, 1200), y + rng.uniform(-1200, 1200), -3100, f + "/SeaStacks", rng, s=rng.uniform(400, 1000))
    for _ in range(1100):
        x, y = rng.uniform(1000, 69000), rng.uniform(-24500, 24500)
        h = rng.uniform(300, 1800)
        m.block((x, y, -3000 + h / 2), (20, 50, h), "Kelp", f + "/Kelp", "Flora", yaw=rng.uniform(0, 180), roll=rng.uniform(-10, 10),
                collision=False)
    _scatter(m, rng, 300, (-14000, -24500, -300, 24500), lambda x, y: m.block((x, y, 15), (rng.uniform(150, 400), 25, 25), "Driftwood",
                                                                            f + "/Driftwood", "Wood", yaw=rng.uniform(0, 180),
                                                                            collision=False), pad=0)
    _scatter(m, rng, 150, (-14000, -24500, -900, 24500), lambda x, y: P.crate_stack(m, x, y, 0, rng.uniform(0, 90), f + "/Crates", rng), pad=0)
    _scatter(m, rng, 300, (-14000, -24500, -900, 24500), lambda x, y: P.barrel(m, x, y, 0, f + "/Barrels", rng.choice(["Rust", "Hazard"]),
                                                                             fallen=rng.random() < 0.4, yaw=rng.uniform(0, 360)), pad=0)
    for _ in range(40):
        x, y = rng.uniform(-6000, 30000), rng.uniform(-24000, 24000)
        z = 0 if x < 0 else -400
        P.boat(m, x, y, z, rng.uniform(0, 360), f + "/Wrecks", rng.choice(["Wood", "Rust"]), wrecked=True, rng=rng)
    _scatter(m, rng, 60, (-14000, -24000, -900, 24000), lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Lamps", h=700,
                                                                          head="Dark", broken=rng.random() < 0.5, rng=rng), pad=0)
    _scatter(m, rng, 40, (-14000, -24000, -900, 24000), lambda x, y: m.block((x, y, 5), (rng.uniform(300, 600), rng.uniform(200, 400), 4),
                                                                           "Net", f + "/Nets", "Dark", yaw=rng.uniform(0, 180),
                                                                           collision=False), pad=0)
    for k in range(12):   # tide pools in the rocks at the waterline
        y = -22000 + k * 4000 + rng.uniform(-800, 800)
        m.water(-3500, y - 500, -150, -2500, y + 500, -10, "TidePool", f + "/TidePools")
        m.box(-3550, y - 550, -200, -2450, y + 550, -150, "TidePoolBed", f + "/TidePools", "Rock")
    # Lighthouse detail: stone bands, windows, door, gallery rail, lamp-room mullions, roof, foundation steps.
    ix = 62000
    for k in range(28):
        m.block((ix, 0, 250 + k * 245), (1830, 1830, 20), "StoneBand", f + "/Lighthouse", "Stone", shape="cylinder", collision=False)
    for k in range(16):
        p = m.polar((ix, 0, 0), 905, k * 90 + (k // 4) * 45, 1200 + (k // 4) * 1400)
        m.block(p, (20, 120, 200), "LH_Window", f + "/Lighthouse", "Lamp", yaw=k * 90 + (k // 4) * 45, collision=False)
    m.block((ix - 905, 0, 150), (20, 160, 300), "LH_Door", f + "/Lighthouse", "Wood", collision=False)
    for k in range(48):
        p = m.polar((ix, 0, 0), 1380, k * 7.5, 7060)
        m.block(p, (10, 10, 110), "GalleryPost", f + "/Lighthouse", "Metal", collision=False)
    for k in range(24):
        a, b = m.polar((ix, 0, 0), 1380, k * 15, 7115), m.polar((ix, 0, 0), 1380, (k + 1) * 15, 7115)
        m.block(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 7115), (370, 8, 8), "GalleryRail", f + "/Lighthouse", "Metal",
                yaw=k * 15 + 97.5, collision=False)
    for k in range(16):
        p = m.polar((ix, 0, 0), 650, k * 22.5, 7600)
        m.block(p, (16, 16, 1000), "Mullion", f + "/Lighthouse", "Metal", collision=False)
    m.block((ix, 0, 8400), (1500, 1500, 600), "LH_Roof", f + "/Lighthouse", "NeonRed", shape="cone", collision=False)
    for k in range(20):
        m.block((ix, 0, 30 + k * 4), (2400 + (20 - k) * 60, 2400 + (20 - k) * 60, 4), "LH_Plinth", f + "/Lighthouse", "Stone",
                shape="cylinder", collision=False)
    for _ in range(80):   # more waves
        m.mover((rng.uniform(5000, 68000), rng.uniform(-23000, 23000), -150), (500, rng.uniform(2000, 6000), 180), "WaveCrest", f + "/Waves",
                "Foam", offset=(-3000, 0, 150), period=rng.uniform(5, 9), phase=rng.random(), collision=False, tags=["Wave"])


# ============================================================================ 4. The Hanging City

def ch04(m, rng, log):
    import greybox_neotokyo as NT
    f = "Detail"
    R = 20000
    # Ring roads and radial avenues on the disc.
    for r in (8000, 14000):
        for k in range(48):
            a, b = m.polar((0, 0, 0), r, k * 7.5, 0), m.polar((0, 0, 0), r, (k + 1) * 7.5, 0)
            _road_line(m, rng, a, b, f + "/Roads", seg=3000, dashes=500)
    for k in range(8):
        _road_line(m, rng, m.polar((0, 0, 0), 3000, k * 45, 0), m.polar((0, 0, 0), R - 800, k * 45, 0), f + "/Roads", dashes=600)
    # More buildings between the avenues, each with a climb route and rooftop clutter.
    pool = ["/Game/Greybox/Kits/K1/%s" % p for p in ["HighRise/SM_K1_HighRise_%d" % i for i in range(1, 15)]
            + ["Corporation/SM_K1_Corporation_%d" % i for i in range(1, 11)] + ["Cityscape/SM_K1_Cityscape_%d" % i for i in range(1, 5)]]
    placed = 0
    for _ in range(400):
        if placed >= 70:
            break
        r, az = rng.uniform(5000, R - 2200), rng.uniform(0, 360)
        if min(abs((az % 45) - 0), abs((az % 45) - 45)) < 7 or any(abs(r - rr) < 1300 for rr in (8000, 14000)):
            continue
        p = m.polar((0, 0, 0), r, az, 0)
        if m.blocked(p[0], p[1], 900) or math.hypot(p[0], p[1] + 11000) < 5000:
            continue
        b = m.place_kit(rng.choice(pool), p[0], p[1], 0, rng.choice([0, 90, 180, 270]), rng.uniform(0.5, 0.8), "Building", f + "/Buildings",
                        ["Building", "Climbable"])
        placed += 1
    for b in m.kits:
        if b["ground"] == 0:
            x0, y0, x1, y1 = b["rect"]
            NT._climb_route(m, rng, (x0 + x1) / 2, y0, -1, True, min(b["top"] - 100, 3600), f + "/ClimbRoutes")
        _rooftop(m, rng, b, f + "/Rooftops")
    disc = lambda: (lambda r, a: (r * math.cos(a), r * math.sin(a)))(rng.uniform(4000, R - 1200), rng.uniform(0, 2 * math.pi))
    _scatter(m, rng, 340, disc, lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Lamps"))
    _scatter(m, rng, 130, disc, lambda x, y: P.bench(m, x, y, 0, rng.uniform(0, 360), f + "/Street"))
    _scatter(m, rng, 90, disc, lambda x, y: P.car(m, x, y, 0, rng.uniform(0, 360), f + "/Cars", rng.choice(["Metal", "Grey", "Dark"])))
    _scatter(m, rng, 110, disc, lambda x, y: P.bin_(m, x, y, 0, f + "/Street"))
    _scatter(m, rng, 120, disc, lambda x, y: P.crate_stack(m, x, y, 0, rng.uniform(0, 90), f + "/Crates", rng))
    # Rim: railing posts, rails, hanging lanterns, cable anchors.
    for k in range(420):
        p = m.polar((0, 0, 0), R - 60, k * 360 / 420, 0)
        m.block((p[0], p[1], 55), (10, 10, 110), "RimPost", f + "/Rim", "Metal", collision=False)
    for k in range(64):
        a, b = m.polar((0, 0, 0), R - 60, k * 5.625, 110), m.polar((0, 0, 0), R - 60, (k + 1) * 5.625, 110)
        m.block(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 110), (2 * math.pi * (R - 60) / 64, 8, 8), "RimRail", f + "/Rim", "Metal",
                yaw=k * 5.625 + 92.8, collision=False)
    for k in range(520):
        p = m.polar((0, 0, 0), R + 100, k * 360 / 520, -1600 - rng.uniform(0, 600))
        m.block((p[0], p[1], p[2] + 300), (4, 4, 600), "LanternCord", f + "/Lanterns", "Dark", collision=False)
        m.block(p, (60, 60, 80), "Lantern", f + "/Lanterns", rng.choice(["Lamp", "NeonRed", "NeonYellow"]), shape="sphere", collision=False)
    # Underside girders, rings, pipes and lights.
    for k in range(48):
        a = m.polar((0, 0, 0), 2000, k * 7.5, -1700)
        b = m.polar((0, 0, 0), R - 500, k * 7.5, -1700)
        P.pipe(m, a, b, f + "/Underside", r=90, mat="Dark")
    for r in (6000, 11000, 16000):
        for k in range(48):
            a, b = m.polar((0, 0, 0), r, k * 7.5, -1750), m.polar((0, 0, 0), r, (k + 1) * 7.5, -1750)
            P.pipe(m, a, b, f + "/Underside", r=60, mat="Metal")
    for _ in range(140):
        p = m.polar((0, 0, 0), rng.uniform(3000, R - 1000), rng.uniform(0, 360), -1800 - rng.uniform(0, 1500))
        m.block(p, (40, 40, rng.uniform(300, 1500)), "HangingPipe", f + "/Underside", "Metal", collision=False)
        m.block((p[0], p[1], p[2] - 800), (50, 50, 50), "UndersideLight", f + "/Underside", "Lamp", shape="sphere", collision=False)
    # Chain collars every few links.
    for k in range(0, 150, 4):
        m.block((0, 0, 1200 + k * 2000), (1900, 1900, 200), "ChainCollar", f + "/Chain", "Metal", shape="cylinder", collision=False)
        for j in range(4):
            p = m.polar((0, 0, 0), 980, j * 90 + 45, 1200 + k * 2000)
            m.block(p, (120, 120, 400), "CollarBolt", f + "/Chain", "Dark", collision=False)
    # Hanging districts: crates, barrels, lamps and laundry lines.
    for k in range(4):
        c = m.polar((0, 0, 0), R + 7500, 45 + k * 90, -6000 - k * 300)
        for _ in range(40):
            p = m.polar(c, rng.uniform(1200, 5600), rng.uniform(0, 360), c[2])
            if m.blocked(p[0], p[1], 150):
                continue
            pick = rng.random()
            if pick < 0.4:
                P.crate_stack(m, p[0], p[1], c[2], rng.uniform(0, 90), f + "/Districts", rng)
            elif pick < 0.7:
                P.barrel(m, p[0], p[1], c[2], f + "/Districts", "Rust")
            else:
                P.lamp(m, p[0], p[1], c[2], rng.uniform(0, 360), f + "/Districts", h=400, head="NeonYellow")
        for j in range(20):
            a = m.polar(c, rng.uniform(1000, 4500), rng.uniform(0, 360), c[2] + 400)
            b = m.polar(c, rng.uniform(1000, 4500), rng.uniform(0, 360), c[2] + 400)
            P.wire(m, a, b, f + "/Districts/Laundry", sag=60)
            for q in range(4):
                t = (q + 1) / 5.0
                m.block((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, c[2] + 300), (80, 6, 100), "Laundry", f + "/Districts/Laundry",
                        rng.choice(["Coral", "Marking", "NeonCyan", "Sunflower"]), yaw=math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])),
                        collision=False)


# ============================================================================ 5. A.R.C TECH INCORPORATED

def ch05(m, rng, log):
    f = "Detail"
    X0, X1, Y0, Y1, H = -30000, 30000, -15000, 15000, 7000
    # Columns, roof trusses, wall ribs and panels.
    for x in range(X0 + 2500, X1, 5000):
        for y in (Y0 + 600, Y1 - 600, 0):
            if y == 0 and any(abs(x - px) < 3200 for px in (-16000, 0, 16000)):
                continue
            m.block((x, y, 40), (700, 700, 80), "ColumnBase", f + "/Structure", "Hazard")
            m.block((x, y, H / 2), (400, 400, H), "Column", f + "/Structure", "Metal")
            m.block((x, y, H - 200), (700, 700, 200), "ColumnCap", f + "/Structure", "Metal")
    for x in range(X0 + 1250, X1, 2500):
        m.block((x, 0, H - 400), (60, 30000, 60), "TrussBottom", f + "/Roof", "Metal", collision=False)
        m.block((x, 0, H - 80), (60, 30000, 60), "TrussTop", f + "/Roof", "Metal", collision=False)
        for k in range(12):
            y = Y0 + 1250 + k * 2500
            m.block((x, y, H - 240), (40, 2800, 40), "TrussDiagonal", f + "/Roof", "Metal", roll=(1 if k % 2 else -1) * 7, collision=False)
    for y in (Y0 + 110, Y1 - 110):
        for x in range(X0 + 500, X1, 1000):
            m.block((x, y, H / 2), (120, 60, H), "WallRib", f + "/Walls", "Dark", collision=False)
        for row in range(8):
            for x in range(X0 + 500, X1, 1000):
                m.block((x + 500, y, 450 + row * 820), (940, 30, 760), "WallPanel", f + "/Walls",
                        rng.choice(["Metal", "Metal", "Grey", "Hazard"]) if row == 0 else rng.choice(["Metal", "Grey"]), collision=False)
    # Overhead pipe runs with hangers, ducts and cable trays.
    for k, y in enumerate((-12500, -9800, -4800, -1500, 1500, 4800, 9800, 12500)):
        z = 5200 + (k % 3) * 300
        for x in range(X0 + 1000, X1 - 1000, 5000):
            P.pipe(m, (x, y, z), (x + 5000, y, z), f + "/Pipes", r=rng.choice([40, 60, 90]), mat=rng.choice(["Metal", "Hazard", "NeonCyan"]))
            m.block((x + 2500, y, (z + H) / 2), (12, 12, H - z), "PipeHanger", f + "/Pipes", "Dark", collision=False)
    for y in (-7000, 7000):
        for x in range(X0 + 1500, X1 - 1500, 3000):
            m.block((x, y, 6200), (3000, 900, 600), "Duct", f + "/Ducts", "Metal", collision=False)
            m.block((x, y, 5880), (600, 600, 40), "DuctVent", f + "/Ducts", "Dark", collision=False)
    for y in (Y0 + 250, Y1 - 250):
        for x in range(X0 + 1000, X1, 2500):
            m.block((x, y, 4500), (2500, 300, 20), "CableTray", f + "/Walls", "Hazard", collision=False)
            m.block((x, y, 4530), (2500, 200, 30), "Cables", f + "/Walls", "Dark", collision=False)
    # Storage racks full of glowing cube bins, tanks, consoles (no operators), feeders, floor markings.
    for k in range(32):
        x = X0 + 2500 + (k % 16) * 3500
        y = rng.choice([-13600, 13600]) if k < 16 else rng.choice([-2600, 2600])
        if k >= 16 and any(abs(x - px) < 3000 for px in (-16000, 0, 16000)):
            continue
        for dx in (-600, 600):
            for dy in (-200, 200):
                m.block((x + dx, y + dy, 600), (30, 30, 1200), "RackUpright", f + "/Racks", "Hazard")
        for lvl in range(4):
            m.block((x, y, 50 + lvl * 380), (1260, 430, 20), "RackShelf", f + "/Racks", "Metal", tags=["JumpUp"])
            for c in range(5):
                m.block((x - 500 + c * 250, y, 60 + lvl * 380 + 40), (30, 30, 30), "Cube", f + "/Racks", "CubeGlow", collision=False, tags=["Cube"])
    for k in range(30):
        P.tank(m, X1 - 3500 - (k % 15) * 1300, (-1 if k < 15 else 1) * 12300, 0, f + "/Tanks", r=500, h=rng.uniform(1500, 2800),
               mat=rng.choice(["Metal", "Grey"]))
    _scatter(m, rng, 250, (X0 + 1500, Y0 + 1500, X1 - 1500, Y1 - 1500), lambda x, y: P.console(m, x, y, 0, rng.choice([0, 90, 180, 270]),
                                                                                                f + "/Consoles", rng), pad=0)
    for k in range(10):   # cross feeders between the line pairs
        x = X0 + 4000 + k * 5500
        for y0, y1 in ((-11000, -6500), (6500, 11000)):
            m.box(x - 250, y0, 0, x + 250, y1, 90, "FeederConveyor", f + "/Feeders", "Dark", tags=["Conveyor", "JumpUp"])
            for j in range(8):
                m.block((x, y0 + (j + 0.5) * (y1 - y0) / 8, 110), (30, 30, 30), "Cube", f + "/Feeders", "CubeGlow", collision=False, tags=["Cube"])
    for y in (-12800, -8800, -4600, 4600, 8800, 12800):
        for x in range(X0 + 600, X1, 400):
            m.block((x, y, 1), (200, 60, 2), "FloorStripe", f + "/FloorMarkings", "Hazard", yaw=45, collision=False)
    # Autonomous machines: forklifts, drones, gantry cranes (no visible workers anywhere).
    for k in range(24):
        x, y = rng.uniform(X0 + 2000, X1 - 2000), rng.choice([-13000, -4000, 4000, 13000])
        m.mover((x, y, 110), (260, 140, 220), "AutoForklift", f + "/Autonomous", "Hazard", offset=(rng.choice([-1, 1]) * 4000, 0, 0),
                period=rng.uniform(8, 14), phase=rng.random(), tags=["AutoForklift"])
    for k in range(80):
        x, y = rng.uniform(X0 + 1000, X1 - 1000), rng.uniform(Y0 + 1000, Y1 - 1000)
        m.mover((x, y, rng.uniform(2500, 5000)), (90, 90, 30), "Drone", f + "/Autonomous", "NeonYellow",
                offset=(rng.uniform(-3000, 3000), rng.uniform(-2000, 2000), rng.uniform(-300, 300)), period=rng.uniform(6, 12),
                phase=rng.random(), collision=False, tags=["Drone"])
    for k in range(4):
        x = X0 + 8000 + k * 14000
        m.mover((x, 0, 5600), (600, 29000, 300), "GantryCrane", f + "/Autonomous", "Hazard", offset=(rng.choice([-1, 1]) * 4000, 0, 0),
                period=20, phase=k * 0.25, collision=False, tags=["Gantry"])
    for y in (Y0 + 300, Y1 - 300):
        m.box(X0, y - 100, 5500, X1, y + 100, 5600, "GantryRail", f + "/Autonomous", "Metal")
    # Static cubes riding the belts between the moving ones, catwalk grating and rails, column safety lights, risers.
    for y in (-11000, -6500, 6500, 11000):
        for x in range(X0 + 2600, X1 - 2100, 450):
            m.block((x, y + rng.uniform(-150, 150), 110), (30, 30, 30), "Cube", f + "/Belts", "CubeGlow", collision=False, tags=["Cube"])
    for y in (-8800, 8800):
        for x in range(X0 + 2250, X1 - 2000, 500):
            m.block((x, y, 3560), (480, 580, 8), "Grating", f + "/Catwalks", "Dark", collision=False)
            for s2 in (-1, 1):
                m.block((x, y + s2 * 290, 3610), (8, 8, 110), "CatwalkPost", f + "/Catwalks", "Hazard", collision=False)
    for x in range(X0 + 2500, X1, 5000):
        for y in (Y0 + 600, Y1 - 600):
            m.block((x, y + (350 if y < 0 else -350), 3000), (60, 30, 60), "SafetyLight", f + "/Structure", "NeonYellow", collision=False)
            m.block((x, y + (350 if y < 0 else -350), 2600), (60, 30, 60), "SafetyLight", f + "/Structure", "NeonRed", collision=False)
    for k in range(60):
        x, y = X0 + 1000 + k * 1000, rng.choice([Y0 + 400, Y1 - 400])
        m.block((x, y, 2900), (80, 80, 5800), "Riser", f + "/Pipes", rng.choice(["Metal", "Hazard", "NeonCyan"]), shape="cylinder", collision=False)
        m.block((x, y, 2900), (140, 140, 60), "RiserFlange", f + "/Pipes", "Dark", shape="cylinder", collision=False)
    _scatter(m, rng, 220, (X0 + 1000, Y0 + 1000, X1 - 1000, Y1 - 1000), lambda x, y: m.block((x, y, 45), (16, 16, 90), "Bollard",
                                                                                            f + "/FloorMarkings", "Hazard", shape="cylinder"), pad=0)
    for _ in range(12):   # more finished-cube pallets
        x, y = rng.uniform(X0 + 2000, X1 - 2000), rng.choice([-14000, 14000])
        m.block((x, y, 60), (1200, 1000, 120), "Pallet", f + "/Pallets", "Wood", tags=["JumpUp"])
        for j in range(40):
            m.block((x + rng.uniform(-500, 500), y + rng.uniform(-400, 400), 140 + (j // 20) * 32), (30, 30, 30), "Cube", f + "/Pallets",
                    "CubeGlow", collision=False, tags=["Cube"])


# ============================================================================ 6. The Moon Kingdom

def ch06(m, rng, log):
    f = "Detail"
    HX, HY = 40000, 30000
    m.keepout += [(-9200, -9200, 9200, 9200)]
    for k in range(-6, 7):
        _road_line(m, rng, (k * 6000, -HY), (k * 6000, HY), f + "/Streets", mat="Stone", broken=0.4, dashes=0, seg=2000)
    for k in range(-4, 5):
        _road_line(m, rng, (-HX, k * 6000), (HX, k * 6000), f + "/Streets", mat="Stone", broken=0.4, dashes=0, seg=2000)
    for _ in range(800):
        m.block((rng.uniform(-HX, HX), rng.uniform(-HY, HY), 10), (rng.uniform(200, 900), rng.uniform(200, 900), 20), "Moss",
                f + "/Moss", "Moss", yaw=rng.uniform(0, 180), collision=False)
    pool = ["/Game/Greybox/Kits/K1/%s" % p for p in ["Cottage/SM_K1_Cottage_%d" % i for i in range(1, 21)]
            + ["Cityscape/SM_K1_Cityscape_%d" % i for i in range(1, 5)] + ["HighRise/SM_K1_HighRise_%d" % i for i in range(1, 9)]]
    from greybox_chapters import _overgrow
    import greybox_neotokyo as NT
    placed = 0
    for _ in range(600):
        if placed >= 95:
            break
        x, y = rng.uniform(-HX + 2500, HX - 2500), rng.uniform(-HY + 2500, HY - 2500)
        if math.hypot(x, y) < 11000 or _near_section(m, x, y, 1500) or m.blocked(x, y, 1200):
            continue
        if abs(x % 6000) < 900 or abs(y % 6000) < 900:
            continue
        b = m.place_kit(rng.choice(pool), x, y, 0, rng.choice([0, 90, 180, 270]), rng.uniform(0.8, 1.1), "Building", f + "/Town",
                        ["Building", "Climbable"])
        _overgrow(m, rng, b, f + "/Flora")
        if rng.random() < 0.4:
            x0, y0, x1, y1 = b["rect"]
            NT._climb_route(m, rng, (x0 + x1) / 2, y0, -1, True, min(b["top"] - 100, 2400), f + "/ClimbRoutes")
        placed += 1
    # A palace dome and a civic spire: the second arc's landmarks (PROPOSED reading of "all the structures you'd expect").
    m.dome((24000, -18000, 0), 6000, f + "/Palace", n_az=32, n_el=8, material="Stone", gap=lambda az, band: band == 0 and 80 < az < 100)
    m.keepout.append((18000, -24000, 30000, -12000))
    m.place_kit("/Game/Greybox/Kits/K1/Skyscraper/SM_K1_Skyscraper_2", -24000, -20000, 0, 0, 1.0, "CivicSpire", f + "/Civic",
                ["Building", "Climbable"])
    area = (-HX + 800, -HY + 800, HX - 800, HY - 800)
    _scatter(m, rng, 300, area, lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Lamps", head="Dark", broken=rng.random() < 0.2,
                                                    rng=rng))
    _scatter(m, rng, 150, area, lambda x, y: P.tree(m, x, y, 0, f + "/Trees", rng, h=rng.uniform(900, 1800), crown="Flora"))
    _scatter(m, rng, 300, area, lambda x, y: m.block((x, y, 30), (rng.uniform(300, 900), 50, 60), "Root", f + "/Roots", "Trunk",
                                                     yaw=rng.uniform(0, 180), pitch=rng.uniform(-10, 10), collision=False))
    _scatter(m, rng, 550, area, lambda x, y: P.bush(m, x, y, 0, f + "/Bushes", rng, mat=rng.choice(["Hedge", "Flora", "Moss"])), pad=50)
    _scatter(m, rng, 560, area, lambda x, y: P.flower_patch(m, x, y, 0, f + "/Flowers", rng, ["PastelLilac", "PastelBlue", "Marking",
                                                                                                 "PastelPink"]), pad=50)
    _scatter(m, rng, 150, area, lambda x, y: P.bench(m, x, y, 0, rng.uniform(0, 360), f + "/Benches"))
    _scatter(m, rng, 50, area, lambda x, y: P.stall(m, x, y, 0, rng.uniform(0, 360), f + "/Market", rng, rng.choice(["Flora", "Moss", "Grey"])))
    _scatter(m, rng, 80, area, lambda x, y: (P.car(m, x, y, 0, rng.uniform(0, 360), f + "/Cars", "Grey", wrecked=True, rng=rng),
                                            m.block((x, y, 160), (450, 200, 60), "CarMoss", f + "/Cars", "Moss", collision=False)))
    for k in range(6):
        p = (rng.uniform(-30000, 30000), rng.uniform(-22000, 22000))
        if m.blocked(p[0], p[1], 800):
            continue
        m.block((p[0], p[1], 60), (1400, 1400, 120), "FountainBasin", f + "/Fountains", "Stone", shape="cylinder", tags=["JumpUp"])
        m.block((p[0], p[1], 300), (200, 200, 500), "FountainColumn", f + "/Fountains", "Stone", shape="cylinder")
        m.block((p[0], p[1], 140), (1200, 1200, 20), "FountainMoss", f + "/Fountains", "Moss", shape="cylinder", collision=False)


# ============================================================================ 7. T.O.M.B.S

def ch07(m, rng, log):
    f = "Detail"
    W = 30000
    OX0, OX1, OY, FLOOR = -W + 2000, -W + 26000, 12000, 450
    hole = (OX0 + 9000, OX0 + 15000)
    for fl in range(0, 11):
        z = fl * FLOOR
        ff = "%s/Floor_%02d" % (f, fl)
        for x in range(OX0 + 2000, OX1, 4000):
            for y in (-9000, -3000, 3000, 9000):
                if hole[0] - 300 < x < hole[1] + 300 and abs(y) < 3300:
                    continue
                if fl < 10:
                    m.block((x, y, z + FLOOR / 2 - 15), (80, 80, FLOOR - 30), "OfficeColumn", ff, "Concrete")
        if fl > 0:
            for x in range(OX0 + 1000, OX1, 2000):
                for y in range(-OY + 1000, OY, 2000):
                    if hole[0] < x < hole[1] and abs(y) < 3000:
                        continue
                    m.block((x, y, z - 40), (600, 120, 10), "CeilingLight", ff, "Lamp", collision=False)
        if fl == 10:
            continue
        section_floor = fl in (3, 6, 9)
        n_desks = 14 if section_floor else 62
        _scatter(m, rng, n_desks, (OX0 + 600, -OY + 600, OX1 - 600, OY - 600),
                 lambda x, y: None if (hole[0] - 300 < x < hole[1] + 300 and abs(y) < 3300) else
                 P.desk_set(m, x, y, z, rng.choice([0, 90, 180, 270]), ff + "/Desks", rng), pad=0)
        _scatter(m, rng, 22, (OX0 + 300, -OY + 300, OX1 - 300, OY - 300),
                 lambda x, y: P.cabinet(m, x, y, z, rng.choice([0, 90, 180, 270]), ff + "/Cabinets"), pad=0)
        _scatter(m, rng, 14, (OX0 + 300, -OY + 300, OX1 - 300, OY - 300), lambda x, y: P.plant(m, x, y, z, ff + "/Plants", rng), pad=0)
        _scatter(m, rng, 30, (OX0 + 300, -OY + 300, OX1 - 300, OY - 300),
                 lambda x, y: m.block((x, y, z + 80), (300, 8, 160), "Whiteboard", ff + "/Boards", "Marking", yaw=rng.choice([0, 90])), pad=0)
        for k in range(4):
            x, y = OX0 + 400 + k * 6000, rng.choice([-OY + 300, OY - 300])
            m.block((x, y, z + 70), (40, 40, 140), "WaterCooler", ff, "Marking", shape="cylinder")
        if not section_floor:
            for k in range(2):   # glass meeting rooms with a table and chairs
                cx, cy = OX0 + 18000 + k * 4500, (-1) ** k * 9000
                for sx, sy, w, d in ((0, -800, 2000, 20), (0, 800, 2000, 20), (-1000, 0, 20, 1600), (1000, 0, 20, 1600)):
                    m.block((cx + sx, cy + sy, z + 150), (w, d, 300), "MeetingGlass", ff + "/Meeting", "Glass")
                m.block((cx, cy, z + 75), (1000, 400, 10), "MeetingTable", ff + "/Meeting", "Wood", tags=["JumpUp"])
                for j in range(6):
                    m.block((cx - 400 + (j % 3) * 400, cy + (-1) ** (j // 3) * 330, z + 45), (50, 50, 90), "MeetingChair", ff + "/Meeting", "Dark")
    # Lobby turnstiles and the exterior: facade windows, fins, plaza columns, lamps, benches, planters.
    for k in range(10):
        m.block((OX0 + 4000, -2250 + k * 500, 55), (300, 30, 110), "Turnstile", f + "/Lobby", "Metal")
    import random as _r
    window_rng = _r.Random(77)
    P.window_grid(m, (-W - 1000, -W, 2000), 90, 2 * W, 20000, f + "/Facade", window_rng, 60, 24, mat="Glass", lit="Lamp", out=10,
                  normal_yaw=180)
    for k in range(61):
        y = -W + k * 1000
        if abs(y) < 1500:
            continue
        m.block((-W - 1100, y, 40000), (200, 120, 80000), "FacadeFin", f + "/Facade", "Dark", collision=False)
    for k in range(40):
        x, y = -W - 3000 - (k // 8) * 2500, -9000 + (k % 8) * 2600
        if abs(y) < 1500:
            continue
        m.block((x, y, 600), (300, 300, 1200), "PlazaPillar", f + "/Plaza", "Concrete", shape="cylinder")
        m.block((x, y, 1250), (420, 420, 100), "PillarCap", f + "/Plaza", "Concrete", shape="cylinder")
    _scatter(m, rng, 60, (-W - 16000, -20000, -W - 1500, 20000), lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Plaza"), pad=0)
    _scatter(m, rng, 40, (-W - 16000, -20000, -W - 1500, 20000), lambda x, y: P.bench(m, x, y, 0, rng.uniform(0, 360), f + "/Plaza"), pad=0)
    _scatter(m, rng, 60, (-W - 16000, -20000, -W - 1500, 20000), lambda x, y: P.plant(m, x, y, 0, f + "/Plaza", rng), pad=0)


# ============================================================================ 8. Intersection

def ch08(m, rng, log):
    f = "Detail"
    L, HW, HT, cx = 150000, 3000, 2600, 70000
    for x in range(500, L, 1000):   # tunnel ribs
        if abs(x - cx) < HW + 200:
            continue
        for s in (-1, 1):
            m.block((x, s * (HW - 60), HT / 2), (150, 120, HT), "Rib", f + "/Ribs", "Concrete", collision=False)
        m.block((x, 0, HT - 60), (150, HW * 2, 120), "RibTop", f + "/Ribs", "Concrete", collision=False)
    for y in list(range(-39500, -HW, 1000)) + list(range(HW + 500, 40000, 1000)):
        for s in (-1, 1):
            m.block((cx + s * (HW - 60), y, HT / 2), (120, 150, HT), "Rib", f + "/Ribs", "Concrete", collision=False)
        m.block((cx, y, HT - 60), (HW * 2, 150, 120), "RibTop", f + "/Ribs", "Concrete", collision=False)
    # Tracks: rails on ties down both tunnels (the angels' transport path).
    for x in range(0, L, 160):
        if 92000 < x < 104000:
            continue
        m.block((x, 0, 6), (40, 1400, 12), "Tie", f + "/Tracks", "Wood", collision=False)
    for x0 in range(0, L, 5000):
        if 92000 <= x0 < 104000:
            continue
        for y in (-500, 500):
            m.block((x0 + 2500, y, 22), (5000, 20, 20), "Rail", f + "/Tracks", "Metal", collision=False)
    for y in range(-40000, 40000, 160):
        if abs(y) < HW:
            continue
        m.block((cx, y, 6), (1400, 40, 12), "Tie", f + "/Tracks", "Wood", collision=False)
    # Cable trays, emergency lights, signs, drains, side doors, ceiling pipes and fans.
    for x in range(0, L, 2500):
        for s in (-1, 1):
            if abs(x - cx) < HW + 1300:
                continue
            m.block((x + 1250, s * (HW - 150), 1900), (2500, 200, 20), "CableTray", f + "/Walls", "Hazard", collision=False)
            m.block((x + 1250, s * (HW - 150), 1925), (2500, 150, 30), "Cables", f + "/Walls", "Dark", collision=False)
    for x in range(250, L, 500):   # wall panel seams
        if abs(x - cx) < HW + 200:
            continue
        for s in (-1, 1):
            m.block((x, s * (HW - 8), 1100), (12, 6, 2200), "PanelSeam", f + "/Walls", "Dark", collision=False)
    for x in range(700, L, 1500):
        for s in (-1, 1):
            m.block((x, s * (HW - 20), 800), (60, 20, 40), "EmergencyLight", f + "/Walls", rng.choice(["NeonRed", "NeonGreen"]), collision=False)
    for x in range(3000, L, 5000):
        for s in (-1, 1):
            m.block((x, s * (HW - 25), 1500), (400, 10, 150), "Sign", f + "/Walls", rng.choice(["NeonGreen", "Marking", "Hazard"]),
                    collision=False)
    for x in range(1000, L, 2000):
        m.block((x, rng.choice([-1800, 1800]), 2), (300, 150, 4), "Drain", f + "/Floor", "Dark", collision=False)
    for k in range(40):
        x, s = rng.uniform(5000, L - 5000), rng.choice([-1, 1])
        if abs(x - cx) < HW + 1500:
            continue
        m.block((x, s * (HW + 10), 120 + 110), (250, 20, 230), "SideDoor", f + "/Walls", "Metal", collision=False)
        m.block((x, s * (HW - 10), 380), (120, 10, 40), "DoorSign", f + "/Walls", "NeonGreen", collision=False)
    for y in (-1600, -800, 800, 1600):
        for x in range(0, L, 5000):
            P.pipe(m, (x, y, HT - 300 - abs(y) / 20), (x + 5000, y, HT - 300 - abs(y) / 20), f + "/CeilingPipes", r=35,
                   mat=rng.choice(["Metal", "Rust"]))
            m.block((x + 2500, y, HT - 180), (10, 10, 160), "Hanger", f + "/CeilingPipes", "Dark", collision=False)
    for x in range(4600, L, 5000):
        m.mover((x, 0, HT + 100), (600, 60, 20), "GrateFan", f + "/Fans", "Dark", rotation=(0, 180, 0), collision=False)
    # Abandoned vehicles half under water, debris, floating debris, puddles.
    for _ in range(80):
        x = rng.uniform(4000, L - 2000)
        if 92000 < x < 104000:
            continue
        P.car(m, x, rng.uniform(-2000, 2000), 0, rng.uniform(-20, 20), f + "/Vehicles", rng.choice(["Rust", "Grey", "Dark"]), wrecked=True, rng=rng)
    for _ in range(300):
        x = rng.uniform(1000, L - 1000)
        if 92000 < x < 104000:
            continue
        P.rubble_pile(m, x, rng.uniform(-2600, 2600), 0, f + "/Debris", rng, mat=rng.choice(["Concrete", "Rubble"]), n=rng.randint(2, 5))
    for _ in range(220):
        m.block((rng.uniform(80000, 125000), rng.uniform(-2600, 2600), rng.choice([120, 400])), (rng.uniform(60, 250), rng.uniform(40, 120), 20),
                "FloatingDebris", f + "/Debris", rng.choice(["Wood", "Dark", "Marking"]), yaw=rng.uniform(0, 180), collision=False)
    for _ in range(300):
        m.block((rng.uniform(0, 20000), rng.uniform(-2800, 2800), 1), (rng.uniform(150, 600), rng.uniform(100, 400), 2), "Puddle",
                f + "/Floor", "Water", collision=False)
    # Angels everywhere: standing in rows on the tracks, gliding down both tunnels.
    for _ in range(950):
        if rng.random() < 0.75:
            x, y = rng.uniform(2000, L - 1000), rng.uniform(-HW + 500, HW - 500)
            if 92000 < x < 104000:
                continue
        else:
            x, y = cx + rng.uniform(-HW + 500, HW - 500), rng.choice([-1, 1]) * rng.uniform(HW + 500, 39000)
        moving = None
        if rng.random() < 0.35:
            along = (rng.choice([-1, 1]) * 9000, 0, 0) if abs(y) < HW else (0, rng.choice([-1, 1]) * 9000, 0)
            moving = (along, rng.uniform(6, 14), rng.random())
        P.angel(m, x, y, rng.uniform(0, 1400) if moving else 0, 0 if abs(y) < HW else 90, f + "/Angels", moving)


# ============================================================================ 9. Remnant Remains

def ch09(m, rng, log):
    f = "Detail"
    half = 45000
    m.keepout += [(-13500, -13500, 13500, 13500)]
    import random as _r
    wr = _r.Random(9)
    # Data centre facade panels and vents on every outer face and the tier.
    for (origin, yaw, width, height, normal) in (((7000, -7000, 300), 90, 14000, 8700, 0), ((-7000, 7000, 300), 0, 14000, 8700, 90),
                                                ((7000, -7000, 300), 180, 14000, 8700, 270), ((-7000, -7000, 2500), 90, 14000, 6500, 180),
                                                ((4000, -4000, 9000), 90, 8000, 6000, 0), ((-4000, 4000, 9000), 0, 8000, 6000, 90),
                                                ((4000, -4000, 9000), 180, 8000, 6000, 270), ((-4000, -4000, 9000), 90, 8000, 6000, 180)):
        cols, rows = int(width // 500), int(height // 500)
        P.window_grid(m, origin, yaw, width, height, f + "/Facade", wr, cols, rows, mat="Metal", lit="Screen", out=15, normal_yaw=normal)
    for _ in range(130):
        P.ac_unit(m, rng.uniform(-6600, 6600), rng.uniform(-6600, 6600), 9000, rng.choice([0, 90]), f + "/Roof") if rng.random() < 0.7 else \
            P.ac_unit(m, rng.uniform(-3600, 3600), rng.uniform(-3600, 3600), 15000, rng.choice([0, 90]), f + "/Roof")
    for _ in range(40):
        P.antenna(m, rng.uniform(-3600, 3600), rng.uniform(-3600, 3600), 15000, f + "/Antennas", h=rng.uniform(600, 2000))
    for k in range(4):   # cooling tower fins
        c = m.polar((0, 0, 0), 11500, k * 90 + 45, 0)
        for j in range(20):
            m.block((c[0], c[1], 300 + j * 290), (3560, 3560, 30), "TowerRing", f + "/CoolingTowers", "Metal", shape="cylinder", collision=False)
    # Perimeter fence (posts and mesh), floodlights.
    for k in range(330):
        a = k * 360 / 330
        p = m.polar((0, 0, 0), 14500, a, 0)
        if 170 < a < 190:
            continue   # the gate faces the dunes
        m.block((p[0], p[1], 150), (10, 10, 300), "FencePost", f + "/Fence", "Metal", pitch=rng.uniform(-8, 8))
        q = m.polar((0, 0, 0), 14500, a + 360 / 660, 150)
        m.block(q, (6, 270, 260), "FenceMesh", f + "/Fence", "Dark", yaw=a, collision=False)
        m.block((q[0], q[1], 300), (8, 280, 8), "FenceRail", f + "/Fence", "Metal", yaw=a, collision=False)
    for k in range(40):
        p = m.polar((0, 0, 0), 13800, k * 9, 0)
        P.lamp(m, p[0], p[1], 0, k * 9 + 180, f + "/Floodlights", h=1200)
    # Pylons along the cables, half-buried server racks, rocks, puddles, wind-blown sheets.
    for k in range(8):
        az = k * 45 + 10
        prev = None
        for j in range(8):
            p = m.polar((0, 0, 0), 15000 + j * 3800, az + 3, 0)
            P.power_pole(m, p[0], p[1], 0, f + "/Pylons", h=1400, mat="Metal")
            top = (p[0], p[1], 1340)
            if prev:
                P.wire(m, prev, top, f + "/Pylons", sag=200)
            prev = top
    area = (-half, -half, half, half)
    _scatter(m, rng, 450, area, lambda x, y: P.server_rack(m, x, y, -rng.uniform(40, 140), rng.uniform(0, 360), f + "/BuriedRacks", rng),
             pad=0)
    _scatter(m, rng, 1400, area, lambda x, y: P.rock(m, x, y, -40, f + "/Rocks", rng, mat=rng.choice(["Rock", "Sand"])), pad=0)
    _scatter(m, rng, 110, area, lambda x, y: m.block((x, y, -rng.uniform(300, 800)), (rng.uniform(4000, 9000), rng.uniform(2500, 6000),
                                                                                     rng.uniform(1200, 2400)), "Dune", f + "/Dunes", "Sand",
                                                     shape="sphere", yaw=rng.uniform(0, 180)), pad=0)
    _scatter(m, rng, 300, area, lambda x, y: m.block((x, y, 1), (rng.uniform(200, 700), rng.uniform(200, 500), 2), "Puddle", f + "/Puddles",
                                                     "Water", collision=False), pad=0)
    _scatter(m, rng, 500, area, lambda x, y: m.block((x, y, rng.uniform(10, 120)), (rng.uniform(80, 200), rng.uniform(60, 150), 3), "Sheet",
                                                     f + "/WindDebris", rng.choice(["Marking", "Metal", "Grey"]), yaw=rng.uniform(0, 360),
                                                     pitch=rng.uniform(-40, 40), collision=False), pad=0)
    # Server hall: racks along both walls (the boss section in the middle stays clear).
    for y in (-4600, 4600):
        for x in range(-6500, 4600, 600):
            P.server_rack(m, x, y, 320, 0 if y < 0 else 180, f + "/Hall", rng)


# ============================================================================ 10. Hallucinations

def ch10(m, rng, log):
    f = "Detail"
    pastel = ["PastelPink", "PastelBlue", "PastelMint", "PastelLemon", "PastelLilac"]
    islands = [(-30000, 0, 0), (-8000, 14000, 2500), (14000, -6000, 5000), (32000, 10000, 7500)]
    for n, c in enumerate(islands):   # checker tiles
        for i in range(-10, 11):
            for j in range(-10, 11):
                x, y = c[0] + i * 500, c[1] + j * 500
                if math.hypot(x - c[0], y - c[1]) > 5250:
                    continue
                m.block((x, y, c[2] + 3), (490, 490, 6), "CheckerTile", f + "/Checker", pastel[(i + j + n) % 2 * 2 + n % 2], collision=False)
    sky = (-50000, -40000, 50000, 40000)
    rnd = lambda: (rng.uniform(sky[0], sky[2]), rng.uniform(sky[1], sky[3]), rng.uniform(-1000, 16000))
    for _ in range(250):   # small islets
        x, y, z = rnd()
        r = rng.uniform(300, 1200)
        m.block((x, y, z), (r * 2, r * 2, 300), "Islet", f + "/Islets", rng.choice(pastel), shape="cylinder", tags=["JumpUp"])
        m.block((x, y, z - 500), (r * 1.4, r * 1.4, 700), "IsletRoot", f + "/Islets", rng.choice(pastel), shape="cone", pitch=180, collision=False)
        m.block((x, y, z + 250), (120, 120, 200), "IsletFlower", f + "/Islets", rng.choice(pastel), shape="sphere", collision=False)
    for _ in range(420):   # floating everyday objects
        x, y, z = rnd()
        kind = rng.randint(0, 5)
        tilt = dict(yaw=rng.uniform(0, 360), pitch=rng.uniform(-60, 60), roll=rng.uniform(-60, 60))
        col = rng.choice(pastel)
        if kind == 0:
            for k in range(3):
                m.block((x, y, z + k * 45), (220, 160, 40), "FloatingBook", f + "/Objects", rng.choice(pastel), collision=False, **tilt)
        elif kind == 1:
            m.block((x, y, z), (20, 120, 240), "FloatingDoor", f + "/Objects", col, collision=False, **tilt)
            m.block((x, y, z), (30, 140, 260), "DoorFrame", f + "/Objects", "Marking", collision=False, **tilt)
            m.block((x, y, z), (40, 10, 10), "DoorKnob", f + "/Objects", "Lamp", collision=False, **tilt)
        elif kind == 2:
            m.block((x, y, z), (300, 40, 300), "FloatingClock", f + "/Objects", col, shape="cylinder", collision=False, **tilt)
            m.block((x, y, z), (10, 50, 120), "ClockHand", f + "/Objects", "Dark", collision=False, **tilt)
        elif kind == 3:
            m.block((x, y, z), (300, 300, 200), "FloatingTeacup", f + "/Objects", col, shape="cylinder", collision=False, **tilt)
            m.block((x, y, z - 100), (500, 500, 20), "Saucer", f + "/Objects", "Marking", shape="cylinder", collision=False, **tilt)
        elif kind == 4:
            for k in range(3):
                m.block((x + k * 60, y, z), (100, 100, 100), "FloatingChairPart", f + "/Objects", col, collision=False, **tilt)
        else:
            m.block((x, y, z), (400, 400, 150), "FloatingUmbrella", f + "/Objects", col, shape="cone", collision=False, **tilt)
            m.block((x, y, z - 200), (10, 10, 400), "UmbrellaStick", f + "/Objects", "Dark", collision=False, **tilt)
    for _ in range(380):   # clouds
        x, y, z = rng.uniform(-60000, 60000), rng.uniform(-50000, 50000), rng.uniform(12000, 22000)
        for k in range(4):
            r = rng.uniform(600, 1500)
            m.block((x + rng.uniform(-1200, 1200), y + rng.uniform(-800, 800), z + rng.uniform(-300, 300)), (r * 2, r * 1.6, r * 1.2), "Cloud",
                    f + "/Clouds", "Cloud", shape="sphere", collision=False)
    for _ in range(200):   # giant flowers
        x, y, z = rnd()
        h = rng.uniform(600, 1600)
        m.block((x, y, z + h / 2), (40, 40, h), "FlowerStem", f + "/GiantFlowers", "Lime", collision=False)
        for k in range(2):
            m.block((x, y, z + h + k * 20), (600 - k * 200, 600 - k * 200, 60), "Petals", f + "/GiantFlowers", rng.choice(pastel),
                    shape="cylinder", pitch=rng.uniform(-20, 20), collision=False)
        m.block((x, y, z + h + 60), (160, 160, 100), "FlowerHeart", f + "/GiantFlowers", "PastelLemon", shape="sphere", collision=False)
    for k in range(12):   # rainbow arches
        c = (rng.uniform(-40000, 40000), rng.uniform(-30000, 30000), rng.uniform(0, 6000))
        yaw = rng.uniform(0, 180)
        for j in range(24):
            a = math.radians(j * 7.5)
            r = 6000
            px, py = P._off(c[0], c[1], yaw, r * math.cos(a), 0)
            for band in range(3):
                m.block((px, py, c[2] + (r - band * 200) * math.sin(a)), (900, 200, 180), "RainbowBand", f + "/Rainbows", pastel[band],
                        yaw=yaw, pitch=-(j * 7.5 - 90), collision=False)
    for _ in range(40):
        x, y, z = rnd()
        m.stairs((x, y, z), rng.uniform(0, 360), 12, 80, 120, 400, f + "/ImpossibleStairs", rng.choice(pastel))
    for _ in range(300):   # balloons, some drifting
        x, y, z = rnd()
        col = rng.choice(pastel + ["Coral", "Turquoise"])
        if rng.random() < 0.3:
            m.mover((x, y, z), (200, 200, 260), "Balloon", f + "/Balloons", col, shape="sphere", offset=(0, 0, rng.uniform(300, 900)),
                    period=rng.uniform(5, 10), phase=rng.random(), collision=False)
        else:
            m.block((x, y, z), (200, 200, 260), "Balloon", f + "/Balloons", col, shape="sphere", collision=False)
        m.block((x, y, z - 350), (4, 4, 450), "BalloonString", f + "/Balloons", "Dark", collision=False)
    for _ in range(100):
        x, y, z = rnd()
        m.block((x, y, z), (20, 400, 300), "PictureFrame", f + "/Objects", "PastelLemon", yaw=rng.uniform(0, 360), collision=False)
        m.block((x, y, z), (24, 340, 240), "Picture", f + "/Objects", rng.choice(["Horror", "PastelBlue", "PastelPink"]),
                yaw=rng.uniform(0, 360), collision=False)
    for n, c in enumerate(islands):   # lollipop trees on the islands
        for _ in range(70):
            p = m.polar(c, rng.uniform(1000, 5000), rng.uniform(0, 360), c[2])
            h = rng.uniform(300, 700)
            m.block((p[0], p[1], p[2] + h / 2), (30, 30, h), "LollipopStick", f + "/Trees", "Marking", collision=False)
            m.block((p[0], p[1], p[2] + h + 120), (300, 300, 300), "LollipopTop", f + "/Trees", rng.choice(pastel), shape="sphere",
                    collision=False)
    for k in range(30):
        c = islands[k % 4]
        p = m.polar(c, rng.uniform(1500, 5000), rng.uniform(0, 360), c[2])
        m.block((p[0], p[1], p[2] + 300), (160, 120, 600), "HORROR_APPARITION_PLACEHOLDER", f + "/Horror", "Horror", collision=False,
                tags=["HorrorApparitionPlaceholder"])


# ============================================================================ 11. Return to Nowhere

def ch11(m, rng, log):
    """Ten denser grey fragments of the earlier maps along the path. One flat grey (force_material), no light colour, no
    life, nothing happens: the walk stays empty, the pasted pieces are what fill it."""
    m.force_material = "PureGrey"
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = eas.get_all_level_actors()
    legs = [a for a in actors if unreal.Name("NowherePath") in a.tags]
    marks = {}
    for a in actors:   # the layout tags one piece of each pasted map Fragment_01..10; match each leg to its fragment
        for t in a.tags:
            if str(t).startswith("Fragment_"):
                marks.setdefault(int(str(t)[9:]) - 1, a.get_actor_location())
    f = "Detail/Fragments"
    for leg in legs:
        o, e = leg.get_actor_bounds(False)
        yaw = leg.get_actor_rotation().yaw
        k = min(marks, key=lambda i: math.hypot(marks[i].x - o.x, marks[i].y - o.y)) if marks else 0
        ff = "%s/%02d" % (f, k + 1)
        for side in (-1, 1):   # path kerbs
            for j in range(-14, 15):
                p = P._off(o.x, o.y, yaw, j * 500, side * 720)
                m.block((p[0], p[1], 15), (480, 40, 30), "Kerb", ff, "PureGrey", yaw=yaw, collision=False)
        spot = lambda: P._off(o.x, o.y, yaw, rng.uniform(-6500, 6500), rng.choice([-1, 1]) * rng.uniform(1200, 6000))
        frag = k % 10
        if frag == 0:
            for _ in range(20):
                x, y = spot()
                b = m.place_kit("/Game/Greybox/Kits/K1/HighRise/SM_K1_HighRise_%d" % rng.randint(1, 14), x, y, 0, rng.choice([0, 90]), 0.5,
                                "Tower", ff, ["NowhereFragment"], material="PureGrey")
                _rooftop(m, rng, b, ff)
            for _ in range(60):
                x, y = spot()
                P.car(m, x, y, 0, yaw, ff)
            for _ in range(30):
                x, y = spot()
                P.traffic_light(m, x, y, 0, rng.uniform(0, 360), ff)
        elif frag == 1:
            for _ in range(170):
                x, y = spot()
                P.rubble_pile(m, x, y, 0, ff, rng)
            for _ in range(40):
                x, y = spot()
                P.car(m, x, y, 0, rng.uniform(0, 360), ff, wrecked=True, rng=rng)
            for _ in range(40):
                x, y = spot()
                P.rebar(m, x, y, 0, ff, rng)
        elif frag == 2:
            for j in range(300):
                p = P._off(o.x, o.y, yaw + 90, -6000 + j * 46, 3000)
                m.block((p[0], p[1], 3), (40, 880, 6), "Plank", ff, "PureGrey", yaw=yaw + 90, collision=False)
            for _ in range(320):
                x, y = spot()
                P.rock(m, x, y, 0, ff, rng)
            for _ in range(40):
                x, y = spot()
                P.crate_stack(m, x, y, 0, rng.uniform(0, 90), ff, rng)
        elif frag == 3:
            for _ in range(6):
                x, y = spot()
                for j in range(40):
                    m.block((x, y, j * 2000 + 1000), (260, 1400 if j % 2 else 260, 2400), "ChainLink", ff, "PureGrey")
            for j in range(200):
                p = P._off(o.x, o.y, yaw, -6000 + j * 60, 900)
                m.block((p[0], p[1], 55), (10, 10, 110), "RimPost", ff, "PureGrey", collision=False)
            for _ in range(380):
                x, y = spot()
                m.block((x, y, rng.uniform(200, 3000)), (60, 60, 80), "Lantern", ff, "PureGrey", shape="sphere", collision=False)
        elif frag == 4:
            for j in range(4):
                p = P._off(o.x, o.y, yaw, 0, -4000 + j * 2500)
                m.block((p[0], p[1], 45), (12000, 600, 90), "Conveyor", ff, "PureGrey", yaw=yaw)
                for c in range(120):
                    q = P._off(p[0], p[1], yaw, -5900 + c * 100, 0)
                    m.block((q[0], q[1], 110), (30, 30, 30), "Cube", ff, "PureGrey", collision=False)
            for _ in range(20):
                x, y = spot()
                P.tank(m, x, y, 0, ff, r=400, h=1800)
        elif frag == 5:
            for _ in range(12):
                x, y = spot()
                m.place_kit("/Game/Greybox/Kits/K1/Cottage/SM_K1_Cottage_%d" % rng.randint(1, 20), x, y, -100, 90, 0.8, "House", ff,
                            ["NowhereFragment"], material="PureGrey")
            for _ in range(130):
                x, y = spot()
                P.tree(m, x, y, 0, ff, rng)
            for _ in range(60):
                x, y = spot()
                P.bench(m, x, y, 0, rng.uniform(0, 360), ff)
        elif frag == 6:
            for i in range(13):
                for j in range(12):
                    p = P._off(o.x, o.y, yaw, -6000 + i * 1000, -5500 + j * 1000)
                    if abs(-5500 + j * 1000) < 900:
                        continue
                    P.desk_set(m, p[0], p[1], 0, yaw, ff, rng)
        elif frag == 7:
            for j in range(80):
                p = P._off(o.x, o.y, yaw, -6000 + j * 150, 0)
                m.block((p[0], p[1], 6), (40, 1400, 12), "Tie", ff, "PureGrey", yaw=yaw, collision=False)
            for j in range(13):
                for side in (-1, 1):
                    p = P._off(o.x, o.y, yaw, -6000 + j * 1000, side * 800)
                    m.block((p[0], p[1], 1300), (150, 100, 2600), "Rib", ff, "PureGrey", yaw=yaw, collision=False)
            for _ in range(180):
                x, y = spot()
                P.angel(m, x, y, 0, rng.uniform(0, 360), ff)   # grey, unlit: shapes only
        elif frag == 8:
            for _ in range(40):
                x, y = spot()
                m.block((x, y, -400), (rng.uniform(3000, 7000), rng.uniform(2000, 4000), 1600), "Dune", ff, "PureGrey", shape="sphere")
            for _ in range(200):
                x, y = spot()
                P.server_rack(m, x, y, -60, rng.uniform(0, 360), ff, rng)
        else:
            for _ in range(60):
                x, y = spot()
                z = rng.uniform(1000, 6000)
                m.block((x, y, z), (rng.uniform(600, 2000), rng.uniform(600, 2000), 300), "Islet", ff, "PureGrey", shape="cylinder")
            for _ in range(380):
                x, y = spot()
                m.block((x, y, rng.uniform(500, 7000)), (rng.uniform(80, 300), rng.uniform(80, 300), rng.uniform(40, 200)), "FloatingObject",
                        ff, "PureGrey", yaw=rng.uniform(0, 360), pitch=rng.uniform(-60, 60), collision=False)
    m.force_material = None


# ============================================================================ 12. Paradise

def ch12(m, rng, log):
    import greybox_neotokyo as NT
    f = "Detail"
    half = 35000
    bright = ["Coral", "Turquoise", "Lime", "Sunflower", "Magenta"]
    m.keepout += [(14000, 14000, half, half)]
    for b in list(m.kits):
        NT._dress_facades(m, rng, b, f + "/Facades")
        _rooftop(m, rng, b, f + "/Rooftops", tank=False)
        x0, y0, x1, y1 = b["rect"]
        for fx, fy, yaw in (((x0 + x1) / 2, y0 - 150, 0), ((x0 + x1) / 2, y1 + 150, 180), (x0 - 150, (y0 + y1) / 2, 90), (x1 + 150, (y0 + y1) / 2, 270)):
            m.block((fx, fy, 350), (900, 300, 12), "Awning", f + "/Awnings", rng.choice(bright), yaw=yaw, roll=10, collision=False)
    canal = lambda x, y: any(abs(abs(v) - 12000) < 1700 for v in (x, y))
    land = lambda: (lambda x, y: None if canal(x, y) or (x > 14000 and y > 14000) else (x, y))(rng.uniform(-half, half), rng.uniform(-half, half))
    _scatter(m, rng, 480, land, lambda x, y: P.palm(m, x, y, 0, f + "/Palms", rng), pad=150)
    for _ in range(80):   # beach palms, umbrellas, loungers
        x, y = rng.uniform(14500, 23500), rng.uniform(14500, 23500)
        if math.hypot(x - 19000, y - 19000) < 4600:
            continue   # the boss section on the beach stays clear
        m.block((x, y, 120), (14, 14, 240), "UmbrellaPole", f + "/Beach", "Marking", collision=False)
        m.block((x, y, 250), (400, 400, 80), "Umbrella", f + "/Beach", rng.choice(bright), shape="cone", collision=False)
        for s in (-1, 1):
            m.block((x + s * 150, y + 150, 25), (200, 70, 20), "Lounger", f + "/Beach", "Marking", pitch=10, tags=["JumpUp"])
    _scatter(m, rng, 60, land, lambda x, y: P.stall(m, x, y, 0, rng.uniform(0, 360), f + "/Market", rng, rng.choice(bright)))
    _scatter(m, rng, 220, land, lambda x, y: P.lamp(m, x, y, 0, rng.uniform(0, 360), f + "/Lamps", head=rng.choice(["Lamp", "NeonPink"])))
    _scatter(m, rng, 140, land, lambda x, y: P.bench(m, x, y, 0, rng.uniform(0, 360), f + "/Benches", mat=rng.choice(bright)))
    _scatter(m, rng, 260, land, lambda x, y: P.flower_patch(m, x, y, 0, f + "/Flowers", rng, bright + ["PastelPink"]), pad=50)
    _scatter(m, rng, 160, land, lambda x, y: P.bush(m, x, y, 0, f + "/Bushes", rng, mat="Palm"), pad=50)
    for _ in range(50):   # boats in the canals, some cruising
        horizontal = rng.random() < 0.5
        off = rng.choice([-12000, 12000])
        along = rng.uniform(-half + 2000, half - 2000)
        x, y = (along, off) if horizontal else (off, along)
        yaw = 0 if horizontal else 90
        if rng.random() < 0.4:
            m.mover((x, y, -50), (600, 200, 100), "CanalBoat", f + "/Boats", rng.choice(bright),
                    offset=(3000, 0, 0) if horizontal else (0, 3000, 0), period=rng.uniform(14, 24), phase=rng.random(), yaw=yaw,
                    tags=["Boat", "JumpUp"])
        else:
            P.boat(m, x, y, -150, yaw, f + "/Boats", rng.choice(bright))
    for _ in range(8):
        x, y = land() or (0, 0)
        if m.blocked(x, y, 800):
            continue
        m.block((x, y, 60), (1200, 1200, 120), "FountainBasin", f + "/Fountains", "Turquoise", shape="cylinder", tags=["JumpUp"])
        m.block((x, y, 50), (1100, 1100, 100), "FountainWater", f + "/Fountains", "Water", shape="cylinder", collision=False)
        m.block((x, y, 350), (150, 150, 600), "FountainJet", f + "/Fountains", "Foam", shape="cylinder", collision=False)


# ============================================================================ 13. A Secretive Soul

def ch13(m, rng, log):
    from greybox_chapters import _house
    f = "Detail"
    # (house plots are added to keep-out as they are found below)
    m.keepout += [(-1000, -1200, 1000, 1200), (-27000, -8000, -12000, 2500), (-30000, 13000, 30000, 16000)]
    # More houses with porches, posts, doors, windows and plot fences.
    for _ in range(26):
        x, y = rng.uniform(-8000, 21000), rng.choice([-1, 1]) * rng.uniform(5000, 11500)
        if math.hypot(x, y) < 6000 or m.blocked(x, y, 900):
            continue
        _house(m, x, y, 0, rng.uniform(800, 1400), rng.uniform(700, 1000), rng.uniform(350, 600), rng.choice([0, 90]), f + "/Village",
               tags=["Climbable"])
        m.keepout.append((x - 900, y - 900, x + 900, y + 900))
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    houses = [a for a in eas.get_all_level_actors() if a.get_actor_label() == "House"]
    for h in houses:
        o, e = h.get_actor_bounds(False)
        x, y, w, d = o.x, o.y, e.x * 2, e.y * 2
        if o.z - e.z > 100:
            continue   # the shrine on its hill
        m.keepout.append((x - e.x - 100, y - e.y - 100, x + e.x + 100, y + e.y + 100))
        m.block((x, y - e.y - 120, 40), (w, 240, 20), "Engawa", f + "/HouseDetail", "Wood", tags=["JumpUp"])
        for k in range(4):
            m.block((x - e.x + 40 + k * (w - 80) / 3, y - e.y - 220, 200), (20, 20, 400), "PorchPost", f + "/HouseDetail", "Wood", collision=False)
        m.block((x, y - e.y - 4, 120), (220, 8, 200), "ShojiDoor", f + "/HouseDetail", "Marking", collision=False)
        for sx in (-1, 1):
            m.block((x + sx * e.x * 0.6, y - e.y - 4, 180), (120, 8, 90), "Window", f + "/HouseDetail", "Lamp" if rng.random() < 0.3 else "Marking",
                    collision=False)
        for k in range(10):   # plot fence
            a = k * 36
            p = m.polar((x, y, 0), max(w, d) * 0.9, a, 0)
            m.block((p[0], p[1], 60), (12, 12, 120), "FencePost", f + "/HouseDetail", "Bamboo", collision=False)
        for k in range(3):
            P.rock(m, x + rng.uniform(-e.x, e.x), y + e.y + rng.uniform(200, 500), 0, f + "/Gardens", rng, s=rng.uniform(60, 140), mat="Stone")
    # Country roads with utility poles and wires.
    for y in (-3500, 3500):
        _road_line(m, rng, (-29000, y), (28000, y), f + "/Roads", width=600, dashes=0, mat="Asphalt")
        prev = None
        for x in range(-28000, 28000, 3000):
            if abs(x) < 1500:
                continue
            P.power_pole(m, x, y + 450, 0, f + "/Poles")
            top = (x, y + 450, 840)
            if prev:
                for dz in (0, -40, -80):
                    P.wire(m, (prev[0], prev[1], prev[2] + dz), (top[0], top[1], top[2] + dz), f + "/Poles", sag=90)
            prev = top
    _road_line(m, rng, (4000, -29000), (4000, 13000), f + "/Roads", width=500, dashes=0, mat="Asphalt")
    # Stone lanterns, small roadside shrines, vending machines, a bus stop, scarecrows, wells, carts.
    area = (-28000, -28000, 28000, 28000)
    valley = lambda: (lambda x, y: (x, y) if math.hypot(x, y) < 27000 else None)(rng.uniform(-28000, 28000), rng.uniform(-28000, 28000))
    def lantern(x, y):
        m.block((x, y, 30), (80, 80, 60), "LanternBase", f + "/Lanterns", "Stone")
        m.block((x, y, 110), (30, 30, 100), "LanternPost", f + "/Lanterns", "Stone")
        m.block((x, y, 190), (70, 70, 60), "LanternLight", f + "/Lanterns", "Lamp")
        m.block((x, y, 245), (110, 110, 40), "LanternRoof", f + "/Lanterns", "Stone", shape="cone", collision=False)
    _scatter(m, rng, 100, valley, lantern, pad=100)
    for k in range(10):
        p = m.polar((0, 0, 0), 2600, k * 36, 0)
        lantern(p[0], p[1])
    def hokora(x, y):
        m.block((x, y, 40), (120, 120, 80), "HokoraBase", f + "/Shrines", "Stone", tags=["JumpUp"])
        m.block((x, y, 130), (80, 70, 100), "Hokora", f + "/Shrines", "Wood")
        m.block((x, y, 200), (110, 100, 30), "HokoraRoof", f + "/Shrines", "NeonRed", collision=False)
        m.block((x, y - 50, 95), (30, 10, 30), "Offering", f + "/Shrines", "Lamp", collision=False)
    _scatter(m, rng, 20, valley, hokora)
    for k in range(8):
        P.vending(m, -6000 + k * 3500, -3500 + 500, 0, 0, f + "/Vending", rng, ["NeonCyan", "Marking", "NeonRed"])
    m.block((9000, -2900, 120), (600, 200, 20), "BusStopBench", f + "/BusStop", "Wood")
    m.block((9000, -2700, 250), (700, 30, 500), "BusStopBack", f + "/BusStop", "Grey")
    m.block((9000, -2900, 520), (800, 400, 20), "BusStopRoof", f + "/BusStop", "Grey")
    m.block((9400, -3100, 150), (20, 20, 300), "BusStopSign", f + "/BusStop", "Metal")
    for k in range(12):
        x, y = -24000 + (k % 4) * 3500, -6000 + (k // 4) * 3500
        m.block((x, y, 90), (12, 12, 180), "ScarecrowPole", f + "/Scarecrows", "Wood", collision=False)
        m.block((x, y, 150), (120, 10, 12), "ScarecrowArms", f + "/Scarecrows", "Wood", collision=False)
        m.block((x, y, 195), (40, 40, 40), "ScarecrowHead", f + "/Scarecrows", "Sand", shape="sphere", collision=False)
        m.block((x, y, 220), (70, 70, 25), "ScarecrowHat", f + "/Scarecrows", "Bamboo", shape="cone", collision=False)
    for _ in range(6):
        x, y = valley() or (5000, 5000)
        if m.blocked(x, y, 300):
            continue
        m.block((x, y, 50), (160, 160, 100), "Well", f + "/Wells", "Stone", shape="cylinder", tags=["JumpUp"])
        for s in (-1, 1):
            m.block((x + s * 70, y, 160), (10, 10, 220), "WellPost", f + "/Wells", "Wood", collision=False)
        m.block((x, y, 280), (200, 160, 20), "WellRoof", f + "/Wells", "Roof", collision=False)
    # Rice plants in rows, paddy levees, bamboo fences.
    for i in range(4):
        for j in range(3):
            cx, cy = -24000 + i * 3500, -6000 + j * 3500
            for a in range(10):
                for b in range(10):
                    m.block((cx - 1350 + a * 300, cy - 1350 + b * 300, 25), (40, 40, 70), "RicePlant", f + "/Rice", "Paddy", collision=False)
    for k in range(300):
        x = -26000 + (k % 60) * 250
        y = -8000 if k < 60 else (4000 if k < 120 else rng.uniform(-8000, 4000))
        if k >= 120:
            x = rng.choice([-26000, -11500])
        m.block((x, y, 50), (12, 12, 100), "BambooFence", f + "/Fences", "Bamboo", collision=False)
    # Bamboo groves, forest trees, a small cemetery.
    for gx, gy in ((-14000, 20000), (18000, -20000)):
        for _ in range(1150):
            x, y = gx + rng.uniform(-5000, 5000), gy + rng.uniform(-3500, 3500)
            h = rng.uniform(700, 1400)
            m.block((x, y, h / 2), (16, 16, h), "BambooStalk", f + "/Bamboo", "Bamboo", roll=rng.uniform(-4, 4), collision=False)
            if rng.random() < 0.3:
                m.block((x, y, h), (200, 200, 120), "BambooLeaves", f + "/Bamboo", "Palm", shape="sphere", collision=False)
    _scatter(m, rng, 280, lambda: (lambda p: (p[0], p[1]))(m.polar((0, 0, 0), rng.uniform(17000, 29000), rng.uniform(0, 360), 0)),
             lambda x, y: P.tree(m, x, y, 0, f + "/Forest", rng, crown=rng.choice(["Tree", "Flora", "Hedge"])), pad=100)
    for k in range(150):
        x, y = -20000 + (k % 15) * 300, 8000 + (k // 15) * 300
        m.block((x, y, 50), (30, 50, 100), "Gravestone", f + "/Cemetery", "Stone", tags=["JumpUp"])
        m.block((x, y, 4), (60, 80, 8), "GraveBase", f + "/Cemetery", "Stone", collision=False)
    for _ in range(400):   # stepping stones along the paths
        x, y = valley() or (0, 0)
        m.block((x, y, 3), (90, 90, 8), "SteppingStone", f + "/Paths", "Stone", shape="cylinder", collision=False)
    for _ in range(200):   # more red glyphs on the monolith
        face = rng.choice([-1, 1])
        m.block((face * 306, rng.uniform(-750, 750), rng.uniform(200, 4800)), (8, rng.uniform(30, 120), rng.uniform(30, 160)), "RedGlyph",
                "Monolith/Glyphs", "RedGlyph", collision=False, tags=["RedGlyph"])


DETAIL = {2: ch02, 3: ch03, 4: ch04, 5: ch05, 6: ch06, 7: ch07, 8: ch08, 9: ch09, 10: ch10, 11: ch11, 12: ch12, 13: ch13}
