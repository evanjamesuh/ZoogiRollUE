# Frozen Ring kit assets (Oct 6). Headless: Scripts\run_python.ps1 -Script Scripts\arenas\make_frozen_kit.py -LogName frozenkit
# Imports SourceArt/Arenas/Frozen/T_*.png into /Game/Env/Arenas and builds:
#   M_FrozenPiece  lit opaque stage-piece material: Color x triplanar detail (T_Frozen*Tile), world-space crack network
#                  (T_FrozenCrackTile, value = crack rank) revealed by "Damage" 0..1 with cyan CrackColor glow, "Melt"
#                  (Ember: wet / warm), "Frost" (refreeze sheen), "Flash" (element hit), EmissiveColor x EmissiveStrength.
#   M_FrozenDecal  unlit translucent floor scar: Tex.rgb x Tint x Bright, alpha Tex.a x Opacity, + Emissive glow.
import unreal, os, glob

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "Frozen")
TA = unreal.TextureAddress
TCS = unreal.TextureCompressionSettings


def log(m):
    unreal.log("FROZENKIT: " + m)


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
        unreal.log_error("FROZENKIT: import failed " + name)
        return None
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("never_stream", True)
    gray = name.endswith("Glow") or name.endswith("Tile") and not name.endswith("ColorTile")
    if name.endswith("Floor"):
        t.set_editor_property("compression_settings", TCS.TC_BC7); t.set_editor_property("srgb", True)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif name.endswith("Glow"):
        t.set_editor_property("compression_settings", TCS.TC_GRAYSCALE); t.set_editor_property("srgb", False)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif name.endswith("Sky"):
        t.set_editor_property("compression_settings", TCS.TC_BC7); t.set_editor_property("srgb", True)
        t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif name.startswith("T_FrozenDisp") or name == "T_FrozenCloud":
        # round 3: colour-baked display decals (pack-derived) and the cloud-sea puff
        t.set_editor_property("compression_settings", TCS.TC_DEFAULT); t.set_editor_property("srgb", True)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif any(name.startswith(p) for p in ("T_FrozenCrack_", "T_FrozenGlowCrack_", "T_FrozenCrater_", "T_FrozenFX_", "T_FrozenPow_", "T_FrozenForm_")):
        # round 3: raw Animations & Powers pack masks / flipbooks (linear data, kept for MF_FrozenDamage / Niagara work)
        t.set_editor_property("compression_settings", TCS.TC_DEFAULT); t.set_editor_property("srgb", "BaseColor" in name or "_FX_" in name)
        tile = "_Tile_" in name
        t.set_editor_property("address_x", TA.TA_WRAP if tile else TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_WRAP if tile else TA.TA_CLAMP)
    elif name.startswith("T_FrozenDmg") or name.startswith("T_FrozenScar"):
        t.set_editor_property("compression_settings", TCS.TC_DEFAULT); t.set_editor_property("srgb", True)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    elif gray:
        t.set_editor_property("compression_settings", TCS.TC_GRAYSCALE); t.set_editor_property("srgb", False)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_WRAP)
    else:
        t.set_editor_property("compression_settings", TCS.TC_BC7); t.set_editor_property("srgb", True)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_WRAP)
    EAL.save_loaded_asset(t)
    log("imported %s" % name)
    return t


texs = {}
for p in sorted(glob.glob(os.path.join(SRC, "T_*.png"))):
    t = import_tex(p)
    if t:
        texs[os.path.splitext(os.path.basename(p))[0]] = t


def tex(name):
    return texs.get(name) or unreal.load_asset(DIR + "/" + name)


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
                unreal.log_error("FROZENKIT: connect failed " + desc + "." + n)
        return c


F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1

TRI = """
float3 nw = abs(normalize(N)); nw = pow(nw, 3.0); nw /= (nw.x + nw.y + nw.z + 1e-4);
"""

# ---------------------------------------------------------------- M_FrozenPiece
m = new_material("M_FrozenPiece")
g = G(m)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1500, -400)
nrm = g.e(unreal.MaterialExpressionVertexNormalWS, -1500, -320)
det = g.texobj("Detail", tex("T_FrozenIceTile"), -1500, -200)
crk = g.texobj("Crack", tex("T_FrozenCrackTile"), -1500, -40)
col = g.vec("Color", (0.62, 0.86, 1.0, 1), -1500, 120)
dsc = g.scalar("DetailScale", 220.0, -1500, 280)
dst = g.scalar("DetailStrength", 0.45, -1500, 360)
csc = g.scalar("CrackScale", 260.0, -1500, 440)
dmg = g.scalar("Damage", 0.0, -1500, 520)
melt = g.scalar("Melt", 0.0, -1500, 600)
frost = g.scalar("Frost", 0.0, -1500, 680)
flash = g.scalar("Flash", 0.0, -1500, 760)
ccol = g.vec("CrackColor", (0.10, 0.76, 0.90, 1), -1500, 840)
cglow = g.scalar("CrackGlow", 2.2, -1500, 1000)
ecol = g.vec("EmissiveColor", (0.25, 0.75, 1.0, 1), -1500, 1080)
estr = g.scalar("EmissiveStrength", 0.0, -1500, 1240)
shade = g.scalar("ShadeLift", 0.12, -1500, 1320)
mask = g.custom(TRI + """
float3 q = WP / CrackScale;
float k = Texture2DSample(Crk, CrkSampler, q.yz).r * nw.x + Texture2DSample(Crk, CrkSampler, q.xz + 0.37).r * nw.y + Texture2DSample(Crk, CrkSampler, q.xy + 0.71).r * nw.z;
return saturate((k - (1.0 - Damage * 0.92)) * 10.0) * step(0.01, Damage);
""", [("WP", wp), ("N", nrm), ("Crk", crk), ("CrackScale", csc), ("Damage", dmg)], F1, -900, 500, "CrackMask")
bc = g.custom(TRI + """
float3 p = WP / DetailScale;
float d = Texture2DSample(Det, DetSampler, p.yz).r * nw.x + Texture2DSample(Det, DetSampler, p.xz).r * nw.y + Texture2DSample(Det, DetSampler, p.xy).r * nw.z;
float3 c = Color.rgb * lerp(1.0, d * 2.0, DetailStrength);
c = lerp(c, Color.rgb, ShadeLift * saturate(N.z));
float3 fissure = lerp(c * 0.28, CrackColor.rgb * 0.55, 0.45);
c = lerp(c, fissure, M);
c *= 1.0 - Damage * 0.16;
c = lerp(c, c * float3(0.72, 0.70, 0.74), Melt * 0.6);
c = lerp(c, float3(0.93, 0.98, 1.0), Frost * 0.45);
return c;
""", [("WP", wp), ("N", nrm), ("Det", det), ("Color", col), ("DetailScale", dsc), ("DetailStrength", dst), ("M", mask),
      ("CrackColor", ccol), ("Damage", dmg), ("Melt", melt), ("Frost", frost), ("ShadeLift", shade)], F3, -600, -100, "PieceBase")
MEL.connect_material_property(bc, "", unreal.MaterialProperty.MP_BASE_COLOR)
em = g.custom("""
float3 e = EmissiveColor.rgb * EmissiveStrength;
e += CrackColor.rgb * CrackGlow * M * (0.55 + 0.45 * Damage);
e += float3(1.0, 0.42, 0.12) * Melt * M * 2.5;
e += Color.rgb * Flash * 0.8 + float3(0.6, 0.9, 1.0) * Frost * 0.25;
return e;
""", [("EmissiveColor", ecol), ("EmissiveStrength", estr), ("CrackColor", ccol), ("CrackGlow", cglow), ("M", mask), ("Damage", dmg),
      ("Melt", melt), ("Color", col), ("Flash", flash), ("Frost", frost)], F3, -600, 400, "PieceEmissive")
MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
rough = g.custom("return lerp(R, 0.9, Damage * 0.5 + Melt * -0.3);", [("R", g.scalar("Roughness", 0.35, -900, 800)), ("Damage", dmg), ("Melt", melt)], F1, -600, 800, "PieceRough")
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(g.scalar("Specular", 0.6, -600, 900), "", unreal.MaterialProperty.MP_SPECULAR)
MEL.connect_material_property(g.scalar("Metallic", 0.0, -600, 980), "", unreal.MaterialProperty.MP_METALLIC)
MEL.recompile_material(m); EAL.save_loaded_asset(m)
log("built M_FrozenPiece")

# ---------------------------------------------------------------- M_FrozenDecal
m = new_material("M_FrozenDecal")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("two_sided", False)
g = G(m)
uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1200, -300)
tx = g.texobj("Tex", tex("T_FrozenDmg_Crater"), -1200, -150)
tint = g.vec("Tint", (1, 1, 1, 1), -1200, 0)
bright = g.scalar("Bright", 0.9, -1200, 160)
opac = g.scalar("Opacity", 1.0, -1200, 240)
emis = g.scalar("Emissive", 0.0, -1200, 320)
ecol = g.vec("EmissiveTint", (0.15, 0.85, 1.0, 1), -1200, 400)
c = g.custom("""
float4 s = Texture2DSample(Tex, TexSampler, UV);
return s.rgb * Tint.rgb * Bright + s.rgb * EmissiveTint.rgb * Emissive;
""", [("UV", uv), ("Tex", tx), ("Tint", tint), ("Bright", bright), ("Emissive", emis), ("EmissiveTint", ecol)], F3, -700, -100, "DecalColor")
MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
a = g.custom("return saturate(Texture2DSample(Tex, TexSampler, UV).a * Opacity);", [("UV", uv), ("Tex", tx), ("Opacity", opac)], F1, -700, 200, "DecalAlpha")
MEL.connect_material_property(a, "", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(m); EAL.save_loaded_asset(m)
log("built M_FrozenDecal")
log("DONE")
