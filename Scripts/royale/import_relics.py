# Oct 31 (Evan: Battle Royale bots "leave relics around for pickup to use as weapon"): import the 4 Tripo relic models
# (C:\dev\art\relics\<name>\*.fbx, from Downloads\*3d model.zip) as static meshes into /Game/Relics/<Name>/.
# Headless: UnrealEditor-Cmd <p5style>\ZoogiRoll.uproject -run=pythonscript -script=<this> -unattended -NullRHI
import unreal, os, glob
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SRC = r"C:\dev\art\relics"
NAMES = {"sword": "SM_RelicSword", "staff": "SM_RelicStaff", "lantern": "SM_RelicLantern", "emblem": "SM_RelicEmblem"}
def log(m): unreal.log("RELICS: " + m)
for key, asset in NAMES.items():
    fbx = glob.glob(os.path.join(SRC, key, "*.fbx"))
    if not fbx: unreal.log_error("RELICS: no fbx for " + key); continue
    dest = "/Game/Relics/" + key.capitalize()
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("import_materials", True)
    ui.set_editor_property("import_textures", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    t = unreal.AssetImportTask()
    t.filename = fbx[0]; t.destination_path = dest; t.destination_name = asset
    t.automated = True; t.replace_existing = True; t.save = True; t.options = ui
    AT.import_asset_tasks([t])
    m = unreal.load_asset(dest + "/" + asset)
    log("%s -> %s %s" % (key, dest + "/" + asset, "OK" if m else "FAILED"))
    if m:
        b = m.get_bounds()
        log("  bounds extent %s" % str(b.box_extent))
    for p in EAL.list_assets(dest, recursive=True):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.Texture2D):
            a.set_editor_property("max_texture_size", 1024)  # mobile: the Tripo maps are 4K
            EAL.save_loaded_asset(a); log("  tex %s -> max 1024" % p)
log("DONE")
