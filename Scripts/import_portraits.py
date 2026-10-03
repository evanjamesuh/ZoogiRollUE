# Imports the menu portraits (Art/Portraits/T_Portrait_<id>.png, captured in-game with -ZoogiNoHUD on the
# Draw hero shot and cropped) into /Game/UI/Portraits as UI textures.
import os
import unreal

src = os.path.join(unreal.Paths.project_dir(), "Art", "Portraits")
tasks = []
for f in sorted(os.listdir(src)):
    if not f.lower().endswith(".png"):
        continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(src, f)
    t.destination_path = "/Game/UI/Portraits"
    t.destination_name = os.path.splitext(f)[0]
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        tex = unreal.EditorAssetLibrary.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_asset(p, only_if_is_dirty=False)
        unreal.log("import_portraits: " + p)
