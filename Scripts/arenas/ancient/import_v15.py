# Nov 6 b162 (Claude): imports the v15 (2x plaza ring) arena pieces; was: v14 arena pieces (C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_v15\*.fbx)
# into /Game/Env/AncientV15 (always cooked via /Game/Env). Walk/bump pieces get complex-as-simple collision; water, nature and the
# gate portal swirl get none (arena_report_v14.txt). Materials come from the FBX (Blender colours); emissive ones are boosted.
import os, unreal
SRC = r"C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_v15"
DEST = "/Game/Env/AncientV15"
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
NOCOL = ("SM_Arena_Water", "SM_Arena_Nature", "SM_Arena_GatePortal")
def log(s): unreal.log("[ZoogiV15] " + s)
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".fbx"): continue
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
    t.filename = os.path.join(SRC, f); t.destination_path = DEST; t.destination_name = os.path.splitext(f)[0]
    t.replace_existing = True; t.automated = True; t.save = True; t.options = ui
    tasks.append(t)
AT.import_asset_tasks(tasks)
for t in tasks:
    name = os.path.splitext(os.path.basename(t.filename))[0]
    m = unreal.load_asset(DEST + "/" + name)
    if not isinstance(m, unreal.StaticMesh):
        log("%s FAILED %s" % (name, list(t.imported_object_paths))); continue
    bs = m.get_editor_property("body_setup")
    if bs:
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if name not in NOCOL else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    b = m.get_bounds()
    log("%s tris %d mats %d bounds origin (%.0f,%.0f,%.0f) extent (%.0f,%.0f,%.0f)" % (name, m.get_num_triangles(0), len(m.get_editor_property("static_materials")),
        b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))
    EAL.save_loaded_asset(m)
# materials: list them (name + type) so emissive ones can be tuned
for a in EAL.list_assets(DEST, recursive=True):
    o = unreal.load_asset(a)
    if isinstance(o, (unreal.Material, unreal.MaterialInstance)): log("material %s (%s)" % (o.get_name(), type(o).__name__))
log("done")
