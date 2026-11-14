# Meadow r4 (Oct 7): re-imports the bold floor seal mask SourceArt/Arenas/MeadowSeal/T_MeadowGlow.png (thick core lines +
# soft halo, rebaked from the Buzzy seal) over /Game/Env/Arenas/T_MeadowGlow (grayscale linear clamp, like make_arena_kit).
# Headless: Scripts\run_python.ps1 Scripts\arenas\make_meadow_r4.py meadowr4
import unreal, os
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
p = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "MeadowSeal", "T_MeadowGlow.png")
t = unreal.AssetImportTask()
t.filename = p; t.destination_path = DIR; t.destination_name = "T_MeadowGlow"
t.automated = True; t.replace_existing = True; t.save = False
AT.import_asset_tasks([t])
tx = unreal.load_asset(DIR + "/T_MeadowGlow")
if not tx:
    unreal.log_error("MEADOWR4: import failed T_MeadowGlow")
else:
    tx.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tx.set_editor_property("never_stream", True)
    tx.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    tx.set_editor_property("srgb", False)
    tx.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP); tx.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    EAL.save_loaded_asset(tx)
    unreal.log("MEADOWR4: imported T_MeadowGlow")
unreal.log("MEADOWR4: DONE")
