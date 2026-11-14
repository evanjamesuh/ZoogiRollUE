# Nov 1 (Claude): dumps the Inferno World Free maps (static mesh placements) + the Stylized VFX Pack systems -> Saved/inferno_dump.json
import json, os
import unreal
out = {"maps": {}, "vfx": [], "meshes": {}}
for mp in ["/Game/Inferno_World_Free/Maps/Overview", "/Game/Inferno_World_Free/Maps/Demonstration"]:
    try:
        w = unreal.EditorLoadingAndSavingUtils.load_map(mp)
        acts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
        rows = []
        for a in acts:
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                sm = c.get_editor_property("static_mesh")
                if not sm: continue
                t = c.get_world_transform()
                mats = [m.get_path_name() if m else "" for m in c.get_materials()]
                rows.append({"actor": a.get_name(), "mesh": sm.get_path_name(), "loc": [t.translation.x, t.translation.y, t.translation.z],
                             "rot": [t.rotation.rotator().pitch, t.rotation.rotator().yaw, t.rotation.rotator().roll], "scale": [t.scale3d.x, t.scale3d.y, t.scale3d.z], "mats": mats})
            if not a.get_components_by_class(unreal.StaticMeshComponent):
                rows.append({"actor": a.get_name(), "class": a.get_class().get_name(), "loc": [a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z]})
        out["maps"][mp] = rows
        unreal.log("[Inferno] %s: %d rows" % (mp, len(rows)))
    except Exception as e:
        unreal.log_warning("[Inferno] map %s failed: %s" % (mp, e))
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for d in ar.get_assets_by_path("/Game/VFX_PackVol1", recursive=True):
    out["vfx"].append([str(d.package_name), str(d.asset_class_path.asset_name)])
for d in ar.get_assets_by_path("/Game/Inferno_World_Free/Meshes", recursive=True):
    sm = unreal.load_asset(str(d.package_name))
    if isinstance(sm, unreal.StaticMesh):
        b = sm.get_bounds()
        out["meshes"][str(d.package_name)] = {"origin": [b.origin.x, b.origin.y, b.origin.z], "extent": [b.box_extent.x, b.box_extent.y, b.box_extent.z], "tris": sm.get_num_triangles(0)}
open(os.path.join(unreal.Paths.project_saved_dir(), "inferno_dump.json"), "w").write(json.dumps(out, indent=1))
unreal.log("[Inferno] dump done")
