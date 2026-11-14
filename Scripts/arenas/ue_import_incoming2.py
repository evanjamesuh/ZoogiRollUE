# Import the other maps' reskin meshes (intact) into /Game/Env/Incoming/<map>. Materials assigned in C++ by slot name.
import unreal, os
SRC = r"C:/dev/ZoogiRollUnreal/SourceArt/Incoming2"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
for m in sorted(os.listdir(SRC)):
    d = os.path.join(SRC, m)
    if not os.path.isdir(d): continue
    for f in sorted(os.listdir(d)):
        if not f.endswith(".glb"): continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(d, f)); t.set_editor_property("destination_path", f"/Game/Env/Incoming/{m}/{f[:-4]}")
        t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
        at.import_asset_tasks([t])
        n = len([p for p in t.get_editor_property("imported_object_paths")])
        unreal.log(f"INC {m}/{f} -> {n} objects")
eal.save_directory("/Game/Env/Incoming", only_if_is_dirty=False, recursive=True)
unreal.log("INC2 saved")
