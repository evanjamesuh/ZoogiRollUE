# Buzzy arena kit assets (headless: Scripts\run_python.ps1 -Script Scripts\arenas\make_arena_kit.py -LogName arenakit).
# Imports every SourceArt/Arenas/T_*.png into /Game/Env/Arenas (cooked via /Game/Env) and (re)builds the shared materials:
#   M_ZoogiArenaFloor  lit opaque floor: planar UV = WorldXY / (2*Radius) + 0.5 over T_<Arena>Floor (base colour) and
#                      T_<Arena>Glow (glow-line mask) -> GlowColor * GlowStrength emissive, plus a tiling T_StoneDetail grain.
#   M_ArenaSky         unlit sky sphere: equirect panorama sampled from the view direction (Cosmic sky without streaks),
#                      Bright / NadirDim / NadirColor / Yaw parameters.
#   M_ArenaWaterfall   unlit translucent, two-sided: T_WaterStreaks panning down, soft side and bottom fade.
# Texture settings by name suffix: *Floor = BC7 sRGB clamp; *Glow = grayscale linear clamp; *Sky = BC7 sRGB no-mips
# wrap-U; T_StoneDetail / T_WaterStreaks / *Tile = grayscale or BC7, wrap.
import unreal, os, glob

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas")
TA = unreal.TextureAddress


def log(m):
    unreal.log("ARENAKIT: " + m)


GRAY_WRAP = ("T_StoneDetail", "T_WaterStreaks", "T_LavaNoise", "T_LavaCrust")


def import_tex(path):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    AT.import_asset_tasks([task])
    t = unreal.load_asset(DIR + "/" + name)
    if not t:
        unreal.log_error("ARENAKIT: import failed " + name)
        return None
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("never_stream", True)
    if name.endswith("Glow"):
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        t.set_editor_property("srgb", False)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif name.endswith("Sky"):
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        t.set_editor_property("srgb", True)
        t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif name in GRAY_WRAP or name.endswith("Tile"):
        gray = name in GRAY_WRAP
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE if gray else unreal.TextureCompressionSettings.TC_BC7)
        t.set_editor_property("srgb", not gray)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_WRAP)
    else:  # *Floor and anything else: colour, clamp
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
        t.set_editor_property("srgb", True)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    EAL.save_loaded_asset(t)
    log("imported %s" % name)
    return t


texs = {}
# ARENAKIT_NO_IMPORT=1: rebuild the materials only (main has no Meadow re-skin textures; Oct 6 Frozen Ring).
for p in ([] if os.environ.get("ARENAKIT_NO_IMPORT") else sorted(glob.glob(os.path.join(SRC, "T_*.png")))):
    t = import_tex(p)
    if t:
        texs[os.path.splitext(os.path.basename(p))[0]] = t


def tex(name, *fallbacks):
    for n in (name,) + fallbacks:
        if not EAL.does_asset_exist(DIR + "/" + n) and n not in texs:
            continue
        t = texs.get(n) or unreal.load_asset(DIR + "/" + n)
        if t:
            return t
    return None


def new_material(name):
    path = DIR + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())


class G:
    def __init__(self, m): self.m = m
    def e(self, cls, x, y): return MEL.create_material_expression(self.m, cls, x, y)
    def scalar(self, name, v, x, y):
        p = self.e(unreal.MaterialExpressionScalarParameter, x, y)
        p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", v); return p
    def vec(self, name, c, x, y):
        p = self.e(unreal.MaterialExpressionVectorParameter, x, y)
        p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*c)); return p
    def texobj(self, name, t, x, y):
        p = self.e(unreal.MaterialExpressionTextureObjectParameter, x, y)
        p.set_editor_property("parameter_name", name)
        if t: p.set_editor_property("texture", t)
        return p
    def custom(self, code, inputs, out_type, x, y, desc):
        c = self.e(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", out_type)
        c.set_editor_property("description", desc)
        ins = []
        for n, _ in inputs:
            ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
        c.set_editor_property("inputs", ins)
        for n, src in inputs:
            if not MEL.connect_material_expressions(src, "", c, n):
                unreal.log_error("ARENAKIT: connect failed " + desc + "." + n)
        return c


F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1

# ---------------------------------------------------------------- M_ZoogiArenaFloor
m = new_material("M_ZoogiArenaFloor")
g = G(m)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1400, -300)
base = g.texobj("Base", tex("T_MeadowFloor", "T_FrozenFloor"), -1400, -150)
glow = g.texobj("Glow", tex("T_MeadowGlow", "T_FrozenGlow"), -1400, 0)
det = g.texobj("Detail", tex("T_StoneDetail"), -1400, 150)
radius = g.scalar("Radius", 1550.0, -1400, 300)
gcol = g.vec("GlowColor", (0.22, 0.89, 0.48, 1), -1400, 380)
gstr = g.scalar("GlowStrength", 2.5, -1400, 540)
gtint = g.scalar("GlowTint", 0.55, -1400, 620)
dscale = g.scalar("DetailScale", 180.0, -1400, 700)
dstr = g.scalar("DetailStrength", 0.35, -1400, 780)
bmul = g.scalar("BaseBright", 1.0, -1400, 860)
tm = g.e(unreal.MaterialExpressionTime, -1400, 940)
pulse = g.scalar("Pulse", 0.08, -1400, 1020)
bc = g.custom("""
float2 uv = WP.xy / (2.0 * Radius) + 0.5;
float3 b = Texture2DSample(Base, BaseSampler, uv).rgb * BaseBright;
float gm = Texture2DSample(Glow, GlowSampler, uv).r;
float d = Texture2DSample(Detail, DetailSampler, WP.xy / DetailScale).r;
b *= lerp(1.0, d * 2.0, DetailStrength);
return lerp(b, GlowColor * 0.9, saturate(gm * 1.3) * GlowTint);
""", [("WP", wp), ("Base", base), ("Glow", glow), ("Detail", det), ("Radius", radius), ("GlowColor", gcol),
      ("GlowTint", gtint), ("DetailScale", dscale), ("DetailStrength", dstr), ("BaseBright", bmul)], F3, -800, -100, "FloorBase")
MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
em = g.custom("""
float2 uv = WP.xy / (2.0 * Radius) + 0.5;
float gm = Texture2DSample(Glow, GlowSampler, uv).r;
float p = 1.0 + Pulse * sin(T * 1.7 - length(WP.xy) * 0.004);
return GlowColor * GlowStrength * gm * p;
""", [("WP", wp), ("Glow", glow), ("Radius", radius), ("GlowColor", gcol), ("GlowStrength", gstr), ("T", tm), ("Pulse", pulse)],
    F3, -800, 300, "FloorGlow")
MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(g.scalar("Roughness", 0.82, -800, 600), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(g.scalar("Specular", 0.35, -800, 700), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

# ---------------------------------------------------------------- M_ArenaSky
m = new_material("M_ArenaSky")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
try:
    m.set_editor_property("is_sky", True)
except Exception as ex:
    log("is_sky not set: %s" % ex)
g = G(m)
pano = g.texobj("Panorama", tex("T_MeadowSky", "T_FrozenSky"), -1200, -200)
camv = g.e(unreal.MaterialExpressionCameraVectorWS, -1200, -400)
tm = g.e(unreal.MaterialExpressionTime, -1200, -500)
sky = g.custom("""
float3 d = -normalize(C);
float u = atan2(d.y, d.x) / (2.0 * PI) + 0.5 + Yaw - T * Drift;
float v = 0.5 - asin(clamp(d.z, -1.0, 1.0)) / PI;
float3 col = Texture2DSampleLevel(Tex, TexSampler, float2(u, v), 0).rgb * Bright;
float nad = saturate((-d.z - NadirStart) / max(1.0 - NadirStart, 0.01));
col = lerp(col, NadirColor * Bright, nad * NadirDim);
return col;
""", [("C", camv), ("T", tm), ("Tex", pano),
      ("Bright", g.scalar("Bright", 1.0, -1000, 0)),
      ("Yaw", g.scalar("Yaw", 0.0, -1000, 80)),
      ("Drift", g.scalar("Drift", 0.0, -1000, 160)),
      ("NadirStart", g.scalar("NadirStart", 0.55, -1000, 240)),
      ("NadirDim", g.scalar("NadirDim", 0.0, -1000, 320)),
      ("NadirColor", g.vec("NadirColor", (0.3, 0.4, 0.3, 1), -1000, 400))],
    F3, -600, -200, "ArenaSky")
MEL.connect_material_property(sky, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

# ---------------------------------------------------------------- M_ArenaWaterfall
m = new_material("M_ArenaWaterfall")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("two_sided", True)
g = G(m)
st = g.texobj("Streaks", tex("T_WaterStreaks"), -1200, -200)
uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1200, -350)
tm = g.e(unreal.MaterialExpressionTime, -1200, -450)
spd = g.scalar("Speed", 0.6, -1200, 0)
tint = g.vec("Tint", (0.75, 0.9, 1.0, 1), -1200, 80)
foam = g.vec("Foam", (1.0, 1.0, 1.0, 1), -1200, 240)
bright = g.scalar("Bright", 1.4, -1200, 400)
emis = g.custom("""
float a = Texture2DSample(Tex, TexSampler, float2(UV.x * 1.0, UV.y * 0.5 - T * Speed)).r;
float b = Texture2DSample(Tex, TexSampler, float2(UV.x * 1.7 + 0.31, UV.y * 0.8 - T * Speed * 1.35)).r;
float s = saturate(a * 0.6 + b * 0.55);
return lerp(Tint, Foam, s * s) * Bright;
""", [("UV", uv), ("T", tm), ("Tex", st), ("Speed", spd), ("Tint", tint), ("Foam", foam), ("Bright", bright)], F3, -700, -200, "FallColor")
MEL.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
op = g.custom("""
float a = Texture2DSample(Tex, TexSampler, float2(UV.x * 1.0, UV.y * 0.5 - T * Speed)).r;
float side = saturate(min(UV.x, 1.0 - UV.x) * 6.0);
float top = saturate(UV.y * 8.0);
float bottom = saturate((VMax - UV.y) / max(VMax * 0.35, 0.01));
return saturate((0.45 + 0.55 * a) * side * top * bottom * Opacity);
""", [("UV", uv), ("T", tm), ("Tex", st), ("Speed", spd),
      ("VMax", g.scalar("VMax", 4.0, -1200, 480)), ("Opacity", g.scalar("Opacity", 0.85, -1200, 560))], F1, -700, 200, "FallOpacity")
MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

# ---------------------------------------------------------------- M_ArenaLava (lava sea + lava falls + magma orb)
# Unlit opaque two-sided: two panned heat-noise layers over a bright base, dark crust plates (T_LavaCrust) drifting on top.
m = new_material("M_ArenaLava")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
g = G(m)
uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1200, -350)
tm = g.e(unreal.MaterialExpressionTime, -1200, -450)
lava = g.custom("""
float2 s = float2(SpeedU, SpeedV);
float2 p = UV * float2(TileU, TileV);
float n1 = Texture2DSample(Noise, NoiseSampler, p + T * s).r;
float n2 = Texture2DSample(Noise, NoiseSampler, p * 2.3 + float2(0.37, 0.11) + T * s * 1.6).r;
float heat = saturate(n1 * 0.65 + n2 * 0.5 - 0.1);
float c = Texture2DSample(Crust, CrustSampler, p * 0.8 + T * s * 0.35).r;
float crust = smoothstep(CrustCut, CrustCut + 0.18, c - heat * 0.35);
float3 col = lerp(Mid, Hot, heat * heat) * Bright;
return lerp(col, CrustCol, crust * CrustAmount);
""", [("UV", uv), ("T", tm),
      ("Noise", g.texobj("Noise", tex("T_LavaNoise"), -1200, -200)),
      ("Crust", g.texobj("Crust", tex("T_LavaCrust"), -1200, -50)),
      ("TileU", g.scalar("TileU", 1.0, -1000, 0)), ("TileV", g.scalar("TileV", 1.0, -1000, 80)),
      ("SpeedU", g.scalar("SpeedU", 0.0, -1000, 160)), ("SpeedV", g.scalar("SpeedV", 0.0, -1000, 240)),
      ("Hot", g.vec("Hot", (1.0, 0.62, 0.18, 1), -1000, 320)), ("Mid", g.vec("Mid", (1.0, 0.22, 0.03, 1), -1000, 480)),
      ("CrustCol", g.vec("CrustCol", (0.03, 0.012, 0.008, 1), -1000, 640)),
      ("Bright", g.scalar("Bright", 6.0, -1000, 800)), ("CrustCut", g.scalar("CrustCut", 0.45, -1000, 880)),
      ("CrustAmount", g.scalar("CrustAmount", 0.9, -1000, 960))], F3, -600, -200, "Lava")
MEL.connect_material_property(lava, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

for n in ["M_ZoogiArenaFloor", "M_ArenaSky", "M_ArenaWaterfall", "M_ArenaLava"] + sorted(texs.keys()):
    log("%s exists=%s" % (n, EAL.does_asset_exist(DIR + "/" + n)))
