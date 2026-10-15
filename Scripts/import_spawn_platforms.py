# Spawn elevator (Oct 14): import each arena pack's spawn-platform parts into /Game/Env/SpawnPlatform/<Map>/<Part>.
import unreal, os
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt/SpawnPlatform")
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
for m in sorted(os.listdir(SRC)):
    d = os.path.join(SRC, m)
    if not os.path.isdir(d): continue
    for f in sorted(os.listdir(d)):
        if not f.endswith(".glb"): continue
        dest = f"/Game/Env/SpawnPlatform/{m}/{f[:-4]}"
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(d, f)); t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
        at.import_asset_tasks([t])
        for p in t.get_editor_property("imported_object_paths"): unreal.log(f"SPIMPORT {m}/{f} -> {p}")
eal.save_directory("/Game/Env/SpawnPlatform", only_if_is_dirty=False, recursive=True)
for p in eal.list_assets("/Game/Env/SpawnPlatform", recursive=True):
    a = eal.find_asset_data(p)
    if str(a.asset_class_path.asset_name) == "StaticMesh":
        sm = eal.load_asset(p); b = sm.get_bounding_box()
        unreal.log(f"SPMESH {p} min={b.min} max={b.max} mats={[str(s.material_slot_name) for s in sm.static_materials]}")
unreal.log("SPIMPORT done")
