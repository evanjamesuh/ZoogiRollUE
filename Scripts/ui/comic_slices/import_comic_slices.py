# Oct 9 comic slices: imports SourceArt/UI/ComicSlices/*.png -> /Game/UI/ComicSlices/T_<name> as UI textures
# (TEXTUREGROUP_UI, sRGB, alpha kept, BC7, no mips, never stream, clamp). PNGs come from cut_slices.py.
import unreal, os, glob
AT = unreal.AssetToolsHelpers.get_asset_tools()
TA = unreal.TextureAddress
DIR = "/Game/UI/ComicSlices"
src = os.path.join(unreal.Paths.project_dir(), "SourceArt", "UI", "ComicSlices")
n = 0
for p in sorted(glob.glob(os.path.join(src, "*.png"))):
    name = "T_" + os.path.splitext(os.path.basename(p))[0]
    task = unreal.AssetImportTask()
    task.filename = p; task.destination_path = DIR; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    AT.import_asset_tasks([task])
    t = unreal.load_asset(DIR + "/" + name)
    if not t:
        unreal.log_error("COMICIMPORT: failed " + name); continue
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    t.set_editor_property("never_stream", True)
    t.set_editor_property("srgb", True)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property("address_x", TA.TA_CLAMP); t.set_editor_property("address_y", TA.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    n += 1
unreal.log("COMICIMPORT: DONE %d" % n)
