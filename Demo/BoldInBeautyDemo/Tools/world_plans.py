"""Per-chapter plans for the large story maps (chapters 14-40) and the driver that builds them with greybox_world.World.

Coordinates are in thousands of cm (1 unit = 10 m); a plan's map is about 100 units = 1 km across. A plan lists the four section
centres (x, y, z), the hero set pieces (world_heroes.py), districts (kind of buildings filling a rectangle), lakes, and the
discovery areas (centre + landmark). The driver adds roads between sections and spurs to discoveries, the edge, the asset
stand-ins, the story markers and a modest detail pass.
"""

import json
import math
import os
import random

import unreal

import greybox_levels as L
import greybox_story as gs
import greybox_world as GW
import world_heroes as H
from greybox_common import MAPS, GreyboxMap

HERE = os.path.dirname(os.path.abspath(__file__))
K = 1000.0
LM_NAMES = {"tower": "Lookout", "arch": "Gateway", "dome": "Glasshouse", "stack": "Cache", "statue": "Monument", "clock": "Timepiece",
            "machine": "Works", "vault": "Strongroom", "bench": "Tribunal", "table": "Wager", "bigtop": "Marquee", "dais": "Rostrum",
            "stage": "Boards", "colossus": "Colossus", "train": "Siding", "monolith": "Obelisk"}
CITY = (-52, -52, 52, 52, "city")


def P(extent, edge, path, districts=(), heroes=(), lakes=(), disc=(), ground=None, sea=False, override=None):
    return dict(extent=extent, edge=edge, path=path, districts=list(districts), heroes=list(heroes), lakes=list(lakes), disc=list(disc),
                ground=ground, sea=sea, override=override)


PLANS = {
    14: P(110, "skyline", [(-36, -36, 0), (-6, -30, 0), (22, -4, 0), (30, 30, 0)], [CITY],
          [("tower", (0, 0, 0), dict(r=4600, floors=30)), ("tower", (30, 44, 0), dict(r=2600, floors=12))],
          disc=[(-22, 12, 0, "tower"), (10, 26, 0, "arch"), (-36, 22, 0, "dome"), (4, -14, 0, "statue"), (40, -28, 0, "stack"), (-14, 40, 0, "clock")]),
    15: P(100, "wall", [(-38, 0, 0), (-14, 0, 0), (12, 0, 0), (36, 0, 0)], [(-48, -48, 48, -8, "industrial"), (-48, 8, 48, 48, "industrial")],
          [("tunnel", ((-47, 0, 0), (47, 0, 0)), dict(width=11000, train=True)), ("tunnel", ((-30, -26, 0), (30, -26, 0)), dict(width=6000, label="TunnelS")),
           ("tunnel", ((-30, 26, 0), (30, 26, 0)), dict(width=6000, label="TunnelN")), ("hall", (36, 0, 0), dict(width=16000, depth=11000, style="gallery"))],
          disc=[(-30, -26, 0, "stack"), (0, -26, 0, "machine"), (30, -26, 0, "arch"), (-30, 26, 0, "vault"), (0, 26, 0, "tower"), (30, 26, 0, "train")]),
    16: P(100, "wall", [(-34, -22, 0), (-6, 24, 0), (22, -16, 0), (36, 32, 0)],
          [(-46, -46, 46, 46, "market"), (-46, 30, -20, 46, "tents")],
          [("hall", (20, -30, 0), dict(width=22000, depth=12000, style="canteen")), ("hall", (-14, 34, 0), dict(width=22000, depth=12000, style="court"))],
          disc=[(-24, 6, 0, "stack"), (6, -6, 0, "arch"), (36, -2, 0, "statue"), (-40, -10, 0, "tower"), (12, 40, 0, "vault"), (-8, -40, 0, "dome")]),
    17: P(100, "wall", [(0, -26, 0), (-32, -6, 0), (32, -6, 0), (0, 30, 0)], [(-46, -46, 46, 46, "campus")],
          [("hall", (0, 30, 0), dict(width=34000, depth=20000, style="court")), ("hall", (-32, -6, 0), dict(width=20000, depth=12000, style="gallery")),
           ("hall", (32, -6, 0), dict(width=20000, depth=12000, style="court")), ("hall", (0, -26, 0), dict(width=22000, depth=14000, style="white"))],
          disc=[(-20, 16, 0, "bench"), (22, 14, 0, "statue"), (-40, 26, 0, "arch"), (40, 26, 0, "dais"), (-6, 6, 0, "clock"), (20, -42, 0, "vault")]),
    18: P(100, "wall", [(-18, -27, 0), (10, -9, 1500), (-10, 9, 3000), (14, 27, 4500)],
          [(-30, -36, 30, -18, "market", 0), (-30, -18, 30, 0, "market", 1500), (-30, 0, 30, 18, "market", 3000), (-30, 18, 30, 36, "market", 4500)],
          [("terraces", (0, -36, 0), dict(steps=4, dz=1500.0, width=64000.0, depth=18000.0))],
          disc=[(-40, -27, 0, "stack"), (40, -9, 1500, "table"), (-40, 9, 3000, "vault"), (40, 27, 4500, "tower")]),
    19: P(100, "skyline", [(0, 0, 0), (-32, 0, 0), (0, 34, 0), (30, -12, 3200)], [CITY],
          [("hall", (0, 0, 0), dict(width=32000, depth=30000, style="casino")), ("hall", (-32, 0, 0), dict(width=18000, depth=12000, style="court")),
           ("vault", (0, 34, 0), dict(size=14000))],
          disc=[(-20, -30, 0, "table"), (22, 24, 0, "stack"), (40, 8, 0, "arch"), (-40, 28, 0, "statue"), (12, -36, 0, "dais"), (-30, 40, 0, "tower")]),
    20: P(110, "wall", [(-30, 0, 0), (0, -24, 0), (-14, 26, 0), (36, -18, 0)], [(-52, -52, 52, 52, "crypt")],
          [("hall", (0, 0, 0), dict(width=54000, depth=14000, style="cathedral")), ("hall", (-14, 26, 0), dict(width=18000, depth=12000, style="gallery")),
           ("hall", (6, -24, 0), dict(width=20000, depth=12000, style="cathedral")), ("tower", (36, -18, 0), dict(r=3600, floors=16, mast=False))],
          disc=[(-40, -18, 0, "clock"), (24, 24, 0, "arch"), (-26, -40, 0, "statue"), (44, 18, 0, "tower"), (10, 42, 0, "dome"), (-44, 30, 0, "vault")]),
    21: P(110, "hills", [(-4, -22, 0), (10, -7, 2000), (-10, 7, 4000), (6, 22, 6000)],
          [(-30, -30, 30, -15, "village", 0), (-30, -15, 30, 0, "village", 2000), (-52, -52, -32, 52, "wild"), (32, -52, 52, 52, "wild")],
          [("terraces", (0, -30, 0), dict(steps=4, dz=2000.0, width=60000.0, depth=15000.0)), ("dome", (-24, -7, 2000), dict(radius=8000.0))],
          lakes=[(26, -50, 48, -34)],
          disc=[(-40, -22, 0, "dome"), (40, 4, 2000, "stack"), (-42, 14, 4000, "machine"), (42, 30, 6000, "tower"), (-20, 40, 0, "arch"), (16, -42, 0, "vault")]),
    22: P(100, "wall", [(-36, -14, 0), (16, 6, 0), (30, -26, 0), (0, 30, 0)], [(-46, -46, 46, 46, "stacks")],
          [("maze", (-10, -8, 0), dict(size=34000.0, cells=20)), ("hall", (30, -26, 0), dict(width=22000, depth=14000, style="library")),
           ("colosseum", (0, 30, 0), dict(r=4800.0, steps=3))],
          disc=[(-40, 20, 0, "stack"), (40, 10, 0, "clock"), (-20, 36, 0, "statue"), (26, 40, 0, "arch"), (-6, -40, 0, "tower"), (40, -42, 0, "vault")]),
    23: P(110, "hills", [(-34, -20, 0), (-4, -8, 2500), (14, 16, 5000), (34, 30, 7500)], [(-52, -52, 52, 52, "wild")],
          [("dome", (0, 0, 2500), dict(radius=11000.0)), ("tower", (-34, -34, 0), dict(r=1800, floors=8))],
          disc=[(-20, 24, 0, "tower"), (24, -24, 0, "machine"), (-44, 6, 0, "arch"), (40, -4, 0, "stack"), (6, 40, 0, "statue"), (-10, -40, 0, "dome")]),
    24: P(100, "wall", [(-34, -6, 0), (-6, -34, 0), (24, 8, 0), (0, 0, 0)], [(-46, -46, 46, 46, "tents"), (-46, 10, -20, 46, "market")],
          [("bigtop", (0, 0, 0), dict(r=9000.0)), ("maze", (26, -26, 0), dict(size=20000.0, cells=12, mat="Glass")),
           ("lanes", (-28, 26, 0), dict(length=26000.0, count=3))],
          disc=[(-40, -30, 0, "bigtop"), (14, -6, 0, "table"), (40, 20, 0, "stage"), (-14, 22, 0, "statue"), (24, 38, 0, "arch"), (-40, 8, 0, "clock")]),
    25: P(100, "wall", [(-38, 0, 0), (-8, -14, 0), (-30, 30, 0), (36, 18, 0)], [(-46, -46, 46, 46, "campus")],
          [("lanes", (4, -14, 0), dict(length=60000.0, count=8)), ("tower", (-30, 30, 0), dict(r=2800, floors=8, mast=False)),
           ("hall", (-38, 0, 0), dict(width=18000, depth=12000, style="canteen")), ("colosseum", (36, 18, 0), dict(r=6000.0, steps=4))],
          disc=[(-10, 36, 0, "tower"), (20, 34, 0, "machine"), (-40, -30, 0, "stack"), (30, -40, 0, "arch"), (-20, -40, 0, "statue"), (44, -10, 0, "dais")]),
    26: P(100, "wall", [(-30, -30, 9600), (-6, -20, 6400), (16, 0, 3200), (32, 28, 0)], [(-46, -46, 46, 46, "crypt")],
          [("hall", (-30, 26, 0), dict(width=20000, depth=12000, style="gallery")), ("vault", (30, -30, 0), dict(size=14000)),
           ("colosseum", (32, 28, 0), dict(r=7200.0, steps=3))],
          disc=[(-40, -4, 0, "vault"), (-10, 6, 0, "statue"), (8, -40, 0, "arch"), (40, 4, 0, "stack"), (-20, 40, 0, "clock"), (20, -10, 0, "dome")]),
    27: P(100, "skyline", [(-34, -10, 0), (0, 0, 0), (30, -30, 0), (36, 30, 0)], [CITY, (14, 14, 50, 50, "industrial")],
          [("hall", (0, 0, 0), dict(width=34000, depth=30000, style="mall")), ("hall", (-34, -10, 0), dict(width=24000, depth=12000, style="canteen")),
           ("vault", (30, -30, 0), dict(size=14000))],
          disc=[(-20, 30, 0, "stack"), (22, 14, 0, "machine"), (-44, 22, 0, "arch"), (10, -40, 0, "tower"), (-30, -40, 0, "statue"), (46, 4, 0, "vault")]),
    28: P(110, "skyline", [(-34, -34, 0), (-10, -14, 3000), (14, -30, 6000), (24, 24, 9000)], [CITY],
          [("tower", (0, 0, 0), dict(r=5500, floors=36)), ("hall", (24, 24, 9000), dict(width=16000, depth=16000, style="council")),
           ("hall", (-34, -34, 0), dict(width=18000, depth=12000, style="white"))],
          disc=[(-22, 20, 0, "arch"), (24, -8, 0, "dais"), (-44, 0, 0, "statue"), (4, 40, 0, "tower"), (44, 44, 0, "clock"), (-8, -44, 0, "stack")]),
    29: P(100, "wall", [(-32, 10, 0), (-6, -30, 0), (8, -6, 0), (30, 12, 1200)], [(-46, -46, 46, 46, "campus")],
          [("hall", (22, 10, 0), dict(width=40000, depth=24000, style="stage")), ("hall", (-32, 10, 0), dict(width=18000, depth=14000, style="gallery")),
           ("vault", (-6, -30, 0), dict(size=16000))],
          disc=[(-18, 34, 0, "stage"), (40, -26, 0, "stack"), (-44, -14, 0, "arch"), (20, 40, 0, "dais"), (-30, -42, 0, "table"), (46, 36, 0, "tower")]),
    30: P(100, "wall", [(-36, 0, 0), (-8, 0, 0), (18, 24, 0), (32, -26, 0)], [(-46, -46, 46, 46, "industrial")],
          [("hall", (-10, 0, 0), dict(width=50000, depth=30000, style="factory")), ("machine", (28, 22, 0), dict(size=18000.0)),
           ("hall", (32, -26, 0), dict(width=20000, depth=14000, style="gallery"))],
          disc=[(-30, 30, 0, "machine"), (0, 36, 0, "tower"), (40, 4, 0, "vault"), (-40, -26, 0, "stack"), (10, -38, 0, "arch"), (-10, -42, 0, "statue")]),
    31: P(110, "wall", [(-38, 0, 0), (-12, 0, 0), (14, 0, 0), (38, 0, 0)], [(-52, -52, 52, -10, "industrial"), (-52, 10, 52, 52, "ruin")],
          [("tunnel", ((-50, 0, 0), (50, 0, 0)), dict(width=11000, train=True)), ("tunnel", ((-24, -28, 0), (24, -28, 0)), dict(width=6000, label="TunnelS")),
           ("tunnel", ((-24, 28, 0), (24, 28, 0)), dict(width=6000, label="TunnelN"))],
          disc=[(-24, -28, 0, "train"), (0, -28, 0, "stack"), (24, -28, 0, "arch"), (-24, 28, 0, "machine"), (0, 28, 0, "vault"), (24, 28, 0, "tower")]),
    32: P(100, "wall", [(0, -30, 0), (-28, 0, 0), (28, 0, 0), (0, 0, 0)], [(-46, -46, 46, 46, "crypt")],
          [("vault", (0, 0, 0), dict(size=22000)), ("hall", (-28, 0, 0), dict(width=18000, depth=14000, style="canteen")),
           ("hall", (28, 0, 0), dict(width=18000, depth=14000, style="stage")), ("hall", (0, 28, 0), dict(width=22000, depth=14000, style="court")),
           ("hall", (0, -30, 0), dict(width=22000, depth=14000, style="library"))],
          disc=[(-40, 24, 0, "stack"), (40, 24, 0, "arch"), (-40, -26, 0, "vault"), (40, -26, 0, "statue"), (0, 44, 0, "dais"), (0, -46, 0, "clock")]),
    33: P(100, "wall", [(0, -36, 0), (0, -14, 0), (0, 8, 0), (0, 30, 0)], [(-46, -46, 46, 46, "white")],
          [("hall", (0, -2, 0), dict(width=16000, depth=76000, style="white", gap_sides=("E", "W")))],
          disc=[(-30, -30, 0, "statue"), (30, -10, 0, "arch"), (-30, 14, 0, "dome"), (30, 34, 0, "tower"), (-36, 40, 0, "dais"), (36, -40, 0, "clock")],
          ground="Cloud"),
    34: P(100, "skyline", [(0, 0, 0), (34, -6, 0), (0, 30, 6000), (-34, 0, 0)], [(-46, -46, 46, 46, "tents"), (-46, -46, -20, -20, "market")],
          [("dome", (0, 0, 0), dict(radius=14000.0)), ("hall", (-34, 0, 0), dict(width=26000, depth=16000, style="factory")),
           ("hall", (34, -6, 0), dict(width=18000, depth=12000, style="canteen"))],
          disc=[(-20, 30, 0, "table"), (26, 30, 0, "stack"), (-40, -30, 0, "arch"), (40, -34, 0, "stage"), (10, -38, 0, "dais"), (-44, 34, 0, "tower")]),
    35: P(100, "wall", [(-30, -30, 0), (-10, -10, 0), (10, 10, 0), (30, 30, 0)], [(-46, -46, 46, 46, "stacks")],
          [("hall", (0, -30, 0), dict(width=76000, depth=12000, style="gallery")), ("hall", (0, -10, 0), dict(width=76000, depth=12000, style="gallery")),
           ("hall", (0, 10, 0), dict(width=76000, depth=12000, style="gallery")), ("hall", (0, 30, 0), dict(width=76000, depth=12000, style="gallery"))],
          disc=[(-44, -42, 0, "stack"), (44, -42, 0, "vault"), (-44, 42, 0, "clock"), (44, 42, 0, "statue"), (0, -44, 0, "arch"), (0, 44, 0, "dais")]),
    36: P(110, "skyline", [(-24, -10, 0), (0, -6, 0), (20, -12, 0), (0, 22, 400)], [CITY],
          [("hall", (0, -10, 0), dict(width=64000, depth=30000, style="court")), ("hall", (0, 22, 0), dict(width=30000, depth=14000, style="council"))],
          disc=[(-30, 28, 0, "statue"), (30, 28, 0, "arch"), (-44, -30, 0, "dais"), (44, -30, 0, "tower"), (0, 44, 0, "clock"), (-44, 10, 0, "bench")]),
    37: P(110, "skyline", [(-26, 0, 0), (0, -28, 0), (26, 0, 0), (0, 26, 0)], [(-52, -52, 52, 52, "ruin")],
          [("dome", (-26, 0, 0), dict(radius=9000.0)), ("tower", (26, 36, 0), dict(r=3500, floors=14)), ("hall", (0, -28, 0), dict(width=26000, depth=16000, style="court"))],
          disc=[(-40, 30, 0, "colossus"), (40, -30, 0, "stack"), (-36, -36, 0, "arch"), (40, 30, 0, "machine"), (10, 6, 0, "statue"), (-10, 46, 0, "tower")]),
    38: P(130, "none", [(-30, -20, 0), (0, 25, 0), (30, -5, 0), (5, -32, 0)],
          [], [("lighthouse", (5, -34, 0), dict())], disc=[(-44, 20, 0, "stack"), (-14, -6, 0, "statue"), (40, 30, 0, "arch"), (16, 8, 0, "tower"),
                                                         (-50, -42, 0, "vault"), (46, -36, 0, "clock")], sea=True),
    39: P(110, "wall", [(-38, -8, 0), (-14, -30, 0), (8, 26, 0), (36, 0, 0)], [(-52, -52, 52, 52, "ruin")],
          [("counted", (0, 0, 0), dict(rows=20, cols=60)), ("lighthouse", (-30, -30, 0), dict()), ("tower", (-30, 30, 0), dict(r=3000, floors=12, mast=False)),
           ("hall", (40, 0, 0), dict(width=16000, depth=10000, style="gallery"))],
          disc=[(-46, 6, 0, "statue"), (20, -34, 0, "arch"), (-10, 40, 0, "stack"), (46, 30, 0, "clock"), (22, 18, 0, "dome"), (-24, 0, 0, "vault")],
          ground="PureGrey", override=["PureGrey"]),
    40: P(110, "hills", [(-36, -24, 0), (-20, -2, 0), (10, 8, 0), (30, 0, 0)],
          [(-48, -48, -16, -8, "village"), (-52, 30, -20, 52, "wild"), (14, -52, 52, -20, "wild"), (14, 20, 52, 52, "wild")],
          [("monolith", (30, 0, 0), dict(plaza=9000.0)), ("terraces", (-30, 14, 0), dict(steps=3, dz=1400.0, width=14000.0, depth=8000.0))],
          lakes=[(-2, -52, 2, 52)],
          disc=[(-40, 40, 0, "arch"), (20, 40, 0, "statue"), (44, -36, 0, "stack"), (-14, -38, 0, "dome"), (-44, 10, 0, "tower"), (46, 14, 0, "clock")]),
}


def _p(v):
    """Plan coordinates: x and y in thousands of cm, z in cm."""
    return (v[0] * K, v[1] * K, v[2])


def _archive(n):
    try:
        return json.load(open(os.path.join(HERE, "story_archive.json"))).get(str(n), [])
    except Exception:
        return []


def build_world(n, spec, rng, m, log):
    plan = PLANS[n]
    w = GW.World(m, rng, spec, plan["extent"] * K, "Ch%02d" % n)
    sea = plan["sea"]
    if sea:
        half = w.half
        w.lake(-half, -half, half, half, depth=1300.0, climb_sides=())
    for (x0, y0, x1, y1) in plan["lakes"]:
        w.lake(x0 * K, y0 * K, x1 * K, y1 * K)
    if not sea:
        w.ground(plan["ground"] or ("Grass" if spec["theme"] in ("village", "field", "lab") else "Dark"))
    # hero set pieces first (they reserve their space)
    for name, where, kw in plan["heroes"]:
        fn = H.HEROES[name]
        if name == "tunnel":
            fn(w, _p(where[0]), _p(where[1]), **kw)
        else:
            fn(w, _p(where), **kw)
    # sections
    sections = []
    for i, p in enumerate(plan["path"]):
        c = _p(p)
        r = 5500.0 if i == 3 else 5000.0
        if sea:   # an island under each section
            m.block((c[0], c[1], -650 + c[2] / 2), (r * 2 + 6000, r * 2 + 6000, 1300 + c[2]), "Island", w.f + "/Islands", "Sand", shape="cylinder")
        sections.append(w.section(i + 1, c, r, boss=i == 3))
    # roads along the critical path
    cs = [s[1] for s in sections]
    for a, b in zip(cs, cs[1:]):
        w.road(a, b, 1600.0)
    # discovery areas with spurs
    docs = _archive(n) or ["Field notes"]
    elites = spec["mini"]
    for k, (x, y, z, lm) in enumerate(plan["disc"]):
        c = _p((x, y, z))
        if sea:
            m.block((c[0], c[1], -650), (6200, 6200, 1300), "DiscoveryIsland", w.f + "/Islands", "Sand", shape="cylinder")
        nearest = min(cs, key=lambda q: (q[0] - c[0]) ** 2 + (q[1] - c[1]) ** 2)
        w.discovery(c, LM_NAMES.get(lm, "Find"), docs[k % len(docs)], landmark=lm, elite=elites[k % len(elites)] if k % 2 == 0 else None)
        w.road((nearest[0], nearest[1], nearest[2]), c, 900.0, mat="Concrete", lamps=False)
    # districts last (avoid everything reserved)
    total = 0
    for d in plan["districts"]:
        x0, y0, x1, y1, kind = d[:5]
        z = d[5] * 1.0 if len(d) > 5 else 0.0
        if plan.get("override"):
            GW.KINDS[kind] = dict(GW.KINDS[kind], walls=plan["override"])
        total += w.district(x0 * K, y0 * K, x1 * K, y1 * K, kind, z=z)
    if plan["edge"] != "none":
        w.edge(plan["edge"])
    w.dress(9000 if sea else 12500)
    log("Ch %02d world: %d buildings, %d reserved zones, %d discoveries" % (n, total, len(w.reserved), len(w.discoveries)))
    return sections, (cs[0][0] - 3200.0, cs[0][1], cs[0][2]), w


def build_map(n, spec, materials, log):
    path = MAPS + "/L_Ch%02d_%s" % (n, spec["short"])
    gs._backup(log, path)
    m = GreyboxMap(path, materials, log)
    rng = random.Random(11000 + n)
    m.lighting(sun=2.6, sun_colour=tuple(spec["sun"]), pitch=-35, fog=spec["fog"] * 0.6, fog_colour=(0.35, 0.35, 0.45), volumetric=False)
    sections, start, world = build_world(n, spec, rng, m, log)
    placed = L.place_assets(m, n, sections, log)
    gs.story_markers(m, n, spec, sections, folder="Story", start=start, log=log)
    m.world_settings(unreal.GameModeBase.static_class())
    total = m.save(anime=True)
    log("Ch %02d %s: %d actors, %d asset stand-ins, boss %s (world %d x %d m)" % (
        n, spec["short"], total, placed, spec["boss"], PLANS[n]["extent"] * 10, PLANS[n]["extent"] * 10))
    return total


def build(materials, log, only=None):
    data = gs.load()
    built = {}
    for n in sorted(PLANS):
        if n in gs.LOCKED or (only and n not in only):
            continue
        built[n] = build_map(n, data[n], materials, log)
    return built
