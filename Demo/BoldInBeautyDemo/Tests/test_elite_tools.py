"""Checks for Tools/elite_scan.py and Tools/greybox_dress.py (no Unreal needed):  python3 Tests/test_elite_tools.py"""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Tools"))

import elite_autodress as A  # noqa: E402
import elite_scan  # noqa: E402
import greybox_dress as D  # noqa: E402


def touch(root, rel, size=10, text=None):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as handle:
        handle.write(text if text is not None else "x" * size)


class ScanTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        r = self.tmp.name
        touch(r, "CityPack/Content/Buildings/SM_Tower_01.uasset")
        touch(r, "CityPack/Content/Materials/MI_Concrete.uasset")
        touch(r, "CityPack/Content/Textures/T_Concrete_D.uasset")
        touch(r, "CityPack/CityPack.uproject", text=json.dumps({"EngineAssociation": "5.3"}))
        touch(r, "CityPack/License.txt")
        touch(r, "RawProps/Meshes/Car/car_01.fbx", 500)
        touch(r, "RawProps/Meshes/Foliage/tree_a.fbx", 700)
        touch(r, "RawProps/Tex/rock_d.png", 300)
        touch(r, "Zipped/Everything.zip", 4000)
        touch(r, "readme.txt")
        touch(r, ".DS_Store")
        self.data = elite_scan.scan(r)
        self.packs = {p["name"]: p for p in self.data["packs"]}

    def tearDown(self):
        self.tmp.cleanup()

    def test_packs_and_counts(self):
        self.assertEqual(set(self.packs), {"CityPack", "RawProps", "Zipped", "(loose files in root)"})
        self.assertEqual(self.packs["RawProps"]["files"], 3)
        self.assertEqual(self.packs["RawProps"]["kinds"], {"mesh": 2, "texture": 1})
        self.assertEqual(self.packs["(loose files in root)"]["files"], 1)   # .DS_Store ignored

    def test_unreal_pack(self):
        p = self.packs["CityPack"]
        self.assertTrue(p["has_content_dir"])
        self.assertEqual(p["engine"], ["5.3"])
        self.assertEqual(p["licence_files"], ["License.txt"])
        self.assertEqual([s["path"] for s in p["samples"]["mesh"]], ["Content/Buildings/SM_Tower_01.uasset"])
        self.assertEqual(len(p["samples"]["material"]), 1)
        self.assertIn("Unreal-native", elite_scan.verdict(p))

    def test_raw_and_archive_verdicts(self):
        self.assertIn("raw meshes", elite_scan.verdict(self.packs["RawProps"]))
        self.assertIn("archives only", elite_scan.verdict(self.packs["Zipped"]))
        self.assertEqual(self.packs["Zipped"]["archives"][0]["path"], "Everything.zip")

    def test_categories(self):
        self.assertEqual(elite_scan.classify("Meshes/Foliage/tree_a.fbx"), "foliage")
        self.assertEqual(elite_scan.classify("Content/Vehicles/SM_Truck.uasset"), "vehicle")
        self.assertEqual(elite_scan.classify("zzz/abc.fbx"), "uncategorised")

    def test_markdown_renders_and_is_names_only(self):
        md = elite_scan.render_markdown(self.data)
        self.assertIn("| CityPack |", md)
        self.assertIn("nothing was copied or opened", md)

    def test_missing_root_is_an_error(self):
        with self.assertRaises(SystemExit):
            elite_scan.scan(os.path.join(self.tmp.name, "nope"))


class StubMap:
    """Just enough of GreyboxMap for Dress.place_prop."""

    def __init__(self):
        self.placed = []
        self.heights = {"/Game/E/SM_Lamp": 400.0}

    def kit_footprint(self, path, scale=1.0, yaw=0.0):
        return 100.0, 100.0, self.heights.get(path, 200.0) * scale

    def place_kit(self, path, x, y, z, yaw, scale, label, folder, tags=None, material=None):
        self.placed.append((path, x, y, z, yaw, scale, label, folder, tags))


MAPPING = {
    "version": 1,
    "materials": {"Concrete": "/Game/E/MI_Concrete", "Climb": "/Game/E/MI_ShouldBeIgnored"},
    "kit_materials": {"HighRise": ["/Game/E/MI_FacadeA", "/Game/E/MI_FacadeB"]},
    "props": {
        "car": {"meshes": ["/Game/E/SM_CarA", "/Game/E/SM_CarB"], "damaged_meshes": ["/Game/E/SM_Wreck"], "yaw_offset": 90},
        "lamp": {"meshes": ["/Game/E/SM_Lamp"], "height_from": "h"},
        "tree": {"meshes": ["/Game/E/SM_Tree"]},
    },
    "maps": {"L_Ch10_Hallucinations": {"enabled": False}, "L_Ch03_Docks": {"materials": {"Concrete": "/Game/E/MI_DockStone"}}},
}


class DressTests(unittest.TestCase):
    def test_no_mapping_means_greybox(self):
        self.assertIsNone(D.for_map("L_Ark", mapping={}))
        self.assertIsNone(D.load_mapping(os.path.join(tempfile.gettempdir(), "does-not-exist.json")))

    def test_protected_and_forced_off(self):
        logs = []
        dress = D.for_map("L_NeoTokyo", logs.append, MAPPING)
        self.assertIsNone(dress.material("Climb"))               # traversal colour language stays
        self.assertEqual(dress.material("Concrete"), "/Game/E/MI_Concrete")
        self.assertTrue(any("protected" in line for line in logs))
        nowhere = D.for_map("L_Ch11_ReturnToNowhere", None, MAPPING)   # owner brief: pure grey, no texture
        self.assertIsNone(nowhere.material("Concrete"))
        self.assertFalse(nowhere.place_prop(StubMap(), "car", {"x": 0, "y": 0, "z": 0}))
        halluc = D.for_map("L_Ch10_Hallucinations", None, MAPPING)
        self.assertIsNone(halluc.material("Concrete"))

    def test_per_map_override(self):
        self.assertEqual(D.for_map("L_Ch03_Docks", None, MAPPING).material("Concrete"), "/Game/E/MI_DockStone")

    def test_kit_materials(self):
        dress = D.for_map("L_NeoTokyo", None, MAPPING)
        path = "/Game/Greybox/Kits/K1/HighRise/SM_K1_HighRise_01"
        self.assertIn(dress.kit_material(path), ["/Game/E/MI_FacadeA", "/Game/E/MI_FacadeB"])
        self.assertEqual(dress.kit_material(path), dress.kit_material(path))   # stable across calls
        self.assertIsNone(dress.kit_material("/Game/Greybox/Kits/K1/Cottage/SM_x"))

    def test_place_prop_and_damaged(self):
        dress, m = D.for_map("L_NeoTokyo", None, MAPPING), StubMap()
        self.assertTrue(dress.place_prop(m, "car", {"x": 100, "y": 200, "z": 5, "yaw": 30, "folder": "Street", "wrecked": False}))
        path, x, y, z, yaw, scale, label, folder, tags = m.placed[-1]
        self.assertIn(path, ["/Game/E/SM_CarA", "/Game/E/SM_CarB"])
        self.assertEqual((x, y, z, yaw, scale, label, folder), (100, 200, 5, 120.0, 1.0, "Elite_car", "Street"))
        self.assertTrue(dress.place_prop(m, "car", {"x": 1, "y": 2, "z": 0, "yaw": 0, "folder": "S", "wrecked": True}))
        self.assertEqual(m.placed[-1][0], "/Game/E/SM_Wreck")
        # no damaged mesh listed: the pristine one must not stand in for a wreck, the greybox is built instead
        self.assertFalse(dress.place_prop(m, "tree", {"x": 1, "y": 2, "z": 0, "folder": "S", "dead": True}))
        self.assertFalse(dress.place_prop(m, "bench", {"x": 1, "y": 2, "z": 0, "folder": "S"}))   # no rule

    def test_height_from_argument(self):
        dress, m = D.for_map("L_NeoTokyo", None, MAPPING), StubMap()
        dress.place_prop(m, "lamp", {"x": 0, "y": 0, "z": 0, "yaw": 0, "folder": "S", "h": 800.0})
        self.assertAlmostEqual(m.placed[-1][5], 2.0)   # 800 wanted / 400 native

    def test_choice_is_deterministic(self):
        options = ["a", "b", "c", "d"]
        self.assertEqual(D.pick(options, "k"), D.pick(options, "k"))
        self.assertEqual({D.pick(options, "k%d" % i) for i in range(200)}, set(options))

    def test_dress_props_wraps_only_placeable_functions(self):
        ns = {"__name__": "fake_props"}
        calls = []

        def car(m, x, y, z, yaw, folder, wrecked=False):
            calls.append("greybox car")

        def wire(m, a, b, folder):
            calls.append("greybox wire")
        car.__module__ = wire.__module__ = "fake_props"
        ns.update(car=car, wire=wire)
        D.dress_props(ns)
        self.assertIsNot(ns["car"], car)
        self.assertIs(ns["wire"], wire)

        class M:
            dress = None
        ns["car"](M(), 0, 0, 0, 0, "f")
        self.assertEqual(calls, ["greybox car"])              # no dress object: greybox as before
        m = StubMap()
        m.dress = D.for_map("L_NeoTokyo", None, MAPPING)
        ns["car"](m, 5, 5, 0, 0, "f")
        self.assertEqual(calls, ["greybox car"])              # real asset placed, greybox skipped
        self.assertEqual(len(m.placed), 1)
        ns["car"](m=m, x=6, y=6, z=0, yaw=0, folder="f", wrecked=True)   # keyword call, damaged
        self.assertEqual(m.placed[-1][0], "/Game/E/SM_Wreck")


def mesh(pack, name, h):
    return {"path": "/Game/%s/Meshes/%s" % (pack, name), "pack": pack, "name": name, "size": [100.0, 100.0, float(h)]}


LIBRARY = {"version": 1, "meshes": [
    mesh("UrbanProps_Vol1", "SM_Barrel_01", 90), mesh("UrbanProps_Vol1", "SM_Barrel_Broken_02", 80),
    mesh("UrbanProps_Vol1", "SM_Bench02a", 85), mesh("UrbanProps_Vol1", "SM_StreetLight_03", 620),
    mesh("UrbanProps_Vol1", "SM_Barrel_Giant", 900),                 # too tall for a greybox barrel
    mesh("Foliage_Sets", "SM_OakTree_01", 1200), mesh("Foliage_Sets", "SM_OakTree_Dead_02", 900),
    mesh("Foliage_Sets", "SM_Tree_Stump_01", 80), mesh("Foliage_Sets", "SM_Bush_A", 120),
    mesh("Stone_Decor", "SM_Rock_Large_01", 300), mesh("Stone_Decor", "SM_Rock_Wall_Piece", 300),
    mesh("FeudalJapanExteriorProps_Vol1_Meshingun", "SM_Lantern_Stone", 150), mesh("FeudalJapanExteriorProps_Vol1_Meshingun", "SM_Bench_Wood01", 60),
    mesh("MedievalVOL9", "SM_Table_05a", 90), mesh("MedievalVOL9", "SM_Barrel_Medieval", 100),   # pack allowed on no map
]}
MAP_PACKS = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Tools", "elite_map_packs.json")))


class AutodressTests(unittest.TestCase):
    def setUp(self):
        self.mapping, self.coverage = A.build(LIBRARY, MAP_PACKS)

    def props(self, map_name):
        return self.mapping["maps"].get(map_name, {}).get("props", {})

    def test_words(self):
        self.assertEqual(A.words("SM_BenchWood02a"), "bench wood 02a")
        self.assertEqual(A.words("SM_Barrel_Broken_02"), "barrel broken 02")

    def test_name_and_size_rules(self):
        neo = self.props("L_NeoTokyo")
        self.assertEqual(neo["barrel"]["meshes"], ["/Game/UrbanProps_Vol1/Meshes/SM_Barrel_01"])   # giant barrel too tall
        self.assertEqual(neo["barrel"]["damaged_meshes"], ["/Game/UrbanProps_Vol1/Meshes/SM_Barrel_Broken_02"])
        self.assertEqual(neo["bench"]["meshes"], ["/Game/UrbanProps_Vol1/Meshes/SM_Bench02a"])
        self.assertEqual(neo["lamp"]["height_from"], "h")
        self.assertNotIn("tree", neo)                                       # Foliage_Sets is not a Neo Tokyo pack

    def test_pack_gating_by_brief(self):
        ruins = self.props("L_Ch02_Ruins")
        self.assertEqual(ruins["tree"]["meshes"], ["/Game/Foliage_Sets/Meshes/SM_OakTree_01"])     # stump excluded
        self.assertEqual(ruins["tree"]["damaged_meshes"], ["/Game/Foliage_Sets/Meshes/SM_OakTree_Dead_02"])
        self.assertEqual(self.props("L_Ch02_Ruins")["rock"]["meshes"], ["/Game/Stone_Decor/Meshes/SM_Rock_Large_01"])   # wall piece excluded
        japan = self.props("L_Ch13_SecretiveSoul")
        self.assertEqual(japan["bench"]["meshes"], ["/Game/FeudalJapanExteriorProps_Vol1_Meshingun/Meshes/SM_Bench_Wood01"])
        for none in ("L_Ch11_ReturnToNowhere", "L_Ch10_Hallucinations", "L_Ch05_ArcTech", "L_Ch07_TOMBS", "L_Ark"):
            self.assertNotIn(none, self.mapping["maps"])
        for banned in self.mapping["maps"].values():                          # MedievalVOL9 is allowed on no map
            self.assertNotIn("MedievalVOL9", json.dumps(banned))

    def test_report_lists_every_map(self):
        text = A.report(self.coverage)
        self.assertEqual(len(self.coverage), len(MAP_PACKS["maps"]))
        self.assertIn("none: stays greybox", text)

    def test_generated_mapping_drives_the_dress_layer(self):
        dress, m = D.for_map("L_Ch02_Ruins", None, self.mapping), StubMap()
        self.assertTrue(dress.place_prop(m, "tree", {"x": 0, "y": 0, "z": 0, "folder": "F", "dead": True}))
        self.assertEqual(m.placed[-1][0], "/Game/Foliage_Sets/Meshes/SM_OakTree_Dead_02")
        self.assertFalse(D.for_map("L_Ch11_ReturnToNowhere", None, self.mapping) and
                         D.for_map("L_Ch11_ReturnToNowhere", None, self.mapping).place_prop(m, "tree", {"x": 0, "y": 0, "z": 0}))

    def test_missing_mesh_keeps_the_greybox(self):
        class Missing(StubMap):
            def place_kit(self, *a, **k):
                raise RuntimeError("missing kit mesh")
        logs = []
        dress = D.for_map("L_NeoTokyo", logs.append, MAPPING)
        self.assertFalse(dress.place_prop(Missing(), "tree", {"x": 0, "y": 0, "z": 0, "folder": "F"}))
        self.assertTrue(dress.missing and any("missing" in line for line in logs))


if __name__ == "__main__":
    unittest.main(verbosity=2)
