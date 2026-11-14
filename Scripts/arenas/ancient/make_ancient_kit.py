# Oct 31: Ancient Collision Meadows kit. Headless (see C:\dev\ios\ancient_kit_run.py):
#   UnrealEditor-Cmd.exe <p5style>\ZoogiRoll.uproject -run=pythonscript -script=<this> -unattended -nosplash -NullRHI
# Imports SourceArt/Arenas/Ancient/T_Anc*.png (colour tiles, BC7 sRGB, wrap, mips) into /Game/Env/Arenas and builds
#   M_AncientTile  lit opaque: world-space TOP texture on up-facing faces (WP.xy / TopScale) blended to a SIDE texture (masonry)
#                  on walls (WP.yz | WP.xz / SideScale), x TopTint / SideTint x Bright, Roughness, + EmissiveColor x EmissiveStrength.
import unreal, os, glob
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "Ancient")
TA = unreal.TextureAddress
TCS = unreal.TextureCompressionSettings
def log(m): unreal.log("ANCIENTKIT: " + m)
texs = {}
for p in sorted(glob.glob(os.path.join(SRC, "T_Anc*.png"))):
    name = os.path.splitext(os.path.basename(p))[0]
    task = unreal.AssetImportTask(); task.filename = p; task.destination_path = DIR; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    AT.import_asset_tasks([task])
    t = unreal.load_asset(DIR + "/" + name)
    if not t: unreal.log_error("ANCIENTKIT: import failed " + name); continue
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("compression_settings", TCS.TC_DEFAULT); t.set_editor_property("srgb", True)
    t.set_editor_property("address_x", TA.TA_CLAMP if ("Banner" in name or "Medallion" in name) else TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_CLAMP if (name.endswith("Sky") or "Banner" in name or "Medallion" in name) else TA.TA_WRAP)  # Oct 31: T_AncientSky panorama (Grok art)
    EAL.save_loaded_asset(t); texs[name] = t; log("imported " + name)
def tex(n): return texs.get(n) or unreal.load_asset(DIR + "/" + n)
path = DIR + "/M_AncientTile"
if EAL.does_asset_exist(path): EAL.delete_asset(path)
m = AT.create_asset("M_AncientTile", DIR, unreal.Material, unreal.MaterialFactoryNew())
def e(cls, x, y): return MEL.create_material_expression(m, cls, x, y)
def scalar(n, v, x, y):
    p = e(unreal.MaterialExpressionScalarParameter, x, y); p.set_editor_property("parameter_name", n); p.set_editor_property("default_value", v); return p
def vec(n, c, x, y):
    p = e(unreal.MaterialExpressionVectorParameter, x, y); p.set_editor_property("parameter_name", n); p.set_editor_property("default_value", unreal.LinearColor(*c)); return p
def texobj(n, t, x, y):
    p = e(unreal.MaterialExpressionTextureObjectParameter, x, y); p.set_editor_property("parameter_name", n)
    if t: p.set_editor_property("texture", t)
    return p
def custom(code, inputs, out, x, y, desc):
    c = e(unreal.MaterialExpressionCustom, x, y); c.set_editor_property("code", code); c.set_editor_property("output_type", out); c.set_editor_property("description", desc)
    ins = []
    for n, _ in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    for n, s in inputs:
        if not MEL.connect_material_expressions(s, "", c, n): unreal.log_error("ANCIENTKIT: connect failed " + desc + "." + n)
    return c
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
wp = e(unreal.MaterialExpressionWorldPosition, -1400, -300)
nrm = e(unreal.MaterialExpressionVertexNormalWS, -1400, -220)
top = texobj("TopTex", tex("T_AncHubTile"), -1400, -120)
side = texobj("SideTex", tex("T_AncWallTile"), -1400, 40)
ts = scalar("TopScale", 400.0, -1400, 200); ss = scalar("SideScale", 300.0, -1400, 280)
tt = vec("TopTint", (1, 1, 1, 1), -1400, 360); st = vec("SideTint", (1, 1, 1, 1), -1400, 520)
br = scalar("Bright", 1.0, -1400, 680)
bc = custom("""
float3 n = normalize(N);
float up = saturate((n.z - 0.55) * 5.0);
float3 t = Texture2DSample(Top, TopSampler, WP.xy / TopScale).rgb * TopTint.rgb;
float2 suv = (abs(n.x) > abs(n.y) ? WP.yz : WP.xz) / SideScale; suv.y = -suv.y;
float3 s = Texture2DSample(Side, SideSampler, suv).rgb * SideTint.rgb;
return lerp(s, t, up) * Bright;
""", [("WP", wp), ("N", nrm), ("Top", top), ("Side", side), ("TopScale", ts), ("SideScale", ss), ("TopTint", tt), ("SideTint", st), ("Bright", br)], F3, -800, -100, "AncientTile")
MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
em = custom("return EC.rgb * ES;", [("EC", vec("EmissiveColor", (0, 0, 0, 1), -1100, 760)), ("ES", scalar("EmissiveStrength", 0.0, -1100, 920))], F3, -800, 700, "AncientEmis")
MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(scalar("Roughness", 0.78, -800, 900), "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(scalar("Specular", 0.35, -800, 980), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m); EAL.save_loaded_asset(m)
log("built M_AncientTile")
# Nov 1 v2: M_AncientDecal - lit, masked, two-sided UV0 texture (Z banners, Z medallion): BaseColor = Tex.rgb, OpacityMask = Tex.a,
#   Emissive = Tex.rgb x EmissiveStrength
dpath = DIR + "/M_AncientDecal"
if EAL.does_asset_exist(dpath): EAL.delete_asset(dpath)
d = AT.create_asset("M_AncientDecal", DIR, unreal.Material, unreal.MaterialFactoryNew())
d.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
d.set_editor_property("two_sided", True)
d.set_editor_property("opacity_mask_clip_value", 0.4)
ts_ = MEL.create_material_expression(d, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
ts_.set_editor_property("parameter_name", "Tex")
bt = tex("T_AncBannerZ")
if bt: ts_.set_editor_property("texture", bt)
MEL.connect_material_property(ts_, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(ts_, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
es_ = MEL.create_material_expression(d, unreal.MaterialExpressionScalarParameter, -600, 300); es_.set_editor_property("parameter_name", "EmissiveStrength"); es_.set_editor_property("default_value", 0.1)
mul = MEL.create_material_expression(d, unreal.MaterialExpressionMultiply, -300, 250)
MEL.connect_material_expressions(ts_, "RGB", mul, "A"); MEL.connect_material_expressions(es_, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
rr = MEL.create_material_expression(d, unreal.MaterialExpressionConstant, -300, 400); rr.set_editor_property("r", 0.55)
MEL.connect_material_property(rr, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(d); EAL.save_loaded_asset(d)
log("built M_AncientDecal")
log("DONE")
