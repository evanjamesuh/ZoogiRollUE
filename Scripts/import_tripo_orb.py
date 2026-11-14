# -*- coding: utf-8 -*-
"""Overwrite /Game/Orbs/SM_Orb with Evan Tripo grey/blue stone GLB (UE 5.6 Interchange)."""
import os, json, unreal

SRC = r"C:\Users\skybi\Desktop\zoogi-package-show\orb_in_game_scaled.glb"
DEST = "/Game/Orbs"
WANT = "/Game/Orbs/SM_Orb"
TAG = "TRIPORB"
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
rep = {"errors": [], "imported_paths": [], "before": {}, "after": {}}

def log(m):
    unreal.log_warning("%s: %s" % (TAG, m))

def err(m):
    rep["errors"].append(m)
    unreal.log_warning("%s: ERROR %s" % (TAG, m))

def sp(o, k, v, t):
    try:
        o.set_editor_property(k, v)
        return True
    except Exception as ex:
        err("%s.%s: %s" % (t, k, ex))
        return False

def mesh_info(sm):
    if not sm:
        return None
    try:
        b = sm.get_bounds()
        extent = [round(b.box_extent.x, 2), round(b.box_extent.y, 2), round(b.box_extent.z, 2)]
        origin = [round(b.origin.x, 2), round(b.origin.y, 2), round(b.origin.z, 2)]
        radius = round(float(b.sphere_radius), 2)
    except Exception as ex:
        extent, origin, radius = None, None, None
        err("bounds: %s" % ex)
    try:
        tris = int(sm.get_num_triangles(0))
    except Exception:
        tris = None
    try:
        verts = int(sm.get_num_vertices(0))
    except Exception:
        verts = None
    slots = []
    try:
        for s in sm.get_editor_property("static_materials"):
            sn = str(s.get_editor_property("material_slot_name"))
            mi = s.get_editor_property("material_interface")
            slots.append({"slot": sn, "mat": mi.get_path_name() if mi else None})
    except Exception as ex:
        err("slots: %s" % ex)
    return {
        "path": sm.get_path_name(),
        "tris": tris,
        "verts": verts,
        "sphere_radius": radius,
        "origin": origin,
        "box_extent": extent,
        "slots": slots,
    }

def pipe():
    p = unreal.InterchangeGenericAssetsPipeline()
    sp(p, "use_source_name_for_asset", True, "p")
    sp(p, "scene_name_sub_folder", False, "p")
    try:
        sp(p, "asset_type_sub_folders", False, "p")
    except Exception:
        pass
    c = p.get_editor_property("common_meshes_properties")
    m = p.get_editor_property("mesh_pipeline")
    mp = p.get_editor_property("material_pipeline")
    sp(c, "force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH, "c")
    sp(c, "import_lods", False, "c")
    sp(c, "recompute_normals", False, "c")
    sp(c, "recompute_tangents", True, "c")
    sp(c, "use_mikk_t_space", True, "c")
    sp(m, "import_static_meshes", True, "m")
    sp(m, "import_skeletal_meshes", False, "m")
    sp(m, "combine_static_meshes", True, "m")
    sp(m, "build_nanite", False, "m")
    # Gameplay collision is a sphere on AZoogiOrb; visual mesh only.
    sp(m, "collision", False, "m")
    sp(m, "generate_lightmap_u_vs", False, "m")
    try:
        sp(m, "fallback_collision_type", unreal.InterchangeMeshCollision.NONE, "m")
    except Exception:
        pass
    sp(mp, "import_materials", True, "mp")
    try:
        sp(mp, "search_location", unreal.InterchangeMaterialSearchLocation.LOCAL, "mp")
    except Exception as ex:
        err("search enum: %s" % ex)
    try:
        tp = mp.get_editor_property("texture_pipeline")
        if tp:
            sp(tp, "import_textures", True, "t")
    except Exception as ex:
        err("tex: %s" % ex)
    return p

if EAL.does_asset_exist(WANT):
    old = unreal.load_asset(WANT)
    if isinstance(old, unreal.StaticMesh):
        rep["before"] = mesh_info(old)
        log("BEFORE %s" % json.dumps(rep["before"]))
else:
    log("no existing SM_Orb")

if not os.path.isfile(SRC):
    err("missing source " + SRC)
else:
    src_size = os.path.getsize(SRC)
    log("SRC size=%d path=%s" % (src_size, SRC))
    if not EAL.does_directory_exist(DEST):
        EAL.make_directory(DEST)

    stage = DEST + "/_TripoImport"
    if EAL.does_directory_exist(stage):
        EAL.delete_directory(stage)
    EAL.make_directory(stage)

    k = unreal.AssetImportTask()
    k.filename = SRC
    k.destination_path = stage
    k.automated = True
    k.replace_existing = True
    k.save = False
    k.options = pipe()
    AT.import_asset_tasks([k])
    paths = list(k.imported_object_paths or [])
    rep["imported_paths"] = paths
    log("imported %d paths" % len(paths))
    for pth in paths[:50]:
        log("  " + str(pth))

    sms = []
    mats = []
    texs = []
    for pth in paths:
        a = unreal.load_asset(pth)
        if a is None:
            continue
        name = a.get_name()
        cls = a.get_class().get_name()
        if isinstance(a, unreal.StaticMesh):
            if name.startswith(("UCX_", "UBX_", "UCP_", "USP_")):
                continue
            sms.append(a)
        elif "Material" in cls:
            mats.append(pth)
        elif "Texture" in cls:
            texs.append(pth)
    rep["materials"] = mats
    rep["textures"] = texs

    if not sms:
        err("no static mesh imported")
    else:
        best = None
        best_tris = -1
        for sm in sms:
            try:
                t = int(sm.get_num_triangles(0))
            except Exception:
                t = 0
            if t > best_tris:
                best_tris = t
                best = sm
        log("primary candidates=%d best_tris=%d name=%s" % (
            len(sms), best_tris, best.get_name() if best else None))

        if EAL.does_asset_exist(WANT):
            ok_del = EAL.delete_asset(WANT)
            log("deleted old SM_Orb ok=%s" % ok_del)

        src_path = best.get_path_name().split(".")[0]
        if src_path != WANT:
            ok = EAL.rename_asset(src_path, WANT)
            log("rename %s -> %s ok=%s" % (src_path, WANT, ok))
            if not ok and EAL.does_asset_exist(src_path):
                try:
                    EAL.duplicate_asset("SM_Orb", DEST, src_path)
                    log("duplicated to SM_Orb")
                except Exception as ex:
                    err("dup: %s" % ex)

        sm = unreal.load_asset(WANT) if EAL.does_asset_exist(WANT) else None
        if not isinstance(sm, unreal.StaticMesh):
            err("SM_Orb missing after rename")
        else:
            info = mesh_info(sm)
            rep["after"] = info
            log("AFTER %s" % json.dumps(info))
            tris = info.get("tris") or 0
            rad = info.get("sphere_radius") or 0
            if tris < 100000:
                err("tris too low for Tripo stone (%s) — still monkey/old?" % tris)
            if rad < 20 or rad > 80:
                err("radius unexpected (%s) — expected ~40" % rad)
            EAL.save_loaded_asset(sm)

    EAL.save_directory(DEST, only_if_is_dirty=False, recursive=True)

out = os.path.join(unreal.Paths.project_saved_dir(), "tripo_orb_import.json")
json.dump(rep, open(out, "w"), indent=1)
after = rep.get("after") or {}
ok_flag = 0
if after.get("tris", 0) and after.get("tris", 0) > 100000 and 20 <= (after.get("sphere_radius") or 0) <= 80:
    ok_flag = 1
log("TRIPORB_DONE ok=%d tris=%s radius=%s errors=%d" % (
    ok_flag, after.get("tris"), after.get("sphere_radius"), len(rep["errors"])))
