"""Builds the demo's editor assets under /Game/BIB (Enhanced Input, Blueprints, UMG widgets and maps).

Re-runnable: it deletes /Game/BIB and rebuilds everything from this file.

    UnrealEditor-Cmd <BoldInBeautyDemo.uproject> -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi

Also imports the UI art (Art/Icons, Art/Portraits PNGs -> /Game/BIB/UI/Icons|Portraits/T_<name>), which the Ark's panels
show. (WBP_Hub, the old 2D lobby, was deleted on 2026-10-07: the Ark is the hub.) Needs the PythonScriptPlugin, EditorScriptingUtilities, ToolsetRegistry, EditorToolset and UMGToolSet plugins
(enabled in the .uproject). Widget graphs are written with the EditorToolset Blueprint DSL; widget trees with the
UMGToolSet. Gameplay numbers, names and dialogue all stay in C++ (BIBConfig.h / BIBTuning.h); this script only wires
assets to the existing hooks listed in README.md ("Editor assets"). A summary is written to Saved/BuildAssets.log.
"""

import glob
import json
import os
import traceback

import unreal
import toolset_registry
from editor_toolset.toolsets import blueprint_dsl
from editor_toolset.toolsets.blueprint import BlueprintTools as BT
from editor_toolset.toolsets.blueprint import ContainerType

ROOT = "/Game/BIB"
INPUT = ROOT + "/Input"
BLUEPRINTS = ROOT + "/Blueprints"
UI = ROOT + "/UI"
MAPS = ROOT + "/Maps"
MATERIALS = ROOT + "/Materials"

LOG = []


def log(*parts):
    line = " ".join(str(p) for p in parts)
    LOG.append(line)
    unreal.log("[build_assets] " + line)


def cls(path):
    loaded = unreal.load_class(None, path)
    if not loaded:
        raise RuntimeError("Missing class " + path)
    return loaded


def enum_value(enum_type, name):
    """Python exposes UENUM values in UPPER_SNAKE; match by name ignoring case and underscores."""
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted and not attr.startswith("_"):
            return getattr(enum_type, attr)
    raise RuntimeError("No %s.%s" % (enum_type, name))


def save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def make_blueprint(name, parent, folder=BLUEPRINTS):
    bp = BT.create(folder, name, parent)
    BT.compile_blueprint(bp)
    return bp


def cdo(bp):
    return unreal.get_default_object(bp.generated_class())


# ============================================================================ Enhanced Input

# Must match EBIBInput (BIBInput.h) and ABIBPlayerController::DefaultKey.
BUTTONS = [
    ("MoveForward", "W"), ("MoveBack", "S"), ("MoveLeft", "A"), ("MoveRight", "D"),
    ("Jump", "SpaceBar"), ("Run", "LeftShift"), ("Attack", "LeftMouseButton"), ("Block", "RightMouseButton"),
    ("Parry", "F"), ("Move1", "One"), ("Move2", "Two"), ("Move3", "Three"), ("Special", "Four"),
    ("UltimateE", "E"), ("UltimateR", "R"), ("SwitchOperator", "Tab"), ("Talk", "T"), ("Pause", "P"),
]


def make_key(name):
    key = unreal.Key()
    key.set_editor_property("key_name", name)
    return key


def build_input():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    actions = {}
    for name, _key in BUTTONS:
        action = tools.create_asset("IA_" + name, INPUT, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        actions[name] = action
    look = tools.create_asset("IA_Look", INPUT, unreal.InputAction, unreal.InputAction_Factory())
    look.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    context = tools.create_asset("IMC_BIB_Default", INPUT, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    for name, key in BUTTONS:
        context.map_key(actions[name], make_key(key))
    context.map_key(look, make_key("Mouse2D"))
    for asset in list(actions.values()) + [look, context]:
        save(asset)
    log("input: %d button actions + IA_Look + IMC_BIB_Default" % len(actions))
    return actions, look, context


# ============================================================================ tint material

def build_tint_material():
    """M_BIBTint: opaque, VectorParameter "Color" (default white) -> BaseColor. BIBVisuals tints every block, enemy,
    team member, door and the story NPC through it when it exists."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset("M_BIBTint", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    colour_param = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour_param.set_editor_property("parameter_name", "Color")
    colour_param.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    if not lib.connect_material_property(colour_param, "", unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("M_BIBTint: could not connect Color to BaseColor")
    roughness = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    roughness.set_editor_property("r", 0.6)
    lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(material)
    save(material)
    log("material: M_BIBTint (Color -> BaseColor)")
    return material


def build_fx_material():
    """M_BIBFx: unlit, additive, two-sided glow for combat effects (BIBGame/BIBFx.h): Color x Intensity -> Emissive, Opacity."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = tools.create_asset("M_BIBFx", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("two_sided", True)
    colour = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(1.0, 0.24, 0.55, 1.0))
    intensity = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 200)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 6.0)
    opacity = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 320)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 1.0)
    # soft edges: brighter where the surface faces the camera (a glowing core, fading rim)
    fresnel = lib.create_material_expression(material, unreal.MaterialExpressionFresnel, -700, 440)
    one_minus = lib.create_material_expression(material, unreal.MaterialExpressionOneMinus, -500, 440)
    lib.connect_material_expressions(fresnel, "", one_minus, "")
    mul1 = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 80)
    lib.connect_material_expressions(colour, "", mul1, "A")
    lib.connect_material_expressions(intensity, "", mul1, "B")
    mul2 = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 200)
    lib.connect_material_expressions(mul1, "", mul2, "A")
    lib.connect_material_expressions(one_minus, "", mul2, "B")
    mul3 = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, 260)
    lib.connect_material_expressions(mul2, "", mul3, "A")
    lib.connect_material_expressions(opacity, "", mul3, "B")
    if not lib.connect_material_property(mul3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("M_BIBFx: could not connect the emissive colour")
    lib.recompile_material(material)
    save(material)
    log("material: M_BIBFx (unlit additive glow)")
    return material


def build_screen_material():
    """M_BIBScreen: unlit video screen for the Ark (BIBGame/BIBScreen.h): TextureSampleParameter2D "Screen" (a media texture at
    runtime) x Brightness -> Emissive, with faint scanlines so the screens read as in-world displays."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = tools.create_asset("M_BIBScreen", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tex = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "Screen")
    tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
    bright = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 260)
    bright.set_editor_property("parameter_name", "Brightness")
    bright.set_editor_property("default_value", 1.3)
    mul = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 80)
    if not (lib.connect_material_expressions(tex, "", mul, "A") and lib.connect_material_expressions(bright, "", mul, "B")):
        raise RuntimeError("M_BIBScreen: could not wire the texture")
    if not lib.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("M_BIBScreen: could not connect the emissive colour")
    lib.recompile_material(material)
    save(material)
    log("material: M_BIBScreen (video screen)")
    return material


# ============================================================================ anime style (post process)

# HLSL of M_BIBAnimeStyle's Custom node. Runs after tonemapping on the final colour, so everything the scene drew
# (opaque, translucent VFX, text render damage, fog) is inked and shaded the same way. Lines come from three edges:
# depth (silhouettes, drawn on the nearer surface), normals (creases) and colour (VFX and translucency, which have no
# depth). Hand-drawn touches: a gentle line boil (strokes wobble slightly, re-seeded BoilRate times a second), brush
# pressure (line weight), steady shadow hatching and paper grain; FrameJitter (whole-image shake) exists but is 0. Shading is three hard
# bands with a deep lavender shadow tier, cross-hatching in the darkest areas and a warm flat highlight tier.
# Sizes are pixels at 1080p and scale with the resolution.
# Owner follow-up (2026-10-07): no extreme white light, keep the colours, and more shake and shading on the OBJECTS, not the
# camera. So: (1) highlights are soft-clipped below WhiteCeiling with their hue kept and bleached areas re-saturated (wet
# streets, sky glare and bloom no longer burn to white); (2) object boil: every surface's drawing wobbles with noise fixed
# to its world position (WorldPos input), re-drawn BoilRate times a second, so objects shimmer like hand-drawn cels while the
# camera, sky and HUD stay still; (3) form shading darkens surfaces as they turn away from the view and contact shading
# darkens creases and the ground under things (from the line pass's depth samples).
ANIME_STYLE_HLSL = r"""
const float3 W = float3(0.2126, 0.7152, 0.0722);
float2 VUV = GetViewportUV(Parameters);
float2 Px = GetSceneTextureViewSize(PPI_PostProcessInput0).zw;
float ViewHeight = GetSceneTextureViewSize(PPI_PostProcessInput0).y;
float Res = ViewHeight / 1080.0;
float2 Pix = VUV / Px;                                   // pixel position
// Line boil seed: changes BoilRate times a second (gentle, slower than the 12 fps steps).
float Seed = floor(View.GameTime * BoilRate);
float2 SeedOff = float2(PseudoRandom(float2(Seed, 1.7)), PseudoRandom(float2(Seed, 9.3))) * 512.0;

// Frame jitter (off by default, owner 2026-10-07: too jarring): shifts the whole drawing per seed.
float2 Jitter = (float2(PseudoRandom(float2(Seed, 3.1)), PseudoRandom(float2(Seed, 4.7))) - 0.5) * 2.0 * FrameJitter * Res;
float2 UVImage = VUV + Jitter * Px;

// Object boil: noise fixed to the surface's world position, re-seeded BoilRate times a second. Sky pixels (no surface) and
// the camera do not move; each object's drawing shimmers a little on its own.
float DC = SceneTextureLookup(ViewportUVToSceneTextureUV(VUV, PPI_SceneDepth), PPI_SceneDepth, false).r;
float SkyC = DC > SkyDepth ? 0.0 : 1.0;
float3 WP = WorldPos / ObjectBoilScale + float3(SeedOff.x, SeedOff.y, SeedOff.x * 0.37) * 0.01;
float2 ObjBoil = float2(ValueNoise3D_ALU(WP, false, 0.0), ValueNoise3D_ALU(WP + 17.3, false, 0.0)) * ObjectShake * Res * SkyC
                 * (1.0 - smoothstep(DetailFadeStart, DetailFadeEnd, DC));
UVImage += ObjBoil * Px;
float3 C = SceneTextureLookup(ViewportUVToSceneTextureUV(UVImage, PPI_PostProcessInput0), PPI_PostProcessInput0, false).rgb + Scene * 0.0;

// No blown-out white (owner 2026-10-07): luminance above WhiteKnee rolls off towards WhiteCeiling, hue kept; whatever the
// glare had bleached gets its colour back.
float LIn = dot(C, W);
if (LIn > WhiteKnee)
{
    float Span = max(WhiteCeiling - WhiteKnee, 1e-3);
    float LOut = WhiteKnee + Span * (1.0 - exp(-(LIn - WhiteKnee) / Span));
    C *= LOut / LIn;
    float Bleach = smoothstep(WhiteKnee, WhiteCeiling, LOut);
    C = max(0.0, lerp(dot(C, W).xxx, C, 1.0 + BrightSaturation * Bleach));
}
C = min(C, ChannelCeiling.xxx);

// Line boil: the line layer wobbles with low-frequency noise that changes every frame.
float2 BoilP = (Pix + SeedOff) / (BoilScale * Res);
float2 Boil = float2(ValueNoise3D_ALU(float3(BoilP, 0.0), false, 0.0), ValueNoise3D_ALU(float3(BoilP, 7.3), false, 0.0)) * BoilAmount * Res;
float2 UVLine = UVImage + Boil * Px * 0.6;

float D0 = SceneTextureLookup(ViewportUVToSceneTextureUV(UVLine, PPI_SceneDepth), PPI_SceneDepth, false).r + Depth * 0.0;
float3 N0 = SceneTextureLookup(ViewportUVToSceneTextureUV(UVLine, PPI_WorldNormal), PPI_WorldNormal, false).rgb + Normal * 0.0;
N0 *= rsqrt(max(dot(N0, N0), 1e-6));
float L0 = dot(C, W);
bool Sky0 = D0 > SkyDepth;
float Facing = Sky0 ? 1.0 : abs(dot(N0, Parameters.CameraVector));
// Brush pressure: line weight swells and thins along the stroke.
float Pressure = lerp(1.0 - LineVariation, 1.0 + LineVariation, ValueNoise3D_ALU(float3(Pix / (BoilScale * Res) * 0.35, 3.0), false, 0.0) * 0.5 + 0.5);   // steady, no pulsing
float Width = max(1.0, LineWidth * Res * Pressure * lerp(1.0, 0.45, saturate(D0 / LineFadeEnd)));
float DepthThreshold = DepthSensitivity * (1.0 + 6.0 * (1.0 - Facing));
const float2 Dirs[8] = { float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
                         float2(0.7071, 0.7071), float2(-0.7071, 0.7071), float2(0.7071, -0.7071), float2(-0.7071, -0.7071) };
float InkDepth = 0.0;
float InkNormal = 0.0;
float InkColour = 0.0;
float Contact = 0.0;   // how much nearer the neighbours are: creases, corners, the ground right under an object
for (int Ring = 1; Ring <= 2; Ring++)
{
    float Radius = Width * (Ring == 1 ? 0.5 : 1.0);
    for (int i = 0; i < 8; i++)
    {
        float2 Off = UVLine + Dirs[i] * Radius * Px;
        float D = SceneTextureLookup(ViewportUVToSceneTextureUV(Off, PPI_SceneDepth), PPI_SceneDepth, false).r;
        bool Sky = D > SkyDepth;
        if (!Sky0 && !Sky && D < D0)
        {
            Contact += saturate((D0 - D) / max(D0 * ContactRange, 1.0)) * (Ring == 1 ? 0.09 : 0.035);
        }
        if (!Sky0 && D > D0)
        {
            InkDepth = max(InkDepth, smoothstep(0.8, 1.2, ((D - D0) / max(D0, 1.0)) / DepthThreshold));
        }
        if (!Sky0 && !Sky)
        {
            float3 N = SceneTextureLookup(ViewportUVToSceneTextureUV(Off, PPI_WorldNormal), PPI_WorldNormal, false).rgb;
            N *= rsqrt(max(dot(N, N), 1e-6));
            InkNormal = max(InkNormal, smoothstep(NormalSensitivity * 0.8, NormalSensitivity * 1.2, 1.0 - dot(N, N0)));
        }
        if (Ring == 1)
        {
            float3 CN = SceneTextureLookup(ViewportUVToSceneTextureUV(Off, PPI_PostProcessInput0), PPI_PostProcessInput0, false).rgb;
            InkColour = max(InkColour, smoothstep(ColourSensitivity, ColourSensitivity * 1.6, abs(dot(CN, W) - L0)));
        }
    }
}
// Silhouettes reach far; creases and colour lines are for close detail, so busy distant geometry stays clean.
float Fade = 1.0 - smoothstep(LineFadeStart, LineFadeEnd, D0);
float DetailFade = 1.0 - smoothstep(DetailFadeStart, DetailFadeEnd, D0);
float Ink = saturate(max(InkDepth * Fade, max(InkNormal, InkColour * ColourLines) * DetailFade)) * LineOpacity;

// Heavy cel shading: hard bands at band centres, then a deep coloured shadow tier.
float L = max(L0, 1e-4);
float X = L * Bands - 0.5;
float Q = (floor(X) + smoothstep(0.5 - BandSoftness, 0.5 + BandSoftness, frac(X)) + 0.5) / Bands;
Q = lerp(L, Q, CelStrength);
float3 Col = C * (Q / L);
float Shade = 1.0 - smoothstep(ShadowLevel - 0.04, ShadowLevel + 0.04, L);
Col = lerp(Col, Col * ShadowTint * (1.0 - ShadowDarken), Shade * ShadowTintStrength);
// Highlight tier: the brightest band takes a warm tint over its own colour (multiplied, never pushed towards white).
float Lit = smoothstep(HighlightLevel - 0.03, HighlightLevel + 0.03, L);
Col = lerp(Col, Col * HighlightTint, Lit * HighlightStrength);
// Form shading: surfaces darken and cool as they turn away from the view, so every object reads as a rounded, drawn form.
float Turn = Sky0 ? 0.0 : pow(1.0 - Facing, 2.0);
Col = lerp(Col, Col * ShadowTint * 0.85, Turn * FormShade);
// Contact shading: creases and the ground under things get a soft ink shadow.
Col *= 1.0 - saturate(Contact) * ContactShade * DetailFade;
Col = max(0.0, lerp(dot(Col, W).xxx, Col, Saturation));

// Hand-drawn hatching in the shadows (cross-hatched in the deepest ones); steady so it does not flicker.
float Spacing = HatchSpacing * Res;
float Wob = ValueNoise3D_ALU(float3(Pix / (BoilScale * Res) * 0.6, 11.0), false, 0.0) * 1.5;   // fixed: hatching does not move
float H1 = abs(frac((Pix.x + Pix.y) / Spacing + Wob * 0.15) - 0.5) * 2.0;
float H2 = abs(frac((Pix.x - Pix.y) / Spacing + Wob * 0.15) - 0.5) * 2.0;
float Deep = 1.0 - smoothstep(ShadowLevel * 0.45 - 0.04, ShadowLevel * 0.45 + 0.04, L);
float HatchInk = max((1.0 - smoothstep(0.0, HatchWidth, H1)) * Shade, (1.0 - smoothstep(0.0, HatchWidth, H2)) * Deep) * HatchStrength * DetailFade * (Sky0 ? 0.0 : 1.0);
Col = lerp(Col, Col * 0.35 + LineColour * 0.3, HatchInk);

// Paper grain (fixed to the screen, like the paper under the cels).
Col *= 1.0 - PaperGrain * PseudoRandom(Pix);

// Character pixels (custom stencil CharacterStencil, written by meshes that use the BB character shader in /Game/BIBCharacter,
// owner 2026-10-15): their material already drew the cel bands, shadows and dream-core layer, so the post-process gives them
// back their own colour and keeps only its ink lines (and the haze, grain and vignette below). Nothing in game writes the
// stencil yet, so today's look is unchanged.
float IsChar = (CharacterKeep > 0.0 && abs(Stencil - CharacterStencil) < 0.5) ? saturate(CharacterKeep) : 0.0;
Col = lerp(Col, C, IsChar);
// Screen-space depth rim (optional, character pixels only): a thin light band just inside the silhouette line, where the
// depth falls away behind the character a little further out than the ink reaches.
if (IsChar > 0.0 && DepthRimStrength > 0.0)
{
    float RimR = Width + DepthRimWidth * Res;
    float DepthRim = 0.0;
    for (int k = 0; k < 4; k++)
    {
        float D = SceneTextureLookup(ViewportUVToSceneTextureUV(UVLine + Dirs[k] * RimR * Px, PPI_SceneDepth), PPI_SceneDepth, false).r;
        DepthRim = max(DepthRim, smoothstep(0.8, 1.2, ((D - D0) / max(D0, 1.0)) / DepthThreshold));
    }
    Col += DepthRimColour * DepthRim * (1.0 - InkDepth) * DepthRimStrength * IsChar;
}

// Coloured ink: lines take a little of the colour they cross.
float3 InkCol = lerp(LineColour, C * 0.3, LineChroma);
Col = lerp(Col, InkCol, Ink);

// ---- Dreamcore (owner 2026-10-09: "make the entire building themes and especially shader be a bit more dream core looking and
// especially keep that slightly terrifying theme"). Soft pastel haze that thickens with distance, a hazy glow around bright
// things, colour fringes towards the edges, a slow breathing of the image edges, rose-black corners, film grain and, every so
// often, a thin band of the picture slipping sideways like a half-remembered frame. DreamStrength 0 turns it all off.
float2 Ctr = VUV - 0.5;
float Aspect = Px.y / max(Px.x, 1e-6);
float R = length(Ctr * float2(1.0 / max(Aspect, 1e-3), 1.0)) * 1.15;
float DT = View.GameTime;
// breathing: the edges of the picture swell and settle
float2 BreathOff = Ctr * sin(DT * DreamBreathRate) * DreamBreath * R * R;
// memory glitch: a short horizontal band slips for a moment, a few times a minute
float GlitchSeed = floor(DT * 0.35);
float GlitchOn = step(0.72, PseudoRandom(float2(GlitchSeed, 5.1))) * step(frac(DT * 0.35), 0.06) * DreamGlitch;
float BandY = PseudoRandom(float2(GlitchSeed, 8.3));
float InBand = GlitchOn * (1.0 - smoothstep(0.0, 0.025, abs(VUV.y - BandY)));
float2 DUV = UVImage + BreathOff + float2(InBand * 0.03, 0.0);
// colour fringe grows towards the corners
float2 FringeDir = Ctr * DreamFringe * (0.3 + R * R);
float FR = SceneTextureLookup(ViewportUVToSceneTextureUV(DUV + FringeDir, PPI_PostProcessInput0), PPI_PostProcessInput0, false).r;
float FB = SceneTextureLookup(ViewportUVToSceneTextureUV(DUV - FringeDir, PPI_PostProcessInput0), PPI_PostProcessInput0, false).b;
float3 Shifted = SceneTextureLookup(ViewportUVToSceneTextureUV(DUV, PPI_PostProcessInput0), PPI_PostProcessInput0, false).rgb;
Col += (float3(FR, Shifted.g, FB) - C) * saturate(R * 1.4 + InBand) * DreamStrength;
// hazy glow: bright surroundings bleed into a soft pastel bloom
float3 Glow = 0.0;
float GR = DreamGlowRadius * Res;
for (int g = 0; g < 8; g++)
{
    float A = g * 0.785398 + 0.39;
    float3 S = SceneTextureLookup(ViewportUVToSceneTextureUV(DUV + float2(cos(A), sin(A)) * GR * Px, PPI_PostProcessInput0), PPI_PostProcessInput0, false).rgb;
    Glow += max(0.0, dot(S, W) - DreamGlowThreshold).xxx;
}
Col += Glow / 8.0 * DreamGlowColour * DreamGlow * DreamStrength;
// pastel haze with distance (the sky takes it fully): the far world turns to a lavender dream
float Haze = Sky0 ? 0.65 : (1.0 - exp(-D0 / max(DreamHazeDistance, 1.0)));
// (the haze keeps each pixel's brightness: it tints, it never bleaches - owner 2026-10-07 "no extreme white light")
float LH = dot(Col, W);
float3 HazeTarget = DreamHazeColour * clamp(LH * 1.15 + 0.08, 0.18, 0.72);
Col = lerp(Col, HazeTarget, saturate(Haze * DreamHaze * DreamStrength));
// deep shadows sink into violet-black instead of grey: the dream stays a little frightening
float LC = dot(Col, W);
Col = lerp(Col, DreamShadowColour * LC * 2.2, (1.0 - smoothstep(0.04, 0.2, LC)) * DreamShadowPull * DreamStrength * (1.0 - IsChar));
// corners close in, rose-black
float Vig = smoothstep(0.38, 0.95, R) * DreamVignette * DreamStrength;
Col = lerp(Col, Col * DreamVignetteColour, Vig);
// grain, re-seeded with the line boil
Col += (PseudoRandom(Pix + SeedOff) - 0.5) * DreamGrain * DreamStrength;
// ---- impact frame (2026-10-09, the "13th frame"): a heavy moment's extra frame is drawn two-tone like an anime impact frame -
// near-black and paper-white split by brightness, rose ink on every line and radial speed lines towards the edges.
if (ImpactFrame > 0.5)
{
    float LI = dot(Col, W);
    float3 Imp = lerp(float3(0.035, 0.01, 0.05), float3(0.99, 0.93, 0.96), step(0.4, LI));
    float Ang = atan2(Ctr.y, Ctr.x) / 6.2831853 + 0.5;
    float Spoke = floor(Ang * 48.0);
    float Speed = step(0.55, PseudoRandom(float2(Spoke, Seed))) * step(0.7, frac(Ang * 48.0)) * smoothstep(0.3, 0.75, R);
    Imp = lerp(Imp, float3(1.0, 0.24, 0.55), saturate(Ink * 1.5 + Speed));
    Col = Imp;
}
return clamp(Col, 0.0, ChannelCeiling);
"""

ANIME_SCALARS = [
    # name, default
    ("LineWidth", 4.0), ("LineOpacity", 1.0), ("LineVariation", 0.3), ("LineChroma", 0.3),
    ("DepthSensitivity", 0.06), ("NormalSensitivity", 0.45), ("ColourSensitivity", 0.28), ("ColourLines", 0.85),
    ("LineFadeStart", 8000.0), ("LineFadeEnd", 80000.0), ("DetailFadeStart", 2500.0), ("DetailFadeEnd", 15000.0),
    ("SkyDepth", 1000000.0),
    ("BoilRate", 8.0), ("BoilAmount", 0.5), ("BoilScale", 60.0), ("FrameJitter", 0.0),
    ("Bands", 3.0), ("BandSoftness", 0.02), ("CelStrength", 1.0),
    ("ShadowLevel", 0.38), ("ShadowTintStrength", 0.9), ("ShadowDarken", 0.25),
    ("HighlightLevel", 0.78), ("HighlightStrength", 0.35),
    ("WhiteKnee", 0.6), ("WhiteCeiling", 0.84), ("ChannelCeiling", 0.94), ("BrightSaturation", 0.6),
    ("ObjectShake", 1.4), ("ObjectBoilScale", 140.0),
    ("FormShade", 0.45), ("ContactShade", 0.55), ("ContactRange", 0.04),
    ("HatchSpacing", 7.0), ("HatchWidth", 0.28), ("HatchStrength", 0.55),
    ("PaperGrain", 0.05), ("Saturation", 1.3),
    # dreamcore layer (2026-10-09)
    ("DreamStrength", 1.0), ("DreamHaze", 0.35), ("DreamHazeDistance", 9000.0), ("DreamGlow", 0.45), ("DreamGlowRadius", 16.0),
    ("DreamGlowThreshold", 0.6), ("DreamFringe", 0.012), ("DreamVignette", 0.75), ("DreamGrain", 0.04), ("DreamBreath", 0.012),
    ("DreamBreathRate", 0.5), ("DreamGlitch", 1.0), ("DreamShadowPull", 0.6),
    ("ImpactFrame", 0.0),
    # BB character shader hand-off (2026-10-15): pixels with this custom stencil keep their own shading; optional depth rim
    ("CharacterStencil", 77.0), ("CharacterKeep", 1.0), ("DepthRimStrength", 0.3), ("DepthRimWidth", 3.0),
]
ANIME_VECTORS = [
    ("LineColour", (0.03, 0.01, 0.06)), ("ShadowTint", (0.5, 0.42, 0.95)), ("HighlightTint", (1.0, 0.93, 0.8)),
    ("DreamHazeColour", (0.78, 0.66, 0.95)), ("DreamGlowColour", (1.0, 0.72, 0.9)), ("DreamVignetteColour", (0.16, 0.02, 0.12)),
    ("DreamShadowColour", (0.22, 0.08, 0.36)), ("DepthRimColour", (1.0, 0.9, 0.96)),
]


def build_anime_style_material():
    """M_BIBAnimeStyle (post process, after tonemapping) + MI_BIBAnimeStyle, used by ABIBAnimeStyleVolume
    (BIBGame/BIBAnimeStyle.h): thick ink lines, cel bands, cool shadows, saturation. Tune the instance's parameters."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = tools.create_asset("M_BIBAnimeStyle", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property("description", "BIB anime style")
    custom.set_editor_property("code", ANIME_STYLE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["Scene", "Depth", "Normal", "Stencil", "WorldPos"] + [n for n, _ in ANIME_SCALARS] + [n for n, _ in ANIME_VECTORS]
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)

    # The centre samples come from SceneTexture nodes, which also tell the compiler which scene textures to bind. The custom
    # stencil (2026-10-15) marks the BB character shader's pixels; it reads 0 unless r.CustomDepth=3 and a mesh writes it.
    for index, (name, texture_id, pin) in enumerate([
            ("Scene", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, "Color"),
            ("Depth", unreal.SceneTextureId.PPI_SCENE_DEPTH, "Color"),
            ("Normal", unreal.SceneTextureId.PPI_WORLD_NORMAL, "Color"),
            ("Stencil", unreal.SceneTextureId.PPI_CUSTOM_STENCIL, "Color")]):
        node = lib.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -900, -400 + index * 140)
        node.set_editor_property("scene_texture_id", texture_id)
        source = node
        if name in ("Depth", "Stencil"):
            mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -650, -400 + index * 140)
            mask.set_editor_property("r", True)
            mask.set_editor_property("g", False)
            mask.set_editor_property("b", False)
            mask.set_editor_property("a", False)
            lib.connect_material_expressions(node, pin, mask, "")
            source, pin = mask, ""
        elif pin == "Color":
            mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -650, -400 + index * 140)
            for channel, on in (("r", True), ("g", True), ("b", True), ("a", False)):
                mask.set_editor_property(channel, on)
            lib.connect_material_expressions(node, pin, mask, "")
            source, pin = mask, ""
        if not lib.connect_material_expressions(source, pin, custom, name):
            raise RuntimeError("M_BIBAnimeStyle: could not connect " + name)
    # Absolute world position of the pixel (reconstructed from depth in a post-process material): drives the object boil.
    world = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, 20)
    if not lib.connect_material_expressions(world, "", custom, "WorldPos"):
        raise RuntimeError("M_BIBAnimeStyle: could not connect WorldPos")
    y = 40
    for name, default in ANIME_SCALARS:
        param = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -650, y)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", default)
        param.set_editor_property("group", "Anime style")
        if not lib.connect_material_expressions(param, "", custom, name):
            raise RuntimeError("M_BIBAnimeStyle: could not connect " + name)
        y += 90
    for name, rgb in ANIME_VECTORS:
        param = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -650, y)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        param.set_editor_property("group", "Anime style")
        mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -400, y)
        for channel, on in (("r", True), ("g", True), ("b", True), ("a", False)):
            mask.set_editor_property(channel, on)
        lib.connect_material_expressions(param, "", mask, "")
        if not lib.connect_material_expressions(mask, "", custom, name):
            raise RuntimeError("M_BIBAnimeStyle: could not connect " + name)
        y += 200
    if not lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("M_BIBAnimeStyle: could not connect the output")
    lib.recompile_material(material)
    save(material)

    instance = tools.create_asset("MI_BIBAnimeStyle", MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance, material)
    save(instance)
    log("material: M_BIBAnimeStyle + MI_BIBAnimeStyle (%d parameters)" % (len(ANIME_SCALARS) + len(ANIME_VECTORS)))
    return instance


# ============================================================================ widget helpers

def umg(tool, **kwargs):
    result = unreal.ToolsetRegistry.execute_tool("UMGToolSet.UMGToolSet", tool, json.dumps(kwargs))
    if result.error:
        raise RuntimeError("UMGToolSet.%s: %s" % (tool, result.error))
    return json.loads(result.value).get("returnValue")


def obj(ref):
    found = unreal.find_object(None, ref["refPath"])
    if not found:
        raise RuntimeError("Object not found: " + ref["refPath"])
    return found


def colour(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


TEXT = colour(0.92, 0.93, 0.96)
DIM = colour(0.65, 0.68, 0.75)
ACCENT = colour(0.25, 0.45, 0.85)
BUTTON = colour(0.18, 0.2, 0.28)
DANGER = colour(0.45, 0.14, 0.18)
PANEL = colour(0.02, 0.025, 0.04, 0.88)


class WidgetBuilder:
    """Builds one Widget Blueprint: widget tree via UMGToolSet, graph via the Blueprint DSL."""

    def __init__(self, name, parent="/Script/BoldInBeautyDemo.BIBUserWidget"):
        self.name = name
        self.bp = BT.create(UI, name, cls(parent))
        self.ref = {"refPath": self.bp.get_path_name()}
        self.buttons = []
        self.member_vars = []
        self.root = self.add("CanvasPanel", "Root")

    # ---------------------------------------------------------------- tree
    def add(self, widget_class, name, parent=None, variable=False):
        kwargs = dict(WidgetBlueprint=self.ref, WidgetClass={"refPath": "/Script/UMG." + widget_class}, WidgetDisplayName=name)
        if parent is not None:
            kwargs["ParentWidget"] = parent["widget"]
        info = umg("AddWidget", **kwargs)
        if not info or info.get("widgetName") != name:
            raise RuntimeError("AddWidget %s %s failed: %s" % (widget_class, name, info))
        if variable:
            umg("ToggleWidgetAsVariable", WidgetBlueprint=self.ref, Widget=info["widget"], bIsVariable=True)
        return info

    def text(self, name, parent, value="", size=16, tint=TEXT, variable=True, wrap=True):
        info = self.add("TextBlock", name, parent, variable)
        widget = obj(info["widget"])
        widget.set_text(unreal.Text(value))
        font = widget.get_editor_property("font")
        font.set_editor_property("size", size)
        widget.set_editor_property("font", font)
        widget.set_color_and_opacity(unreal.SlateColor(tint))
        widget.set_auto_wrap_text(wrap)
        return info

    def button(self, name, parent, label, body, tint=BUTTON, size=15, label_var=False):
        info = self.add("Button", name, parent, True)
        obj(info["widget"]).set_background_color(tint)
        self.text(name + "Label", info, label, size=size, variable=label_var, wrap=False)
        self.buttons.append((name, body))
        self.pad(info, 3)
        return info

    def pad(self, info, amount):
        slot = info.get("slot")
        if slot and slot != "None":
            slot_obj = obj(slot)
            if hasattr(slot_obj, "set_padding"):
                slot_obj.set_padding(unreal.Margin(amount, amount, amount, amount))

    def place(self, info, anchor, align, pos, size=None):
        slot = obj(info["slot"])
        anchors = unreal.Anchors()
        anchors.set_editor_property("minimum", unreal.Vector2D(anchor[0], anchor[1]))
        anchors.set_editor_property("maximum", unreal.Vector2D(anchor[2], anchor[3]) if len(anchor) == 4 else unreal.Vector2D(anchor[0], anchor[1]))
        slot.set_anchors(anchors)
        slot.set_alignment(unreal.Vector2D(align[0], align[1]))
        if len(anchor) == 4:
            slot.set_offsets(unreal.Margin(pos[0], pos[1], pos[2], pos[3]))
        else:
            slot.set_position(unreal.Vector2D(pos[0], pos[1]))
            if size:
                slot.set_auto_size(False)
                slot.set_size(unreal.Vector2D(size[0], size[1]))
            else:
                slot.set_auto_size(True)

    def panel(self, name, parent, tint=PANEL, padding=14):
        info = self.add("Border", name, parent, True)
        border = obj(info["widget"])
        border.set_brush_color(tint)
        border.set_padding(unreal.Margin(padding, padding, padding, padding))
        return info

    def screen_frame(self, title):
        """Full-screen menu frame: title, info text, button wrap box and status line."""
        frame = self.panel("Frame", self.root)
        self.place(frame, (0, 0, 1, 1), (0, 0), (60, 50, 60, 50))
        body = self.add("VerticalBox", "Body", frame)
        self.text("Title", body, title, size=28, tint=colour(1.0, 0.85, 0.45), variable=False)
        info = self.add("ScrollBox", "InfoScroll", body)
        slot = obj(info["slot"])
        fill = unreal.SlateChildSize()
        fill.set_editor_property("size_rule", unreal.SlateSizeRule.FILL)
        slot.set_size(fill)
        self.text("Info", info, "", size=16)
        buttons = self.add("WrapBox", "Buttons", body)
        self.pad(buttons, 4)
        self.text("Status", body, "", size=15, tint=colour(1.0, 0.75, 0.4))
        return buttons

    def member(self, name, type_name, array=False):
        BT.add_variable(self.bp, name, type_name, container_type=ContainerType.ARRAY if array else None)
        self.member_vars.append(name)

    # ---------------------------------------------------------------- graph
    def finish(self, tick=None, construct=None):
        umg("CompileWidgetBlueprint", WidgetBlueprint=self.ref)
        for name, _body in self.buttons:
            ok = umg("BindToEventProperty", WidgetBlueprint=self.ref, EventName="OnClicked", PropertyName=name,
                     PropertyClass={"refPath": "/Script/UMG.Button"})
            if not ok:
                raise RuntimeError("BindToEventProperty failed for " + name)
        graph = BT.get_graph(self.bp, "EventGraph")
        code = []
        if construct:
            code.append("(event UserInterface|EventConstruct\n%s)" % construct)
        if tick:
            code.append("(event UserInterface|EventTick (MyGeometry InDeltaTime)\n%s)" % tick)
        for name, body in self.buttons:
            code.append("(event OnClicked(%s)\n%s)" % (name, body))
        source = "\n\n".join(code)
        try:
            write_dsl(graph, source)
        except Exception:
            log("DSL for %s failed:\n%s" % (self.name, source))
            raise
        if not umg("CompileWidgetBlueprint", WidgetBlueprint=self.ref):
            raise RuntimeError("Compile failed: " + self.name)
        save(self.bp)
        log("widget: %s (%d buttons)" % (self.name, len(self.buttons)))
        return self.bp


def finish_plain(w):
    """Compiles and saves a widget whose behaviour is entirely in its C++ parent class (no graph)."""
    if not umg("CompileWidgetBlueprint", WidgetBlueprint=w.ref):
        raise RuntimeError("Compile failed: " + w.name)
    save(w.bp)
    log("widget: %s (C++ driven)" % w.name)
    return w.bp


def _create_node(graph, type_id, pos, declaring_class=None):
    """DSL node factory. Adds 'W|GetX' / 'W|SetX' for this Blueprint's own members (widget variables are not
    listed by the toolset's node search)."""
    if type_id.startswith("W|Get") or type_id.startswith("W|Set"):
        editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        member = unreal.Name(type_id[5:])
        node = editor.add_get_member_variable_node(member) if type_id.startswith("W|Get") else editor.add_set_member_variable_node(member)
        if not isinstance(node, unreal.EdGraphNode):
            raise RuntimeError("No member variable " + type_id[5:])
        node.set_node_pos(pos)
        return node
    return BT.create_node(graph, type_id, pos, declaring_class)


def write_dsl(graph, source):
    blueprint_dsl.Transpiler(
        graph, _create_node, BT.connect_pins, BT._get_node_info, BT.set_pin_value,
        lambda g: BT.find_nodes(g), delete_node_fn=BT.delete_node,
        find_node_types_fn=lambda f: BT.find_node_types(graph, f)).transpile(source)
    blueprint = unreal.Blueprint.cast(graph.get_outer())
    BT.compile_blueprint(blueprint)


# DSL snippets ---------------------------------------------------------------

_counter = [0]


def uid(prefix):
    _counter[0] += 1
    return "%s%d" % (prefix, _counter[0])


STRUCT_OUTS = {
    "BIBHudInfo": ["bValid", "ActiveName", "HP", "MaxHP", "HPFraction", "Ult", "UltFraction", "Colour", "Abilities",
                   "Team", "KitLine", "ActionLine", "bJudgeModeMenu", "bExchangeOpen", "ExchangeValues", "ExchangeFrom",
                   "ExchangeTo", "StageTitle", "Objective", "Messages", "bDialogueOpen", "DialogueSpeaker",
                   "DialogueLine", "ControlsHint"],
    "BIBAbilityInfo": ["Key", "Name", "CooldownRemaining", "Note"],
    "BIBTeamMemberInfo": ["Name", "HPFraction", "bAlive", "bActive"],
    "BIBOperatorInfo": ["RosterIndex", "Name", "Stars", "RarityLabel", "bOwned", "bPlayable", "Level", "LevelCap",
                        "UpgradeStars", "DuplicateUnits", "NextStarCost", "NextLevelGold", "NextLevelBytes",
                        "bNeedsHeartGate", "HeartGateCost", "MaxHP", "DamageMultiplier", "DefencePercent", "Label"],
    "BIBBannerInfo": ["Index", "Title", "Kind", "FeaturedName", "FeaturedStars", "MileageRedeemCost", "RollsSince5Star",
                      "RollsSince6Star", "bLostFiftyFifty", "Description"],
    "BIBEquipmentInfo": ["Id", "Name", "SlotIndex", "SlotName", "bDamageBonus", "TotalPercent", "UpgradeLevel",
                         "UpgradeCostBytes", "EquippedBy", "EquippedByName", "Label"],
}


def brk(struct, expr):
    """Returns (dsl_line, fields) breaking a struct; fields maps member name -> bound variable."""
    prefix = uid("s")
    fields = {name: "%s_%s" % (prefix, name) for name in STRUCT_OUTS[struct]}
    names = " ".join(fields[n] for n in STRUCT_OUTS[struct])
    return "(bind (%s) (Utilities|Struct|Break%s %s))" % (names, struct, expr), fields


def q(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cat(*parts):
    """String concatenation of DSL expressions."""
    expr = parts[0]
    for part in parts[1:]:
        expr = "(Utilities|String|Append %s %s)" % (expr, part)
    return expr


def int_str(expr):
    return "(Utilities|String|ToString(Integer) %s)" % expr


def set_text(widget, string_expr):
    return "(Widget|SetText(Text) :self (W|Get%s) :InText (Utilities|Text|ToText(String) %s))" % (widget, string_expr)


def show(widget, cond):
    return "(if %s\n  (Widget|SetVisibility :self (W|Get%s) :InVisibility \"SelfHitTestInvisible\")\n  (else (Widget|SetVisibility :self (W|Get%s) :InVisibility \"Collapsed\")))" % (cond, widget, widget)


def joined(array_expr):
    return "(Utilities|String|JoinStringArray %s %s)" % (array_expr, q("\n"))


def goto(screen):
    return "(BIB|UI|GotoScreen :Screen %s)" % q(screen)


STATUS = set_text("Status", "(BIB|UI|GetStatusText)")
CURRENCY = "(BIB|Profile|GetCurrencySummary)"  # already ends with "| Day N"
BACK = goto("MainMenu")


def pc_get(prop):
    return "(Class|BIBPlayerController|Get%s :self (BIB|UI|GetBIBController :self self))" % prop


def pc_set(prop, value_expr):
    return "(Class|BIBPlayerController|Set%s :self (BIB|UI|GetBIBController :self self) :%s %s)" % (prop, prop, value_expr)


def lines_loop(array_expr, struct, line_fn):
    """Clears member array Lines and appends one string per struct element; line_fn(fields) -> string expr."""
    elem = uid("e")
    line, fields = brk(struct, elem)
    return "(Utilities|Array|Clear (W|GetLines))\n(for %s %s\n  %s\n  (Utilities|Array|Add (W|GetLines) %s))" % (
        elem, array_expr, line, line_fn(fields))


# ============================================================================ widgets

def build_results():
    w = WidgetBuilder("WBP_Results")
    row = w.screen_frame("RESULTS")
    w.button("Continue", row, "Continue", "(BIB|Stage|ReturntoHub)", tint=ACCENT, size=18)
    ok = uid("ok")
    lines = uid("l")
    tick = ["(bind (%s %s) (BIB|Stage|GetResultLines))" % (ok, lines),  # outputs: bSuccess, ReturnValue
            set_text("Info", cat("(select %s \"STAGE CLEARED\" \"STAGE FAILED\")" % ok, q("\n\n"), joined(lines))), STATUS]
    return w.finish(tick="\n".join(tick))


def build_pause():
    w = WidgetBuilder("WBP_Pause")
    row = w.screen_frame("PAUSED")
    w.button("Resume", row, "Resume", goto("None"), tint=ACCENT, size=18)
    w.button("Abandon", row, "Abandon stage", "(BIB|Stage|AbandonStage)", tint=DANGER, size=18)
    return w.finish(tick="\n".join([set_text("Info", q("P / Resume continues the stage.")), STATUS]))


def build_hud():
    w = WidgetBuilder("WBP_HUD")
    w.member("Lines", "string", array=True)
    root = w.root
    top = w.panel("TopLeft", root, padding=10)
    w.place(top, (0, 0), (0, 0), (20, 20))
    col = w.add("VerticalBox", "TopLeftBox", top)
    w.text("NameText", col, "", size=22, wrap=False)
    hp = w.add("ProgressBar", "HPBar", col, True)
    obj(hp["widget"]).set_fill_color_and_opacity(colour(0.2, 0.8, 0.3))
    w.text("HPText", col, "", size=14, wrap=False)
    ult = w.add("ProgressBar", "UltBar", col, True)
    obj(ult["widget"]).set_fill_color_and_opacity(colour(0.95, 0.75, 0.2))
    w.text("UltText", col, "", size=14, wrap=False)
    w.text("TeamText", col, "", size=15, wrap=False)

    objective = w.panel("ObjectivePanel", root, padding=10)
    w.place(objective, (1, 0), (1, 0), (-20, 20))
    w.text("ObjectiveText", objective, "", size=16, wrap=False)

    # Left column under the team panel: the controls line is wide and overlapped the objective panel at top centre.
    kit = w.panel("KitPanel", root, padding=8)
    w.place(kit, (0, 0), (0, 0), (20, 225))
    w.text("KitText", kit, "", size=15, wrap=False)

    messages = w.panel("MessagePanel", root, padding=8)
    w.place(messages, (0, 1), (0, 1), (20, -130))
    w.text("MessagesText", messages, "", size=14, wrap=False)

    abilities = w.add("HorizontalBox", "Abilities", root)
    w.place(abilities, (0.5, 1), (0.5, 1), (0, -20))
    for index in range(6):
        box = w.panel("AbilityBox%d" % index, abilities, tint=colour(0.08, 0.09, 0.14, 0.9), padding=8)
        w.pad(box, 3)
        w.text("Ab%d" % index, box, "", size=14, wrap=False)

    dialogue = w.panel("DialogueBox", root, tint=colour(0.03, 0.03, 0.06, 0.92), padding=16)
    w.place(dialogue, (0.5, 1), (0.5, 1), (0, -110), (900, 150))
    dcol = w.add("VerticalBox", "DialogueCol", dialogue)
    w.text("DialogueSpeaker", dcol, "", size=18, tint=colour(1.0, 0.85, 0.45))
    w.text("DialogueLine", dcol, "", size=17)
    w.text("DialogueHint", dcol, "T: continue", size=13, tint=DIM, variable=False)

    court = w.panel("CourtPanel", root, tint=colour(0.12, 0.08, 0.02, 0.95), padding=16)
    w.place(court, (0.5, 0.25), (0.5, 0.5), (0, 0))
    ccol = w.add("VerticalBox", "CourtCol", court)
    w.text("CourtTitle", ccol, "COURT SETTING", size=20, tint=colour(1.0, 0.85, 0.45), variable=False)
    w.text("CourtText", ccol, "", size=17, wrap=False)

    exchange = w.panel("ExchangePanel", root, tint=colour(0.05, 0.04, 0.09, 0.95), padding=16)
    w.place(exchange, (0.5, 0.5), (0.5, 0.5), (0, 0))
    ecol = w.add("VerticalBox", "ExchangeCol", exchange)
    w.text("ExchangeTitle", ecol, "GLOBAL EXCHANGE", size=20, tint=colour(1.0, 0.85, 0.45), variable=False)
    w.text("ExchangeText", ecol, "", size=15, wrap=False)
    from_row = w.add("HorizontalBox", "FromRow", ecol)
    to_row = w.add("HorizontalBox", "ToRow", ecol)
    for stat in range(5):
        w.button("From%d" % stat, from_row, "From %d" % stat, "(BIB|HUD|ExchangeSelect :bFrom true :StatIndex %d)" % stat, size=13, label_var=True)
        w.button("To%d" % stat, to_row, "To %d" % stat, "(BIB|HUD|ExchangeSelect :bFrom false :StatIndex %d)" % stat, size=13, label_var=True)
    actions = w.add("HorizontalBox", "ExchangeActions", ecol)
    w.button("ExchangeApply", actions, "Exchange 10%", "(BIB|HUD|ExchangeApply)", tint=ACCENT)
    w.button("ExchangeClose", actions, "Close", "(BIB|HUD|CloseExchange)", tint=DANGER)
    obj(dialogue["widget"]).set_visibility(unreal.SlateVisibility.COLLAPSED)
    obj(exchange["widget"]).set_visibility(unreal.SlateVisibility.COLLAPSED)
    obj(court["widget"]).set_visibility(unreal.SlateVisibility.COLLAPSED)

    construct = []
    for stat in range(5):
        name = "(BIB|HUD|GetExchangeStatName %d)" % stat
        construct.append(set_text("From%dLabel" % stat, cat(q("From "), name)))
        construct.append(set_text("To%dLabel" % stat, cat(q("To "), name)))

    hud_line, h = brk("BIBHudInfo", "(BIB|UI|GetHud :self self)")
    tick = [hud_line,
            set_text("NameText", h["ActiveName"]),
            "(Progress|SetPercent :self (W|GetHPBar) :InPercent %s)" % h["HPFraction"],
            set_text("HPText", cat(q("HP "), int_str("(Math|Float|Truncate %s)" % h["HP"]), q(" / "), int_str("(Math|Float|Truncate %s)" % h["MaxHP"]))),
            "(Progress|SetPercent :self (W|GetUltBar) :InPercent %s)" % h["UltFraction"],
            set_text("UltText", cat(q("Ultimate "), int_str("(Math|Float|Truncate (* %s 100))" % h["UltFraction"]), q("%"))),
            lines_loop(h["Team"], "BIBTeamMemberInfo", lambda f: cat(
                "(select %s \"> \" \"   \")" % f["bActive"], f["Name"], q("  "),
                int_str("(Math|Float|Truncate (* %s 100))" % f["HPFraction"]), q("%"),
                "(select %s \"\" \"  (down)\")" % f["bAlive"])),
            set_text("TeamText", joined("(W|GetLines)")),
            set_text("ObjectiveText", cat(h["StageTitle"], q("\n"), h["Objective"])),
            set_text("KitText", cat(h["KitLine"], q("\n"), h["ActionLine"], q("\n"), h["ControlsHint"])),
            set_text("MessagesText", joined(h["Messages"]))]
    for index in range(6):
        ab_line, a = brk("BIBAbilityInfo", "(Utilities|Array|Get(acopy) %s %d)" % (h["Abilities"], index))
        cooldown = "(select (> %s 0.0) %s \"\")" % (a["CooldownRemaining"], cat(q("  "), "(Utilities|String|ToString(Float) %s)" % a["CooldownRemaining"], q("s")))
        tick.append("(if (Utilities|Array|IsValidIndex %s %d)\n  %s\n  %s\n  (else %s))" % (
            h["Abilities"], index, ab_line, set_text("Ab%d" % index, cat(a["Key"], q(": "), a["Name"], cooldown, q("\n"), a["Note"])),
            set_text("Ab%d" % index, q(""))))
    tick.append(show("DialogueBox", h["bDialogueOpen"]))
    tick.append(set_text("DialogueSpeaker", h["DialogueSpeaker"]))
    tick.append(set_text("DialogueLine", h["DialogueLine"]))
    # Judge's court-setting menu: ActionLine carries the choice prompt while bJudgeModeMenu is true.
    tick.append(show("CourtPanel", h["bJudgeModeMenu"]))
    tick.append(set_text("CourtText", h["ActionLine"]))
    tick.append(show("ExchangePanel", h["bExchangeOpen"]))
    stat_lines = ["(Utilities|Array|Clear (W|GetLines))"]
    for stat in range(5):
        value = "(Utilities|String|ToString(Float) (Utilities|Array|Get(acopy) %s %d))" % (h["ExchangeValues"], stat)
        entry = cat("(BIB|HUD|GetExchangeStatName %d)" % stat, q(": "), value,
                    "(select (== %s %d) \"   <- from\" \"\")" % (h["ExchangeFrom"], stat),
                    "(select (== %s %d) \"   -> to\" \"\")" % (h["ExchangeTo"], stat))
        stat_lines.append("(if (Utilities|Array|IsValidIndex %s %d)\n  (Utilities|Array|Add (W|GetLines) %s))" % (h["ExchangeValues"], stat, entry))
    tick.append("(if %s\n  %s\n  %s)" % (h["bExchangeOpen"], "\n  ".join(stat_lines), set_text("ExchangeText", joined("(W|GetLines)"))))
    return w.finish(tick="\n".join(tick), construct="\n".join(construct))


# ============================================================================ UI art

def import_art():
    """Imports Art/Icons/*.png and Art/Portraits/*.png as UI textures T_<file name> under /Game/BIB/UI/. Replacing a PNG and
    re-running build_assets.py (or just this function) swaps the art; the hub finds textures by these names."""
    project = unreal.Paths.project_dir()
    tasks = []
    for source, destination in (("Art/Icons", UI + "/Icons"), ("Art/Portraits", UI + "/Portraits")):
        for path in sorted(glob.glob(os.path.join(project, source, "*.png"))):
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", path)
            task.set_editor_property("destination_path", destination)
            task.set_editor_property("destination_name", "T_" + os.path.splitext(os.path.basename(path))[0])
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            tasks.append(task)
    if not tasks:
        log("art: no PNGs found under Art/ (run Tools/make_icons.py and Tools/make_portraits.py)")
        return 0
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    count = 0
    for task in tasks:
        for asset_path in task.get_editor_property("imported_object_paths"):
            texture = unreal.load_asset(asset_path)
            if isinstance(texture, unreal.Texture2D):
                texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                texture.set_editor_property("never_stream", True)
                save(texture)
                count += 1
    log("art: %d textures imported" % count)
    return count


def enum_suffix(enum_type, suffix):
    """Enum member whose name ends with the suffix (HorizontalAlignment: CENTER -> H_ALIGN_CENTER)."""
    for attr in dir(enum_type):
        if not attr.startswith("_") and attr.upper().endswith(suffix.upper()):
            return getattr(enum_type, attr)
    raise RuntimeError("No %s ending in %s" % (enum_type, suffix))


def fill_slot(info, value=1.0):
    size = unreal.SlateChildSize()
    size.set_editor_property("size_rule", unreal.SlateSizeRule.FILL)
    size.set_editor_property("value", value)
    obj(info["slot"]).set_size(size)


def align_slot(info, horizontal=None, vertical=None):
    slot = obj(info["slot"])
    if horizontal:
        slot.set_horizontal_alignment(enum_suffix(unreal.HorizontalAlignment, horizontal))
    if vertical:
        slot.set_vertical_alignment(enum_suffix(unreal.VerticalAlignment, vertical))


def image(w, name, parent, icon=None, size=None):
    info = w.add("Image", name, parent, True)
    widget = obj(info["widget"])
    if size:
        try:
            widget.set_desired_size_override(unreal.Vector2D(size[0], size[1]))
        except Exception:
            widget.set_editor_property("brush", widget.get_editor_property("brush"))
    if icon:
        texture = unreal.load_asset("%s/Icons/T_%s" % (UI, icon))
        if texture:
            widget.set_brush_from_texture(texture, False)
    return info


def centre_text(info, wrap):
    """Centred text. Wrapping text fills the slot width (an auto-wrapping text in a centred slot wraps far too early)."""
    if wrap:
        align_slot(info, "FILL")
        obj(info["widget"]).set_editor_property("justification", unreal.TextJustify.CENTER)
    else:
        align_slot(info, "CENTER")


def hub_tile(w, parent, prefix, index, icon_size=44, badge=False, wrap_label=True):
    """A button with an icon, a label and a small sub line: Prefix<i>, PrefixIcon<i>, PrefixLabel<i>, PrefixSub<i>."""
    button = w.add("Button", "%s%d" % (prefix, index), parent, True)
    obj(button["widget"]).set_background_color(BUTTON)
    box = w.add("VerticalBox", "%sBox%d" % (prefix, index), button)
    align_slot(box, "FILL", "CENTER")  # the button centres its content by default, which squeezes wrapping text
    icon = image(w, "%sIcon%d" % (prefix, index), box, size=(icon_size, icon_size))
    align_slot(icon, "CENTER")
    label = w.text("%sLabel%d" % (prefix, index), box, "", size=14, wrap=wrap_label)
    centre_text(label, wrap_label)
    sub = w.text("%sSub%d" % (prefix, index), box, "", size=11, tint=DIM, wrap=True)
    centre_text(sub, True)
    if badge:
        mark = w.text("%sBadge%d" % (prefix, index), box, "(!)", size=12, tint=colour(1.0, 0.45, 0.4), wrap=False)
        align_slot(mark, "CENTER")
    return button


HUB_COLUMNS = 4
# WBP_Hub (the 2D lobby, parent UBIBHubWidget) was deleted on 2026-10-07 (owner request): the Ark is the hub (BIBArkUI.h).


def build_actor_blueprints():
    result = {}
    result["Door"] = make_blueprint("BP_BIBDoor", cls("/Script/BoldInBeautyDemo.BIBDoor"))
    result["Section"] = make_blueprint("BP_BIBSection", cls("/Script/BoldInBeautyDemo.BIBSection"))
    result["StoryNPC"] = make_blueprint("BP_BIBStoryNPC", cls("/Script/BoldInBeautyDemo.BIBStoryNPC"))
    result["Character"] = make_blueprint("BP_BIBPlayerCharacter", cls("/Script/BoldInBeautyDemo.BIBPlayerCharacter"))
    result["GameInstance"] = make_blueprint("BP_BIBGameInstance", cls("/Script/BoldInBeautyDemo.BIBGameInstance"))
    enemy_parent = cls("/Script/BoldInBeautyDemo.BIBEnemy")
    result["Enemies"] = {}
    for kind in ("Grunt", "Brute", "MiniBoss", "Boss"):
        bp = make_blueprint("BP_Enemy_" + kind, enemy_parent)
        cdo(bp).set_editor_property("kind", enum_value(unreal.BIBEnemyKind, kind))
        result["Enemies"][kind] = bp
    for value in result.values():
        for bp in (value.values() if isinstance(value, dict) else [value]):
            save(bp)
    log("blueprints: doors/sections/NPC/character/game instance + 4 enemies")
    return result


def build_controller(actions, look, context, hud, screens):
    bp = make_blueprint("BP_BIBPlayerController", cls("/Script/BoldInBeautyDemo.BIBPlayerController"))
    defaults = cdo(bp)
    defaults.set_editor_property("default_mapping_context", context)
    defaults.set_editor_property("button_actions", {enum_value(unreal.BIBInput, n): a for n, a in actions.items()})
    defaults.set_editor_property("look_action", look)
    defaults.set_editor_property("hud_widget_class", hud.generated_class())
    defaults.set_editor_property("screen_widget_classes", {enum_value(unreal.BIBScreen, n): w.generated_class() for n, w in screens.items()})
    save(bp)
    log("controller: %d button actions, %d screens" % (len(actions), len(screens)))
    return bp


def set_soft(target, prop, worlds):
    """Assigns TSoftObjectPtr<UWorld> (or an array of them) from map object paths."""
    errors = []
    for convert in (lambda p: unreal.load_object(None, p), unreal.SoftObjectPath):
        try:
            value = [convert(w) for w in worlds] if isinstance(worlds, list) else convert(worlds)
            target.set_editor_property(prop, value)
            return
        except Exception as error:
            errors.append(str(error))
    raise RuntimeError("Could not set %s: %s" % (prop, errors))


def build_game_mode(bps, controller, hub, stages):
    bp = make_blueprint("BP_BIBGameMode", cls("/Script/BoldInBeautyDemo.BIBGameMode"))
    defaults = cdo(bp)
    defaults.set_editor_property("default_pawn_class", bps["Character"].generated_class())
    defaults.set_editor_property("player_controller_class", controller.generated_class())
    defaults.set_editor_property("enemy_classes", {enum_value(unreal.BIBEnemyKind, k): b.generated_class() for k, b in bps["Enemies"].items()})
    set_soft(defaults, "hub_map", hub)
    set_soft(defaults, "stage_maps", stages)
    save(bp)
    log("game mode: hub + %d stage maps, 4 enemy classes" % len(stages))
    return bp


# ============================================================================ maps

CUBE = "/Engine/BasicShapes/Cube.Cube"
START_HALF = 1500.0      # BIBTuning::StartRoomHalfSize
SECTION_HALF = 5600.0    # BIBTuning::SectionHalfSize (700 cm/s x 8 s, MAIN §7)
WALL_HEIGHT = 600.0      # BIBTuning::WallHeight
CEILING = 2200.0         # BIBTuning::CeilingHeight
DOOR_WIDTH = 700.0       # BIBTuning::DoorWidth

# Enemy counts per (level, section): the same compositions as BIB::StagePlan (BIBRules/BIBMeta.cpp, PLACEHOLDER there).
# Chapter 1 has eight levels; level 8 ends with the mini-boss and the chapter boss.
def _plan(*rows):
    return [dict(grunts=g, brutes=b, mini_bosses=m, bosses=x) for g, b, m, x in rows]


SECTION_PLANS = {
    1: _plan((10, 0, 0, 0), (9, 3, 1, 0)),
    2: _plan((12, 2, 0, 0), (10, 4, 1, 0)),
    3: _plan((12, 4, 0, 0), (10, 4, 0, 0), (8, 4, 1, 0)),
    4: _plan((14, 4, 0, 0), (12, 6, 0, 0), (10, 6, 1, 0)),
    5: _plan((14, 6, 0, 0), (12, 6, 0, 0), (10, 8, 1, 0)),
    6: _plan((16, 6, 0, 0), (14, 6, 0, 0), (12, 8, 0, 0), (10, 8, 1, 0)),
    7: _plan((16, 8, 0, 0), (14, 8, 0, 0), (12, 10, 0, 0), (12, 10, 1, 0)),
    8: _plan((14, 6, 0, 0), (14, 8, 0, 0), (12, 10, 0, 0), (10, 10, 0, 0), (8, 6, 1, 1)),
}


class MapBuilder:
    def __init__(self, path):
        self.path = path
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not self.levels.new_level(path):
            raise RuntimeError("new_level failed: " + path)
        self.cube = unreal.load_asset(CUBE)
        self.count = 0

    def spawn(self, actor_class, location, rotation=(0, 0, 0), label=None, folder=None):
        actor = self.actors.spawn_actor_from_class(actor_class, unreal.Vector(*location), unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1]))
        if not actor:
            raise RuntimeError("spawn failed: %s" % actor_class)
        if label:
            actor.set_actor_label(label)
        if folder:
            actor.set_folder_path(folder)
        return actor

    def block(self, centre, size, label, visible=True, folder="Geometry"):
        self.count += 1
        actor = self.spawn(unreal.StaticMeshActor, centre, label="%s_%02d" % (label, self.count), folder=folder)
        comp = actor.static_mesh_component
        comp.set_static_mesh(self.cube)
        comp.set_collision_profile_name("BlockAll")
        actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        if not visible:
            actor.set_actor_hidden_in_game(True)
            comp.set_editor_property("cast_shadow", False)
        return actor

    def lights(self):
        sun = self.spawn(unreal.DirectionalLight, (0, 0, 1000), (-45, 30, 0), "Sun", "Lighting")
        sun.light_component.set_editor_property("atmosphere_sun_light", True)
        sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky = self.spawn(unreal.SkyLight, (0, 0, 1200), label="SkyLight", folder="Lighting")
        sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.light_component.set_editor_property("real_time_capture", True)
        self.spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere", folder="Lighting")
        self.spawn(unreal.ExponentialHeightFog, (0, 0, -200), label="HeightFog", folder="Lighting")

    # Same layout as the code-built blockout (ABIBGameMode::AddRoom / AddWallAlongY / AddWallAlongX).
    def wall_along_y(self, x, half_width, door_gap):
        upper = CEILING - WALL_HEIGHT

        def segment(centre_y, length):
            self.block((x, centre_y, WALL_HEIGHT * 0.5), (100, length, WALL_HEIGHT), "Wall")
            self.block((x, centre_y, WALL_HEIGHT + upper * 0.5), (100, length, upper), "Blocker", visible=False, folder="Blockers")
        if not door_gap:
            segment(0.0, half_width * 2.0 + 100.0)
            return
        half_door = DOOR_WIDTH * 0.5
        length = half_width + 50.0 - half_door
        segment(-(half_door + length * 0.5), length)
        segment(half_door + length * 0.5, length)

    def wall_along_x(self, y, min_x, max_x):
        upper = CEILING - WALL_HEIGHT
        centre = (min_x + max_x) * 0.5
        length = max_x - min_x + 100.0
        self.block((centre, y, WALL_HEIGHT * 0.5), (length, 100, WALL_HEIGHT), "Wall")
        self.block((centre, y, WALL_HEIGHT + upper * 0.5), (length, 100, upper), "Blocker", visible=False, folder="Blockers")

    def room(self, min_x, max_x, half_width):
        centre = (min_x + max_x) * 0.5
        self.block((centre, 0, -50), (max_x - min_x + 100, half_width * 2 + 100, 100), "Floor")
        self.block((centre, 0, CEILING + 50), (max_x - min_x + 100, half_width * 2 + 100, 100), "Ceiling", visible=False, folder="Blockers")
        self.wall_along_x(half_width, min_x, max_x)
        self.wall_along_x(-half_width, min_x, max_x)

    def anime_style(self):
        # Every gameplay map gets the anime look (BIBGame/BIBAnimeStyle.h); the hub (main menu) does not.
        volume = self.spawn(unreal.BIBAnimeStyleVolume, (0, 0, 0), label="AnimeStyle", folder="Lighting")
        volume.set_editor_property("tags", [unreal.Name("AnimeStyle")])
        return volume

    def save(self):
        if not self.levels.save_current_level():
            raise RuntimeError("save failed: " + self.path)
        # Return the object path, not the World: loading the next map invalidates this World object.
        return "%s.%s" % (self.path, self.path.rsplit("/", 1)[1])


def build_hub():
    m = MapBuilder(MAPS + "/L_Hub")
    m.lights()
    m.block((0, 0, -50), (4000, 4000, 100), "Floor")
    m.spawn(unreal.PlayerStart, (0, 0, 100), label="PlayerStart")
    world = m.save()
    log("map: L_Hub")
    return world


def build_stage(stage, bps):
    m = MapBuilder(MAPS + "/L_Chapter1_Stage%d" % stage)
    m.lights()
    m.room(-START_HALF, START_HALF, START_HALF)
    m.wall_along_y(-START_HALF, START_HALF, False)
    m.spawn(unreal.PlayerStart, (-900, 0, 100), label="PlayerStart")
    npc = m.spawn(bps["StoryNPC"].generated_class(), (700, 0, 110), (0, 180, 0), "StoryNPC")
    plans = SECTION_PLANS[stage]
    for index, plan in enumerate(plans):
        min_x = START_HALF + SECTION_HALF * 2.0 * index
        m.room(min_x, min_x + SECTION_HALF * 2.0, SECTION_HALF)
        m.wall_along_y(min_x, SECTION_HALF, True)
        door = m.spawn(bps["Door"].generated_class(), (min_x, 0, 0), label="Door_Section%d" % (index + 1), folder="Flow")
        section = m.spawn(bps["Section"].generated_class(), (min_x + SECTION_HALF, 0, CEILING * 0.5), label="Section%d" % (index + 1), folder="Flow")
        section.set_editor_property("stage_number", stage)
        section.set_editor_property("section_index", index)
        section.set_editor_property("entry_door", door)
        section.set_editor_property("grunts", plan.get("grunts", 0))
        section.set_editor_property("brutes", plan.get("brutes", 0))
        section.set_editor_property("mini_bosses", plan.get("mini_bosses", 0))
        section.set_editor_property("bosses", plan.get("bosses", 0))
    m.wall_along_y(START_HALF + SECTION_HALF * 2.0 * len(plans), SECTION_HALF, False)
    m.anime_style()
    world = m.save()
    log("map: L_Chapter1_Stage%d (%d sections, %d blocks)" % (stage, len(plans), m.count))
    return world


# ============================================================================ main

def main():
    if unreal.EditorAssetLibrary.does_directory_exist(ROOT):
        unreal.EditorAssetLibrary.delete_directory(ROOT)
    import_art()
    build_tint_material()
    build_fx_material()
    build_screen_material()
    build_anime_style_material()
    actions, look, context = build_input()
    bps = build_actor_blueprints()
    hud = build_hud()
    screens = {
        "Results": build_results(),
        "Pause": build_pause(),
    }
    controller = build_controller(actions, look, context, hud, screens)
    hub = build_hub()
    stages = [build_stage(level, bps) for level in sorted(SECTION_PLANS)]
    build_game_mode(bps, controller, hub, stages)
    unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    log("DONE")


def run():
    status = "FAILED"
    try:
        with toolset_registry.tool_raising_exceptions():
            main()
        status = "OK"
    except Exception:
        log(traceback.format_exc())
    log("STATUS " + status)
    out = os.path.join(unreal.Paths.project_saved_dir(), "BuildAssets.log")
    with open(out, "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
