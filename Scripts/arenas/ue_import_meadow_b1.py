# r23: import Meadow Arena Builder batch 1 (glb) to /Game/Env/MeadowBatch1 (combined meshes; materials set in C++ by slot)
import unreal, os
SRC = r"C:/dev/ZoogiRollUnreal/SourceArt/MeadowBatch1"
DEST = "/Game/Env/MeadowBatch1"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".glb"): continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, f)); t.set_editor_property("destination_path", DEST + "/" + f[:-4])
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
    at.import_asset_tasks([t])
    for p in t.get_editor_property("imported_object_paths"):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            b = a.get_bounding_box()
            slots = [str(s.get_editor_property("material_slot_name")) for s in a.get_editor_property("static_materials")]
            unreal.log(f"MB1 {p} min=({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f}) max=({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f}) slots={slots}")
eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
unreal.log("MB1 saved")
