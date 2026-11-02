"""Elite Bundle dressing layer for the greybox generators (owner request 2026-10-07: replace the greybox with real assets).

Pure Python (no `unreal` import) so the rules are testable anywhere. It does nothing until a mapping file exists, so the
greybox maps build exactly as before. The mapping says which imported asset stands in for which greybox piece:

    Tools/elite_dress.json   (override with BIB_ELITE_DRESS=<path>; written after Tools/elite_scan.py has catalogued the bundle)

    {
      "version": 1,
      "materials": {"Concrete": "/Game/Elite/Materials/MI_Concrete", "Grass": "/Game/Elite/Materials/MI_Grass"},
      "kit_materials": {"HighRise": ["/Game/Elite/Materials/MI_Facade_A", "/Game/Elite/Materials/MI_Facade_B"]},
      "props": {
        "car":  {"meshes": ["/Game/Elite/Vehicles/SM_Car_A"], "damaged_meshes": ["/Game/Elite/Vehicles/SM_Car_Wreck"],
                 "yaw_offset": 0, "scale": 1.0},
        "lamp": {"meshes": ["/Game/Elite/Props/SM_Streetlight"], "height_from": "h"},
        "tree": {"meshes": ["/Game/Elite/Foliage/SM_Tree_A", "/Game/Elite/Foliage/SM_Tree_B"], "target_height": 900}
      },
      "maps": {"L_Ch10_Hallucinations": {"enabled": false}, "L_Ch03_Docks": {"materials": {"Sand": "/Game/Elite/Materials/MI_Sand"}}}
    }

Prop names are the function names in greybox_props.py (car, truck, lamp, bench, tree, palm, ...). A prop without a rule stays
greybox. A damaged call (wrecked / broken / dead / fallen) uses `damaged_meshes` and stays greybox when none are listed, so a
pristine mesh never stands in for a wreck. Several meshes in a list are chosen by a hash of the position: the same build always
gives the same map.

Never dressed, whatever the mapping says (owner briefs, docs/chapter-maps.md):
  * Chapter 11, Return to Nowhere: pure grey, nothing in it, no texture.
  * PROTECTED_MATERIALS: the traversal colour language (climb orange, jump teal, water, section rings) and the pieces whose
    colour is part of the owner's brief (the glowing A.R.C TECH cubes, the monolith and its red glyphs, the angels, the screen).
    How climb and jump routes should read in the finished look is OPEN (owner).
"""

import functools
import hashlib
import inspect
import json
import os

MAPPING_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "elite_dress.json")
NEVER_DRESS_MAPS = {"L_Ch11_ReturnToNowhere"}
PROTECTED_MATERIALS = {
    "Climb", "Ladder", "Jump", "Water", "Foam", "Section", "Void",
    "CubeGlow", "Monolith", "RedGlyph", "Angel", "Screen", "PureGrey", "FakeSky", "Holo",
}
DAMAGED_FLAGS = ("wrecked", "broken", "dead", "fallen")


def load_mapping(path=None):
    """The parsed mapping, or None when there is none (the normal greybox build)."""
    path = path or os.environ.get("BIB_ELITE_DRESS") or MAPPING_PATH
    if not os.path.isfile(path):
        return None
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("version") != 1:
        raise ValueError("%s: unsupported mapping version %r (expected 1)" % (path, data.get("version")))
    return data


def pick(options, key):
    """Deterministic choice from `options` by a hash of `key` (stable across rebuilds, unlike random)."""
    if not options:
        return None
    digest = hashlib.sha1(key.encode("utf-8")).digest()
    return options[int.from_bytes(digest[:4], "big") % len(options)]


class Dress:
    """The mapping resolved for one map."""

    def __init__(self, mapping, map_name, log=None):
        self.map_name = map_name
        self.log = log or (lambda *parts: None)
        override = (mapping.get("maps") or {}).get(map_name, {})
        self.enabled = bool(override.get("enabled", True)) and map_name not in NEVER_DRESS_MAPS
        self.materials = dict(mapping.get("materials") or {})
        self.materials.update(override.get("materials") or {})
        for name in list(self.materials):
            if name in PROTECTED_MATERIALS:
                self.log("dress: material %s is protected, mapping entry ignored" % name)
                del self.materials[name]
        self.props = dict(mapping.get("props") or {})
        self.props.update(override.get("props") or {})
        self.kit_materials = dict(mapping.get("kit_materials") or {})
        self.kit_materials.update(override.get("kit_materials") or {})
        self.used = {}   # what was actually dressed, for the build log
        self.missing = set()

    def material(self, name):
        """Asset path replacing the greybox material `name`, or None."""
        if not self.enabled:
            return None
        path = self.materials.get(name)
        if path:
            self.used["material " + name] = self.used.get("material " + name, 0) + 1
        return path

    def kit_material(self, mesh_path):
        """Material for an untextured Fab kit building (the first `kit_materials` key that is a substring of the mesh path,
        e.g. "HighRise" or "K2_Factory"); a list gives a hash-chosen variety. None keeps the kit as imported."""
        if not self.enabled:
            return None
        for needle, options in self.kit_materials.items():
            if needle in mesh_path:
                path = pick(options if isinstance(options, list) else [options], needle + "|" + mesh_path)
                self.used["kit " + needle] = self.used.get("kit " + needle, 0) + 1
                return path
        return None

    def place_prop(self, m, name, args):
        """Places the real asset for prop `name`. `args` are the prop call's bound arguments. True when placed (the greybox
        version is then skipped), False to build the greybox prop."""
        rule = self.props.get(name) if self.enabled else None
        if not rule or "x" not in args or "y" not in args or "z" not in args:
            return False
        damaged = any(args.get(flag) for flag in DAMAGED_FLAGS)
        options = rule.get("damaged_meshes") if damaged else rule.get("meshes")
        x, y, z = args["x"], args["y"], args["z"]
        path = pick(options, "%s|%s|%d|%d" % (self.map_name, name, round(x), round(y)))
        if not path:
            return False
        scale = float(rule.get("scale", 1.0))
        target = rule.get("target_height")
        if rule.get("height_from") and args.get(rule["height_from"]):
            target = args[rule["height_from"]]
        if target:
            _, _, height = m.kit_footprint(path, 1.0, 0.0)
            if height > 0:
                scale = float(target) / height
        try:
            m.place_kit(path, x, y, z, float(args.get("yaw", 0.0) or 0.0) + float(rule.get("yaw_offset", 0.0)), scale,
                        "Elite_" + name, args.get("folder") or "Props", tags=["Elite", "Elite_" + name])
        except RuntimeError as error:   # a mesh named in the mapping that is not in the project: keep the greybox prop
            if path not in self.missing:
                self.missing.add(path)
                self.log("dress: mesh missing, greybox kept for %s (%s)" % (name, error))
            return False
        self.used["prop " + name] = self.used.get("prop " + name, 0) + 1
        return True

    def summary(self):
        if self.missing:
            self.log("dress: %d mapped meshes are missing from the project (is the pack at Content/<PackName>?)" % len(self.missing))
        if not self.used:
            return "dress: nothing replaced in %s" % self.map_name
        return "dress %s: %s" % (self.map_name, ", ".join("%s x%d" % kv for kv in sorted(self.used.items())))


def for_map(map_name, log=None, mapping=None):
    """The Dress for a map, or None when no mapping exists (greybox as before)."""
    mapping = mapping if mapping is not None else load_mapping()
    return Dress(mapping, map_name, log) if mapping else None


def dressable(name):
    """Decorator for the greybox_props functions: when the map has a rule for `name`, the real asset replaces the whole prop.
    Props without x / y / z arguments (wires, pipes, window grids) pass through unchanged."""
    def decorate(fn):
        signature = inspect.signature(fn)

        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            m = args[0] if args else kwargs.get("m")
            dress = getattr(m, "dress", None)
            if dress is not None:
                bound = signature.bind(*args, **kwargs)
                bound.apply_defaults()
                if dress.place_prop(m, name, bound.arguments):
                    return None
            return fn(*args, **kwargs)
        return wrapper
    return decorate


def dress_props(namespace):
    """Wraps every public prop function in `namespace` (the globals() of greybox_props) with `dressable`."""
    for name, fn in list(namespace.items()):
        if callable(fn) and not name.startswith("_") and getattr(fn, "__module__", None) == namespace.get("__name__"):
            params = inspect.signature(fn).parameters
            if all(p in params for p in ("m", "x", "y", "z", "folder")):
                namespace[name] = dressable(name.rstrip("_"))(fn)
