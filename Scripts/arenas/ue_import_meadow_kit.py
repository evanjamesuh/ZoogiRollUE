# r22: import A&P Meadow kit meshes (intact) to /Game/Env/MeadowBuildings; materials are assigned in C++ by slot name.
import unreal, os
SRC = r"C:/dev/ZoogiRollUnreal/SourceArt/MeadowBuildings"
DEST = "/Game/Env/MeadowBuildings"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".fbx"): continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, f)); t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
    o = unreal.FbxImportUI(); o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = o.get_editor_property("static_mesh_import_data"); sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False); sm.set_editor_property("generate_lightmap_u_vs", True)
    t.set_editor_property("options", o); at.import_asset_tasks([t])
    for p in t.get_editor_property("imported_object_paths"):
        m = eal.load_asset(p)
        if isinstance(m, unreal.StaticMesh):
            b = m.get_bounding_box()
            slots = [str(s.get_editor_property("material_slot_name")) for s in m.get_editor_property("static_materials")]
            unreal.log(f"MEADOWKIT {m.get_name()} min={b.min} max={b.max} slots={slots}")
eal.save_directory(DEST, only_if_is_dirty=False, recursive=True)
unreal.log("MEADOWKIT saved")
