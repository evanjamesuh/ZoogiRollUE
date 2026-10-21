# Meadow crater slab kit (A&P pack, Oct 6 23:50): imports SourceArt/Arenas/MeadowCrater/*.fbx (SM_MeadowCraterSlab_A..D, cm scale,
# pivot = outer top edge, +X to crater centre) and the crater decal textures into /Game/Env/Arenas/Meadow. Hand-built kit, no Aura.
# Headless: Scripts\run_python.ps1 Scripts\arenas\make_meadow_crater.py meadowcrater
import unreal, os, glob

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas/Meadow"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "MeadowCrater")


def log(m):
    unreal.log("MEADOWCRATER: " + m)


for p in sorted(glob.glob(os.path.join(SRC, "*.fbx"))):
    name = os.path.splitext(os.path.basename(p))[0]
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    # The A&P FBX comes in 100x large through the FBX importer (bounds ~80 m); scale to the authored cm size (~80 cm).
    sm.set_editor_property("import_uniform_scale", 0.01)
    t = unreal.AssetImportTask()
    t.filename = p
    t.destination_path = DIR
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = ui
    AT.import_asset_tasks([t])
    a = unreal.load_asset(DIR + "/" + name)
    if a:
        b = a.get_bounding_box()
        log("imported %s bounds %s..%s mats %d" % (name, b.min, b.max, len(a.static_materials)))
    else:
        unreal.log_error("MEADOWCRATER: import failed " + name)

for p in sorted(glob.glob(os.path.join(SRC, "T_*.png"))):
    name = os.path.splitext(os.path.basename(p))[0]
    t = unreal.AssetImportTask()
    t.filename = p
    t.destination_path = DIR
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    AT.import_asset_tasks([t])
    tx = unreal.load_asset(DIR + "/" + name)
    if not tx:
        unreal.log_error("MEADOWCRATER: import failed " + name)
        continue
    if name.endswith("Normal"):
        tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tx.set_editor_property("srgb", False)
    elif name.endswith("Emissive"):
        tx.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tx)
    log("imported %s" % name)
log("DONE")
