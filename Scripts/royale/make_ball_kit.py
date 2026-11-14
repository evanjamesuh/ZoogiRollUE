# Oct 31 (Evan's 5 ball designs -> the new Zoogis' ball forms): imports SourceArt/Balls/T_Ball_*.png (equirect wraps made from
# his front renders: front hemisphere = the render, back = mirror) into /Game/Zoogis/Balls and builds M_ZoogiBallSkin:
# lit, BaseColor = Tex(UV0), Emissive = the bright parts (glowing cores / seams) x Glow, Roughness / Metallic params.
import unreal, os, glob
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary
DIR = "/Game/Zoogis/Balls"; SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Balls")
def log(m): unreal.log("BALLKIT: " + m)
first = None
for p in sorted(glob.glob(os.path.join(SRC, "T_Ball_*.png"))):
    n = os.path.splitext(os.path.basename(p))[0]
    t = unreal.AssetImportTask(); t.filename = p; t.destination_path = DIR; t.destination_name = n; t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t])
    tex = unreal.load_asset(DIR + "/" + n)
    if not tex: unreal.log_error("BALLKIT: import failed " + n); continue
    tex.set_editor_property("srgb", True); tex.set_editor_property("max_texture_size", 1024)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP); tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(tex); first = first or tex; log("imported " + n)
path = DIR + "/M_ZoogiBallSkin"
if EAL.does_asset_exist(path): EAL.delete_asset(path)
m = AT.create_asset("M_ZoogiBallSkin", DIR, unreal.Material, unreal.MaterialFactoryNew())
def e(cls, x, y): return MEL.create_material_expression(m, cls, x, y)
tp = e(unreal.MaterialExpressionTextureSampleParameter2D, -900, 0); tp.set_editor_property("parameter_name", "Tex")
if first: tp.set_editor_property("texture", first)
def sc(n, v, y):
    p = e(unreal.MaterialExpressionScalarParameter, -900, y); p.set_editor_property("parameter_name", n); p.set_editor_property("default_value", v); return p
glow = sc("Glow", 2.2, 300); rough = sc("Roughness", 0.32, 400); metal = sc("Metallic", 0.35, 500)
MEL.connect_material_property(tp, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
c = e(unreal.MaterialExpressionCustom, -500, 200)
c.set_editor_property("code", "float l = dot(C.rgb, float3(0.3, 0.59, 0.11)); float m = saturate((max(C.r, max(C.g, C.b)) - 0.78) * 4.0) * saturate((max(C.r, max(C.g, C.b)) - min(C.r, min(C.g, C.b))) * 2.0 + 0.2); return C.rgb * m * G;")
c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3); c.set_editor_property("description", "BallGlow")
ins = []
for n in ("C", "G"):
    ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
c.set_editor_property("inputs", ins)
MEL.connect_material_expressions(tp, "RGB", c, "C"); MEL.connect_material_expressions(glow, "", c, "G")
MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS); MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
MEL.recompile_material(m); EAL.save_loaded_asset(m); log("built M_ZoogiBallSkin"); log("DONE")
