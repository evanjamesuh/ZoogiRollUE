# Cosmic arena "space journey" backdrop assets (headless: Scripts\run_python.ps1 -Script Scripts\make_cosmic_backdrop.py).
# Imports SourceArt/Cosmic textures and builds three materials in /Game/Env/Cosmic (cooked via /Game/Env):
#   M_CosmicSky    unlit sky-sphere: equirect nebula panorama sampled from the view direction (at infinity),
#                  far layer drifts slowly, a second "near wisps" layer (texture alpha) drifts faster -> parallax.
#   M_CosmicStreak additive star streaks for an ISM of planes; all motion is World Position Offset on the GPU
#                  (PerInstanceRandom phase/speed/tint), faded near the arena so the play area stays clean.
#   M_CosmicPlanet translucent unlit billboard for the distant planets (Tex / Bright parameters).
import unreal, os

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Cosmic"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Cosmic")

def import_tex(name, nomips):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, name + ".png")
    task.destination_path = DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    AT.import_asset_tasks([task])
    t = unreal.load_asset(DIR + "/" + name)
    if not t:
        unreal.log_error("COSMIC: import failed " + name)
        return None
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property("srgb", True)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("never_stream", True)
    if nomips:
        t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        t.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    else:
        t.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        t.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(t)
    return t

neb = import_tex("T_CosmicNebula", True)
pa = import_tex("T_CosmicPlanetA", False)
pb = import_tex("T_CosmicPlanetB", False)

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
    def const3(self, c, x, y):
        p = self.e(unreal.MaterialExpressionConstant3Vector, x, y)
        p.set_editor_property("constant", unreal.LinearColor(c[0], c[1], c[2], 0)); return p
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
                unreal.log_error("COSMIC: connect failed " + desc + "." + n)
        return c

# ---------------------------------------------------------------- M_CosmicSky
m = new_material("M_CosmicSky")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)
g = G(m)
texobj = g.e(unreal.MaterialExpressionTextureObjectParameter, -1200, -200)
texobj.set_editor_property("parameter_name", "Panorama")
texobj.set_editor_property("texture", neb)
camv = g.e(unreal.MaterialExpressionCameraVectorWS, -1200, -400)
tm = g.e(unreal.MaterialExpressionTime, -1200, -500)
sky = g.custom("""
float3 d = -normalize(C);
float u = atan2(d.y, d.x) / (2.0 * PI) + 0.5;
float v = 0.5 - asin(clamp(d.z, -1.0, 1.0)) / PI;
float4 farS = Texture2DSampleLevel(Tex, TexSampler, float2(u - T * RateFar, v), 0);
float4 nearS = Texture2DSampleLevel(Tex, TexSampler, float2(u - T * RateNear + 0.37, v), 0);
float3 col = farS.rgb * Bright + nearS.a * NearTint * NearStr;
float nad = saturate((-d.z - 0.5) / 0.5);
col *= lerp(1.0, NadirDim, nad);
return col;
""", [("C", camv), ("T", tm), ("Tex", texobj),
      ("Bright", g.scalar("Bright", 1.0, -1000, 0)),
      ("RateFar", g.scalar("RateFar", 0.0006, -1000, 80)),
      ("RateNear", g.scalar("RateNear", 0.0012, -1000, 160)),
      ("NearTint", g.vec("NearTint", (0.45, 0.3, 1.0, 1), -1000, 240)),
      ("NearStr", g.scalar("NearStr", 0.10, -1000, 400)),
      ("NadirDim", g.scalar("NadirDim", 0.7, -1000, 480))],
    unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, -200, "CosmicSky")
MEL.connect_material_property(sky, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

# ---------------------------------------------------------------- M_CosmicStreak
m = new_material("M_CosmicStreak")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property("two_sided", True)
m.set_editor_property("used_with_instanced_static_meshes", True)
g = G(m)
pir = g.e(unreal.MaterialExpressionPerInstanceRandom, -1400, -300)
tm = g.e(unreal.MaterialExpressionTime, -1400, -200)
speed = g.scalar("Speed", 2200.0, -1400, -100)
rng = g.scalar("Range", 26000.0, -1400, -20)
dirv = g.vec("TravelDir", (-1.0, 0.0, 0.0, 0), -1400, 60)
wpo = g.custom("""
float r2 = frac(R * 13.37 + 0.21);
float sp = Speed * (0.55 + 0.9 * r2);
float ph = frac(T * sp / Range + R * 7.13);
return normalize(Dir) * (ph - 0.5) * Range;
""", [("R", pir), ("T", tm), ("Speed", speed), ("Range", rng), ("Dir", dirv)],
    unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 200, "StreakWPO")
MEL.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1400, 300)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1600, 400)
centre = g.const3((0.0, 0.0, -400.0), -1600, 500)
dist = g.e(unreal.MaterialExpressionDistance, -1400, 420)
MEL.connect_material_expressions(wp, "", dist, "A"); MEL.connect_material_expressions(centre, "", dist, "B")
emis = g.custom("""
float r2 = frac(R * 13.37 + 0.21);
float sp = Speed * (0.55 + 0.9 * r2);
float ph = frac(T * sp / Range + R * 7.13);
float wrap = sin(ph * PI);
float across = 1.0 - abs(UV.y * 2.0 - 1.0); across *= across;
float along = pow(saturate(sin(UV.x * PI)), 0.7);
float fade = saturate((Dist - ClearR) / FadeW);
float3 tint = lerp(ColA, ColB, frac(R * 5.71));
return tint * Bright * across * along * wrap * fade * (0.45 + r2);
""", [("R", pir), ("T", tm), ("Speed", speed), ("Range", rng), ("UV", uv), ("Dist", dist),
      ("Bright", g.scalar("Bright", 5.0, -1400, 600)),
      ("ClearR", g.scalar("ClearR", 2800.0, -1400, 680)),
      ("FadeW", g.scalar("FadeW", 3500.0, -1400, 760)),
      ("ColA", g.vec("ColA", (0.6, 0.85, 1.0, 1), -1400, 840)),
      ("ColB", g.vec("ColB", (1.0, 0.5, 0.9, 1), -1400, 1000))],
    unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 600, "StreakEmissive")
MEL.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

# ---------------------------------------------------------------- M_CosmicPlanet
m = new_material("M_CosmicPlanet")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("two_sided", True)
g = G(m)
ts = g.e(unreal.MaterialExpressionTextureSampleParameter2D, -800, 0)
ts.set_editor_property("parameter_name", "Tex")
ts.set_editor_property("texture", pa)
br = g.scalar("Bright", 0.8, -800, 300)
mul = g.e(unreal.MaterialExpressionMultiply, -400, 0)
MEL.connect_material_expressions(ts, "RGB", mul, "A"); MEL.connect_material_expressions(br, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(m); EAL.save_loaded_asset(m)

for n in ("T_CosmicNebula", "T_CosmicPlanetA", "T_CosmicPlanetB", "M_CosmicSky", "M_CosmicStreak", "M_CosmicPlanet"):
    unreal.log("COSMIC: %s exists=%s" % (n, EAL.does_asset_exist(DIR + "/" + n)))
