# Nov 7 b171e (Claude): Evan "take the ring and the stands from the new meadows fbx and apply it to our existing meadow hollow arena".
# Imports Grok's Meadow reference v3 rim + pavilion stands + Zoogi crowds (art\meadow_reference_build\grok\pieces) into /Game/Env/MeadowRef.
import os, unreal
SRC = r"C:\dev\art\meadow_reference_build\grok\pieces"
DEST = "/Game/Env/MeadowRef"
NAMES = ("SM_MeadowRef_Rim", "SM_MeadowRef_SpectatorStands_Left", "SM_MeadowRef_SpectatorStands_Right", "SM_MeadowRef_ZoogiSpectators_Left", "SM_MeadowRef_ZoogiSpectators_Right")
COL = ("SM_MeadowRef_SpectatorStands_Left", "SM_MeadowRef_SpectatorStands_Right")
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary
def log(s): unreal.log("[ZoogiMeadowRef] " + s)
tasks = []
for n in NAMES:
    f = os.path.join(SRC, n + ".fbx")
    if not os.path.exists(f): log("%s MISSING" % f); continue
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True); ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.set_editor_property("import_materials", True); ui.set_editor_property("import_textures", True); ui.set_editor_property("import_animations", False)
    sm = ui.get_editor_property("static_mesh_import_data")
    sm.set_editor_property("combine_meshes", True); sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False); sm.set_editor_property("remove_degenerates", True)
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = DEST; t.destination_name = n
    t.replace_existing = True; t.automated = True; t.save = True; t.options = ui; tasks.append(t)
AT.import_asset_tasks(tasks)
for n in NAMES:
    m = unreal.load_asset(DEST + "/" + n)
    if not isinstance(m, unreal.StaticMesh): log("%s FAILED" % n); continue
    bs = m.get_editor_property("body_setup")
    if bs: bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if n in COL else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    b = m.get_bounds(); log("%s tris %d mats %d origin (%.0f,%.0f,%.0f) extent (%.0f,%.0f,%.0f)" % (n, m.get_num_triangles(0), len(m.static_materials), b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))
    for sl in m.static_materials: log("  slot %s -> %s" % (sl.material_slot_name, sl.material_interface.get_path_name() if sl.material_interface else "None"))
    EAL.save_loaded_asset(m)
log("done")
