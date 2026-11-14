# b178c (Claude, Evan Nov 7 19:22: "make this the new meadows and new underworld realm for episode 4 lvl 1"): imports the rest of Grok's
# Meadow v3 (art\meadow_reference_build\grok\pieces -> /Game/Env/MeadowRef) and the Shadow Realm v3 underworld
# (art\shadow_realm\grok\pieces -> /Game/Env/ShadowRealm). Decor has no collision; the six Z rune rocks collide (complex as simple).
import os, unreal
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary
def log(s): unreal.log("[ZoogiArenaV3] " + s)
JOBS = [
 (r"C:\dev\art\meadow_reference_build\grok\pieces", "/Game/Env/MeadowRef",
  ["SM_MeadowRef_Floor", "SM_MeadowRef_CentreMedallion", "SM_MeadowRef_Circuits", "SM_MeadowRef_FrontArches", "SM_MeadowRef_RearGate",
   "SM_MeadowRef_Crystals", "SM_MeadowRef_Banners", "SM_MeadowRef_Flowers", "SM_MeadowRef_Trees", "SM_MeadowRef_Landscape",
   "SM_MeadowRef_Mountains", "SM_MeadowRef_Village", "SM_MeadowRef_Water", "SM_MeadowRef_SkyDetails", "SM_MeadowRef_LogsStumps"]
  + ["SM_MeadowRef_RuneRock_%02d" % i for i in range(1, 7)]),
 (r"C:\dev\art\shadow_realm\grok\pieces", "/Game/Env/ShadowRealm",
  ["SM_Shadow_Floor", "SM_Shadow_Rim", "SM_Shadow_Foundation", "SM_Shadow_Rocks", "SM_Shadow_Trim", "SM_Shadow_Energy",
   "SM_Shadow_Environment", "SM_Shadow_Architecture", "SM_Shadow_Ghostfire", "SM_Shadow_Emblem", "SM_Shadow_Void", "SM_Shadow_Mist"]),
]
for SRC, DEST, NAMES in JOBS:
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
        if bs: bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if "RuneRock" in n else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        b = m.get_bounds(); log("%s tris %d mats %d origin (%.0f,%.0f,%.0f) extent (%.0f,%.0f,%.0f)" % (n, m.get_num_triangles(0), len(m.static_materials), b.origin.x, b.origin.y, b.origin.z, b.box_extent.x, b.box_extent.y, b.box_extent.z))
        EAL.save_loaded_asset(m)
log("done")
