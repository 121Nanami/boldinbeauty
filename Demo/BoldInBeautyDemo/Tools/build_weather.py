"""Builds /Game/Weather: the assets of the day / night and weather system (BIBGame/BIBWeather.h).

Re-runnable: deletes /Game/Weather and rebuilds it from this file. Never touches /Game/BIB or /Game/Greybox.

    Tools/build_weather.sh            (UnrealEditor-Cmd ... -run=pythonscript -script=Tools/build_weather.py)

Builds:
  MPC_BIBWeather     the parameters ABIBWeatherSystem drives every frame (sun / moon, palette, rain, wetness, wind ...)
  M_BIBSky           stylised sky dome (IsSky): anime gradient, sun + moon, stars, Milky Way, shooting stars,
                     three-tone cel clouds with silver linings, overcast sheet, lightning inside the clouds
  M_BIBRain          lit translucent rain layer (world-sized streaks, stops under roofs via the shelter capture)
  M_BIBSplash        rain splash crowns, M_BIBWindStreak anime wind lines, M_BIBLightning bolts
  M_BIBWetDarken / M_BIBWetGloss / M_BIBPuddle   DBuffer decals: wet darkening, wet gloss, drips running down walls,
                     puddles with rain ripples (world-space normals) - on every surface incl. characters' clothes
  Audio/S_RainLoop, S_WindLoop, S_Thunder1..3    synthesised here (Art/Weather/*.wav, no third-party audio)
All look numbers are PROPOSED (owner decides). Summary in Saved/BuildWeather.log.
"""

import math
import os
import random
import struct
import traceback
import wave

import unreal

ROOT = "/Game/Weather"
AUDIO = ROOT + "/Audio"
DEMO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ART = os.path.join(DEMO, "Art", "Weather")
LOG = []

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(*parts):
    line = " ".join(str(p) for p in parts)
    LOG.append(line)
    unreal.log("[build_weather] " + line)


def save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


# ============================================================================ parameter collection

MPC_SCALARS = [
    ("RainAmount", 0.0), ("Wetness", 0.0), ("Puddles", 0.0), ("WindAmount", 0.2), ("CloudCover", 0.3),
    ("StarAmount", 0.0), ("NightAmount", 0.0), ("DayHour", 12.0), ("Storm", 0.0), ("Lightning", 0.0),
    ("RainBrightness", 1.0),
]
MPC_VECTORS = [
    ("SunDir", (0.0, 0.3, 0.9, 1.0)), ("MoonDir", (0.0, -0.3, -0.9, 0.0)),
    ("ZenithColor", (0.03, 0.26, 1.05, 1.0)), ("HorizonColor", (0.55, 0.8, 1.1, 1.0)), ("GlowColor", (1.1, 1.05, 0.95, 1.0)),
    ("CloudLit", (2.0, 2.0, 2.0, 1.0)), ("CloudShade", (0.55, 0.66, 1.0, 1.0)), ("AmbientColor", (0.3, 0.5, 1.0, 1.0)),
    ("WindVector", (0.85, 0.53, 3.0, 0.2)), ("CloudOffset", (0.0, 0.0, 0.0, 0.0)), ("CameraPos", (0.0, 0.0, 0.0, 0.0)),
    ("ShelterInfo", (0.0, 0.0, 0.0, 0.0)), ("LightningDir", (0.0, 0.0, 1.0, 0.0)),
]


def build_mpc():
    mpc = tools.create_asset("MPC_BIBWeather", ROOT, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
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
    log("MPC_BIBWeather: %d scalars, %d vectors" % (len(scalars), len(vectors)))
    return mpc


# ============================================================================ material helpers

class Mat:
    def __init__(self, name, mpc, domain=None, blend=None, shading=None, two_sided=False, props=None):
        self.name = name
        self.mpc = mpc
        self.m = tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
        if domain is not None:
            self.m.set_editor_property("material_domain", domain)
        if blend is not None:
            self.m.set_editor_property("blend_mode", blend)
        if shading is not None:
            self.m.set_editor_property("shading_model", shading)
        self.m.set_editor_property("two_sided", two_sided)
        for key, value in (props or {}).items():
            self.m.set_editor_property(key, value)
        self.y = -600

    def node(self, cls, x=-900):
        n = lib.create_material_expression(self.m, cls, x, self.y)
        self.y += 110
        return n

    def collection(self, name):
        n = self.node(unreal.MaterialExpressionCollectionParameter)
        n.set_editor_property("collection", self.mpc)
        n.set_editor_property("parameter_name", name)
        return n

    def scalar(self, name, default):
        n = self.node(unreal.MaterialExpressionScalarParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", default)
        return n

    def constant(self, value):
        n = self.node(unreal.MaterialExpressionConstant)
        n.set_editor_property("r", value)
        return n

    def constant3(self, rgb):
        n = self.node(unreal.MaterialExpressionConstant3Vector)
        n.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return n

    def texture_object(self, name):
        n = self.node(unreal.MaterialExpressionTextureObjectParameter)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/Black"))
        return n

    def mask(self, source, channels):
        m = self.node(unreal.MaterialExpressionComponentMask, -650)
        for c in "rgba":
            m.set_editor_property(c, c in channels)
        if not lib.connect_material_expressions(source, "", m, ""):
            raise RuntimeError(self.name + ": mask connect failed")
        return m

    def custom(self, code, inputs, output_type, outputs=()):
        """inputs: [(name, expression)]; outputs: [(name, CustomMaterialOutputType)] extra out-parameters."""
        c = lib.create_material_expression(self.m, unreal.MaterialExpressionCustom, -300, 0)
        c.set_editor_property("description", self.name)
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
            if not lib.connect_material_expressions(expr, "", c, name):
                raise RuntimeError("%s: could not connect input %s" % (self.name, name))
        return c

    def out(self, expr, prop, pin=""):
        if not lib.connect_material_property(expr, pin, prop):
            raise RuntimeError("%s: could not connect %s" % (self.name, prop))

    def finish(self):
        lib.recompile_material(self.m)
        save(self.m)
        log("material:", self.name)
        return self.m


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
MP = unreal.MaterialProperty

# Shared noise helpers (a struct with methods: the usual way to put functions in a Custom node).
NOISE = r"""
struct FBIBNoise
{
    float H21(float2 p) { p = frac(p * float2(123.34, 456.21)); p += dot(p, p + 45.32); return frac(p.x * p.y); }
    float H31(float3 p) { p = frac(p * float3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yzx + 33.33); return frac((p.x + p.y) * p.z); }
    float N2(float2 p)
    {
        float2 i = floor(p); float2 f = frac(p); float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(H21(i), H21(i + float2(1, 0)), u.x), lerp(H21(i + float2(0, 1)), H21(i + float2(1, 1)), u.x), u.y);
    }
    float Fbm(float2 p, int Octaves)
    {
        float a = 0.5; float s = 0.0;
        for (int k = 0; k < Octaves; k++) { s += a * N2(p); p = p * 2.03 + float2(17.1, 9.2); a *= 0.5; }
        return s;
    }
};
FBIBNoise N;
"""

# ============================================================================ sky

SKY_HLSL = NOISE + r"""
float3 D = normalize(V);
float h = D.z;
float up = saturate(h);
float3 S = SunDir.xyz;
float3 M = MoonDir.xyz;
float sd = dot(D, S);
float md = dot(D, M);

// Anime gradient: deep zenith to a bright horizon band.
float3 col = lerp(Hor, Zen, pow(up, 0.55));
col = lerp(col, Hor * 1.2, exp(-max(h, 0.0) * 16.0) * 0.55);
if (h < 0.0)
{
    col = lerp(Hor * 0.85, Hor * 0.3 + Zen * 0.15, saturate(-h * 3.0));
}

// Sun glow and the dawn / dusk band that wraps the horizon around the sun's side.
float2 dh = normalize(D.xy + 1e-5);
float2 sh = normalize(S.xy + 1e-5);
float band = exp(-abs(h) * 5.0) * pow(saturate(dot(dh, sh) * 0.5 + 0.5), 3.0) * (1.0 - smoothstep(0.25, 0.7, S.z));
col += Glow * (band * 0.9 + pow(saturate(sd), 10.0) * 0.45 + pow(saturate(sd), 160.0) * 1.6) * (1.0 - Cover * 0.5);

// Moon halo.
col += float3(0.35, 0.45, 0.85) * Night * MoonDir.w * (pow(saturate(md), 40.0) * 0.18 + pow(saturate(md), 700.0) * 0.7);

// Stars (two layers, twinkling), the Milky Way and an occasional shooting star; the sky turns with the hour.
if (Stars > 0.001 && h > -0.05)
{
    float a = Hour / 24.0 * 6.2831853;
    float3 axis = normalize(float3(-0.55, 0.0, 0.83));
    float ca = cos(a); float sa = sin(a);
    float3 R = D * ca + cross(axis, D) * sa + axis * dot(axis, D) * (1.0 - ca);
    float3 st = 0;
    for (int L = 0; L < 2; L++)
    {
        float sc = L == 0 ? 70.0 : 180.0;
        float3 g = R * sc; float3 c = floor(g); float3 f = frac(g);
        float hh = N.H31(c + L * 17.0);
        if (hh > (L == 0 ? 0.82 : 0.80))
        {
            float3 pos = float3(N.H31(c + 1.3), N.H31(c + 2.7), N.H31(c + 5.1)) * 0.6 + 0.2;
            float d = length(f - pos);
            float b = smoothstep(L == 0 ? 0.11 : 0.08, 0.0, d);
            b = b * b * (0.55 + 0.45 * sin(T * (1.3 + hh * 4.0) + hh * 91.0));
            float3 tint = lerp(float3(0.65, 0.78, 1.0), float3(1.0, 0.85, 0.65), N.H31(c + 9.0));
            st += tint * b * (L == 0 ? 14.0 : 5.0) * (hh - 0.75) * 4.0;
        }
    }
    float3 bandN = normalize(float3(0.3, 0.8, 0.5));
    float mwBand = exp(-pow(dot(R, bandN), 2.0) * 14.0);
    float neb = N.Fbm(R.xy * 3.5 + R.z * 2.0, 5);
    float3 mw = (float3(0.32, 0.16, 0.55) * neb + float3(0.05, 0.3, 0.42) * N.Fbm(R.yz * 6.0 + 4.0, 4)) * mwBand * smoothstep(0.3, 0.75, neb + mwBand * 0.25);
    float fade = Stars * smoothstep(-0.02, 0.25, h);
    col += (st + mw * 1.1) * fade;

    float slot = floor(T / 7.0);
    float lt = frac(T / 7.0) * 7.0;
    if (N.H21(float2(slot, 3.7)) > 0.4 && lt < 0.9)
    {
        float3 p0 = normalize(float3(N.H21(float2(slot, 1.0)) * 2.0 - 1.0, N.H21(float2(slot, 2.0)) * 2.0 - 1.0, 0.45 + N.H21(float2(slot, 4.0)) * 0.5));
        float3 dv = normalize(cross(p0, float3(N.H21(float2(slot, 5.0)) - 0.5, N.H21(float2(slot, 6.0)) - 0.5, 0.25)));
        float prog = lt / 0.9;
        float3 head = normalize(p0 + dv * prog * 0.45);
        float3 tail = normalize(p0 + dv * max(prog - 0.3, 0.0) * 0.45);
        float3 ab = head - tail;
        float tt = saturate(dot(D - tail, ab) / max(dot(ab, ab), 1e-6));
        float dd = length(D - (tail + ab * tt));
        col += float3(0.85, 0.95, 1.0) * smoothstep(0.003, 0.0, dd) * tt * (1.0 - prog) * 6.0 * fade;
    }
}

// Moon: a big stylised gibbous disc with soft maria.
if (md > 0.9 && MoonDir.w > 0.001)
{
    float r = 0.028;
    float3 Mx = normalize(cross(M, float3(0, 0, 1)) + 1e-5);
    float3 My = cross(Mx, M);
    float2 q = float2(dot(D, Mx), dot(D, My)) / r;
    float rr = length(q);
    if (rr < 1.0)
    {
        float3 nrm = float3(q, sqrt(1.0 - rr * rr));
        float lit = smoothstep(-0.15, 0.25, dot(nrm, normalize(float3(0.55, 0.2, 0.8))));
        float maria = N.Fbm(q * 2.5 + 7.0, 4);
        float3 mc = float3(1.0, 0.96, 0.88) * lerp(0.62, 1.0, smoothstep(0.35, 0.6, maria)) * lerp(0.06, 1.0, lit);
        col = lerp(col, mc * 2.4, smoothstep(1.0, 0.93, rr) * MoonDir.w);
    }
}

// Sun disc.
col = lerp(col, Glow * 1.5 + float3(4.0, 3.7, 3.2), smoothstep(0.99955, 0.99972, sd) * SunDir.w * (1.0 - Cover * 0.6));

// Cel-shaded clouds drifting with the wind: crisp edges, three tones, silver linings, lightning inside.
if (h > -0.03)
{
    float2 p = D.xy / (D.z + 0.2) * 0.85 + CloudOff * 10.0;
    float2 w = float2(N.Fbm(p * 0.7 + 3.1, 3), N.Fbm(p * 0.7 + 8.3, 3));
    float2 pw = p + (w - 0.5) * 0.9;
    float dens = N.Fbm(pw, 5);
    float thr = lerp(0.72, 0.28, Cover);
    float c = smoothstep(thr, thr + 0.035, dens);
    float d2 = N.Fbm(pw + normalize(S.xy + 1e-4) * 0.1 * (S.z > -0.1 ? 1.0 : -1.0), 5);
    float lightness = saturate(0.55 + (dens - d2) * 7.0);
    float tone = lightness < 0.34 ? 0.0 : (lightness < 0.68 ? 0.55 : 1.0);
    tone = lerp(lightness, tone, 0.8);
    float3 cc = lerp(CloudShade, CloudLit, tone);
    float rim = c * (1.0 - smoothstep(thr + 0.02, thr + 0.1, dens));
    cc += Glow * rim * (pow(saturate(sd), 4.0) * 1.6 + 0.25);
    cc += float3(0.7, 0.75, 1.0) * Light.w * (0.6 + 3.0 * pow(saturate(dot(D, Light.xyz)), 6.0)) * 2.5;
    float fade = smoothstep(-0.03, 0.12, h);
    col = lerp(col, cc, c * fade);
    float sheet = smoothstep(0.72, 1.0, Cover);
    float3 sc = lerp(CloudShade, CloudLit * 0.6, 0.3 + 0.35 * N.Fbm(p * 1.4 + 2.0, 4));
    sc += float3(0.7, 0.75, 1.0) * Light.w * (0.5 + 2.5 * pow(saturate(dot(D, Light.xyz)), 4.0)) * 1.8;
    col = lerp(col, sc, sheet * fade * 0.93);
}
col += float3(0.5, 0.55, 0.8) * Light.w * 0.2;
return max(col, 0.0);
"""


def build_sky(mpc):
    m = Mat("M_BIBSky", mpc, shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True, props={"is_sky": True})
    cam = m.node(unreal.MaterialExpressionCameraVectorWS)
    view = m.node(unreal.MaterialExpressionMultiply)
    lib.connect_material_expressions(cam, "", view, "A")
    view.set_editor_property("const_b", -1.0)
    rgb = lambda n: m.mask(m.collection(n), "rgb")
    inputs = [
        ("V", view), ("T", m.node(unreal.MaterialExpressionTime)),
        ("SunDir", m.collection("SunDir")), ("MoonDir", m.collection("MoonDir")),
        ("Zen", rgb("ZenithColor")), ("Hor", rgb("HorizonColor")), ("Glow", rgb("GlowColor")),
        ("CloudLit", rgb("CloudLit")), ("CloudShade", rgb("CloudShade")),
        ("Cover", m.collection("CloudCover")), ("Stars", m.collection("StarAmount")), ("Night", m.collection("NightAmount")),
        ("Hour", m.collection("DayHour")), ("CloudOff", m.mask(m.collection("CloudOffset"), "rg")),
        ("Light", m.collection("LightningDir")),
    ]
    c = m.custom(SKY_HLSL, inputs, F3)
    m.out(c, MP.MP_EMISSIVE_COLOR)
    return m.finish()


# ============================================================================ rain layer

SHELTER = r"""
// Height of the first rain-blocking surface above P (overhead depth capture); returns -1e9 for open sky.
float BIBShelterTop(float3 Q)
{
    if (Shelter.w <= 0.0) return -1e9;
    float2 suv = float2(0.5 + (Q.y - Shelter.y) / Shelter.w, 0.5 - (Q.x - Shelter.x) / Shelter.w);
    if (any(suv < 0.002) || any(suv > 0.998)) return -1e9;
    float depth = Texture2DSampleLevel(Tex, TexSampler, suv, 0).r;
    if (depth > 5e7) return -1e9;
    return Shelter.z - depth;
}
"""

RAIN_HLSL = NOISE + r"""
float x = UV.x * Columns;
float colId = floor(x);
float fx = frac(x) - 0.5;
float h1 = N.H21(float2(colId, LayerSeed));
float h2 = N.H21(float2(colId, LayerSeed + 7.3));
float speed = 9.0 + h1 * 3.5;
float y = UV.y + T * speed + h2 * 100.0;
float period = 1.6 + h1 * 2.6;
float cell = floor(y / period);
float fy = frac(y / period) * period;
float present = step(1.0 - Rain * 0.92, N.H21(float2(colId + 0.37, cell + LayerSeed)));
float len = StreakLength * (0.7 + 0.6 * N.H21(float2(cell, colId)));
float streak = fy < len ? (1.0 - abs(fy / len * 2.0 - 1.0)) : 0.0;
float jitter = (N.H21(float2(cell, colId + 3.1)) - 0.5) * 0.5;
float width = StreakWidth * (0.6 + 0.8 * h2);
float across = saturate(1.0 - abs(fx - jitter) / max(width * 0.5, 1e-3));
float a = present * streak * across;
float top = -1e9;
if (Shelter.w > 0.0)
{
    float2 suv = float2(0.5 + (P.y - Shelter.y) / Shelter.w, 0.5 - (P.x - Shelter.x) / Shelter.w);
    if (all(suv > 0.002) && all(suv < 0.998))
    {
        float depth = Texture2DSampleLevel(Tex, TexSampler, suv, 0).r;
        top = depth > 5e7 ? -1e9 : Shelter.z - depth;
    }
}
a *= saturate((P.z - top + 30.0) / 60.0);
a *= saturate((SceneD - PixD) / 80.0);
a *= saturate((PixD - 50.0) / 150.0);
a *= LayerOpacity * saturate(Rain * 2.2) * (0.85 + 0.3 * Wind.w);
Alpha = saturate(a);
return (float3(0.78, 0.85, 1.0) + Amb * 0.6) * Bright;
"""


def build_rain(mpc):
    m = Mat("M_BIBRain", mpc, blend=unreal.BlendMode.BLEND_TRANSLUCENT, shading=unreal.MaterialShadingModel.MSM_DEFAULT_LIT, two_sided=True,
            props={"translucency_lighting_mode": unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL})
    inputs = [
        ("UV", m.node(unreal.MaterialExpressionTextureCoordinate)), ("P", m.node(unreal.MaterialExpressionWorldPosition)),
        ("T", m.node(unreal.MaterialExpressionTime)), ("SceneD", m.node(unreal.MaterialExpressionSceneDepth)),
        ("PixD", m.node(unreal.MaterialExpressionPixelDepth)), ("Tex", m.texture_object("ShelterDepth")),
        ("Rain", m.collection("RainAmount")), ("Shelter", m.collection("ShelterInfo")), ("Wind", m.collection("WindVector")),
        ("Bright", m.collection("RainBrightness")), ("Amb", m.mask(m.collection("AmbientColor"), "rgb")),
        ("Columns", m.scalar("Columns", 6.0)), ("StreakLength", m.scalar("StreakLength", 0.7)), ("StreakWidth", m.scalar("StreakWidth", 0.08)),
        ("LayerOpacity", m.scalar("LayerOpacity", 0.5)), ("LayerSeed", m.scalar("LayerSeed", 17.0)),
    ]
    c = m.custom(RAIN_HLSL, inputs, F3, [("Alpha", F1)])
    m.out(c, MP.MP_BASE_COLOR)
    emissive = m.node(unreal.MaterialExpressionMultiply, -100)
    lib.connect_material_expressions(c, "", emissive, "A")
    emissive.set_editor_property("const_b", 0.25)
    m.out(emissive, MP.MP_EMISSIVE_COLOR)
    m.out(c, MP.MP_OPACITY, "Alpha")
    return m.finish()


# ============================================================================ particles

SPLASH_HLSL = r"""
float t = VC.r; float seed = VC.g;
float2 p = float2(UV.x * 2.0 - 1.0, 1.0 - UV.y);
float a = 0.0;
float r = lerp(0.15, 0.95, sqrt(t));
float e = length(float2(p.x, (p.y - 0.06) * 5.0));
a += smoothstep(0.14, 0.0, abs(e - r)) * (1.0 - t);
for (int k = 0; k < 5; k++)
{
    float kx = (k / 4.0 * 2.0 - 1.0) * r * 0.85;
    float hgt = (0.3 + 0.5 * frac(seed * 13.7 + k * 0.37)) * sin(saturate(t * 1.6) * 3.14159);
    a += smoothstep(0.07, 0.0, abs(p.x - kx)) * step(p.y, hgt) * step(0.03, p.y) * (1.0 - t) * 0.7;
    float2 dp = float2(kx * 1.25, hgt + 0.12 * t);
    a += smoothstep(0.1, 0.0, length(p - dp)) * (1.0 - t);
}
Alpha = saturate(a) * 0.8;
return (float3(0.82, 0.88, 1.0) + Amb * 0.6) * Bright;
"""

WIND_HLSL = r"""
float t = VC.r; float s = UV.x;
float head = saturate(t * 1.8);
float tail = saturate(t * 1.8 - 0.8);
float vis = smoothstep(tail, tail + 0.08, s) * (1.0 - smoothstep(head - 0.08, head, s));
float across = smoothstep(0.0, 0.7, 1.0 - abs(UV.y * 2.0 - 1.0));
float taper = sin(saturate((s - tail) / max(head - tail, 1e-3)) * 3.14159);
Alpha = vis * across * taper * 0.75;
return (float3(0.95, 0.97, 1.0) + Amb * 0.4) * max(Bright, 0.18) * 1.3;
"""

BOLT_HLSL = r"""
float across = 1.0 - abs(UV.y * 2.0 - 1.0);
float core = smoothstep(0.5, 0.85, across);
return float3(0.78, 0.84, 1.0) * (core * 70.0 + across * across * 9.0) * VC.r * Flash;
"""


def build_particle(name, code, mpc, additive=False):
    blend = unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT
    m = Mat(name, mpc, blend=blend, shading=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    inputs = [("UV", m.node(unreal.MaterialExpressionTextureCoordinate)), ("VC", m.node(unreal.MaterialExpressionVertexColor))]
    if additive:
        inputs.append(("Flash", m.collection("Lightning")))
        c = m.custom(code, inputs, F3)
        m.out(c, MP.MP_EMISSIVE_COLOR)
    else:
        inputs += [("Bright", m.collection("RainBrightness")), ("Amb", m.mask(m.collection("AmbientColor"), "rgb"))]
        c = m.custom(code, inputs, F3, [("Alpha", F1)])
        m.out(c, MP.MP_EMISSIVE_COLOR)
        m.out(c, MP.MP_OPACITY, "Alpha")
    return m.finish()


# ============================================================================ wetness decals

DECAL_COMMON = NOISE + r"""
float3 dpx = ddx(P); float3 dpy = ddy(P);
float3 Nrm = normalize(cross(dpy, dpx) + 1e-6);
if (dot(Nrm, Cam.xyz - P) < 0.0) Nrm = -Nrm;
float upness = Nrm.z;
// Shelter: look just off the surface (walls test the street in front of them; characters are not in the capture).
float sheltered = 0.0;
float topDist = 1e5;
if (Shelter.w > 0.0)
{
    float3 probe = P + float3(Nrm.xy, 0.0) * 120.0;
    float2 suv = float2(0.5 + (probe.y - Shelter.y) / Shelter.w, 0.5 - (probe.x - Shelter.x) / Shelter.w);
    if (all(suv > 0.002) && all(suv < 0.998))
    {
        float depth = Texture2DSampleLevel(Tex, TexSampler, suv, 0).r;
        if (depth < 5e7)
        {
            float top = Shelter.z - depth;
            sheltered = saturate((top - P.z - 25.0) / 40.0);
            topDist = abs(P.z - top);
        }
    }
}
float open = 1.0 - sheltered;
float edge = 1.0 - smoothstep(4500.0, 5800.0, length(P.xy - Cam.xy));
float poros = N.Fbm(P.xy / 180.0 + P.z / 300.0, 3);
float wet = saturate(Wet * 1.35 - poros * 0.4) * open * edge;
// Drips running down vertical surfaces (walls, clothes) while it rains and a while after.
float vertical = 1.0 - saturate(abs(upness) * 1.6);
float2 tdir = normalize(float2(-Nrm.y, Nrm.x) + 1e-5);
float s = dot(P.xy, tdir) + P.z * 0.0;
float colId = floor(s / 9.0);
float hs = N.H21(float2(colId, 4.1));
float flow = frac(P.z / (250.0 + hs * 300.0) + T * (0.25 + hs * 0.35) + hs);
float streak = step(0.5, hs) * smoothstep(0.32, 0.0, abs(frac(s / 9.0) - 0.5)) * smoothstep(0.0, 0.2, flow) * smoothstep(1.0, 0.55, flow);
streak *= vertical * saturate(Wet * 1.5) * open * edge;
// Puddles: flat top surfaces only, growing out of low-frequency noise as the puddle level rises.
float flatness = smoothstep(0.93, 0.985, upness);
float isTop = 1.0 - smoothstep(10.0, 40.0, topDist);
float pn = N.Fbm(P.xy / 420.0 + 5.3, 4) * 0.82 + N.Fbm(P.xy / 90.0, 2) * 0.18;
float level = Pud * 0.72;
float puddle = smoothstep(1.0 - level, 1.0 - level + 0.05, pn) * flatness * isTop * open * edge * step(0.01, Pud);
"""

DECAL_DARKEN = DECAL_COMMON + r"""
return saturate(wet * 0.42 + streak * 0.3 + puddle * 0.25);
"""

DECAL_GLOSS = DECAL_COMMON + r"""
return saturate(wet * 0.9 + streak * 0.7 + puddle);
"""

DECAL_PUDDLE = DECAL_COMMON + r"""
float2 rip = 0.0;
if (Rain > 0.01)
{
    for (int L = 0; L < 3; L++)
    {
        float2 q = P.xy / 70.0 + L * float2(3.7, 1.3);
        float2 ci = floor(q); float2 cf = frac(q) - 0.5;
        float hr = N.H21(ci + L * 11.0);
        float2 off = (float2(N.H21(ci + 1.7), N.H21(ci + 2.9)) - 0.5) * 0.5;
        float lt = frac(T * (0.9 + hr * 0.6) + hr);
        float2 d = cf - off; float dl = length(d);
        float ring = sin(clamp((dl - lt * 0.45) * 40.0, -3.14159, 3.14159)) * (1.0 - lt) * smoothstep(0.5, 0.0, dl) * step(1.0 - Rain, hr);
        rip += (d / max(dl, 1e-3)) * ring;
    }
}
float2 wq = P.xy / 120.0 + Wind.xy * T * Wind.z * 0.02;
float2 wave = float2(N.N2(wq + 1.3) - N.N2(wq + float2(1.6, 1.3)), N.N2(wq + 7.1) - N.N2(wq + float2(7.1, 7.4))) * Wind.w * 2.0;
Normal = normalize(float3(rip * 0.35 * saturate(Rain * 2.0) + wave, 1.0));
return puddle;
"""


def build_decal(name, code, mpc, base_colour=None, roughness=None, normal=False):
    m = Mat(name, mpc, domain=unreal.MaterialDomain.MD_DEFERRED_DECAL, blend=unreal.BlendMode.BLEND_TRANSLUCENT,
            props={"tangent_space_normal": not normal})   # decal blend mode: the default, Translucent
    inputs = [
        ("P", m.node(unreal.MaterialExpressionWorldPosition)), ("T", m.node(unreal.MaterialExpressionTime)),
        ("Tex", m.texture_object("ShelterDepth")), ("Shelter", m.collection("ShelterInfo")), ("Cam", m.collection("CameraPos")),
        ("Wet", m.collection("Wetness")), ("Pud", m.collection("Puddles")), ("Rain", m.collection("RainAmount")),
        ("Wind", m.collection("WindVector")),
    ]
    c = m.custom(code, inputs, F1, [("Normal", F3)] if normal else [])
    m.out(c, MP.MP_OPACITY)
    if base_colour is not None:
        m.out(m.constant3(base_colour), MP.MP_BASE_COLOR)
    if roughness is not None:
        m.out(m.constant(roughness), MP.MP_ROUGHNESS)
    if normal:
        m.out(c, MP.MP_NORMAL, "Normal")
    return m.finish()


# ============================================================================ audio (synthesised, no third-party files)

RATE = 22050


def write_wav(path, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    scale = 0.89 / peak
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))


def loop_crossfade(samples, seconds=0.6):
    n = int(RATE * seconds)
    body = samples[:-n]
    tail = samples[-n:]
    for i in range(n):
        t = i / n
        body[i] = body[i] * t + tail[i] * (1.0 - t)
    return body


def synth_rain(rng):
    n = RATE * 9
    out = [0.0] * n
    lp = 0.0
    prev = 0.0
    for i in range(n):
        white = rng.uniform(-1, 1)
        hp = white - prev   # bright hiss
        prev = white
        lp += (white - lp) * 0.08   # body
        out[i] = hp * 0.35 + lp * 0.9
    # Individual drops: short bright ticks on things nearby.
    for _ in range(int(9 * 140)):
        start = rng.randrange(0, n - 400)
        amp = rng.uniform(0.15, 0.6)
        freq = rng.uniform(1800, 5200)
        for k in range(300):
            out[start + k] += amp * math.exp(-k / 35.0) * math.sin(2 * math.pi * freq * k / RATE)
    return loop_crossfade(out)


def synth_wind(rng):
    n = RATE * 11
    out = [0.0] * n
    brown = 0.0
    lp = 0.0
    # resonant band-pass for a soft whistle that drifts
    y1 = y2 = 0.0
    for i in range(n):
        brown = max(-1.0, min(1.0, brown * 0.997 + rng.uniform(-1, 1) * 0.06))
        lp += (brown - lp) * 0.05
        t = i / RATE
        gust = 0.55 + 0.45 * math.sin(t * 0.9) * math.sin(t * 0.37 + 1.1)
        freq = 420 + 160 * math.sin(t * 0.5)
        r = 0.995
        a1 = -2 * r * math.cos(2 * math.pi * freq / RATE)
        x = rng.uniform(-1, 1)
        y = x * 0.02 - a1 * y1 - r * r * y2
        y2, y1 = y1, y
        out[i] = (lp * 2.2 + y * 0.35) * gust
    return loop_crossfade(out, 1.0)


def synth_thunder(rng, seconds):
    n = int(RATE * seconds)
    out = [0.0] * n
    brown = 0.0
    lp = lp2 = 0.0
    rolls = [(rng.uniform(0.0, 0.25), rng.uniform(0.8, 1.0))] + [(rng.uniform(0.4, seconds * 0.6), rng.uniform(0.3, 0.8)) for _ in range(3)]
    for i in range(n):
        t = i / RATE
        brown = max(-1.0, min(1.0, brown * 0.999 + rng.uniform(-1, 1) * 0.08))
        lp += (brown - lp) * 0.03
        lp2 += (lp - lp2) * 0.05
        env = 0.0
        for start, amp in rolls:
            if t >= start:
                d = t - start
                env += amp * (1 - math.exp(-d * 18.0)) * math.exp(-d * 1.1)
        crack = math.exp(-t * 25.0) * rng.uniform(-1, 1) * 0.6 if t < 0.25 else 0.0
        out[i] = lp2 * env * 3.0 + crack
    fade = int(RATE * 0.3)
    for i in range(fade):
        out[n - 1 - i] *= i / fade
    return out


def import_sound(path, name, looping):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", AUDIO)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    sound = unreal.load_asset("%s/%s" % (AUDIO, name))
    if not sound:
        raise RuntimeError("could not import " + path)
    sound.set_editor_property("looping", looping)
    save(sound)
    log("sound:", name, "(loop)" if looping else "")


def build_audio():
    os.makedirs(ART, exist_ok=True)
    rng = random.Random(1007)
    files = [("S_RainLoop", lambda: synth_rain(rng), True), ("S_WindLoop", lambda: synth_wind(rng), True)]
    for k, secs in ((1, 6.0), (2, 7.5), (3, 5.0)):
        files.append(("S_Thunder%d" % k, (lambda s=secs: synth_thunder(rng, s)), False))
    for name, make, looping in files:
        path = os.path.join(ART, name + ".wav")
        if not os.path.exists(path):   # synthesised once and kept in Art/Weather (deterministic seed)
            write_wav(path, make())
        import_sound(path, name, looping)


# ============================================================================ main

def main():
    if unreal.EditorAssetLibrary.does_directory_exist(ROOT):
        unreal.EditorAssetLibrary.delete_directory(ROOT)
    mpc = build_mpc()
    build_sky(mpc)
    build_rain(mpc)
    build_particle("M_BIBSplash", SPLASH_HLSL, mpc)
    build_particle("M_BIBWindStreak", WIND_HLSL, mpc)
    build_particle("M_BIBLightning", BOLT_HLSL, mpc, additive=True)
    build_decal("M_BIBWetDarken", DECAL_DARKEN, mpc, base_colour=(0.0, 0.0, 0.0))
    # Wet sheen, not a mirror (owner 2026-10-07: the whole street reflected the sky as white glare); puddles stay mirrors.
    build_decal("M_BIBWetGloss", DECAL_GLOSS, mpc, roughness=0.24)
    build_decal("M_BIBPuddle", DECAL_PUDDLE, mpc, base_colour=(0.012, 0.014, 0.018), roughness=0.02, normal=True)
    build_audio()


def run():
    status = "OK"
    try:
        main()
    except Exception:
        status = "FAILED"
        log(traceback.format_exc())
    log("STATUS " + status)
    path = os.path.join(DEMO, "Saved", "BuildWeather.log")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as handle:
        handle.write("\n".join(LOG) + "\n")


run()
