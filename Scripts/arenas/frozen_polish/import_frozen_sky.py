# Frozen polish r6: re-imports only SourceArt/Arenas/FrozenPolish/T_FrozenBrightSky.png (same settings as import_frozen_polish.py).
import unreal, os
AT = unreal.AssetToolsHelpers.get_asset_tools()
TA = unreal.TextureAddress
DIR = "/Game/Env/Arenas"; name = "T_FrozenBrightSky"
p = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Arenas", "FrozenPolish", name + ".png")
task = unreal.AssetImportTask()
task.filename = p; task.destination_path = DIR; task.destination_name = name
task.automated = True; task.replace_existing = True; task.save = False
AT.import_asset_tasks([task])
t = unreal.load_asset(DIR + "/" + name)
if not t:
    unreal.log_error("FROZENSKY: import failed")
else:
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    t.set_editor_property("never_stream", True)
    t.set_editor_property("srgb", True)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property("address_x", TA.TA_WRAP); t.set_editor_property("address_y", TA.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    unreal.log("FROZENSKY: DONE")
