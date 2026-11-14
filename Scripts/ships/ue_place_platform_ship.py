"""Place all PlatformShip parts as VipPlatformShip_* above Meadow; save /Game/Maps/VipShipGallery.
NullRHI-safe (no HighResShot). UE 5.6 LevelEditorSubsystem APIs."""
import os, json, unreal

DEST = "/Game/Ships/PlatformShip"
PARTS = DEST + "/Parts"
GALLERY = "/Game/Maps/VipShipGallery"
TAG = "SHIPPLACE"
rep = {"ok": False, "errors": [], "actors": [], "meshes": []}

def log(m): unreal.log_warning("%s: %s" % (TAG, m))
def err(m):
    rep["errors"].append(m)
    unreal.log_warning("%s: ERROR %s" % (TAG, m))

EAL = unreal.EditorAssetLibrary
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

meshes = []
if EAL.does_directory_exist(PARTS):
    for p in EAL.list_assets(PARTS, False, False):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            meshes.append(a)

if not meshes:
    err("no parts in " + PARTS)
else:
    log("parts=%d" % len(meshes))
    rep["meshes"] = [m.get_path_name().split(".")[0] for m in meshes]

    hull = next((m for m in meshes if m.get_name() == "Hull_Lower"), None)
    if hull:
        want = DEST + "/SM_PlatformShip"
        try:
            if not EAL.does_asset_exist(want):
                src = hull.get_path_name().split(".")[0]
                ok = EAL.duplicate_asset(src, want)
                log("SM_PlatformShip dup=%s" % ok)
            rep["sm_alias"] = want
        except Exception as ex:
            err("SM alias: %s" % ex)

    loaded = False
    try:
        loaded = bool(LES.load_level("/Game/Maps/Meadow"))
    except Exception as ex:
        err("LES.load_level: %s" % ex)
        try:
            loaded = bool(unreal.EditorLevelLibrary.load_level("/Game/Maps/Meadow"))
        except Exception as ex2:
            err("ELL.load_level: %s" % ex2)

    if not loaded:
        err("load Meadow failed")
    else:
        actors = EAS.get_all_level_actors() if EAS else unreal.EditorLevelLibrary.get_all_level_actors()
        for a in list(actors):
            try:
                if a and a.get_actor_label().startswith("VipPlatformShip"):
                    if EAS: EAS.destroy_actor(a)
                    else: unreal.EditorLevelLibrary.destroy_actor(a)
            except Exception:
                pass

        loc = unreal.Vector(0.0, 0.0, 2500.0)
        rot = unreal.Rotator(0.0, 35.0, 0.0)
        for m in meshes:
            if EAS:
                actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            else:
                actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
            if not actor:
                err("spawn failed " + m.get_name()); continue
            actor.set_actor_label("VipPlatformShip_" + m.get_name())
            smc = actor.static_mesh_component
            smc.set_editor_property("static_mesh", m)
            try:
                smc.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
            except Exception:
                pass
            rep["actors"].append(actor.get_actor_label())

        # Save-as via EditorLoadingAndSavingUtils.save_map
        ok = False
        try:
            world = LES.get_world_context() if hasattr(LES, "get_world_context") else None
        except Exception:
            world = None
        try:
            world = unreal.EditorLevelLibrary.get_editor_world()
        except Exception as ex:
            err("get_editor_world: %s" % ex)
            world = None
        if world:
            try:
                ok = bool(unreal.EditorLoadingAndSavingUtils.save_map(world, GALLERY))
                log("save_map %s -> %s" % (GALLERY, ok))
            except Exception as ex:
                err("save_map: %s" % ex)
        # Fallback: save current level then duplicate asset
        if not ok:
            try:
                LES.save_current_level()
                # duplicate Meadow package to gallery if gallery missing
                if not EAL.does_asset_exist(GALLERY):
                    EAL.duplicate_asset("/Game/Maps/Meadow", GALLERY)
                    log("duplicated Meadow -> gallery, re-load and re-place needed")
                    rep["needs_replace"] = True
                else:
                    # current world was Meadow; save_map failed — try export
                    err("save_map failed and gallery exists oddly")
            except Exception as ex:
                err("fallback save: %s" % ex)

        try:
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        except Exception as ex:
            err("save dirty: %s" % ex)

        rep["ok"] = bool(ok) or EAL.does_asset_exist(GALLERY)
        rep["gallery_map"] = GALLERY
        rep["actor_count"] = len(rep["actors"])
        log("actors=%d ok=%s exists=%s" % (len(rep["actors"]), rep["ok"], EAL.does_asset_exist(GALLERY)))

json.dump(rep, open(os.path.join(unreal.Paths.project_saved_dir(), "ship_platform_place.json"), "w"), indent=1)
log("DONE %s" % {k: rep[k] for k in rep if k != "meshes"})
