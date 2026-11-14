"""Oct 23: import 0.1.2 blue orbs (Lars character orb = original mesh+texture; Glow = recreation from 0.1.2 glowBall/CFX2_Glow textures tinted deco blue).
Assets only, NOT placed; Tripo arena orb untouched."""
"""Import two Zoogi Roll 0.1.2 legacy GLBs as static meshes (Oct 23).
  Import/Legacy012/ship_LerpzShip.glb -> /Game/Legacy012/SM_LegacyShip   (native 0.1.2 size, ~12.7 m)
  Import/Legacy012/bumper_star.glb    -> /Game/Legacy012/SM_StarBumper   (native, ~5.8 m wide)
GLB units are 0.1.2 Unity metres (Interchange x100 -> cm). Game scale (0.25) is applied at placement in ZoogiArenaLegacy012.cpp.
Run with unreal.lock held:  .\\Scripts\\run_python_here.ps1 -Script Scripts\\import_legacy012.py -LogName legacy012-import
"""
import os, json, unreal
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/Legacy012"
ITEMS = [("orb_blue_Lars.glb", "SM_OrbBlue_Lars", 1.0), ("orb_blue_Glow.glb", "SM_OrbBlue_Glow", 1.0)]
rep = {}
import sys
ONLY = os.environ.get("LEG012_ONLY", "")
if ONLY:
    ITEMS = [i for i in ITEMS if i[1] == ONLY]
    for a in ("SM_OrbBlue_Glow", "OrbBlue_Core", "OrbBlue_Shell", "M_OrbBlue_Core", "M_OrbBlue_Shell", "OrbBlue_core_cyan", "OrbBlue_shell_cyan", "glowBall_60", "glowBall_601", "CFX2_T_Glow_70", "CFX2_T_Glow_701"):
        if EAL.does_asset_exist(DEST + "/" + a): EAL.delete_asset(DEST + "/" + a)
    if EAL.does_directory_exist(DEST + "/_stage_SM_OrbBlue_Glow"): EAL.delete_directory(DEST + "/_stage_SM_OrbBlue_Glow")

def pipe(scale):
    p = unreal.InterchangeGenericAssetsPipeline()
    p.set_editor_property("use_source_name_for_asset", True)
    for k in ("scene_name_sub_folder", "asset_type_sub_folders"):
        try: p.set_editor_property(k, False)
        except Exception: pass
    try: p.set_editor_property("import_offset_uniform_scale", scale)
    except Exception as ex: unreal.log_warning("LEG012: scale prop %s" % ex)
    c = p.get_editor_property("common_meshes_properties")
    m = p.get_editor_property("mesh_pipeline")
    mp = p.get_editor_property("material_pipeline")
    c.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    c.set_editor_property("recompute_normals", False)
    m.set_editor_property("import_static_meshes", True)
    m.set_editor_property("import_skeletal_meshes", False)
    m.set_editor_property("combine_static_meshes", True)
    m.set_editor_property("build_nanite", False)
    m.set_editor_property("collision", True)
    try: m.set_editor_property("fallback_collision_type", unreal.InterchangeMeshCollision.BOX)
    except Exception: pass
    mp.set_editor_property("import_materials", True)
    return p

for src, name, scale in ITEMS:
    path = os.path.join(unreal.Paths.project_dir(), "Import/Legacy012", src)
    want = DEST + "/" + name
    stage = DEST + "/_stage_" + name
    if EAL.does_directory_exist(stage): EAL.delete_directory(stage)
    EAL.make_directory(stage)
    t = unreal.AssetImportTask()
    t.filename = path; t.destination_path = stage; t.automated = True; t.replace_existing = True; t.save = False
    t.options = pipe(scale)
    AT.import_asset_tasks([t])
    paths = list(t.imported_object_paths or [])
    r = {"imported": paths}
    sm = None; other = []
    for pth in paths:
        a = unreal.load_asset(pth)
        if isinstance(a, unreal.StaticMesh) and (sm is None or a.get_num_triangles(0) > sm.get_num_triangles(0)): sm = a
        elif a is not None: other.append(a)
    if sm is None:
        r["error"] = "no static mesh"; rep[name] = r; continue
    if EAL.does_asset_exist(want): EAL.delete_asset(want)
    EAL.rename_asset(sm.get_path_name().split(".")[0], want)
    # move materials/textures beside the mesh
    for a in other:
        p0 = a.get_path_name().split(".")[0]
        nn = DEST + "/" + a.get_name()
        if EAL.does_asset_exist(nn): EAL.delete_asset(nn)
        EAL.rename_asset(p0, nn)
    sm = unreal.load_asset(want)
    b = sm.get_bounds()
    r.update({"path": sm.get_path_name(), "tris": sm.get_num_triangles(0),
              "origin": [b.origin.x, b.origin.y, b.origin.z], "extent": [b.box_extent.x, b.box_extent.y, b.box_extent.z],
              "mats": [str(s.get_editor_property("material_interface").get_path_name()) if s.get_editor_property("material_interface") else None for s in sm.get_editor_property("static_materials")],
              "others": [a.get_path_name() for a in other]})
    EAL.save_loaded_asset(sm)
    rep[name] = r
EAL.save_directory(DEST, only_if_is_dirty=False, recursive=True)
for d in EAL.list_assets(DEST, recursive=True, include_folder=True):
    pass
for name in [i[1] for i in ITEMS]:
    st = DEST + "/_stage_" + name
    if EAL.does_directory_exist(st) and not EAL.list_assets(st): EAL.delete_directory(st)
json.dump(rep, open(os.path.join(unreal.Paths.project_saved_dir(), "legacy012_orbs_import.json"), "w"), indent=1)
unreal.log_warning("LEG012_ORBS_DONE " + json.dumps(rep))




