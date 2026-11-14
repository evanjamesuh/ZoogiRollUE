# Nov 3 b87 (Evan: "for cosmic make these stage alterants that grow from the floor one per round"): imports the rune marker
# (SourceArt/Props/CosmicMarker, Evan's zip) as SM_CosmicMarker + 4 textures + M_CosmicMarker (Color, Normal, Metal, Emission * Glow).
import os
import unreal

SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Props", "CosmicMarker")
DEST = "/Game/Env/CosmicMarker"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
def log(s): unreal.log("[ZoogiMarker] " + s)

ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
sd = ui.get_editor_property("static_mesh_import_data")
sd.set_editor_property("combine_meshes", True)
sd.set_editor_property("auto_generate_collision", False)
t = unreal.AssetImportTask()
t.filename = os.path.join(SRC, "marker_1.fbx"); t.destination_path = DEST; t.destination_name = "SM_CosmicMarker"
t.replace_existing = True; t.automated = True; t.save = True; t.options = ui
AT.import_asset_tasks([t])
sm = None
for p in t.imported_object_paths:
    a = unreal.load_asset(p)
    log("mesh import: %s (%s)" % (p, type(a).__name__))
    if isinstance(a, unreal.StaticMesh): sm = a
if not sm: sm = unreal.load_asset(DEST + "/SM_CosmicMarker")
if sm:
    b = sm.get_bounds()
    log("BOUNDS origin %s extent %s" % (b.origin, b.box_extent))

def tex(file, name, normal=False, srgb=True):
    tt = unreal.AssetImportTask()
    tt.filename = os.path.join(SRC, file); tt.destination_path = DEST; tt.destination_name = name
    tt.replace_existing = True; tt.automated = True; tt.save = True
    AT.import_asset_tasks([tt])
    x = unreal.load_asset(DEST + "/" + name)
    if x:
        if normal: x.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP); x.set_editor_property("srgb", False)
        elif not srgb: x.set_editor_property("srgb", False)
        x.set_editor_property("max_texture_size", 1024)
        EAL.save_loaded_asset(x)
    log("texture %s -> %s" % (name, x))
    return x
tc = tex("T_Marker_Color.png", "T_Marker_Color")
tn = tex("T_Marker_Normal.png", "T_Marker_Normal", normal=True)
tm = tex("T_Marker_Metal.png", "T_Marker_Metal", srgb=False)
te = tex("T_Marker_Emission.png", "T_Marker_Emission")

mpath = DEST + "/M_CosmicMarker"
if EAL.does_asset_exist(mpath): EAL.delete_asset(mpath)
m = AT.create_asset("M_CosmicMarker", DEST, unreal.Material, unreal.MaterialFactoryNew())
def samp(x, y, name, t, kind=None):
    s = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    s.set_editor_property("parameter_name", name)
    if kind is not None: s.set_editor_property("sampler_type", kind)
    if t: s.set_editor_property("texture", t)
    return s
sc = samp(-900, -200, "Color", tc)
sn = samp(-900, 100, "Normal", tn, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
sme = samp(-900, 400, "Metal", tm, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
se = samp(-900, 700, "Emission", te)
MEL.connect_material_property(sc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(sn, "RGB", unreal.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(sme, "R", unreal.MaterialProperty.MP_METALLIC)
# Unity-style metal map: smoothness in alpha -> roughness = 1 - A
om = MEL.create_material_expression(m, unreal.MaterialExpressionOneMinus, -500, 450)
MEL.connect_material_expressions(sme, "A", om, "")
MEL.connect_material_property(om, "", unreal.MaterialProperty.MP_ROUGHNESS)
glow = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 950)
glow.set_editor_property("parameter_name", "Glow"); glow.set_editor_property("default_value", 3.0)
mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 750)
MEL.connect_material_expressions(se, "RGB", mul, "A"); MEL.connect_material_expressions(glow, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m)
EAL.save_loaded_asset(m)
if sm:
    n = len(sm.get_editor_property("static_materials"))
    for i in range(max(1, n)): sm.set_material(i, m)
    EAL.save_loaded_asset(sm)
    log("material slots: %d" % n)
log("DONE")
