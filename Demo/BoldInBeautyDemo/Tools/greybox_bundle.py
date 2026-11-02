"""Bundle dressing pass for the greybox maps (owner, 2026-10-08: "Here's a bundle with 100s of basically random assets to fill up
the 13 main chapter levels and arc with those assets"). The bundle is the owner's `Elite_Bundle` folder (Dekogon, Meshingun and
Scansfactory packs, UE 5.4 assets); Tools/import_elite_bundle.sh copies it to Content/Elite_Bundle, where its package paths
(/Game/Elite_Bundle/...) expect it. It is licensed store content of about 2 GB, so it stays out of Git like /Game/Greybox.

Runs last in every map build (after the first and second detail passes), so a rebuild keeps it. When the bundle is not in
Content the pass logs a warning and the maps build exactly as before.

- Catalogue: every StaticMesh under /Game/Elite_Bundle, sorted into packs and groups (flora, food, containers, furniture, decor,
  graves, structure, tools) by name, and into small / medium / large by its bounds. Slivers, planes, single tiny tools and
  whole-room backdrops are left out. Several packs in the bundle ship without their materials: those slots get the nearest
  greybox palette colour (wood, metal, stone, flora ...), so nothing shows the engine's checkerboard.
- Placement: "vignettes" (one medium or large piece with a few small pieces around it) and loose clutter, on the floor that is
  really there (an editor line trace) and only where a box over the piece touches nothing that blocks (an editor overlap test), so
  nothing lands in a wall, a cover piece, a jump-up step or the water. The middle of every combat section stays open for the
  fight; station rings in the Ark stay clear.
- Themes: each chapter takes the packs that fit the owner's level idea (docks gear in Docks, graves and dead trees in The Ruins,
  books and cabinets in T.O.M.B.S, flora over the Moon Kingdom ...). Chapter 10 recolours everything pastel except the realistic
  grave scans (the brief's "hyper realistic terrifying things"); Chapter 11 forces the one flat grey on every piece and dresses
  each leg of the walk with the props of the map it echoes. The Ark gets themed corners: food and baskets at Merchant Row,
  bookshelves in the library, storage in the armoury area, flora and fountains in the civilian area, crates in the Underworld.

Small pieces have no collision and a short draw distance (they are clutter, not cover). Every placement and theme choice is
the assistant's reading (PROPOSED); the owner's level ideas are unchanged.
"""

import math
import random

import unreal

from greybox_common import NEON

BUNDLE = "/Game/Elite_Bundle"
FOLDER = "Bundle"

PACKS = {
    "Dekogon/Dockhouse": "dock",
    "Dekogon/MedievalKitchen": "kitchen",
    "Dekogon/MedievalVOL9": "manor",
    "Dekogon/Victorian": "manor",
    "Dekogon/Anemoia": "anemoia",
    "Meshingun/GothicCemetery_Meshingun": "gothic",
    "Scansfactory/Cemetery": "cemetery",
    "Scansfactory/RuinedCrypt": "crypt",
    "Scansfactory/UrbanProps_Vol1": "urban",
}

# Name keywords -> group (first match wins; checked on the lower-case asset name).
GROUPS = [
    ("tree", ["treedead", "tree_01", "treestump", "stump"]),
    ("flora", ["fern", "nettle", "bush", "daisy", "ivy", "grass", "burdock"]),
    ("food", ["apple", "bread", "corn", "lettuce", "onion", "pepper", "potato", "pumpkin", "sausage", "tomato", "food",
              "veggie", "teapot", "plate", "pot_0"]),
    ("container", ["barrel", "basket", "bucket", "sack", "box_0", "crate", "jar", "cauldron", "spool", "storage", "carton",
                   "pallet", "tool_box_nn_01a", "firewood"]),
    ("furniture", ["table", "chair", "bed", "dresser", "cabinet", "cupboard", "bookshelf", "night_stand", "shelf", "shelve",
                   "clock", "holder", "boat_stand"]),
    ("decor", ["candle", "picture", "book", "coat_of_arms", "lyre", "rug", "curtain", "wallpaper", "chandalier", "ornament",
               "hat", "spoon"]),
    ("grave", ["grave", "statue", "stand", "pillar", "wheel", "carriage", "debris", "banister", "church", "column", "brick"]),
    ("structure", ["wall", "arch", "fireplace", "window", "door", "floor", "staircase", "stairs", "ramp", "pier", "plank", "tarp",
                   "ladder", "path", "tileground", "balk", "wooden_pieces"]),
    ("tool", ["tool", "rope", "wire", "tube", "oar", "powerbox", "fusebox", "bellow"]),
]

# Left out: backdrops, whole rooms, single roof / frame / glass pieces, planes and effects.
SKIP = ["backgrounds", "room_01a", "ceiling", "wall_frame", "roof", "supporting_beams", "glass", "emissivep", "planebended",
        "rope_nn_01c", "rope_nn_01d", "flame", "letter", "key_", "mandible", "first_floor", "room_wall", "room_bottom",
        "wall_cap", "door_hinge", "stone_wall", "wooden_wall", "wall_0", "big_holder", "dry_pepper_01b", "fresh_pepper_01a",
        "lettuce_01c", "lettuce_01d"]
FLAT = ["rug", "floor", "path", "tileground", "tarp", "plank", "wooden_pieces", "stairs_01", "gravedebris"]

# Palette colour for material slots the bundle left empty.
FALLBACK = [
    ("Flora", ["fern", "nettle", "bush", "daisy", "ivy", "grass", "burdock", "tree_01"]),
    ("Trunk", ["treedead", "stump"]),
    ("Metal", ["tool", "wire", "tube", "powerbox", "fusebox", "spool", "storage", "light", "hinge", "clock"]),
    ("Stone", ["grave", "church", "brick", "path", "stairs", "column", "sculpture", "debris", "tileground", "floor_01_04"]),
    ("Sand", ["carton", "picture", "wallpaper", "book"]),
    ("Dark", ["hat"]),
    ("Rust", ["tarp", "rope"]),
]

# Chapter themes: weighted (pack, groups) pools. vignettes / clutter = per combat section; route = vignettes per route gap;
# ambient = tries across the whole map; outer = large set pieces just outside each section ring.
THEMES = {
    1: dict(pools=[("urban", None, 4), ("dock", ["container", "tool", "furniture"], 4), ("kitchen", ["container", "food"], 2),
                   ("anemoia", ["decor"], 1)], street=900, roof=260),
    2: dict(pools=[("gothic", None, 5), ("cemetery", None, 3), ("crypt", None, 4), ("kitchen", ["structure", "container"], 2),
                   ("manor", ["furniture"], 1)], vignettes=16, clutter=34, outer=7, route=6, ambient=320),
    3: dict(pools=[("dock", None, 8), ("urban", None, 2), ("crypt", ["structure", "tree"], 2), ("kitchen", ["container"], 2)],
            vignettes=16, clutter=36, outer=6, route=6, ambient=320),
    4: dict(pools=[("manor", None, 4), ("kitchen", ["container", "furniture", "food", "decor"], 4), ("dock", ["container", "tool"], 2),
                   ("gothic", ["grave"], 1), ("anemoia", None, 1)], vignettes=15, clutter=34, outer=4, route=5, ambient=260),
    5: dict(pools=[("dock", ["container", "tool", "furniture", "structure"], 6), ("urban", None, 5)],
            vignettes=16, clutter=36, outer=4, route=6, ambient=200),
    6: dict(pools=[("cemetery", ["flora", "grave"], 5), ("crypt", ["flora", "tree"], 5), ("kitchen", None, 3), ("manor", None, 3),
                   ("gothic", ["grave"], 2)], vignettes=18, clutter=44, outer=7, route=6, ambient=420, near=40),
    7: dict(pools=[("manor", None, 6), ("anemoia", None, 4), ("dock", ["furniture", "container"], 2), ("urban", None, 2),
                   ("kitchen", ["decor", "container"], 1)], vignettes=22, clutter=50, outer=0, route=4, ambient=0, near=70),
    8: dict(pools=[("urban", None, 5), ("dock", ["container", "structure", "tool"], 5), ("gothic", ["grave"], 1)],
            vignettes=20, clutter=44, outer=3, route=8, ambient=0, near=60),
    9: dict(pools=[("urban", None, 4), ("dock", ["container", "tool", "furniture"], 4), ("crypt", ["tree"], 3),
                   ("gothic", ["grave"], 2)], vignettes=14, clutter=30, outer=6, route=6, ambient=320),
    10: dict(pools=[("kitchen", None, 3), ("manor", None, 3), ("dock", None, 2), ("anemoia", None, 2), ("gothic", None, 3),
                    ("cemetery", None, 2), ("crypt", None, 2), ("urban", None, 1)], vignettes=16, clutter=36, outer=6, route=6,
             ambient=300, pastel=True),
    11: dict(grey=True, per_leg=26),
    12: dict(pools=[("kitchen", ["food", "container", "furniture"], 5), ("crypt", ["flora", "tree"], 4), ("cemetery", ["flora"], 3),
                    ("dock", ["structure", "container", "tool"], 2), ("manor", ["furniture", "decor"], 2), ("urban", None, 1)],
             vignettes=16, clutter=38, outer=6, route=6, ambient=340),
    13: dict(pools=[("kitchen", ["container", "food", "furniture", "tool"], 5), ("crypt", None, 5), ("cemetery", ["flora"], 3),
                    ("dock", ["tool", "structure"], 2), ("gothic", ["grave"], 1)], vignettes=16, clutter=36, outer=6, route=6,
             ambient=340),
}
PASTELS = ["PastelPink", "PastelBlue", "PastelMint", "PastelLemon", "PastelLilac"]


# ============================================================================ catalogue

_CATALOG = None


def available():
    return unreal.EditorAssetLibrary.does_directory_exist(BUNDLE)


def _match(name, table):
    for value, words in table:
        if any(w in name for w in words):
            return value
    return None


# Base materials the bundle ships without their default textures: they fail to compile ("Param2D> Found NULL, requires
# Texture2D"), so a packaged game draws the engine default material. Their slots get a palette colour like empty ones.
BROKEN_BASES = ["/Scansfactory/Cemetery/BaseMaterials/m_BaseMaterial_01_01", "/Scansfactory/RuinedCrypt/BaseMaterials/m_BaseMaterial_01_01"]


def _broken(material):
    if material is None:
        return True
    try:
        base = material.get_base_material()
    except Exception:
        return False
    path = base.get_path_name() if base else ""
    return any(b in path for b in BROKEN_BASES)


def _resave_materials(found, log):
    """The bundle's materials were saved with UE 5.4: a packaged 5.8 game rejects some of their shader maps ("Invalid shader map
    ID ... will use default material"). Resaving them here, flagged for Nanite (the bundle meshes are Nanite), fixes that."""
    saved = 0
    for data in found:
        cls = str(data.asset_class_path.asset_name)
        if cls not in ("Material", "MaterialInstanceConstant"):
            continue
        asset = unreal.load_asset(str(data.package_name))
        if not asset:
            continue
        if cls == "Material":
            try:
                if not asset.get_editor_property("used_with_nanite"):
                    asset.set_editor_property("used_with_nanite", True)
                    unreal.MaterialEditingLibrary.recompile_material(asset)
            except Exception:
                pass
        if unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            saved += 1
    log("bundle: %d materials resaved for UE 5.8 (Nanite usage on)" % saved)


def catalog(log):
    """Every usable bundle mesh: dict(path, mesh, pack, group, size, lo, hi, cls, missing slots, fallback colour, flat)."""
    global _CATALOG
    if _CATALOG is not None:
        return _CATALOG
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = registry.get_assets(unreal.ARFilter(package_paths=[BUNDLE], recursive_paths=True))
    _resave_materials(found, log)
    out, skipped = [], 0
    for data in found:
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name)
        parts = path.split("/")
        pack = PACKS.get("/".join(parts[3:5]))
        name = parts[-1].lower()
        if not pack or any(s in name for s in SKIP):
            skipped += 1
            continue
        mesh = unreal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            skipped += 1
            continue
        box = mesh.get_bounding_box()
        size = (box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z)
        flat = any(w in name for w in FLAT)
        biggest = max(size)
        if biggest < 18 or biggest > 2500 or (min(size) < 0.5 and not flat):
            skipped += 1
            continue
        slots = mesh.get_editor_property("static_materials")
        missing = [k for k, s in enumerate(slots) if _broken(s.get_editor_property("material_interface"))]
        out.append(dict(path=path, mesh=mesh, pack=pack, group=_match(name, GROUPS) or "decor", size=size, lo=box.min, hi=box.max,
                        cls="small" if biggest < 130 else ("medium" if biggest < 600 else "large"), missing=missing,
                        nslots=max(1, len(slots)), fallback=_match(name, FALLBACK) or "Wood", flat=flat, name=parts[-1]))
    _CATALOG = out
    counts = {}
    for e in out:
        counts[e["pack"]] = counts.get(e["pack"], 0) + 1
    log("bundle: %d meshes catalogued (%s), %d left out, %d with palette fallback materials"
        % (len(out), ", ".join("%s %d" % kv for kv in sorted(counts.items())), skipped, sum(1 for e in out if e["missing"])))
    return out


# ============================================================================ placement

class Dresser:
    def __init__(self, m, log, pools, seed, pastel=False, grey=False):
        self.m = m
        self.log = log
        self.rng = random.Random(seed)
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.cat = catalog(log)
        self.pastel = pastel
        self.grey = grey
        self.taken = []        # (x, y, r) of this pass's pieces (small ones have no collision, so overlaps cannot see them)
        self.keep = []         # (x, y, r) circles kept clear (section middles, station rings)
        self.placed = 0
        self.used = set()
        self.misses = 0
        self.traces_ok = True
        self.open_sky = True   # False under a roof (Moon Kingdom cavern, T.O.M.B.S, the Intersection tunnel)
        self.set_pools(pools)

    def set_pools(self, pools):
        weighted = []
        for pack, groups, weight in pools:
            entries = [e for e in self.cat if e["pack"] == pack and (groups is None or e["group"] in groups)]
            if entries:
                weighted.append((entries, weight))
        self.pools = weighted

    def _pick(self, classes, groups=None):
        for _ in range(12):
            total = sum(w for _, w in self.pools)
            if not total:
                return None
            r = self.rng.uniform(0, total)
            for entries, w in self.pools:
                r -= w
                if r <= 0:
                    break
            choice = [e for e in entries if e["cls"] in classes and (groups is None or e["group"] in groups)]
            if choice:
                return self.rng.choice(choice)
        return None

    # ---------------------------------------------------------------- world queries
    def floor(self, x, y, z_ref, up=250.0, down=400.0):
        if not self.traces_ok:
            return z_ref
        try:
            hit = unreal.SystemLibrary.line_trace_single(self.world, unreal.Vector(x, y, z_ref + up), unreal.Vector(x, y, z_ref - down),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
        except Exception as error:
            self.traces_ok = False
            self.log("bundle: floor traces unavailable (%s); pieces use the reference height" % error)
            return z_ref
        if isinstance(hit, tuple):
            hit = hit[1] if len(hit) > 1 and hit[0] else None
        if not hit:
            self.misses += 1
            return None
        parts = hit.to_tuple()
        if not parts[0] or parts[7].z < 0.85 or parts[5].z > z_ref + up - 20:
            self.misses += 1
            return None
        return parts[5].z

    def clear(self, x, y, z, half_w, height):
        """True when a box over the footprint (from 15 cm above the floor) touches nothing that blocks."""
        if not self.traces_ok:
            return True
        try:
            res = unreal.SystemLibrary.box_overlap_actors(self.world, unreal.Vector(x, y, z + 15 + height / 2),
                                                          unreal.Vector(half_w, half_w, max(height / 2, 10)),
                                                          [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2],
                                                          None, [])
        except Exception as error:
            self.traces_ok = False
            self.log("bundle: overlap tests unavailable (%s)" % error)
            return True
        if isinstance(res, tuple):
            res = res[1] if len(res) > 1 else []
        return not res

    def free(self, x, y, r, use_blocked=True):
        if use_blocked and self.m.blocked(x, y, r * 0.5):
            return False
        if any(math.hypot(x - a, y - b) < r + c for a, b, c in self.keep):
            return False
        return all(math.hypot(x - a, y - b) > r + c for a, b, c in self.taken)

    # ---------------------------------------------------------------- spawning
    def put(self, e, x, y, z, yaw, folder, scale=1.0):
        m = self.m
        lo, hi = e["lo"], e["hi"]
        cx, cy, cz = (lo.x + hi.x) * 0.5 * scale, (lo.y + hi.y) * 0.5 * scale, lo.z * scale
        a = math.radians(yaw)
        rx, ry = cx * math.cos(a) - cy * math.sin(a), cx * math.sin(a) + cy * math.cos(a)
        actor = m.spawn(unreal.StaticMeshActor, (x - rx, y - ry, z - cz), yaw, 0.0, 0.0, e["name"], folder, ["BundleProp", e["pack"]])
        comp = actor.static_mesh_component
        comp.set_static_mesh(e["mesh"])
        if self.grey or m.force_material:
            for k in range(e["nslots"]):
                comp.set_material(k, m.mat[m.force_material or "PureGrey"])
        elif self.pastel and e["pack"] not in ("gothic", "cemetery"):
            colour = m.mat[self.rng.choice(PASTELS)]
            for k in range(e["nslots"]):
                comp.set_material(k, colour)
        else:
            for k in e["missing"]:
                comp.set_material(k, m.mat[e["fallback"]])
        small = e["cls"] == "small" or e["flat"]
        comp.set_collision_profile_name("NoCollision" if small else "BlockAll")
        comp.set_editor_property("ld_max_draw_distance", 6000.0 if e["cls"] == "small" else (25000.0 if e["cls"] == "medium" else 0.0))
        try:
            comp.set_editor_property("cast_shadow", e["cls"] != "small")
        except Exception:
            pass
        if scale != 1.0:
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        self.placed += 1
        self.used.add(e["path"])
        return actor

    def _try(self, e, x, y, z_ref, folder, up=250.0, use_blocked=True, yaw=None, check=True):
        w = max(e["size"][0], e["size"][1]) * 0.5
        r = max(60.0, w)
        if not self.free(x, y, r, use_blocked):
            return False
        z = self.floor(x, y, z_ref, up)
        if z is None:
            return False
        if check and not e["flat"] and not self.clear(x, y, z, w, e["size"][2]):
            return False
        self.taken.append((x, y, r))
        self.put(e, x, y, z, self.rng.uniform(0, 360) if yaw is None else yaw, folder)
        return True

    def vignette(self, x, y, z_ref, folder, classes=("medium",), up=250.0, use_blocked=True, groups=None):
        """One anchor piece and a few small pieces around it."""
        e = self._pick(classes, groups)
        if not e or not self._try(e, x, y, z_ref, folder, up, use_blocked):
            return False
        ring = max(e["size"][0], e["size"][1]) * 0.5 + 60
        for _ in range(self.rng.randint(2, 5)):
            s = self._pick(("small",))
            if not s:
                break
            az = self.rng.uniform(0, 360)
            d = ring + self.rng.uniform(20, 160)
            self._try(s, x + d * math.cos(math.radians(az)), y + d * math.sin(math.radians(az)), z_ref, folder, up, use_blocked)
        return True

    def clutter(self, x, y, z_ref, folder, up=250.0, use_blocked=True):
        e = self._pick(("small",))
        return bool(e) and self._try(e, x, y, z_ref, folder, up, use_blocked)

    def ring(self, centre, rmin, rmax, n, fn, tries=16):
        done = 0
        for _ in range(n * tries):
            if done >= n:
                break
            az = self.rng.uniform(0, 360)
            d = self.rng.uniform(rmin, rmax)
            if fn(centre[0] + d * math.cos(math.radians(az)), centre[1] + d * math.sin(math.radians(az))):
                done += 1
        return done

    def area(self, rect, z_ref, n, fn, tries=8):
        done = 0
        for _ in range(n * tries):
            if done >= n:
                break
            if fn(self.rng.uniform(rect[0], rect[2]), self.rng.uniform(rect[1], rect[3]), z_ref):
                done += 1
        return done


def _map_rect(m, margin=3000.0):
    """The middle 90% of the map's mesh pieces (x0, y0, x1, y1), from the actors spawned so far."""
    xs, ys = [], []
    for actor in m.actors.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            p = actor.get_actor_location()
            xs.append(p.x)
            ys.append(p.y)
    if not xs:
        return (-10000, -10000, 10000, 10000)
    xs.sort()
    ys.sort()
    k = len(xs) // 20
    return (xs[k] - margin, ys[k] - margin, xs[-k - 1] + margin, ys[-k - 1] + margin)


def _high_floor(d, x, y, top=40000.0):
    """For ambient pieces: the first upward-facing surface from high above (roofs, discs, terraces)."""
    return d.floor(x, y, top - 250.0, up=250.0, down=top * 2)


# ============================================================================ chapters

def chapter(m, n, log):
    if not available():
        log("bundle: WARNING /Game/Elite_Bundle missing (run Tools/import_elite_bundle.sh); chapter %d not dressed" % n)
        return
    t = THEMES[n]
    before = m.counts.get(FOLDER, 0)
    if n == 11:
        _nowhere(m, log, t)
    else:
        d = Dresser(m, log, t["pools"], 7000 + n, pastel=t.get("pastel", False))
        d.open_sky = n not in (6, 7, 8)
        secs = sorted(m.sections, key=lambda s: s[0])
        for num, centre, radius, boss in secs:
            d.keep.append((centre[0], centre[1], radius * 0.3))    # the fight's middle stays open
        for num, centre, radius, boss in secs:
            f = "%s/Section%d" % (FOLDER, num)
            cz = centre[2]
            d.ring(centre, radius * 0.4, radius * 1.05, t["vignettes"] + (4 if boss else 0),
                   lambda x, y: d.vignette(x, y, cz, f + "/Vignettes"))
            d.ring(centre, radius * 0.35, radius * 1.1, t["clutter"], lambda x, y: d.clutter(x, y, cz, f + "/Clutter"))
            if t["outer"]:
                d.ring(centre, radius * 1.05, radius * 1.6, t["outer"],
                       lambda x, y: d.vignette(x, y, cz, f + "/SetPieces", classes=("large", "medium")))
        for k in range(len(secs) - 1):
            (ax, ay, az), (bx, by, bz) = secs[k][1], secs[k + 1][1]
            length = math.hypot(bx - ax, by - ay)
            if length < 1:
                continue
            ux, uy = (bx - ax) / length, (by - ay) / length
            f = "%s/Routes/Route%d" % (FOLDER, k + 1)
            for _ in range(t["route"] * 3):
                s = d.rng.uniform(0.2, 0.8)
                side = d.rng.choice((-1, 1)) * d.rng.uniform(500, 1400)
                x, y = ax + (bx - ax) * s - uy * side, ay + (by - ay) * s + ux * side
                d.vignette(x, y, az + (bz - az) * s, f)
        for num, centre, radius, boss in secs:   # just outside the ring: office floors, tunnel bays, terraces at the section's height
            if t.get("near"):
                f = "%s/Section%d/Around" % (FOLDER, num)
                cz = centre[2]
                d.ring(centre, radius * 1.0, radius * 2.4, t["near"],
                       lambda x, y: d.vignette(x, y, cz, f, up=300.0) if d.rng.random() < 0.5 else d.clutter(x, y, cz, f, up=300.0),
                       tries=20)
        if t["ambient"]:
            rect = _map_rect(m)
            f = FOLDER + "/Ambient"
            near_z = lambda x, y: min(secs, key=lambda s: math.hypot(s[1][0] - x, s[1][1] - y))[1][2] if secs else 0.0
            d.area(rect, 0, t["ambient"] // 3, lambda x, y, z: _ambient(d, x, y, near_z(x, y), f, ("medium", "large")), tries=5)
            d.area(rect, 0, t["ambient"], lambda x, y, z: _ambient(d, x, y, near_z(x, y), f, ("small", "medium")), tries=4)
        _report(d, log, "chapter %d" % n, m.counts.get(FOLDER, 0) - before)


def _ambient(d, x, y, z_ref, folder, classes, up=800.0, down=3000.0):
    """A piece anywhere on the map: the first floor below `up` cm above the nearest section's height (under cavern roofs and
    ceilings; z_ref None = from high above: roofs, discs, terraces)."""
    e = d._pick(classes)
    if not e:
        return False
    z = _high_floor(d, x, y) if z_ref is None else d.floor(x, y, z_ref, up=up, down=down)
    if z is None and z_ref is not None and d.open_sky:   # floating islands and discs far above or below the nearest section
        z = _high_floor(d, x, y)
    if z is None:
        return False
    return d.vignette(x, y, z, folder, classes=(e["cls"],)) if e["cls"] != "small" else d.clutter(x, y, z, folder)


def _nowhere(m, log, t):
    """Return to Nowhere: one flat grey, no texture. Each leg of the walk echoes one past map (its _fragment), so each leg gets
    the props of that chapter, beside the path."""
    d = Dresser(m, log, THEMES[2]["pools"], 7011, grey=True)
    legs = getattr(m, "legs", [])
    for k, (a, b, heading) in enumerate(legs):
        d.set_pools(THEMES[min(10, k + 1)]["pools"])
        f = "%s/Leg%02d" % (FOLDER, k + 1)
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        ux, uy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
        done = 0
        for _ in range(t["per_leg"] * 6):
            if done >= t["per_leg"]:
                break
            s = d.rng.uniform(0.05, 0.95)
            side = d.rng.choice((-1, 1)) * d.rng.uniform(250, 650)    # on the 14 m path, off its middle line
            x, y = a[0] + (b[0] - a[0]) * s - uy * side, a[1] + (b[1] - a[1]) * s + ux * side
            fn = d.vignette if d.rng.random() < 0.55 else d.clutter
            if fn(x, y, 0, f, use_blocked=False):
                done += 1


def _report(d, log, what, added):
    log("bundle: %s: +%d bundle pieces (%d different meshes), %d floor misses" % (what, added, len(d.used), d.misses))


# ============================================================================ Neo Tokyo (Chapter 1)

def neotokyo(m, log, half):
    if not available():
        log("bundle: WARNING /Game/Elite_Bundle missing (run Tools/import_elite_bundle.sh); Neo Tokyo not dressed")
        return
    t = THEMES[1]
    before = m.counts.get(FOLDER, 0)
    d = Dresser(m, log, t["pools"], 7001)
    d.keep.append((0, 0, 2600))    # the scramble crossing stays open
    rect = (-half, -half, half, half)
    # Street level: a trace from 4 m down finds pavements and roads; inside a building the overlap test rejects the spot.
    d.area(rect, 0, t["street"], lambda x, y, z: (d.vignette(x, y, 0, FOLDER + "/Streets", up=400.0) if d.rng.random() < 0.4
                                                  else d.clutter(x, y, 0, FOLDER + "/Streets", up=400.0)), tries=6)
    d.set_pools([("dock", ["container", "tool", "structure"], 4), ("urban", None, 4)])
    d.area(rect, 0, t["roof"], lambda x, y, z: _ambient(d, x, y, None, FOLDER + "/Rooftops", ("medium", "small")), tries=6)
    _report(d, log, "Neo Tokyo", m.counts.get(FOLDER, 0) - before)


# ============================================================================ the Ark

def ark(m, log, town, civ, half_y, under_floor, platform_x0, reserved, factory, anchors, specials_rects=()):
    if not available():
        log("bundle: WARNING /Game/Elite_Bundle missing (run Tools/import_elite_bundle.sh); the Ark not dressed")
        return
    before = m.counts.get(FOLDER, 0)
    d = Dresser(m, log, [("kitchen", None, 1)], 7100)
    for x, y, _ in anchors:
        d.keep.append((x, y, 700))     # station rings and where the player lands

    def zone(name, rect, z, pools, vignettes, clutter, use_blocked=False):
        d.set_pools(pools)
        f = "%s/%s" % (FOLDER, name)
        d.area(rect, z, vignettes, lambda x, y, zz: d.vignette(x, y, zz, f, use_blocked=use_blocked))
        d.area(rect, z, clutter, lambda x, y, zz: d.clutter(x, y, zz, f, use_blocked=use_blocked))

    x0, y0, x1, y1 = reserved["merchant_row"]
    zone("MerchantRow", (x0 + 600, y0 + 2600, x1 - 900, y1 - 400), 8,
         [("kitchen", ["food", "container", "furniture"], 6), ("manor", ["decor"], 1), ("dock", ["container"], 1)], 40, 80)
    zone("Library", reserved["library"], 0, [("manor", ["furniture", "decor"], 6), ("anemoia", None, 3), ("kitchen", ["decor"], 1)], 30, 50)
    zone("TrainingGrounds", reserved["training"], 8, [("dock", ["container", "tool"], 4), ("urban", None, 3), ("kitchen", ["container"], 1)], 22, 30)
    zone("ArcadeHall", reserved["arcade"], 8, [("urban", None, 3), ("dock", ["container"], 2), ("anemoia", ["decor"], 1)], 12, 20)
    zone("GangHall", reserved["ganghall"], 0, [("gothic", ["grave"], 2), ("urban", None, 3), ("dock", ["container", "tool"], 3)], 16, 24)
    zone("Factory", factory, 0, [("dock", ["container", "tool", "furniture"], 5), ("urban", None, 4)], 30, 50)
    strip = 2000.0
    town_area = (town[0] + 1500, -half_y + strip + 800, town[1] - 1500, half_y - strip - 800)
    zone("TownSquare", town_area, 0, [("kitchen", ["container", "food"], 3), ("urban", None, 3), ("dock", ["container", "furniture"], 2),
                                      ("crypt", ["flora"], 2), ("cemetery", ["flora"], 2), ("manor", ["furniture"], 1)], 120, 200, True)
    zone("Underworld", (town[0] + 800, -half_y + 1200, town[1] - 800, half_y - 1200), under_floor,
         [("dock", None, 5), ("urban", None, 4), ("gothic", ["grave"], 1), ("kitchen", ["container"], 2)], 120, 200, True)
    zone("Civilian", (civ[0] + 1000, -half_y + 1500, civ[1] - 1000, half_y - 1500), 0,
         [("crypt", ["flora", "tree"], 5), ("cemetery", ["flora"], 4), ("gothic", ["grave"], 2), ("manor", ["furniture"], 1)], 120, 160, True)
    zone("Operators", (platform_x0 + 1500, -half_y + 1500, town[0] - 21500, half_y - 1500), 0,
         [("dock", ["container", "tool", "furniture"], 5), ("urban", None, 3), ("kitchen", ["container"], 1)], 60, 100, True)
    _report(d, log, "the Ark", m.counts.get(FOLDER, 0) - before)
