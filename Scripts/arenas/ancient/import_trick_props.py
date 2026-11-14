# b157: imports SM_Arena_PlazaPropsTrick (the plaza props without the central drum ring) next to the v14 pieces, same settings as
# import_v14.py (complex-as-simple collision, materials from the FBX reuse the existing v14 materials by name).
import os, unreal
SRC = r"C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_v14_trick\SM_Arena_PlazaPropsTrick.fbx"
DEST = "/Game/Env/AncientV14"
def log(s): unreal.log("[ZoogiTrick] " + s)
ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", False)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
ui.set_editor_property("import_materials", True)
ui.set_editor_property("import_textures", True)
ui.set_editor_property("import_animations", False)
sm = ui.get_editor_property("static_mesh_import_data")
sm.set_editor_property("combine_meshes", True)
sm.set_editor_property("auto_generate_collision", False)
sm.set_editor_property("generate_lightmap_u_vs", False)
sm.set_editor_property("remove_degenerates", True)
t = unreal.AssetImportTask()
t.filename = SRC; t.destination_path = DEST; t.destination_name = "SM_Arena_PlazaPropsTrick"
t.replace_existing = True; t.automated = True; t.save = True; t.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
m = unreal.load_asset(DEST + "/SM_Arena_PlazaPropsTrick")
o = unreal.load_asset(DEST + "/SM_Arena_PlazaProps")
if isinstance(m, unreal.StaticMesh):
    bs = m.get_editor_property("body_setup")
    if bs: bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    b = m.get_bounds(); ob = o.get_bounds() if o else None
    log("TRICK tris %d (original %d), mats %d, origin (%.0f,%.0f,%.0f) vs original (%.0f,%.0f,%.0f)" % (m.get_num_triangles(0), o.get_num_triangles(0) if o else -1,
        len(m.get_editor_property("static_materials")), b.origin.x, b.origin.y, b.origin.z, ob.origin.x if ob else 0, ob.origin.y if ob else 0, ob.origin.z if ob else 0))
    for s in m.get_editor_property("static_materials"):
        mi = s.get_editor_property("material_interface")
        log("slot %s -> %s" % (s.get_editor_property("material_slot_name"), mi.get_path_name() if mi else "None"))
else:
    log("FAILED %s" % list(t.imported_object_paths))
log("done")
try: unreal.SystemLibrary.quit_editor()
except Exception: pass
