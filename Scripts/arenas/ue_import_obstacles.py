# Oct 14 (arenas-import): import the Meadow batch 6 + Night Circuit rev 18.1 obstacle GLBs into /Game/Env/Obstacles/<map>/<name>.
# UCX_* parts become invisible collision meshes (one exact convex hull each, from the hull itself); TRIGGER_* parts are skipped in C++.
import unreal, os, json
SRC = r"C:/dev/ZoogiRollUnreal/SourceArt/Obstacles"
OUT = r"C:/dev/ZoogiRollUnreal/Saved/obstacles_import_" + (os.environ.get("OBS_MAPS", "all").replace(",", "_")) + ".json"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
rows = []
ONLY = [x for x in os.environ.get("OBS_MAPS", "").split(",") if x]
for m in sorted(os.listdir(SRC)):
    if ONLY and m not in ONLY: continue
    d = os.path.join(SRC, m)
    if not os.path.isdir(d): continue
    for f in sorted(os.listdir(d)):
        if not f.endswith(".glb"): continue
        dest = f"/Game/Env/Obstacles/{m}/{f[:-4]}"
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(d, f)); t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
        at.import_asset_tasks([t])
        for p in eal.list_assets(dest, recursive=True):
            a = eal.load_asset(p)
            if not isinstance(a, unreal.StaticMesh): continue
            n = a.get_name()
            if n.upper().startswith("UCX_"):
                sms.remove_collisions(a)
                sms.set_convex_decomposition_collisions(a, 1, 32, 100000)
            b = a.get_bounding_box()
            rows.append({"map": m, "file": f[:-4], "path": a.get_path_name(), "name": n,
                         "min": [b.min.x, b.min.y, b.min.z], "max": [b.max.x, b.max.y, b.max.z],
                         "convex": sms.get_convex_collision_count(a), "simple": sms.get_simple_collision_count(a),
                         "slots": [str(s.material_slot_name) for s in a.static_materials]})
        unreal.log(f"OBS {m}/{f}")
eal.save_directory("/Game/Env/Obstacles", only_if_is_dirty=False, recursive=True)
json.dump(rows, open(OUT, "w"), indent=1)
unreal.log(f"OBS saved {len(rows)} meshes")


