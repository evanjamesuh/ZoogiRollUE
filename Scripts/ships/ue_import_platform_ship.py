"""Import VIP platform ship GLB into /Game/Ships/PlatformShip (UE 5.6 Interchange).
Preserves materials/textures from GLB as much as Interchange allows.
Creates BP_PlatformShip placeable actor. Writes Saved/ship_platform_import.json."""
import os, json, unreal

SRC = r"C:/dev/ZoogiRollUnreal-combined/Import/Ships/PlatformShip/ship_platform.glb"
DEST = "/Game/Ships/PlatformShip"
TAG = "SHIPPLAT"
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
ESML = unreal.EditorStaticMeshLibrary
rep = {"meshes": {}, "materials": [], "textures": [], "errors": [], "imported_paths": []}

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
    # Combine so one placeable SM with multiple material slots
    sp(m, "combine_static_meshes", True, "m")
    sp(m, "build_nanite", False, "m")
    sp(m, "collision", True, "m")
    sp(m, "import_collision_according_to_mesh_name", True, "m")
    sp(m, "one_convex_hull_per_ucx", True, "m")
    sp(m, "generate_lightmap_u_vs", False, "m")
    try:
        sp(m, "fallback_collision_type", unreal.InterchangeMeshCollision.MESH_COMPLEX_AS_SIMPLE, "m")
    except Exception:
        try:
            sp(m, "fallback_collision_type", unreal.InterchangeMeshCollision.NONE, "m")
        except Exception as ex:
            err("fallback_collision: %s" % ex)
    # KEEP materials + textures (PBR white/gold/glass/lights)
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

if not os.path.isfile(SRC):
    err("missing source " + SRC)
else:
    if not EAL.does_directory_exist(DEST):
        EAL.make_directory(DEST)
    k = unreal.AssetImportTask()
    k.filename = SRC
    k.destination_path = DEST
    k.automated = True
    k.replace_existing = True
    k.save = False
    k.options = pipe()
    AT.import_asset_tasks([k])
    paths = list(k.imported_object_paths or [])
    rep["imported_paths"] = paths
    log("imported %d paths" % len(paths))
    for pth in paths[:40]:
        log("  " + pth)

    sms = []
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
            rep["materials"].append(pth)
        elif "Texture" in cls:
            rep["textures"].append(pth)

    # Flatten primary mesh to DEST/SM_PlatformShip
    want = DEST + "/SM_PlatformShip"
    primary = None
    if len(sms) == 1:
        sm = sms[0]
        src_path = sm.get_path_name().split(".")[0]
        if src_path != want:
            if EAL.does_asset_exist(want):
                EAL.delete_asset(want)
            ok = EAL.rename_asset(src_path, want)
            log("flatten %s -> %s ok=%s" % (src_path, want, ok))
        primary = unreal.load_asset(want) if EAL.does_asset_exist(want) else sm
    elif len(sms) > 1:
        # Keep parts under DEST/Parts; also try to pick largest as primary name
        parts_dir = DEST + "/Parts"
        if not EAL.does_directory_exist(parts_dir):
            EAL.make_directory(parts_dir)
        best = None
        best_vol = -1.0
        for sm in sms:
            b = sm.get_bounding_box()
            dx = b.max.x - b.min.x
            dy = b.max.y - b.min.y
            dz = b.max.z - b.min.z
            vol = abs(dx * dy * dz)
            sub = sm.get_name()
            tgt = "%s/%s" % (parts_dir, sub)
            src_path = sm.get_path_name().split(".")[0]
            if src_path != tgt:
                if EAL.does_asset_exist(tgt):
                    EAL.delete_asset(tgt)
                try:
                    EAL.rename_asset(src_path, tgt)
                except Exception as ex:
                    err("rename part %s: %s" % (sub, ex))
            sm2 = unreal.load_asset(tgt) if EAL.does_asset_exist(tgt) else sm
            if vol > best_vol:
                best_vol = vol
                best = sm2
            slots = []
            for i, s in enumerate(sm2.get_editor_property("static_materials")):
                slots.append(str(s.get_editor_property("material_slot_name")))
            bb = sm2.get_bounding_box()
            rep["meshes"][sm2.get_path_name()] = {
                "slots": slots,
                "min": [round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)],
                "max": [round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1)],
            }
            EAL.save_loaded_asset(sm2)
        primary = best
        # Also duplicate/rename best to SM_PlatformShip for easy refs
        if primary:
            src_path = primary.get_path_name().split(".")[0]
            if src_path != want:
                try:
                    if EAL.does_asset_exist(want):
                        EAL.delete_asset(want)
                    # duplicate rather than move so Parts stay
                    EAL.duplicate_asset(os.path.basename(want), DEST, src_path)
                    primary = unreal.load_asset(want)
                    log("duplicated primary -> %s" % want)
                except Exception as ex:
                    err("dup primary: %s" % ex)
    else:
        err("no static meshes imported")

    if primary and isinstance(primary, unreal.StaticMesh):
        slots = []
        for i, s in enumerate(primary.get_editor_property("static_materials")):
            sn = str(s.get_editor_property("material_slot_name"))
            mi = s.get_editor_property("material_interface")
            slots.append({"slot": sn, "mat": mi.get_path_name() if mi else None})
        bb = primary.get_bounding_box()
        # UE cm: expect ~1800 x 1000 x 700
        size = [round(bb.max.x - bb.min.x, 1), round(bb.max.y - bb.min.y, 1), round(bb.max.z - bb.min.z, 1)]
        rep["primary"] = {
            "path": primary.get_path_name().split(".")[0],
            "slots": slots,
            "size_cm": size,
            "min": [round(bb.min.x, 1), round(bb.min.y, 1), round(bb.min.z, 1)],
            "max": [round(bb.max.x, 1), round(bb.max.y, 1), round(bb.max.z, 1)],
        }
        # If imported in meters (tiny ~18cm), scale up x100
        if max(size) < 50.0:
            try:
                # Build scale via import uniform scale re-import is heavy; set mesh nanite off and note
                rep["unit_warn"] = "mesh appears meter-sized in cm space; will apply 100x on actor"
                log("UNIT WARN size_cm=%s (expect ~1800)" % size)
            except Exception as ex:
                err("unit check: %s" % ex)
        EAL.save_loaded_asset(primary)

        # Create Blueprint actor BP_PlatformShip
        bp_path = DEST + "/BP_PlatformShip"
        if EAL.does_asset_exist(bp_path):
            EAL.delete_asset(bp_path)
        try:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", unreal.Actor)
            bp = AT.create_asset("BP_PlatformShip", DEST, unreal.Blueprint, factory)
            if bp:
                # Add static mesh component via SubobjectDataSubsystem is complex in Py;
                # instead stamp a simple note — placement script uses StaticMeshActor.
                EAL.save_loaded_asset(bp)
                rep["blueprint"] = bp_path
                log("created empty BP_PlatformShip (mesh assigned at place time)")
        except Exception as ex:
            err("BP create: %s" % ex)

    EAL.save_directory(DEST, only_if_is_dirty=False, recursive=True)

out = os.path.join(unreal.Paths.project_saved_dir(), "ship_platform_import.json")
json.dump(rep, open(out, "w"), indent=1)
log("DONE meshes=%d mats=%d tex=%d errors=%d primary=%s" % (
    len(rep["meshes"]) + (1 if rep.get("primary") else 0),
    len(rep["materials"]), len(rep["textures"]), len(rep["errors"]),
    (rep.get("primary") or {}).get("path")))
