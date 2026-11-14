# Nov 6 b163 (Claude): imports the League Training Court pieces (art\ancient_meadows\grok_design\blender\arena\pieces_court, build_court.py)
# into /Game/Env/LeagueCourt. Ring, curb, stands, scoreboard and banners get complex-as-simple collision; the rest none.
import os, unreal
SRC = r"C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_court"
DEST = "/Game/Env/LeagueCourt"
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary
COL = ("SM_Arena_Court_Ring", "SM_Arena_Court_Curb", "SM_Arena_Court_Stands", "SM_Arena_Court_Scoreboard", "SM_Arena_Court_Banners")
def log(s): unreal.log("[ZoogiCourt] " + s)
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".fbx"): continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True); ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", True); ui.set_editor_property("import_textures", True); ui.set_editor_property("import_animations", False)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True); sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False); sm.set_editor_property("remove_degenerates", True)
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, f); t.destination_path = DEST; t.destination_name = os.path.splitext(f)[0]
    t.replace_existing = True; t.automated = True; t.save = True; t.options = ui; tasks.append(t)
AT.import_asset_tasks(tasks)
for t in tasks:
    name = os.path.splitext(os.path.basename(t.filename))[0]
    m = unreal.load_asset(DEST + "/" + name)
    if not isinstance(m, unreal.StaticMesh): log("%s FAILED" % name); continue
    bs = m.get_editor_property("body_setup")
    if bs: bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if name in COL else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    b = m.get_bounds(); log("%s tris %d extent (%.0f,%.0f,%.0f)" % (name, m.get_num_triangles(0), b.box_extent.x, b.box_extent.y, b.box_extent.z))
    EAL.save_loaded_asset(m)
log("done")
