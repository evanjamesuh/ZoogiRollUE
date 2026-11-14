# -*- coding: utf-8 -*-
import json, unreal
EAL = unreal.EditorAssetLibrary
WANT = "/Game/Orbs/SM_Orb"
sm = unreal.load_asset(WANT)
rep = {}
if not isinstance(sm, unreal.StaticMesh):
    rep["error"] = "missing"
else:
    b = sm.get_bounds()
    ns = sm.get_editor_property("nanite_settings")
    nanite = bool(ns.get_editor_property("enabled"))
    slots = []
    for s in sm.get_editor_property("static_materials"):
        mi = s.get_editor_property("material_interface")
        slots.append({"slot": str(s.get_editor_property("material_slot_name")),
                      "mat": mi.get_path_name() if mi else None})
    # list textures under Orbs
    assets = EAL.list_assets("/Game/Orbs", True, False)
    tex = [a for a in assets if "Texture" in str(unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name)]
    mats = [a for a in assets if "Material" in str(unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name)]
    rep = {
        "path": sm.get_path_name(),
        "tris0": int(sm.get_num_triangles(0)),
        "nanite": nanite,
        "radius": float(b.sphere_radius),
        "origin": [float(b.origin.x), float(b.origin.y), float(b.origin.z)],
        "extent": [float(b.box_extent.x), float(b.box_extent.y), float(b.box_extent.z)],
        "slots": slots,
        "textures": tex[:20],
        "materials": mats[:20],
        "gameplay_scale": (40.0 / float(b.sphere_radius)) if b.sphere_radius > 0 else None,
    }
    unreal.log_warning("VERIFY_ORB %s" % json.dumps(rep))
json.dump(rep, open(unreal.Paths.project_saved_dir() + "/verify_sm_orb.json", "w"), indent=1)
unreal.log_warning("VERIFY_ORB_DONE")
