"""Builds /Game/BIBCharacter: the BB character shader (owner request 2026-10-15: redo the BB shader so characters read as AAA
anime gacha quality - Genshin / ZZZ - with the project's dream-core / horror atmosphere built into the same material system).

Re-runnable: deletes /Game/BIBCharacter and rebuilds it from this file. Never touches /Game/BIB (Tools/build_assets.py), /Game/Weather
or /Game/Greybox. The old look stays: operators in game keep M_BIBTint until the owner decides to switch (owner 2026-10-15:
"assets + test map only").

    Tools/build_character_shader.sh            (UnrealEditor-Cmd ... -run=pythonscript -script=Tools/build_character_shader.py)

Builds:
  MPC_BIBCharacter        scene / story-beat dials: key light override, light colours, and the global Dread / Liminal / Horror /
                          Uncanny levels that every character instance adds to its own
  MF_BIBChar_*            reusable pieces (each a Custom HLSL node behind named inputs / outputs):
                          Cel (banded ramp, soft terminator), Face (SDF or stabilised normal, forward-facing rule), Spec (hard
                          threshold + hair angel ring), Rim (scene-responsive), Matcap, Uncanny (drift / pulse / shadow shift),
                          Compose, Dreamcore (palette shift, grain, hollow shadows, readability floor), ComposeLow (fallback),
                          Wobble (stepped wrongness / outline wobble)
  M_BIBCharacter          the master: unlit opaque, its own cel lighting from the weather sun (MPC_BIBWeather.SunDir / MoonDir)
                          or MPC_BIBCharacter's key override; Quality Switch + ForceLowEnd select the low-end permutation
  M_BIBCharacterOutline   inverted-hull outline for a mesh's Overlay Material: distance-scaled width, per-surface width and tint
                          from vertex colour (optional), horror ink and wobble
  MI_BIBChar_Skin / Face / Hair / Eye / Cloth / Metal / Emissive / Cloth_Low, MI_BIBChar_Outline
  T_BIBChar_DefaultColor / DefaultData   4x4 neutral data textures (generated here; they only keep unused samplers valid)
  Maps/L_BIBCharacterShaderTest          before (M_BIBTint) / after rows on the operators' shapes, a reference operator, camera,
                                         lights and the BB anime volume (character pixels write custom stencil 77)
Texture inputs (all optional, off by default; see reports/demo/2026-10-15-bb-character-shader.md "Texture inputs"):
  BaseMap (sRGB colour), NormalMap (tangent normal), ILMMap (linear, Masks: R spec mask, G shadow bias 0.5 = neutral, B hair-band
  jitter / spec, A identity mask), RampMap (sRGB, U = light, V = RampRow), FaceSDF (linear, Masks, authored for light from the head's
  left: lit where SDF > light angle 0 front .. 1 behind), MatcapMap (sRGB sphere map), EmissiveMap (sRGB).
All look numbers are PROPOSED (owner decides). Summary in Saved/BuildCharacterShader.log.
"""

import os
import struct
import traceback
import zlib

import unreal

ROOT = "/Game/BIBCharacter"
FUNCS = ROOT + "/Functions"
MATS = ROOT + "/Materials"
INST = ROOT + "/Instances"
TEX = ROOT + "/Textures"
MAPS = ROOT + "/Maps"
TEST_MAP = MAPS + "/L_BIBCharacterShaderTest"
DEMO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CHARACTER_STENCIL = 77   # M_BIBAnimeStyle leaves pixels with this custom stencil to the character shader (CharacterStencil)

LOG = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary

F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
S = unreal.FunctionInputType.FUNCTION_INPUT_SCALAR
V2 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR2
V3 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3
MP = unreal.MaterialProperty


def log(*parts):
    line = " ".join(str(p) for p in parts)
    LOG.append(line)
    unreal.log("[build_character_shader] " + line)


def save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def connect(src, src_pin, dst, dst_pin, what):
    if not lib.connect_material_expressions(src, src_pin, dst, dst_pin):
        raise RuntimeError("could not connect %s (%s -> %s)" % (what, src_pin or "out", dst_pin or "in"))


# ============================================================================ HLSL of the material functions

CEL_HLSL = r"""
// Half-lambert position on the ramp, nudged by the shadow bias (ILM green, 0.5 = neutral) and the horror / drift shift.
float x = saturate(NdotL * 0.5 + 0.5 + (Bias - 0.5));
RampU = saturate(x - Shift);
float s = max(Softness, 1e-3);
float t1 = Threshold + Shift;
float lit = smoothstep(t1 - s, t1 + s, x);
if (Bands > 2.5)
{
    // a third tone above the terminator (cloth folds read better with a mid band)
    float t2 = t1 + BandStep;
    lit = lit * 0.5 + smoothstep(t2 - s, t2 + s, x) * 0.5;
}
// the soft terminator band itself (gets its own warm / saturated tint in Compose)
Edge = saturate(1.0 - abs(x - t1) / (s * 3.0));
return lit;
"""

FACE_HLSL = r"""
// Faces: no terminator crawling over the nose. Light is projected onto the head's horizontal plane (vertical light changes
// do not move the shadow) and either an authored SDF shadow map decides the shadow shape, or the normal is flattened
// towards the face's forward direction.
float3 F = normalize(Fwd);
float3 R = Right - F * dot(Right, F);
R = dot(R, R) > 1e-6 ? normalize(R) : normalize(cross(float3(0.0, 0.0, 1.0), F) + 1e-5);
float3 U = normalize(cross(F, R));
float3 Lh = L - U * dot(L, U) * saturate(LightFlatten);
Lh = dot(Lh, Lh) > 1e-6 ? normalize(Lh) : F;
if (UseSDF > 0.5)
{
    float Front = dot(Lh, F);                            // 1 light in front .. -1 behind
    float Side = dot(Lh, R);
    float Value = Side > 0.0 ? SDFMirror : SDF;         // authored for light from the left; mirrored for the right
    float Angle = acos(clamp(Front, -1.0, 1.0)) / 3.14159265;   // 0 front .. 1 behind (forward-facing rule)
    return clamp((Value - Angle) / max(SDFSoftness, 1e-3), -1.0, 1.0);
}
float3 Ns = normalize(lerp(N, F, saturate(FaceFlatten)));
return dot(Ns, Lh);
"""

SPEC_HLSL = r"""
float3 H = normalize(L + V);
float s = max(Softness, 1e-3);
// hard-edged stylised highlight
float Hard = smoothstep(Threshold - s, Threshold + s, saturate(dot(N, H)));
// hair: Kajiya-Kay band along the strand direction (the bitangent = UV v on most hair meshes), shifted along the normal:
// the anime "angel ring". Jitter (ILM blue) breaks the ring per strand clump.
float3 B = cross(N, T);
B = dot(B, B) > 1e-6 ? normalize(B) : float3(0.0, 0.0, 1.0);
float3 Ts = normalize(B + N * (AnisoShift + (Jitter - 0.5) * 0.3));
float Th = dot(Ts, H);
float Band = sqrt(saturate(1.0 - Th * Th));
float Ring = smoothstep(1.0 - AnisoWidth - s, 1.0 - AnisoWidth + s, Band);
return lerp(Hard, Ring, saturate(HairMode)) * Mask * Intensity;
"""

RIM_HLSL = r"""
float s = max(Softness, 1e-3);
float Fres = 1.0 - saturate(dot(N, V));
float Rim = smoothstep(1.0 - Width - s, 1.0 - Width + s, Fres);
// follows the scene: strongest on the side the key light comes from, and stronger in backlight
float3 Lf = L - V * dot(L, V);
float SideLit = dot(Lf, Lf) > 1e-5 ? saturate(dot(normalize(Lf), N) * 0.5 + 0.5) : 0.5;
float Back = saturate(-dot(L, V));
return Rim * lerp(1.0, SideLit, saturate(LightFollow)) * (1.0 + Back * LightFollow);
"""

MATCAP_HLSL = r"""
// Procedural studio matcap when no MatcapMap is set: a hard key highlight top-left and a soft horizon line.
float2 P = VN.xy;
float s = max(Softness, 1e-3);
float Key = 1.0 - smoothstep(Size - s, Size + s, length(P - float2(-0.35, 0.4)));
float Horizon = 1.0 - smoothstep(0.06, 0.06 + s * 2.0, abs(P.y + 0.1));
return UseMap > 0.5 ? Sample : (Key + Horizon * 0.35).xxx;
"""

UNCANNY_HLSL = r"""
const float TAU = 6.2831853;
// eye glint pulse: a slow, slightly too regular breath of light
Pulse = pow(0.5 + 0.5 * sin(Time * PulseRate * TAU), 6.0) * Uncanny;
// horror pulls the terminator towards the light - capped so the lit side (and the silhouette) always survives
Shift = Horror * clamp(ShadowShift, 0.0, 0.25);
// the shadow ramp drifts very slowly, two incommensurate waves so it never quite repeats
return (sin(Time * DriftRate * TAU) * 0.6 + sin(Time * DriftRate * 2.31 * TAU + 1.7) * 0.4) * DriftAmount * Uncanny;
"""

COMPOSE_HLSL = r"""
float l = lerp(saturate(Lit), 1.0, saturate(Flatten));
float3 Tint = UseRamp > 0.5 ? RampColor : lerp(ShadowColor, LitColor, l);
Tint += TerminatorColor * Edge * TerminatorStrength * (1.0 - saturate(Flatten));
// the scene tints the character but never decides its brightness: stable under changing light
float3 Amb = lerp(float3(1.0, 1.0, 1.0), Ambient, saturate(AmbientStrength));
float3 Key = lerp(float3(1.0, 1.0, 1.0), KeyColor, saturate(KeyStrength));
float3 LightTint = lerp(Amb, Key, l);
float3 C = Base * Tint * LightTint;
C += Matcap * Base * MatcapStrength;
C += Spec * SpecColor * lerp(0.35, 1.0, l) * LightTint;
C += Rim * RimColor * RimStrength;
Shadow = 1.0 - l;
Glow = Emissive * EmissiveStrength * EmissiveMask + GlintColor * Glint;
return max(C, 0.0);
"""

DREAM_HLSL = r"""
const float3 W = float3(0.2126, 0.7152, 0.0722);
float3 C = Diffuse;
// liminal: a lift towards a pastel tint, slightly washed out, brightness kept
float Lum = dot(C, W);
float3 Pastel = LiminalTint * (Lum * 0.85 + 0.12);
C = lerp(C, lerp(C, Pastel, 0.55), Liminal);
C = lerp(dot(C, W).xxx, C, 1.0 - 0.3 * Liminal);
// dread: the colour drains into a sickly cast
C = lerp(C, dot(C, W) * DreadTint * 1.25, Dread * 0.85);
// film / VHS grain, only in the shadows, stepped like the 12 fps look
float2 Cell = floor(Pixel / max(GrainScale, 1.0));
float StepT = floor(Time * max(GrainRate, 1.0));
float Noise = frac(sin(dot(Cell + StepT * float2(17.0, 31.0), float2(12.9898, 78.233))) * 43758.5453) - 0.5;
C += Noise * Grain * Shadow * (0.3 + 0.7 * max(Dread, Horror)) * 0.25;
// horror: ink-dark, hollow tones bleed into the shadows; the lit side keeps the operator readable
C = lerp(C, HollowColor * (dot(C, W) * 0.5 + 0.03), saturate(Shadow * Horror * 1.4));
// readability floor: the rim survives every level, so the silhouette always reads
C += Rim * RimColor * ReadabilityRim * max(max(Dread, Horror), Liminal * 0.5);
// identity: glow (eyes, accessories, signature weapons) and ILM-alpha-masked parts keep their own colours
float3 G = lerp(Glow * (1.0 - 0.6 * max(Dread, Horror)), Glow, saturate(IdentityProtect));
C = lerp(C, Diffuse, saturate(IdentityMask * IdentityProtect));
return max(C, 0.0) + G;
"""

COMPOSE_LOW_HLSL = r"""
// Low-end fallback: one hard band, a stepped rim, emissive, the horror hollow shadow. No textures beyond the base map.
float Lit = step(Threshold, dot(N, L) * 0.5 + 0.5);
float3 C = Base * lerp(ShadowColor, LitColor, Lit);
C += RimColor * RimStrength * step(1.0 - RimWidth, 1.0 - saturate(dot(N, V)));
C = lerp(C, HollowColor * 0.5, (1.0 - Lit) * saturate(Horror) * 0.8);
return max(C, 0.0) + Emissive * EmissiveStrength;
"""

WOBBLE_HLSL = r"""
// Stepped (12 fps like the anime look) low-frequency wobble along the normal, plus a rare glitch jump. Amount in cm.
float Rate = max(StepRate, 1.0);
float t = floor(Time * Rate) / Rate;
float3 P = Pos * Frequency;
float Wv = sin(P.x * 1.7 + t * 5.1 + sin(P.y * 2.3 + t * 3.7)) * cos(P.z * 1.9 + t * 4.3);
float Gl = step(0.94, frac(sin(floor(Time * 2.0) * 91.7) * 43758.5)) * Glitch;
float Jump = Gl * 3.0 * sign(sin(P.z * 9.0 + t * 30.0));
return normalize(Nrm) * (Wv + Jump) * Amount;
"""

# Glue in the master / outline (not reusable on their own).
INPUTS_HLSL = r"""
// World normal (vertex normal + optional tangent-space normal map; unlit materials have no Normal pin), tangent, light.
float3 Nv = normalize(VN);
float3 Tv = VT - Nv * dot(VT, Nv);
Tv = dot(Tv, Tv) > 1e-6 ? normalize(Tv) : normalize(cross(Nv, float3(0.0, 0.0, 1.0)) + float3(1e-4, 0.0, 0.0));
float3 Bv = cross(Nv, Tv);
float3 n = NM.xyz;
T = Tv;
// Key light: the weather sun by day, the moon by night (MPC_BIBWeather), unless the scene overrides it (MPC_BIBCharacter).
float3 Ls = Sun.w > -0.05 ? Sun.xyz : Moon.xyz;
Ls = dot(Ls, Ls) > 1e-6 ? normalize(Ls) : float3(0.0, 0.3, 0.95);
float3 Lk = dot(KeyDir.xyz, KeyDir.xyz) > 1e-6 ? normalize(KeyDir.xyz) : Ls;
L = normalize(lerp(Ls, Lk, saturate(KeyOverride)) + 1e-5);
return normalize(Tv * n.x + Bv * n.y + Nv * max(n.z, 1e-3));
"""

GLOBALS_HLSL = r"""
// Each character's own level plus the scene / story beat's global level (MPC_BIBCharacter) times its response.
Dread = saturate(LocDread + GDread * Response);
Horror = saturate(LocHorror + GHorror * Response);
Uncanny = saturate(LocUncanny + GUncanny * Response);
return saturate(LocLiminal + GLiminal * Response);
"""

MATCAP_UV_HLSL = r"""
return VN.xy * float2(0.5, -0.5) + 0.5;
"""

OUTLINE_OFFSET_HLSL = r"""
// Width in cm at RefDist, scaled with the object's distance (constant across the mesh: no popping), clamped.
float d = length(Cam - Obj);
float Scale = clamp(d / max(RefDist, 1.0), MinScale, MaxScale);
float Wd = Width * Scale * VCWidth * (1.0 + saturate(Horror) * Grow);
return normalize(Nrm) * Wd + Wobble;
"""

OUTLINE_COLOUR_HLSL = r"""
float3 C = lerp(OutlineColor, OutlineColor * VC * 2.0, saturate(TintStrength));
return lerp(C, InkColor, saturate(Horror));
"""

# name, description, code, inputs [(name, type)], outputs [(name, custom output type)] (first = the node's return value)
FUNCTIONS = [
    ("MF_BIBChar_Cel", "Banded cel ramp with a soft terminator; Lit 0 shadow .. 1 lit, Edge = terminator band, RampU for a ramp map.",
     CEL_HLSL, [("NdotL", S), ("Bias", S), ("Threshold", S), ("Softness", S), ("Bands", S), ("BandStep", S), ("Shift", S)],
     [("Lit", F1), ("Edge", F1), ("RampU", F1)]),
    ("MF_BIBChar_Face", "Face shading without terminator artifacts: SDF shadow map (forward-facing rule) or a stabilised normal.",
     FACE_HLSL, [("N", V3), ("L", V3), ("Fwd", V3), ("Right", V3), ("FaceFlatten", S), ("LightFlatten", S), ("SDF", S),
                 ("SDFMirror", S), ("UseSDF", S), ("SDFSoftness", S)],
     [("NdotL", F1)]),
    ("MF_BIBChar_Spec", "Hard-threshold stylised specular; HairMode 1 = Kajiya-Kay angel-ring band.",
     SPEC_HLSL, [("N", V3), ("V", V3), ("L", V3), ("T", V3), ("Mask", S), ("Threshold", S), ("Softness", S), ("Intensity", S),
                 ("HairMode", S), ("AnisoShift", S), ("AnisoWidth", S), ("Jitter", S)],
     [("Spec", F1)]),
    ("MF_BIBChar_Rim", "Fresnel rim that follows the key light side and grows in backlight.",
     RIM_HLSL, [("N", V3), ("V", V3), ("L", V3), ("Width", S), ("Softness", S), ("LightFollow", S)],
     [("Rim", F1)]),
    ("MF_BIBChar_Matcap", "Matcap accent: the MatcapMap sample, or a procedural studio matcap.",
     MATCAP_HLSL, [("VN", V3), ("Sample", V3), ("UseMap", S), ("Size", S), ("Softness", S)],
     [("Matcap", F3)]),
    ("MF_BIBChar_Uncanny", "Uncanny cues: slow ramp drift, eye-glint pulse, capped horror shadow shift.",
     UNCANNY_HLSL, [("Time", S), ("Uncanny", S), ("Horror", S), ("DriftAmount", S), ("DriftRate", S), ("PulseRate", S),
                    ("ShadowShift", S)],
     [("Drift", F1), ("Pulse", F1), ("Shift", F1)]),
    ("MF_BIBChar_Compose", "Combines base, tints, scene light, spec, rim, matcap; outputs diffuse, glow and the shadow amount.",
     COMPOSE_HLSL, [("Base", V3), ("Lit", S), ("Edge", S), ("ShadowColor", V3), ("LitColor", V3), ("TerminatorColor", V3),
                    ("TerminatorStrength", S), ("RampColor", V3), ("UseRamp", S), ("Flatten", S), ("Ambient", V3),
                    ("AmbientStrength", S), ("KeyColor", V3), ("KeyStrength", S), ("Spec", S), ("SpecColor", V3), ("Rim", S),
                    ("RimColor", V3), ("RimStrength", S), ("Matcap", V3), ("MatcapStrength", S), ("Emissive", V3),
                    ("EmissiveStrength", S), ("EmissiveMask", S), ("Glint", S), ("GlintColor", V3)],
     [("Diffuse", F3), ("Glow", F3), ("Shadow", F1)]),
    ("MF_BIBChar_Dreamcore", "Dream-core / horror layer: liminal pastel, dread tint, shadow grain, hollow shadows, readability floor.",
     DREAM_HLSL, [("Diffuse", V3), ("Glow", V3), ("Shadow", S), ("Rim", S), ("Pixel", V2), ("Time", S), ("Liminal", S),
                  ("LiminalTint", V3), ("Dread", S), ("DreadTint", V3), ("Grain", S), ("GrainScale", S), ("GrainRate", S),
                  ("Horror", S), ("HollowColor", V3), ("IdentityProtect", S), ("IdentityMask", S), ("ReadabilityRim", S),
                  ("RimColor", V3)],
     [("Color", F3)]),
    ("MF_BIBChar_ComposeLow", "Low-end fallback shading (one band, stepped rim, emissive, hollow shadow).",
     COMPOSE_LOW_HLSL, [("Base", V3), ("N", V3), ("V", V3), ("L", V3), ("Threshold", S), ("ShadowColor", V3), ("LitColor", V3),
                        ("RimColor", V3), ("RimStrength", S), ("RimWidth", S), ("Emissive", V3), ("EmissiveStrength", S),
                        ("Horror", S), ("HollowColor", V3)],
     [("Color", F3)]),
    ("MF_BIBChar_Wobble", "Stepped wobble along the normal plus a rare glitch jump (cm): wrongness shimmer, outline wobble.",
     WOBBLE_HLSL, [("Pos", V3), ("Nrm", V3), ("Time", S), ("Amount", S), ("Frequency", S), ("StepRate", S), ("Glitch", S)],
     [("Offset", F3)]),
]


def make_function(name, description, code, inputs, outputs):
    mf = tools.create_asset(name, FUNCS, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    mf.set_editor_property("description", description)
    mf.set_editor_property("expose_to_library", True)
    custom = lib.create_material_expression_in_function(mf, unreal.MaterialExpressionCustom, -200, 0)
    custom.set_editor_property("description", name)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", outputs[0][1])
    entries = []
    for input_name, _ in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", input_name)
        entries.append(entry)
    custom.set_editor_property("inputs", entries)
    if len(outputs) > 1:
        extra = []
        for output_name, kind in outputs[1:]:
            out = unreal.CustomOutput()
            out.set_editor_property("output_name", output_name)
            out.set_editor_property("output_type", kind)
            extra.append(out)
        custom.set_editor_property("additional_outputs", extra)
    for index, (input_name, kind) in enumerate(inputs):
        node = lib.create_material_expression_in_function(mf, unreal.MaterialExpressionFunctionInput, -700, index * 90)
        node.set_editor_property("input_name", input_name)
        node.set_editor_property("input_type", kind)
        node.set_editor_property("sort_priority", index)
        connect(node, "", custom, input_name, "%s.%s" % (name, input_name))
    for index, (output_name, _) in enumerate(outputs):
        node = lib.create_material_expression_in_function(mf, unreal.MaterialExpressionFunctionOutput, 300, index * 120)
        node.set_editor_property("output_name", output_name)
        node.set_editor_property("sort_priority", index)
        connect(custom, "" if index == 0 else output_name, node, "", "%s -> %s" % (name, output_name))
    lib.update_material_function(mf)
    save(mf)
    log("function:", name, "(%d in, %d out)" % (len(inputs), len(outputs)))
    return mf


# ============================================================================ parameter collection

MPC_SCALARS = [
    ("KeyOverride", 0.0),      # 0 = follow the weather sun / moon, 1 = use KeyLightDir (cutscenes, story beats, the test map)
    ("Dread", 0.0), ("Liminal", 0.0), ("Horror", 0.0), ("Uncanny", 0.0),   # scene / story-beat levels, added per character
]
MPC_VECTORS = [
    ("KeyLightDir", (0.45, -0.35, 0.82, 0.0)),    # direction TO the light
    ("KeyLightColor", (1.0, 0.97, 0.94, 1.0)),
    ("AmbientColor", (0.66, 0.6, 0.86, 1.0)),     # lavender ambient (the anime look's shadow tier, M_BIBAnimeStyle ShadowTint)
]


def build_mpc():
    mpc = tools.create_asset("MPC_BIBCharacter", ROOT, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    scalars = []
    for name, default in MPC_SCALARS:
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", default)
        scalars.append(p)
    vectors = []
    for name, v in MPC_VECTORS:
        p = unreal.CollectionVectorParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", unreal.LinearColor(*v))
        vectors.append(p)
    mpc.set_editor_property("scalar_parameters", scalars)
    mpc.set_editor_property("vector_parameters", vectors)
    save(mpc)
    log("MPC_BIBCharacter: %d scalars, %d vectors" % (len(scalars), len(vectors)))
    return mpc


# ============================================================================ default data textures (no art)

def write_png(path, rgba):
    """4x4 RGBA PNG of one colour, written with the standard library only."""
    width = height = 4
    raw = b"".join(b"\x00" + bytes(rgba) * width for _ in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(path, "wb") as handle:
        handle.write(png)


def build_default_textures():
    folder = os.path.join(DEMO, "Saved", "CharacterShader", "DefaultTextures")
    os.makedirs(folder, exist_ok=True)
    made = {}
    # colour default: white (sRGB); data default: no spec, neutral shadow bias, neutral jitter, no identity mask (linear, Masks)
    for name, rgba, srgb in (("T_BIBChar_DefaultColor", (255, 255, 255, 255), True), ("T_BIBChar_DefaultData", (0, 128, 128, 0), False)):
        path = os.path.join(folder, name + ".png")
        write_png(path, rgba)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", TEX)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(TEX + "/" + name)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("could not import " + name)
        texture.set_editor_property("srgb", srgb)
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_DEFAULT if srgb else unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        save(texture)
        made[name] = texture
    log("textures: T_BIBChar_DefaultColor (sRGB white), T_BIBChar_DefaultData (linear neutral masks)")
    return made


# ============================================================================ material builder

class Graph:
    """Node helpers for one material. Parameters are recorded so the report table and the tests stay in sync."""

    def __init__(self, material, mpc_char, mpc_weather, textures):
        self.m = material
        self.mpc_char = mpc_char
        self.mpc_weather = mpc_weather
        self.textures = textures
        self.x = -2600
        self.y = -1200
        self.params = []

    def node(self, cls, x=None):
        n = lib.create_material_expression(self.m, cls, self.x if x is None else x, self.y)
        self.y += 100
        if self.y > 3600:
            self.y = -1200
            self.x += 350
        return n

    def scalar(self, name, default, group, lo=None, hi=None):
        n = self.node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", default)
        n.set_editor_property("group", group)
        if lo is not None:
            n.set_editor_property("slider_min", lo)
            n.set_editor_property("slider_max", hi)
        self.params.append(("scalar", name, default, group))
        return n

    def vector(self, name, rgb, group):
        n = self.node(unreal.MaterialExpressionVectorParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        n.set_editor_property("group", group)
        self.params.append(("vector", name, rgb, group))
        return self.rgb(n)

    def rgb(self, source, pin=""):
        mask = self.node(unreal.MaterialExpressionComponentMask)
        for channel, on in (("r", True), ("g", True), ("b", True), ("a", False)):
            mask.set_editor_property(channel, on)
        connect(source, pin, mask, "", "rgb mask")
        return mask

    def channel(self, source, pin, which):
        mask = self.node(unreal.MaterialExpressionComponentMask)
        for channel in "rgba":
            mask.set_editor_property(channel, channel == which)
        connect(source, pin, mask, "", "channel mask")
        return mask

    def const(self, value):
        n = self.node(unreal.MaterialExpressionConstant)
        n.set_editor_property("r", value)
        return n

    def const3(self, rgb):
        n = self.node(unreal.MaterialExpressionConstant3Vector)
        n.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return n

    def switch(self, name, default, true_expr, false_expr, group, true_pin="", false_pin=""):
        n = self.node(unreal.MaterialExpressionStaticSwitchParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", default)
        n.set_editor_property("group", group)
        connect(true_expr, true_pin, n, "True", name + ".True")
        connect(false_expr, false_pin, n, "False", name + ".False")
        self.params.append(("switch", name, default, group))
        return n

    def flag(self, name, default, group):
        """A static switch as a 0 / 1 value (compiled out)."""
        return self.switch(name, default, self.const(1.0), self.const(0.0), group)

    def texture(self, name, group, data=False, normal=False, uv=None):
        n = self.node(unreal.MaterialExpressionTextureSampleParameter2D)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("group", group)
        if normal:
            n.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
            n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif data:
            n.set_editor_property("texture", self.textures["T_BIBChar_DefaultData"])
            n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        else:
            n.set_editor_property("texture", self.textures["T_BIBChar_DefaultColor"])
            n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        if uv is not None:
            connect(uv, "", n, "UVs", name + ".UVs")
        self.params.append(("texture", name, "data" if data else ("normal" if normal else "colour"), group))
        return n

    def collection(self, mpc, name):
        n = self.node(unreal.MaterialExpressionCollectionParameter)
        n.set_editor_property("collection", mpc)
        n.set_editor_property("parameter_name", name)
        return n

    def custom(self, label, code, inputs, output_type, outputs=()):
        c = self.node(unreal.MaterialExpressionCustom, self.x + 700)
        c.set_editor_property("description", label)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", output_type)
        entries = []
        for name, _ in inputs:
            e = unreal.CustomInput()
            e.set_editor_property("input_name", name)
            entries.append(e)
        c.set_editor_property("inputs", entries)
        if outputs:
            extra = []
            for name, kind in outputs:
                o = unreal.CustomOutput()
                o.set_editor_property("output_name", name)
                o.set_editor_property("output_type", kind)
                extra.append(o)
            c.set_editor_property("additional_outputs", extra)
        for name, expr in inputs:
            source, pin = expr if isinstance(expr, tuple) else (expr, "")
            connect(source, pin, c, name, "%s.%s" % (label, name))
        return c

    def call(self, function, inputs):
        """inputs: {function input name: expression or (expression, output pin)}."""
        n = self.node(unreal.MaterialExpressionMaterialFunctionCall, self.x + 1100)
        if hasattr(n, "set_material_function"):
            n.set_material_function(function)   # also rebuilds the call's input / output pins
        else:
            n.set_editor_property("material_function", function)
        for name, expr in inputs.items():
            source, pin = expr if isinstance(expr, tuple) else (expr, "")
            connect(source, pin, n, name, "%s.%s" % (function.get_name(), name))
        return n

    def add(self, a, b, a_pin="", b_pin=""):
        n = self.node(unreal.MaterialExpressionAdd, self.x + 900)
        connect(a, a_pin, n, "A", "add.A")
        connect(b, b_pin, n, "B", "add.B")
        return n

    def quality(self, default_expr, low_expr, default_pin="", low_pin=""):
        n = self.node(unreal.MaterialExpressionQualitySwitch, self.x + 1500)
        connect(default_expr, default_pin, n, "Default", "quality.Default")
        connect(low_expr, low_pin, n, "Low", "quality.Low")
        return n

    def transform(self, source, src_space, dst_space, pin=""):
        n = self.node(unreal.MaterialExpressionTransform)
        n.set_editor_property("transform_source_type", src_space)
        n.set_editor_property("transform_type", dst_space)
        connect(source, pin, n, "", "transform")
        return n

    def out(self, expr, prop, pin=""):
        if not lib.connect_material_property(expr, pin, prop):
            raise RuntimeError("%s: could not connect %s" % (self.m.get_name(), prop))


# ============================================================================ the master

def build_master(fn, mpc_char, mpc_weather, textures):
    material = tools.create_asset("M_BIBCharacter", MATS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    for flag in ("used_with_skeletal_mesh", "used_with_morph_targets"):
        try:
            material.set_editor_property(flag, True)
        except Exception as error:
            log("warn: M_BIBCharacter.%s not set (%s)" % (flag, error))
    g = Graph(material, mpc_char, mpc_weather, textures)

    # ---- shared inputs
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate)
    time = g.node(unreal.MaterialExpressionTime)
    vertex_normal = g.node(unreal.MaterialExpressionVertexNormalWS)
    vertex_tangent = g.node(unreal.MaterialExpressionVertexTangentWS)
    view = g.node(unreal.MaterialExpressionCameraVectorWS)
    screen = g.node(unreal.MaterialExpressionScreenPosition)
    world_pos = g.node(unreal.MaterialExpressionWorldPosition)

    # ---- base and maps
    base_colour = g.vector("BaseColor", (1.0, 1.0, 1.0), "01 Base")
    base_map = g.texture("BaseMap", "01 Base", uv=uv0)
    base_sample = g.switch("UseBaseMap", False, g.rgb(base_map, "RGB"), g.const3((1.0, 1.0, 1.0)), "01 Base")
    base = g.node(unreal.MaterialExpressionMultiply, g.x + 900)
    connect(base_colour, "", base, "A", "base.A")
    connect(base_sample, "", base, "B", "base.B")
    normal_map = g.texture("NormalMap", "01 Base", normal=True, uv=uv0)
    normal_sample = g.switch("UseNormalMap", False, g.rgb(normal_map, "RGB"), g.const3((0.0, 0.0, 1.0)), "01 Base")

    ilm = g.texture("ILMMap", "02 Masks", data=True, uv=uv0)
    spec_mask = g.switch("UseILMMap", False, g.channel(ilm, "RGBA", "r"), g.scalar("SpecMask", 1.0, "02 Masks", 0.0, 1.0), "02 Masks")
    shadow_bias = g.switch("UseILMMap", False, g.channel(ilm, "RGBA", "g"), g.scalar("ShadowBias", 0.5, "02 Masks", 0.0, 1.0), "02 Masks")
    jitter = g.switch("UseILMMap", False, g.channel(ilm, "RGBA", "b"), g.const(0.5), "02 Masks")
    identity_mask = g.switch("UseILMMap", False, g.channel(ilm, "RGBA", "a"), g.scalar("IdentityMask", 0.0, "02 Masks", 0.0, 1.0), "02 Masks")

    # ---- vectors and light
    inputs = g.custom("BIBChar inputs", INPUTS_HLSL, [
        ("VN", vertex_normal), ("VT", vertex_tangent), ("NM", normal_sample),
        ("Sun", g.collection(mpc_weather, "SunDir")), ("Moon", g.collection(mpc_weather, "MoonDir")),
        ("KeyDir", g.collection(mpc_char, "KeyLightDir")), ("KeyOverride", g.collection(mpc_char, "KeyOverride"))],
        F3, [("T", F3), ("L", F3)])
    n_ws = inputs
    t_ws = (inputs, "T")
    l_ws = (inputs, "L")
    dot = g.node(unreal.MaterialExpressionDotProduct, g.x + 900)
    connect(inputs, "", dot, "A", "NdotL.A")
    connect(inputs, "L", dot, "B", "NdotL.B")

    # ---- dream-core / horror levels (character + scene)
    response = g.scalar("GlobalResponse", 1.0, "09 Dreamcore", 0.0, 1.0)
    levels = g.custom("BIBChar levels", GLOBALS_HLSL, [
        ("LocLiminal", g.scalar("Liminal", 0.0, "09 Dreamcore", 0.0, 1.0)), ("LocDread", g.scalar("Dread", 0.0, "09 Dreamcore", 0.0, 1.0)),
        ("LocHorror", g.scalar("Horror", 0.0, "09 Dreamcore", 0.0, 1.0)), ("LocUncanny", g.scalar("Uncanny", 0.0, "09 Dreamcore", 0.0, 1.0)),
        ("GLiminal", g.collection(mpc_char, "Liminal")), ("GDread", g.collection(mpc_char, "Dread")),
        ("GHorror", g.collection(mpc_char, "Horror")), ("GUncanny", g.collection(mpc_char, "Uncanny")), ("Response", response)],
        F1, [("Dread", F1), ("Horror", F1), ("Uncanny", F1)])
    uncanny = g.call(fn["MF_BIBChar_Uncanny"], {
        "Time": time, "Uncanny": (levels, "Uncanny"), "Horror": (levels, "Horror"),
        "DriftAmount": g.scalar("RampDrift", 0.03, "09 Dreamcore", 0.0, 0.1),
        "DriftRate": g.scalar("RampDriftRate", 0.07, "09 Dreamcore", 0.0, 1.0),
        "PulseRate": g.scalar("EyePulseRate", 0.4, "09 Dreamcore", 0.0, 2.0),
        "ShadowShift": g.scalar("HorrorShadowShift", 0.15, "09 Dreamcore", 0.0, 0.25)})
    shift = g.add(uncanny, uncanny, "Drift", "Shift")

    # ---- faces
    head_fwd = g.transform(g.vector("HeadForwardAxis", (1.0, 0.0, 0.0), "04 Face"),
                           unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    head_right = g.transform(g.vector("HeadRightAxis", (0.0, 1.0, 0.0), "04 Face"),
                             unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    sdf_map = g.texture("FaceSDF", "04 Face", data=True, uv=uv0)
    mirror_uv = g.custom("BIBChar mirror UV", "return float2(1.0 - UV.x, UV.y);", [("UV", uv0)], F2)
    sdf_mirror = g.texture("FaceSDF", "04 Face", data=True, uv=mirror_uv)   # same parameter, mirrored lookup
    face = g.call(fn["MF_BIBChar_Face"], {
        "N": n_ws, "L": l_ws, "Fwd": head_fwd, "Right": head_right,
        "FaceFlatten": g.scalar("FaceFlatten", 0.6, "04 Face", 0.0, 1.0),
        "LightFlatten": g.scalar("FaceLightFlatten", 1.0, "04 Face", 0.0, 1.0),
        # the samples only exist in the UseFaceSDF permutation (unused maps cost nothing)
        "SDF": g.switch("UseFaceSDF", False, g.channel(sdf_map, "RGBA", "r"), g.const(0.5), "04 Face"),
        "SDFMirror": g.switch("UseFaceSDF", False, g.channel(sdf_mirror, "RGBA", "r"), g.const(0.5), "04 Face"),
        "UseSDF": g.flag("UseFaceSDF", False, "04 Face"),
        "SDFSoftness": g.scalar("FaceSDFSoftness", 0.03, "04 Face", 0.0, 0.2)})
    ndotl = g.switch("FaceShading", False, face, dot, "04 Face")

    # ---- cel bands
    cel = g.call(fn["MF_BIBChar_Cel"], {
        "NdotL": ndotl, "Bias": shadow_bias,
        "Threshold": g.scalar("ShadowThreshold", 0.5, "03 Cel", 0.0, 1.0),
        "Softness": g.scalar("ShadowSoftness", 0.025, "03 Cel", 0.0, 0.2),
        "Bands": g.scalar("Bands", 2.0, "03 Cel", 2.0, 3.0),
        "BandStep": g.scalar("BandStep", 0.22, "03 Cel", 0.0, 0.5),
        "Shift": shift})
    ramp_uv = g.custom("BIBChar ramp UV", "return float2(U, Row);", [("U", (cel, "RampU")), ("Row", g.scalar("RampRow", 0.5, "03 Cel", 0.0, 1.0))], F2)
    ramp_map = g.texture("RampMap", "03 Cel", uv=ramp_uv)

    # ---- specular, rim, matcap
    spec = g.call(fn["MF_BIBChar_Spec"], {
        "N": n_ws, "V": view, "L": l_ws, "T": t_ws, "Mask": spec_mask,
        "Threshold": g.scalar("SpecThreshold", 0.92, "05 Specular", 0.5, 1.0),
        "Softness": g.scalar("SpecSoftness", 0.02, "05 Specular", 0.0, 0.1),
        "Intensity": g.scalar("SpecIntensity", 1.0, "05 Specular", 0.0, 3.0),
        "HairMode": g.flag("HairShading", False, "05 Specular"),
        "AnisoShift": g.scalar("AnisoShift", 0.0, "05 Specular", -1.0, 1.0),
        "AnisoWidth": g.scalar("AnisoWidth", 0.06, "05 Specular", 0.0, 0.5),
        "Jitter": jitter})
    rim = g.call(fn["MF_BIBChar_Rim"], {
        "N": n_ws, "V": view, "L": l_ws,
        "Width": g.scalar("RimWidth", 0.25, "06 Rim", 0.0, 1.0),
        "Softness": g.scalar("RimSoftness", 0.05, "06 Rim", 0.0, 0.3),
        "LightFollow": g.scalar("RimLightFollow", 0.6, "06 Rim", 0.0, 1.0)})
    view_normal = g.transform(inputs, unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                              unreal.MaterialVectorCoordTransform.TRANSFORM_VIEW)
    matcap_uv = g.custom("BIBChar matcap UV", MATCAP_UV_HLSL, [("VN", view_normal)], F2)
    matcap_map = g.texture("MatcapMap", "07 Matcap", uv=matcap_uv)
    matcap = g.call(fn["MF_BIBChar_Matcap"], {
        "VN": view_normal, "Sample": g.switch("UseMatcapMap", False, g.rgb(matcap_map, "RGB"), g.const3((0.0, 0.0, 0.0)), "07 Matcap"),
        "UseMap": g.flag("UseMatcapMap", False, "07 Matcap"),
        "Size": g.scalar("MatcapSize", 0.35, "07 Matcap", 0.05, 1.0),
        "Softness": g.scalar("MatcapSoftness", 0.05, "07 Matcap", 0.0, 0.3)})

    # ---- emissive
    emissive_map = g.texture("EmissiveMap", "08 Emissive", uv=uv0)
    emissive_mask = g.switch("UseEmissiveMap", False, g.channel(emissive_map, "RGBA", "r"), g.const(1.0), "08 Emissive")
    emissive_colour = g.vector("EmissiveColor", (0.0, 0.0, 0.0), "08 Emissive")
    emissive_strength = g.scalar("EmissiveStrength", 0.0, "08 Emissive", 0.0, 20.0)
    glint = g.node(unreal.MaterialExpressionMultiply, g.x + 900)
    connect(uncanny, "Pulse", glint, "A", "glint.A")
    connect(g.scalar("GlintStrength", 0.0, "08 Emissive", 0.0, 5.0), "", glint, "B", "glint.B")

    # ---- compose (full quality)
    shadow_colour = g.vector("ShadowColor", (0.62, 0.55, 0.82), "03 Cel")
    lit_colour = g.vector("LitColor", (1.0, 1.0, 1.0), "03 Cel")
    rim_colour = g.vector("RimColor", (0.86, 0.9, 1.0), "06 Rim")
    rim_strength = g.scalar("RimStrength", 0.35, "06 Rim", 0.0, 3.0)
    hollow = g.vector("HollowColor", (0.06, 0.02, 0.1), "09 Dreamcore")
    compose = g.call(fn["MF_BIBChar_Compose"], {
        "Base": base, "Lit": (cel, "Lit"), "Edge": (cel, "Edge"), "ShadowColor": shadow_colour, "LitColor": lit_colour,
        "TerminatorColor": g.vector("TerminatorColor", (1.0, 0.42, 0.52), "03 Cel"),
        "TerminatorStrength": g.scalar("TerminatorStrength", 0.25, "03 Cel", 0.0, 1.0),
        "RampColor": g.switch("UseRampMap", False, g.rgb(ramp_map, "RGB"), g.const3((1.0, 1.0, 1.0)), "03 Cel"),
        "UseRamp": g.flag("UseRampMap", False, "03 Cel"),
        "Flatten": g.scalar("ShadingFlatten", 0.0, "03 Cel", 0.0, 1.0),
        "Ambient": g.rgb(g.collection(mpc_char, "AmbientColor")), "AmbientStrength": g.scalar("AmbientStrength", 0.5, "10 Scene", 0.0, 1.0),
        "KeyColor": g.rgb(g.collection(mpc_char, "KeyLightColor")), "KeyStrength": g.scalar("KeyColorStrength", 0.6, "10 Scene", 0.0, 1.0),
        "Spec": g.switch("UseSpecular", False, spec, g.const(0.0), "05 Specular"),
        "SpecColor": g.vector("SpecColor", (1.0, 0.96, 1.0), "05 Specular"),
        "Rim": rim, "RimColor": rim_colour, "RimStrength": rim_strength,
        # specular and matcap only exist in the permutations that use them (skin / cloth do not pay for them)
        "Matcap": g.switch("UseMatcap", False, matcap, g.const3((0.0, 0.0, 0.0)), "07 Matcap"),
        "MatcapStrength": g.scalar("MatcapStrength", 0.0, "07 Matcap", 0.0, 2.0),
        "Emissive": emissive_colour, "EmissiveStrength": emissive_strength, "EmissiveMask": emissive_mask,
        "Glint": glint, "GlintColor": g.vector("GlintColor", (1.0, 0.95, 1.0), "08 Emissive")})
    dream = g.call(fn["MF_BIBChar_Dreamcore"], {
        "Diffuse": (compose, "Diffuse"), "Glow": (compose, "Glow"), "Shadow": (compose, "Shadow"), "Rim": rim,
        "Pixel": (screen, "PixelPosition"), "Time": time, "Liminal": levels,
        "LiminalTint": g.vector("LiminalTint", (0.86, 0.78, 1.0), "09 Dreamcore"),
        "Dread": (levels, "Dread"), "DreadTint": g.vector("DreadTint", (0.74, 0.8, 0.56), "09 Dreamcore"),
        "Grain": g.scalar("ShadowGrain", 0.2, "09 Dreamcore", 0.0, 1.0),
        "GrainScale": g.scalar("GrainScale", 1.5, "09 Dreamcore", 1.0, 8.0),
        "GrainRate": g.scalar("GrainRate", 12.0, "09 Dreamcore", 1.0, 30.0),
        "Horror": (levels, "Horror"), "HollowColor": hollow,
        "IdentityProtect": g.scalar("IdentityProtect", 0.85, "09 Dreamcore", 0.0, 1.0), "IdentityMask": identity_mask,
        "ReadabilityRim": g.scalar("ReadabilityRim", 0.6, "09 Dreamcore", 0.0, 2.0), "RimColor": rim_colour})

    # ---- low-end fallback permutation (Material Quality Low, or ForceLowEnd)
    low = g.call(fn["MF_BIBChar_ComposeLow"], {
        "Base": base, "N": vertex_normal, "V": view, "L": l_ws, "Threshold": g.scalar("LowThreshold", 0.5, "11 Quality", 0.0, 1.0),
        "ShadowColor": shadow_colour, "LitColor": lit_colour, "RimColor": rim_colour, "RimStrength": rim_strength,
        "RimWidth": g.scalar("LowRimWidth", 0.2, "11 Quality", 0.0, 1.0), "Emissive": emissive_colour, "EmissiveStrength": emissive_strength,
        "Horror": (levels, "Horror"), "HollowColor": hollow})
    final = g.switch("ForceLowEnd", False, low, g.quality(dream, low), "11 Quality")
    g.out(final, MP.MP_EMISSIVE_COLOR)

    # ---- wrongness: a faint stepped vertex shimmer (off at 0 and on low quality)
    wobble = g.call(fn["MF_BIBChar_Wobble"], {
        "Pos": world_pos, "Nrm": vertex_normal, "Time": time,
        "Amount": g.scalar("Wrongness", 0.0, "09 Dreamcore", 0.0, 2.0),
        "Frequency": g.scalar("WrongnessFrequency", 0.05, "09 Dreamcore", 0.001, 0.5),
        "StepRate": g.scalar("WrongnessStepRate", 12.0, "09 Dreamcore", 1.0, 30.0),
        "Glitch": (levels, "Uncanny")})
    wpo = g.switch("ForceLowEnd", False, g.const3((0.0, 0.0, 0.0)), g.quality(wobble, g.const3((0.0, 0.0, 0.0))), "11 Quality")
    g.out(wpo, MP.MP_WORLD_POSITION_OFFSET)

    lib.recompile_material(material)
    save(material)
    log("material: M_BIBCharacter (%d parameters)" % len({p[1] for p in g.params}))
    return material, g.params


# ============================================================================ the outline

def build_outline(fn, mpc_char, mpc_weather, textures):
    material = tools.create_asset("M_BIBCharacterOutline", MATS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("opacity_mask_clip_value", 0.5)
    for flag in ("used_with_skeletal_mesh", "used_with_morph_targets"):
        try:
            material.set_editor_property(flag, True)
        except Exception as error:
            log("warn: M_BIBCharacterOutline.%s not set (%s)" % (flag, error))
    g = Graph(material, mpc_char, mpc_weather, textures)
    time = g.node(unreal.MaterialExpressionTime)
    vertex_normal = g.node(unreal.MaterialExpressionVertexNormalWS)
    vertex_colour = g.node(unreal.MaterialExpressionVertexColor)
    world_pos = g.node(unreal.MaterialExpressionWorldPosition)
    response = g.scalar("GlobalResponse", 1.0, "03 Dreamcore", 0.0, 1.0)
    levels = g.custom("BIBChar levels", GLOBALS_HLSL, [
        ("LocLiminal", g.const(0.0)), ("LocDread", g.const(0.0)),
        ("LocHorror", g.scalar("Horror", 0.0, "03 Dreamcore", 0.0, 1.0)), ("LocUncanny", g.scalar("Uncanny", 0.0, "03 Dreamcore", 0.0, 1.0)),
        ("GLiminal", g.const(0.0)), ("GDread", g.const(0.0)),
        ("GHorror", g.collection(mpc_char, "Horror")), ("GUncanny", g.collection(mpc_char, "Uncanny")), ("Response", response)],
        F1, [("Dread", F1), ("Horror", F1), ("Uncanny", F1)])
    wobble_amount = g.node(unreal.MaterialExpressionMultiply, g.x + 900)
    connect(g.scalar("OutlineWobble", 0.3, "03 Dreamcore", 0.0, 3.0), "", wobble_amount, "A", "wobble.A")
    connect(levels, "Uncanny", wobble_amount, "B", "wobble.B")
    wobble = g.call(fn["MF_BIBChar_Wobble"], {
        "Pos": world_pos, "Nrm": vertex_normal, "Time": time, "Amount": wobble_amount,
        "Frequency": g.scalar("WobbleFrequency", 0.08, "03 Dreamcore", 0.001, 0.5),
        "StepRate": g.scalar("WobbleStepRate", 8.0, "03 Dreamcore", 1.0, 30.0),
        "Glitch": (levels, "Uncanny")})
    vc_width = g.switch("UseVertexColorWidth", False, vertex_colour, g.const(1.0), "01 Outline", true_pin="A")
    offset = g.custom("BIBChar outline offset", OUTLINE_OFFSET_HLSL, [
        ("Nrm", vertex_normal), ("Cam", g.node(unreal.MaterialExpressionCameraPositionWS)),
        ("Obj", g.node(unreal.MaterialExpressionObjectPositionWS)),
        ("Width", g.scalar("OutlineWidth", 0.6, "01 Outline", 0.0, 5.0)),
        ("RefDist", g.scalar("OutlineRefDistance", 500.0, "01 Outline", 50.0, 5000.0)),
        ("MinScale", g.scalar("OutlineMinScale", 0.5, "01 Outline", 0.1, 2.0)),
        ("MaxScale", g.scalar("OutlineMaxScale", 4.0, "01 Outline", 1.0, 20.0)),
        ("VCWidth", vc_width), ("Horror", (levels, "Horror")),
        ("Grow", g.scalar("HorrorOutlineGrow", 0.6, "03 Dreamcore", 0.0, 2.0)),
        ("Wobble", g.quality(wobble, g.const3((0.0, 0.0, 0.0))))], F3)
    g.out(offset, MP.MP_WORLD_POSITION_OFFSET)
    vc_tint = g.switch("UseVertexColorTint", False, g.rgb(vertex_colour), g.const3((0.5, 0.5, 0.5)), "02 Colour")
    colour = g.custom("BIBChar outline colour", OUTLINE_COLOUR_HLSL, [
        ("OutlineColor", g.vector("OutlineColor", (0.05, 0.02, 0.08), "02 Colour")),
        ("VC", vc_tint), ("TintStrength", g.scalar("OutlineTintStrength", 0.0, "02 Colour", 0.0, 1.0)),
        ("InkColor", g.vector("InkColor", (0.01, 0.0, 0.02), "03 Dreamcore")), ("Horror", (levels, "Horror"))], F3)
    g.out(colour, MP.MP_EMISSIVE_COLOR)
    # inverted hull: only the back faces of the pushed-out shell are drawn
    mask = g.custom("BIBChar back faces", "return saturate(-Sign);", [("Sign", g.node(unreal.MaterialExpressionTwoSidedSign))], F1)
    g.out(mask, MP.MP_OPACITY_MASK)
    lib.recompile_material(material)
    save(material)
    log("material: M_BIBCharacterOutline (%d parameters)" % len({p[1] for p in g.params}))
    return material, g.params


# ============================================================================ instances

# PROPOSED looks (owner decides). Colours come from the project's own palette: the anime look's lavender shadow tier and
# violet-black ink (M_BIBAnimeStyle), the BB trailer colours (BIBFx: Rose #FF3D8D, Violet #7657FF, Angel Cyan #64E8FF).
INSTANCES = {
    "MI_BIBChar_Skin": {"vectors": {"BaseColor": (1.0, 0.86, 0.8), "ShadowColor": (0.86, 0.66, 0.78), "TerminatorColor": (1.0, 0.45, 0.45),
                                    "RimColor": (1.0, 0.9, 0.92)},
                        "scalars": {"ShadowSoftness": 0.035, "TerminatorStrength": 0.35, "RimStrength": 0.25, "RimWidth": 0.2}},
    "MI_BIBChar_Face": {"vectors": {"BaseColor": (1.0, 0.86, 0.8), "ShadowColor": (0.88, 0.7, 0.8), "TerminatorColor": (1.0, 0.45, 0.45)},
                        "scalars": {"ShadowSoftness": 0.03, "TerminatorStrength": 0.3, "RimStrength": 0.15, "FaceFlatten": 0.65},
                        "switches": {"FaceShading": True}},
    "MI_BIBChar_Hair": {"vectors": {"BaseColor": (0.09, 0.07, 0.13), "ShadowColor": (0.55, 0.45, 0.8), "SpecColor": (0.82, 0.78, 1.0),
                                    "RimColor": (0.66, 0.6, 1.0)},
                        "scalars": {"SpecIntensity": 0.7, "AnisoWidth": 0.06, "AnisoShift": 0.1, "SpecSoftness": 0.02,
                                    "RimStrength": 0.45},
                        "switches": {"UseSpecular": True, "HairShading": True}},
    "MI_BIBChar_Eye": {"vectors": {"BaseColor": (0.35, 0.6, 1.0), "EmissiveColor": (0.39, 0.91, 1.0), "GlintColor": (1.0, 1.0, 1.0)},
                       "scalars": {"ShadingFlatten": 0.75, "EmissiveStrength": 0.6, "GlintStrength": 1.5, "SpecIntensity": 1.0,
                                   "SpecThreshold": 0.97, "RimStrength": 0.0, "ShadowGrain": 0.0},
                       "switches": {"UseSpecular": True}},
    "MI_BIBChar_Cloth": {"vectors": {"BaseColor": (0.24, 0.2, 0.42), "ShadowColor": (0.58, 0.5, 0.86), "TerminatorColor": (0.46, 0.34, 1.0)},
                         "scalars": {"Bands": 3.0, "BandStep": 0.22, "ShadowSoftness": 0.02, "TerminatorStrength": 0.2, "RimStrength": 0.35}},
    "MI_BIBChar_Metal": {"vectors": {"BaseColor": (0.78, 0.8, 0.88), "ShadowColor": (0.4, 0.36, 0.62), "SpecColor": (1.0, 1.0, 1.0)},
                         "scalars": {"SpecIntensity": 1.4, "SpecThreshold": 0.86, "SpecSoftness": 0.01, "MatcapStrength": 0.8,
                                     "ShadowSoftness": 0.01, "RimStrength": 0.5},
                         "switches": {"UseSpecular": True, "UseMatcap": True}},
    "MI_BIBChar_Emissive": {"vectors": {"BaseColor": (0.2, 0.06, 0.12), "EmissiveColor": (1.0, 0.24, 0.55)},
                            "scalars": {"EmissiveStrength": 6.0, "IdentityProtect": 1.0, "RimStrength": 0.2}},
    "MI_BIBChar_Cloth_Low": {"vectors": {"BaseColor": (0.24, 0.2, 0.42), "ShadowColor": (0.58, 0.5, 0.86)},
                             "scalars": {"RimStrength": 0.35}, "switches": {"ForceLowEnd": True}},
}


def build_instances(master, outline):
    made = {}
    for name, spec in INSTANCES.items():
        mi = tools.create_asset(name, INST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi, master)
        for key, value in spec.get("scalars", {}).items():
            lib.set_material_instance_scalar_parameter_value(mi, key, value)
        for key, rgb in spec.get("vectors", {}).items():
            lib.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        for key, value in spec.get("switches", {}).items():
            lib.set_material_instance_static_switch_parameter_value(mi, key, value)
        lib.update_material_instance(mi)
        save(mi)
        made[name] = mi
    mi = tools.create_asset("MI_BIBChar_Outline", INST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mi, outline)
    lib.update_material_instance(mi)
    save(mi)
    made["MI_BIBChar_Outline"] = mi
    log("instances: %s" % ", ".join(sorted(made)))
    return made


# ============================================================================ test map

def build_test_map(instances):
    """Before (M_BIBTint, today's look) and after rows on the operators' own shapes (engine cylinder body + sphere head), a
    reference operator in each row, one camera. Character pixels write custom stencil CHARACTER_STENCIL so the anime volume
    leaves them to the new shader. Tools/verify_character_shader.py switches the lighting setups and takes the screenshots."""
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.new_level(TEST_MAP):
        raise RuntimeError("new_level failed: " + TEST_MAP)
    cylinder = unreal.load_asset("/Engine/BasicShapes/Cylinder")
    sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
    plane = unreal.load_asset("/Engine/BasicShapes/Plane")
    tint = unreal.load_asset("/Game/BIB/Materials/M_BIBTint")
    if tint is None:
        log("warn: /Game/BIB/Materials/M_BIBTint missing (run Tools/build_assets.sh first); the before row uses the engine material")

    def before_mi(name, rgb):
        if tint is None:
            return None
        mi = tools.create_asset(name, ROOT + "/Test", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(mi, tint)
        lib.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        lib.update_material_instance(mi)
        save(mi)
        return mi

    def spawn_mesh(mesh, location, scale, material, label, after):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0))
        actor.set_actor_label(label)
        actor.set_folder_path("After" if after else "Before")
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        if material is not None:
            comp.set_material(0, material)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        if after:
            comp.set_render_custom_depth(True)
            comp.set_custom_depth_stencil_value(CHARACTER_STENCIL)
            try:
                comp.set_editor_property("overlay_material", instances["MI_BIBChar_Outline"])
            except Exception as error:
                log("warn: overlay_material not set on %s (%s)" % (label, error))
        actor.set_editor_property("tags", [unreal.Name("BIBCharacterShaderTest"), unreal.Name("After" if after else "Before")])
        return actor

    swatches = [   # (label, instance, shape, before colour)
        ("Skin", "MI_BIBChar_Skin", "sphere", (1.0, 0.86, 0.8)),
        ("Face", "MI_BIBChar_Face", "sphere", (1.0, 0.86, 0.8)),
        ("Hair", "MI_BIBChar_Hair", "sphere", (0.09, 0.07, 0.13)),
        ("Eye", "MI_BIBChar_Eye", "sphere", (0.35, 0.6, 1.0)),
        ("Cloth", "MI_BIBChar_Cloth", "cylinder", (0.24, 0.2, 0.42)),
        ("Metal", "MI_BIBChar_Metal", "cylinder", (0.78, 0.8, 0.88)),
        ("Emissive", "MI_BIBChar_Emissive", "cylinder", (1.0, 0.24, 0.55)),
        ("ClothLow", "MI_BIBChar_Cloth_Low", "cylinder", (0.24, 0.2, 0.42)),
    ]
    # After row on the floor, before row on a shelf above it (same distance to the camera, nothing hides anything).
    for after in (False, True):
        z0 = 0.0 if after else 420.0
        prefix = "After_" if after else "Before_"
        for index, (label, mi_name, shape, rgb) in enumerate(swatches):
            y = -900.0 + index * 200.0
            material = instances[mi_name] if after else before_mi("MI_Before_" + label, rgb)
            if shape == "sphere":
                spawn_mesh(sphere, (0.0, y, z0 + 90.0), (1.0, 1.0, 1.0), material, prefix + label, after)
            else:
                spawn_mesh(cylinder, (0.0, y, z0 + 80.0), (0.8, 0.8, 1.6), material, prefix + label, after)
        # the reference operator: the in-game operator's shapes (cylinder body, sphere head) with cloth + face
        body = instances["MI_BIBChar_Cloth"] if after else before_mi("MI_Before_OperatorBody", (0.24, 0.2, 0.42))
        head = instances["MI_BIBChar_Face"] if after else before_mi("MI_Before_OperatorHead", (1.0, 0.86, 0.8))
        spawn_mesh(cylinder, (0.0, 760.0, z0 + 68.0), (0.8, 0.8, 1.35), body, prefix + "OperatorBody", after)
        spawn_mesh(sphere, (20.0, 760.0, z0 + 183.0), (0.64, 0.64, 0.64), head, prefix + "OperatorHead", after)

    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(200.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
    floor.set_actor_label("Floor")
    floor.static_mesh_component.set_static_mesh(plane)
    floor.set_actor_scale3d(unreal.Vector(30.0, 30.0, 1.0))
    grey = before_mi("MI_Before_Floor", (0.32, 0.3, 0.36))
    if grey is not None:
        floor.static_mesh_component.set_material(0, grey)

    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0), unreal.Rotator(0.0, -45.0, 140.0))
    sun.set_actor_label("KeyLight")
    sun.set_editor_property("tags", [unreal.Name("BIBCharacterShaderTest"), unreal.Name("KeyLight")])
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0), unreal.Rotator(0.0, 0.0, 0.0))
    sky.set_actor_label("SkyLight")
    camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-1650.0, -60.0, 300.0), unreal.Rotator(0.0, 0.0, 0.0))
    camera.set_actor_label("ShaderTestCamera")
    camera.set_editor_property("tags", [unreal.Name("BIBCharacterShaderTest"), unreal.Name("Camera")])
    try:
        camera.camera_component.set_editor_property("field_of_view", 70.0)
    except Exception as error:
        log("warn: camera fov not set (%s)" % error)
    if hasattr(unreal, "BIBAnimeStyleVolume"):
        volume = actors.spawn_actor_from_class(unreal.BIBAnimeStyleVolume, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))
        volume.set_actor_label("AnimeStyle")
    else:
        log("warn: BIBAnimeStyleVolume missing (build the editor module first); the test map has no anime volume")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    try:
        world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase.static_class())
    except Exception as error:
        log("warn: test map game mode not set (%s)" % error)
    if not levels.save_current_level():
        raise RuntimeError("save failed: " + TEST_MAP)
    log("map: %s (before / after rows of %d swatches + reference operator)" % (TEST_MAP, len(swatches)))


# ============================================================================ cost (needs compiled shaders; the editor reports them)

def report_cost(assets):
    for name, asset in assets:
        try:
            stats = lib.get_statistics(asset)
            log("cost: %-24s VS %4d  PS %4d  samplers %2d  PS texture samples %2d  interpolators %2d" % (
                name, stats.num_vertex_shader_instructions, stats.num_pixel_shader_instructions, stats.num_samplers,
                stats.num_pixel_texture_samples, stats.num_interpolator_scalars))
        except Exception as error:
            log("cost: %s unavailable here (%s) - Tools/verify_character_shader.py measures it in the editor" % (name, error))


def main():
    if unreal.EditorAssetLibrary.does_directory_exist(ROOT):
        unreal.EditorAssetLibrary.delete_directory(ROOT)
    mpc_weather = unreal.load_asset("/Game/Weather/MPC_BIBWeather")
    if mpc_weather is None:
        raise RuntimeError("/Game/Weather/MPC_BIBWeather missing: run Tools/build_weather.sh first (the characters follow its sun)")
    mpc_char = build_mpc()
    textures = build_default_textures()
    fn = {name: make_function(name, description, code, inputs, outputs) for name, description, code, inputs, outputs in FUNCTIONS}
    master, master_params = build_master(fn, mpc_char, mpc_weather, textures)
    outline, outline_params = build_outline(fn, mpc_char, mpc_weather, textures)
    instances = build_instances(master, outline)
    build_test_map(instances)
    report_cost([("M_BIBCharacter", master), ("M_BIBCharacterOutline", outline)] + sorted(instances.items()))
    log("parameters: master %d, outline %d" % (len({p[1] for p in master_params}), len({p[1] for p in outline_params})))


def run():
    status = "OK"
    try:
        main()
    except Exception:
        status = "FAILED"
        log(traceback.format_exc())
    log("STATUS " + status)
    path = os.path.join(DEMO, "Saved", "BuildCharacterShader.log")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
