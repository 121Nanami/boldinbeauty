"""Shared helpers for the greybox maps (build_greybox.py): greybox materials, a map builder that places kit meshes by their
bounds, basic-shape blocks, editor labels, domes, stairs and railings. Units are centimetres; Z up; yaw in degrees."""

import math
import os

import unreal

import greybox_dress

GREYBOX = "/Game/Greybox"
MATERIALS = GREYBOX + "/Materials"
MAPS = GREYBOX + "/Maps"
SHAPES = {
    "cube": "/Engine/BasicShapes/Cube.Cube",
    "cylinder": "/Engine/BasicShapes/Cylinder.Cylinder",
    "sphere": "/Engine/BasicShapes/Sphere.Sphere",
    "cone": "/Engine/BasicShapes/Cone.Cone",
}

# Greybox palette: (base colour, emissive strength, roughness). Emissive entries are placeholders for real neon / signage.
PALETTE = {
    "Grey": ((0.35, 0.35, 0.37), 0.0, 0.7),
    "Dark": ((0.08, 0.08, 0.09), 0.0, 0.8),
    "Concrete": ((0.22, 0.22, 0.24), 0.0, 0.9),
    # Owner 2026-10-07 (no extreme white light): wet roads reflect blurred neon instead of mirroring the bright sky as glare.
    "WetRoad": ((0.03, 0.03, 0.035), 0.0, 0.2),
    "Sidewalk": ((0.12, 0.12, 0.13), 0.0, 0.32),
    "Metal": ((0.4, 0.42, 0.45), 0.0, 0.35),
    "Climb": ((0.85, 0.55, 0.1), 0.0, 0.6),     # anything a player can climb or hang on: orange, as usual in greybox
    "Ladder": ((0.9, 0.8, 0.1), 0.0, 0.6),
    "Wear": ((0.05, 0.04, 0.03), 0.0, 0.95),
    "RedStripe": ((0.8, 0.02, 0.02), 0.6, 0.5),
    "Grass": ((0.08, 0.45, 0.08), 0.0, 0.8),
    "Hedge": ((0.04, 0.25, 0.05), 0.0, 0.8),
    "Tree": ((0.06, 0.38, 0.1), 0.0, 0.8),
    "Trunk": ((0.2, 0.12, 0.06), 0.0, 0.9),
    "FakeSky": ((0.45, 0.7, 1.0), 1.5, 0.9),
    "Glass": ((0.25, 0.35, 0.4), 0.0, 0.05),
    "Marking": ((0.9, 0.9, 0.9), 0.3, 0.6),
    "Lamp": ((1.0, 0.85, 0.6), 8.0, 0.5),
    "NeonPink": ((1.0, 0.05, 0.6), 20.0, 0.4),
    "NeonCyan": ((0.0, 0.9, 1.0), 20.0, 0.4),
    "NeonPurple": ((0.55, 0.1, 1.0), 20.0, 0.4),
    "NeonYellow": ((1.0, 0.8, 0.0), 20.0, 0.4),
    "NeonGreen": ((0.1, 1.0, 0.3), 20.0, 0.4),
    "NeonRed": ((1.0, 0.05, 0.05), 20.0, 0.4),
    "Holo": ((0.2, 0.6, 1.0), 10.0, 0.4),
    "Chute": ((0.3, 0.32, 0.36), 0.0, 0.3),
    "Void": ((0.0, 0.0, 0.0), 0.0, 1.0),
    # Chapter maps (greybox_chapters.py). Teal = a ledge or block you can jump up onto; blue = swimmable water.
    "Jump": ((0.1, 0.75, 0.7), 0.0, 0.6),
    "Water": ((0.03, 0.25, 0.6), 0.2, 0.05),
    "Foam": ((0.85, 0.9, 0.95), 0.5, 0.4),
    "Section": ((1.0, 1.0, 1.0), 2.0, 0.6),     # section edge ring and gates
    "Rubble": ((0.28, 0.25, 0.22), 0.0, 0.95),
    "Sand": ((0.6, 0.48, 0.3), 0.0, 0.95),
    "Rock": ((0.18, 0.18, 0.2), 0.0, 0.9),
    "Wood": ((0.35, 0.22, 0.12), 0.0, 0.85),
    "Chain": ((0.25, 0.25, 0.28), 0.0, 0.3),
    "CubeGlow": ((1.0, 0.85, 0.05), 200.0, 0.3),   # the A.R.C TECH cubes: "insanely bright yellow"
    "Hazard": ((0.95, 0.75, 0.05), 0.0, 0.5),
    "Flora": ((0.12, 0.5, 0.15), 0.0, 0.85),
    "Moon": ((0.55, 0.58, 0.7), 0.15, 0.9),
    "Angel": ((1.0, 0.97, 0.9), 30.0, 0.3),
    "Screen": ((0.3, 1.0, 0.4), 15.0, 0.2),
    "Monolith": ((0.0, 0.0, 0.0), 0.0, 0.3),
    "RedGlyph": ((1.0, 0.0, 0.02), 25.0, 0.4),
    "Paddy": ((0.25, 0.4, 0.15), 0.0, 0.3),
    "Roof": ((0.12, 0.12, 0.16), 0.0, 0.7),
    "Horror": ((0.3, 0.12, 0.1), 0.0, 0.25),     # "hyper realistic terrifying things": placeholder only
    "PureGrey": ((0.5, 0.5, 0.5), 0.0, 1.0),     # Return to Nowhere: one flat grey, nothing else
    "PastelPink": ((1.0, 0.7, 0.8), 0.3, 0.9),
    "PastelBlue": ((0.65, 0.8, 1.0), 0.3, 0.9),
    "PastelMint": ((0.7, 1.0, 0.85), 0.3, 0.9),
    "PastelLemon": ((1.0, 0.97, 0.65), 0.3, 0.9),
    "PastelLilac": ((0.82, 0.72, 1.0), 0.3, 0.9),
    "Coral": ((1.0, 0.35, 0.3), 0.0, 0.6),
    "Turquoise": ((0.0, 0.8, 0.75), 0.0, 0.5),
    "Lime": ((0.55, 0.95, 0.1), 0.0, 0.6),
    "Sunflower": ((1.0, 0.8, 0.1), 0.0, 0.6),
    "Magenta": ((0.95, 0.1, 0.6), 0.0, 0.6),
    "Palm": ((0.05, 0.6, 0.2), 0.0, 0.7),
    "Rust": ((0.35, 0.15, 0.06), 0.0, 0.8),
    "Asphalt": ((0.06, 0.06, 0.07), 0.0, 0.6),
    "Moss": ((0.1, 0.32, 0.08), 0.0, 0.95),
    "Bamboo": ((0.4, 0.55, 0.2), 0.0, 0.6),
    "Stone": ((0.42, 0.41, 0.38), 0.0, 0.9),
    "Cloud": ((1.0, 0.97, 1.0), 0.6, 1.0),
    # Story placeholders (greybox_story.py): RED = NeonRed, BLUE = MarkBlue, YELLOW = NeonYellow, GREEN = NeonGreen, PURPLE = NeonPurple
    "MarkBlue": ((0.1, 0.3, 1.0), 14.0, 0.4),
}
NEON = ["NeonPink", "NeonCyan", "NeonPurple", "NeonYellow", "NeonGreen", "NeonRed"]


def build_materials():
    """M_GB: Color / Emissive / Roughness parameters; one MI_GB_<name> per palette entry."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    eal = unreal.EditorAssetLibrary
    if eal.does_directory_exist(MATERIALS):
        eal.delete_directory(MATERIALS)
    mat = tools.create_asset("M_GB", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    colour = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(0.35, 0.35, 0.37, 1.0))
    emissive = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 250)
    emissive.set_editor_property("parameter_name", "Emissive")
    emissive.set_editor_property("default_value", 0.0)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 400)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.7)
    multiply = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    mel.connect_material_expressions(colour, "", multiply, "A")
    mel.connect_material_expressions(emissive, "", multiply, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Bundle meshes (greybox_bundle.py) are Nanite and take these colours for slots the bundle left empty; without the flag a
    # packaged game shows the engine default material on them.
    try_set(mat, "used_with_nanite", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, only_if_is_dirty=False)
    out = {}
    for name, (rgb, glow, roughness) in PALETTE.items():
        mi = tools.create_asset("MI_GB_" + name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", mat)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Emissive", glow)
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        eal.save_loaded_asset(mi, only_if_is_dirty=False)
        out[name] = mi
    return out


def try_set(obj, prop, value, log=None):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as error:  # property names drift between engine versions; a missing one must not stop the build
        if log:
            log("warn: %s.%s not set (%s)" % (type(obj).__name__, prop, error))
        return False


class GreyboxMap:
    def __init__(self, path, materials, log):
        self.path = path
        self.mat = materials
        self.log = log
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
        if not self.levels.new_level(path):
            raise RuntimeError("new_level failed: " + path)
        self.shapes = {k: unreal.load_asset(v) for k, v in SHAPES.items()}
        self.mesh_cache = {}
        self.counts = {}
        self.kits = []        # every place_kit result: detail passes dress them and keep props out of them
        self.keepout = []     # extra (x0, y0, x1, y1) rects props must avoid
        self.sections = []    # (n, centre, radius, boss)
        self.force_material = None   # Chapter 11: every piece uses one flat grey
        # Elite Bundle dressing (greybox_dress.py): None until Tools/elite_dress.json exists, then the greybox build is unchanged.
        self.dress = greybox_dress.for_map(os.path.basename(path), log)
        self._dress_assets = {}

    # ------------------------------------------------------------------ spawning
    def spawn(self, actor_class, location, yaw=0.0, pitch=0.0, roll=0.0, label=None, folder=None, tags=None):
        actor = self.actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
        if not actor:
            raise RuntimeError("spawn failed: %s" % actor_class)
        if label:
            actor.set_actor_label(label)
        if folder:
            actor.set_folder_path(folder)
        if tags:
            actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
        top = (folder or "Misc").split("/")[0]
        self.counts[top] = self.counts.get(top, 0) + 1
        return actor

    def _dress_asset(self, path):
        """A real material from the mapping, loaded once; None (greybox stays) when it cannot be loaded."""
        if path not in self._dress_assets:
            asset = unreal.load_asset(path)
            if asset is None:
                self.log("warn: dress material missing, greybox kept: " + path)
            self._dress_assets[path] = asset
        return self._dress_assets[path]

    def _mesh_actor(self, mesh, location, yaw, pitch, roll, scale, label, folder, tags, material, collision=True):
        actor = self.spawn(unreal.StaticMeshActor, location, yaw, pitch, roll, label, folder, tags)
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        if self.force_material:
            material = self.force_material
        if material:
            real = None
            if self.dress and not self.force_material:
                real_path = self.dress.material(material)
                real = self._dress_asset(real_path) if real_path else None
            for slot in range(max(1, comp.get_num_materials())):   # kit meshes have several slots
                comp.set_material(slot, real or self.mat[material])
        comp.set_collision_profile_name("BlockAll" if collision else "NoCollision")
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    def block(self, centre, size, label, folder, material="Grey", yaw=0.0, pitch=0.0, roll=0.0, tags=None, shape="cube",
              collision=True, hidden_in_game=False):
        """A basic shape centred on `centre` with `size` in cm along its own axes."""
        actor = self._mesh_actor(self.shapes[shape], centre, yaw, pitch, roll, (size[0] / 100.0, size[1] / 100.0, size[2] / 100.0),
                                 label, folder, tags, material, collision)
        if hidden_in_game:
            actor.set_actor_hidden_in_game(True)
        return actor

    def box(self, x0, y0, z0, x1, y1, z1, label, folder, material="Grey", tags=None, **kw):
        """Axis-aligned block from two corners."""
        return self.block(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)), label, folder,
                          material, tags=tags, **kw)

    def kit(self, path):
        if path not in self.mesh_cache:
            mesh = unreal.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError("missing kit mesh " + path)
            box = mesh.get_bounding_box()
            self.mesh_cache[path] = (mesh, box.min, box.max)
        return self.mesh_cache[path]

    def kit_footprint(self, path, scale=1.0, yaw=0.0):
        """(width along world X, depth along world Y, height) after scale and a yaw that is a multiple of 90."""
        _, lo, hi = self.kit(path)
        sx, sy, sz = (hi.x - lo.x) * scale, (hi.y - lo.y) * scale, (hi.z - lo.z) * scale
        if int(round(yaw / 90.0)) % 2:
            sx, sy = sy, sx
        return sx, sy, sz

    def place_kit(self, path, x, y, ground_z, yaw, scale, label, folder, tags=None, material=None):
        """Puts the bottom centre of the mesh bounds at (x, y, ground_z): the kits' pivots are far off-centre."""
        mesh, lo, hi = self.kit(path)
        cx, cy, cz = (lo.x + hi.x) * 0.5 * scale, (lo.y + hi.y) * 0.5 * scale, lo.z * scale
        a = math.radians(yaw)
        rx, ry = cx * math.cos(a) - cy * math.sin(a), cx * math.sin(a) + cy * math.cos(a)
        actor = self._mesh_actor(mesh, (x - rx, y - ry, ground_z - cz), yaw, 0.0, 0.0, (scale, scale, scale), label, folder,
                                 tags, material)
        real_path = self.dress.kit_material(path) if (self.dress and not material and not self.force_material) else None
        real = self._dress_asset(real_path) if real_path else None
        if real:
            for slot in range(max(1, actor.static_mesh_component.get_num_materials())):
                actor.static_mesh_component.set_material(slot, real)
        w, d, h = self.kit_footprint(path, scale, yaw)
        result = {"actor": actor, "rect": (x - w / 2, y - d / 2, x + w / 2, y + d / 2), "top": ground_z + h, "path": path,
                  "ground": ground_z, "folder": folder}
        self.kits.append(result)
        return result

    def blocked(self, x, y, pad=200.0):
        """True when (x, y) is inside a placed kit building or a keep-out rect (plus `pad`)."""
        for r in [k["rect"] for k in self.kits] + self.keepout:
            if r[0] - pad <= x <= r[2] + pad and r[1] - pad <= y <= r[3] + pad:
                return True
        return False

    # ------------------------------------------------------------------ dressing
    def label(self, text, location, folder, size=300.0, yaw=180.0, in_game=False, colour=(255, 255, 255)):
        """Text in the level. Editor-only (hidden in game) unless `in_game`: unmarked buildings stay unmarked in play."""
        actor = self.spawn(unreal.TextRenderActor, location, yaw=yaw, label="LABEL_" + text.replace(" ", "_")[:40], folder=folder)
        comp = actor.text_render
        comp.set_editor_property("text", text)
        comp.set_editor_property("world_size", size)
        comp.set_editor_property("text_render_color", unreal.Color(r=colour[0], g=colour[1], b=colour[2], a=255))
        for enum_name in ("HorizTextAligment", "HorizTextAlignment"):
            if hasattr(unreal, enum_name):
                comp.set_editor_property("horizontal_alignment", getattr(unreal, enum_name).EHTA_CENTER)
                break
        if not in_game:
            actor.set_actor_hidden_in_game(True)
        return actor

    def point_light(self, location, folder, intensity=50000.0, radius=8000.0, colour=(1.0, 0.9, 0.8), shadows=False):
        light = self.spawn(unreal.PointLight, location, label="PointLight", folder=folder)
        comp = light.point_light_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        try_set(comp, "intensity", intensity, self.log)
        try_set(comp, "attenuation_radius", radius, self.log)
        try_set(comp, "light_color", unreal.Color(r=int(colour[0] * 255), g=int(colour[1] * 255), b=int(colour[2] * 255), a=255), self.log)
        try_set(comp, "cast_shadows", shadows, self.log)
        return light

    def railing(self, a, b, folder, height=100.0, spacing=400.0, rail=True, material="Metal", skip=None):
        """Posts from point a to point b (floor level) plus one thin rail on top. `skip(i)` leaves post i out."""
        dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        if length < 1:
            return
        steps = max(1, int(length // spacing))
        for i in range(steps + 1):
            if skip and skip(i):
                continue
            t = i / float(steps)
            self.block((a[0] + dx * t, a[1] + dy * t, a[2] + dz * t + height / 2), (8, 8, height), "RailPost", folder, material)
        if rail:
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
            mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2 + height)
            self.block(mid, (length, 6, 6), "Rail", folder, material, yaw=yaw, pitch=pitch)

    def stairs(self, start, yaw, steps, rise, run, width, folder, material="Concrete", tags=None):
        """Straight stairs from `start` (bottom front edge centre) going along `yaw`; returns the top landing point."""
        a = math.radians(yaw)
        fx, fy = math.cos(a), math.sin(a)
        for k in range(steps):
            d = run * (k + 0.5)
            top = start[2] + rise * (k + 1)
            self.block((start[0] + fx * d, start[1] + fy * d, top - rise / 2), (run, width, rise), "Step", folder, material,
                       yaw=yaw, tags=tags)
        return (start[0] + fx * run * steps, start[1] + fy * run * steps, start[2] + rise * steps)

    def dome(self, centre, radius, folder, n_az=32, n_el=8, thickness=150.0, material="Concrete", gap=None):
        """A hemisphere shell of panels; `gap(az_deg, band)` returns True to leave a panel out (doors, exits)."""
        cx, cy, cz = centre
        band = 90.0 / n_el
        for j in range(n_el):
            el = (j + 0.5) * band
            ring_r = radius * math.cos(math.radians(el))
            height = radius * math.radians(band) * 1.06
            width = 2 * math.pi * ring_r / n_az * 1.08 + 40
            for i in range(n_az):
                az = i * 360.0 / n_az
                if gap and gap(az, j):
                    continue
                x = cx + ring_r * math.cos(math.radians(az))
                y = cy + ring_r * math.sin(math.radians(az))
                z = cz + radius * math.sin(math.radians(el))
                self.block((x, y, z), (thickness, width, height), "DomePanel", folder, material, yaw=az, pitch=el)
        self.block((cx, cy, cz + radius), (radius * 0.25, radius * 0.25, thickness), "DomeCap", folder, material, shape="cylinder")

    def polar(self, centre, r, az_deg, z):
        return (centre[0] + r * math.cos(math.radians(az_deg)), centre[1] + r * math.sin(math.radians(az_deg)), z)

    # ------------------------------------------------------------------ chapter-map traversal (greybox_chapters.py)
    def mover(self, centre, size, label, folder, material="Climb", offset=(0, 0, 0), period=6.0, phase=0.0, rotation=(0, 0, 0),
              swing=False, yaw=0.0, pitch=0.0, roll=0.0, shape="cube", tags=None, collision=True):
        """A moving block (ABIBGreyboxMover): eased back and forth along `offset` every `period` s, and/or spinning (pitch,
        yaw, roll deg/s) or swinging (`swing`: rotation = amplitude in degrees)."""
        tags = (tags or []) + ["Mover"]
        if not hasattr(unreal, "BIBGreyboxMover"):
            self.log("WARNING: BIBGreyboxMover missing (build the editor module); static placeholder for " + label)
            return self.block(centre, size, label, folder, material, yaw, pitch, roll, tags + ["MoverPlaceholder"], shape, collision)
        actor = self.spawn(unreal.BIBGreyboxMover, centre, yaw, pitch, roll, label, folder, tags)
        comp = actor.get_editor_property("mesh")
        comp.set_static_mesh(self.shapes[shape])
        comp.set_material(0, self.mat[self.force_material or material])
        comp.set_collision_profile_name("BlockAll" if collision else "NoCollision")
        actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        actor.set_editor_property("offset", unreal.Vector(*offset))
        actor.set_editor_property("period", float(period))
        actor.set_editor_property("phase", float(phase))
        actor.set_editor_property("rotation_rate", unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]))
        actor.set_editor_property("swing", bool(swing))
        return actor

    def water(self, x0, y0, z0, x1, y1, z1, label, folder, surface=True):
        """Swimmable water: a water PhysicsVolume (characters swim inside it) and a non-blocking blue surface at z1."""
        centre = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
        size = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0))
        vol = self.spawn(unreal.PhysicsVolume, centre, label=label, folder=folder, tags=["Swim", "WaterVolume"])
        try_set(vol, "water_volume", True, self.log)
        try_set(vol, "fluid_friction", 0.3, self.log)
        _, extent = vol.get_actor_bounds(False)
        base = max(extent.x * 2, 1.0), max(extent.y * 2, 1.0), max(extent.z * 2, 1.0)   # the volume factory's default brush
        vol.set_actor_scale3d(unreal.Vector(size[0] / base[0], size[1] / base[1], size[2] / base[2]))
        if surface:
            self.block((centre[0], centre[1], z1 - 2), (size[0], size[1], 4), label + "_Surface", folder, "Water",
                       tags=["WaterSurface"], collision=False)
        return vol

    def jump_steps(self, start, yaw, count, folder, rise=80.0, run=260.0, width=320.0, material="Jump", gap=0.0, tags=None):
        """Blocks rising `rise` cm each (the demo jump clears about 90 cm): a route to jump up. Returns the top point."""
        a = math.radians(yaw)
        fx, fy = math.cos(a), math.sin(a)
        for k in range(count):
            d = (run + gap) * k + run / 2
            top = start[2] + rise * (k + 1)
            self.block((start[0] + fx * d, start[1] + fy * d, top - 100), (run, width, 200), "JumpUp", folder, material, yaw=yaw,
                       tags=(tags or []) + ["JumpUp"])
        d = (run + gap) * count
        return (start[0] + fx * d, start[1] + fy * d, start[2] + rise * count)

    def climb_wall(self, base, yaw, width, height, folder, material="Climb", tags=None):
        """A climbable face (orange) with hand ledges every 4 m, its outer face at `base` looking along `yaw`."""
        a = math.radians(yaw)
        fx, fy = math.cos(a), math.sin(a)
        tags = (tags or []) + ["Climbable", "ClimbWall"]
        self.block((base[0] - fx * 25, base[1] - fy * 25, base[2] + height / 2), (50, width, height), "ClimbWall", folder, material,
                   yaw=yaw, tags=tags)
        z = base[2] + 400
        while z < base[2] + height:
            self.block((base[0] + fx * 25, base[1] + fy * 25, z), (50, width * 0.8, 20), "ClimbLedge", folder, material, yaw=yaw, tags=tags)
            z += 400

    def section(self, n, centre, folder, radius=4500.0, boss=False, floor=None, gate_yaw=None):
        """One combat section (MAIN section 7: 2-5 per level, about 8 s from centre to wall): a marker, an edge ring and,
        when `gate_yaw` is given, a section gate on the ring at that bearing. `floor` = material for a round floor."""
        x, y, z = centre
        self.sections.append((n, centre, radius, boss))
        tags = ["Section", "Section_%d" % n] + (["BossSection"] if boss else [])
        if floor:
            self.block((x, y, z - 50), (radius * 2, radius * 2, 100), "SectionFloor_%d" % n, folder, floor, shape="cylinder")
        self.spawn(unreal.TargetPoint, (x, y, z + 100), label="Section_%d%s" % (n, "_Boss" if boss else ""), folder=folder, tags=tags)
        for k in range(24):
            az = k * 15.0
            p = self.polar(centre, radius, az, z + 3)
            self.block(p, (60, 2 * math.pi * radius / 24 * 0.5, 6), "SectionEdge", folder, "Section", yaw=az, collision=False)
        if gate_yaw is not None:
            g = self.polar(centre, radius, gate_yaw, z)
            for side in (-1, 1):
                q = self.polar(g, 400 * side, gate_yaw + 90, z + 300)
                self.block(q, (60, 60, 600), "SectionGatePost", folder, "Section", tags=["SectionGate"])
            self.block((g[0], g[1], z + 620), (60, 860, 40), "SectionGate_%d" % n, folder, "Section", yaw=gate_yaw,
                       tags=["SectionGate"], collision=False)
        self.label(("BOSS " if boss else "") + "SECTION %d" % n, (x, y, z + 900), folder, 300)

    def rain_placeholder(self, half, folder="FX", top=30000.0, text="RAIN PLACEHOLDER"):
        rain = self.box(-half, -half, 0, half, half, top, "RAIN_VOLUME_PLACEHOLDER", folder, "NeonCyan", tags=["RainPlaceholder"],
                        collision=False, hidden_in_game=True)
        rain.static_mesh_component.set_editor_property("visible", False)
        self.label(text, (0, 0, 6000), folder, 500, colour=(0, 220, 255))

    def lighting(self, sun=2.0, sun_colour=(255, 245, 230), pitch=-45.0, sky=1.0, fog=0.02, fog_colour=(0.3, 0.35, 0.45),
                 volumetric=False, shadows=True, atmosphere=True):
        light = self.spawn(unreal.DirectionalLight, (0, 0, 30000), yaw=35, pitch=pitch, label="Sun", folder="Lighting")
        lc = light.light_component
        lc.set_mobility(unreal.ComponentMobility.MOVABLE)
        try_set(lc, "intensity", sun, self.log)
        try_set(lc, "light_color", unreal.Color(r=sun_colour[0], g=sun_colour[1], b=sun_colour[2], a=255), self.log)
        try_set(lc, "cast_shadows", shadows, self.log)
        try_set(lc, "atmosphere_sun_light", atmosphere, self.log)
        sk = self.spawn(unreal.SkyLight, (0, 0, 30000), label="SkyLight", folder="Lighting")
        sk.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        try_set(sk.light_component, "real_time_capture", True, self.log)
        try_set(sk.light_component, "intensity", sky, self.log)
        if atmosphere:
            self.spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="Lighting")
        f = self.spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="HeightFog", folder="Lighting")
        try_set(f.component, "fog_density", fog, self.log)
        try_set(f.component, "fog_inscattering_luminance", unreal.LinearColor(fog_colour[0], fog_colour[1], fog_colour[2], 1.0), self.log)
        try_set(f.component, "enable_volumetric_fog", volumetric, self.log)
        return light

    # ------------------------------------------------------------------ world
    def world_settings(self, game_mode_class):
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings = world.get_world_settings()
        try_set(settings, "default_game_mode", game_mode_class, self.log)
        try_set(settings, "kill_z", -40000.0, self.log)

    def save(self, anime=True):
        # The anime look (BIBGame/BIBAnimeStyle.h; material from build_assets.py) is in every gameplay map, except where the
        # owner asked for none (Chapter 11, Return to Nowhere: "no post processing").
        if not anime:
            self.log("map %s: no anime style volume (owner: no post-processing)" % self.path)
        elif hasattr(unreal, "BIBAnimeStyleVolume"):
            self.spawn(unreal.BIBAnimeStyleVolume, (0, 0, 0), label="AnimeStyle", folder="Lighting", tags=["AnimeStyle"])
        else:
            self.log("WARNING: BIBAnimeStyleVolume missing (build the editor module first); no anime style in " + self.path)
        if not self.levels.save_current_level():
            raise RuntimeError("save failed: " + self.path)
        if self.dress:
            self.log(self.dress.summary())
        total = sum(self.counts.values())
        if total < 8000:
            self.log("WARNING: %s has %d actors, below the 8,000 detail floor (owner 2026-10-07)" % (self.path, total))
        self.log("map %s: %d actors (%s)" % (self.path, total, ", ".join("%s %d" % kv for kv in sorted(self.counts.items()))))
        return total
