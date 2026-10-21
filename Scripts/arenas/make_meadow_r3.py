# Meadow r3 (Oct 7): imports SourceArt/Arenas/MeadowScars/T_MeadowSky.png (Buzzy sky panorama, Skies & Lighting pack) and the
# baked Meadow floor-scar decal set T_MeadowDisp_S1/S2/S3/Titan/Crater (tools/bake_disp.py from the A&P Meadow crack masks).
# Headless: Scripts\run_python.ps1 Scripts\arenas\make_meadow_r3.py meadowr3
import unreal, os, glob
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Env/Arenas"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "MeadowScars")
TA = unreal.TextureAddress
TCS = unreal.TextureCompressionSettings
for p in sorted(glob.glob(os.path.join(SRC, "T_*.png"))):
    name = os.path.splitext(os.path.basename(p))[0]
    t = unreal.AssetImportTask()
    t.filename = p; t.destination_path = DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t])
    tx = unreal.load_asset(DIR + "/" + name)
    if not tx:
        unreal.log_error("MEADOWR3: import failed " + name); continue
    tx.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tx.set_editor_property("never_stream", True)
    if name.endswith("Sky"):
        tx.set_editor_property("compression_settings", TCS.TC_BC7); tx.set_editor_property("srgb", True)
        tx.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tx.set_editor_property("address_x", TA.TA_WRAP); tx.set_editor_property("address_y", TA.TA_CLAMP)
    else:
        tx.set_editor_property("compression_settings", TCS.TC_DEFAULT); tx.set_editor_property("srgb", True)
        tx.set_editor_property("address_x", TA.TA_CLAMP); tx.set_editor_property("address_y", TA.TA_CLAMP)
    EAL.save_loaded_asset(tx)
    unreal.log("MEADOWR3: imported " + name)
unreal.log("MEADOWR3: DONE")
