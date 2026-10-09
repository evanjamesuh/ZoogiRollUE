# Frozen Ring polish (Oct 8): imports SourceArt/Arenas/FrozenPolish/T_*.png into /Game/Env/Arenas.
#   T_FrozenBrightSky  equirect sky panorama (M_ArenaSky via AddPanoramaSky("FrozenBright"))
#   T_FrozenSealHalo   RGBA glowing rune-seal overlay for the court (M_FrozenDecal FlatQuad, UV = WorldXY / 3100 + 0.5)
# Headless, run against the checkout this file lives in:
#   UnrealEditor-Cmd.exe <worktree>\ZoogiRoll.uproject -run=pythonscript -script=<this file> -unattended -nosplash -NullRHI
import unreal, os, glob
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
TA = unreal.TextureAddress
TCS = unreal.TextureCompressionSettings
DIR = "/Game/Env/Arenas"
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "FrozenPolish")
for p in sorted(glob.glob(os.path.join(SRC, "T_*.png"))):
    name = os.path.splitext(os.path.basename(p))[0]
    task = unreal.AssetImportTask()
    task.filename = p; task.destination_path = DIR; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    AT.import_asset_tasks([task])
    t = unreal.load_asset(DIR + "/" + name)
    if not t:
        unreal.log_error("FROZENPOLISH: import failed " + name); continue
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("never_stream", True)
    t.set_editor_property("srgb", True)
    if name.endswith("Sky"):
        t.set_editor_property("compression_settings", TCS.TC_BC7)
        t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_CLAMP)
    else:
        t.set_editor_property("compression_settings", TCS.TC_DEFAULT)
        t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    EAL.save_loaded_asset(t)
    unreal.log("FROZENPOLISH: imported " + name)
unreal.log("FROZENPOLISH: DONE")
