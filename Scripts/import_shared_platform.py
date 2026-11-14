"""Import SharedPlatform GLB as the outside-arena hover launch pad deck.

Evan (2026-10-22 6:57 PM CT): SharedPlatform hovers OUTSIDE the arena for Zoogis
to shoot from (fully unobstructed) — NOT the arena floor. Resize for DeckRadius.

Source GLB: Import/LaunchPad/SharedPlatform.glb
  (static bake: Sketchfab skins/joints stripped via Scripts/glb_strip_skin.js so
   Interchange yields UStaticMesh, not SkeletalMesh)

Dest asset: /Game/Env/LaunchPad/SM_SharedPlatform

Run with merge lock held:
  .\Scripts\run_python_here.ps1 -Script Scripts\import_shared_platform.py -LogName sharedplatform-import
"""import os
import unreal

SRC = unreal.Paths.project_dir() + "Import/LaunchPad/SharedPlatform.glb"
DEST = "/Game/Env/LaunchPad"
NAME = "SM_SharedPlatform"
WANT = f"{DEST}/{NAME}"
EAL = unreal.EditorAssetLibrary


def make_pipeline():
    p = unreal.InterchangeGenericAssetsPipeline()
    p.set_editor_property("use_source_name_for_asset", False)
    try:
        p.set_editor_property("scene_name_sub_folder", False)
    except Exception:
        pass
    c = p.get_editor_property("common_meshes_properties")
    m = p.get_editor_property("mesh_pipeline")
    mp = p.get_editor_property("material_pipeline")
    c.set_editor_property("force_all_mesh_as_type", unreal.InterchangeForceMeshType.IFMT_STATIC_MESH)
    m.set_editor_property("import_static_meshes", True)
    m.set_editor_property("import_skeletal_meshes", False)
    m.set_editor_property("combine_static_meshes", True)
    m.set_editor_property("build_nanite", False)
    mp.set_editor_property("import_materials", True)
    return p


def wipe():
    if EAL.does_directory_exist(DEST):
        for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
            EAL.delete_asset(p)
        # also drop nested dirs if any
        try:
            EAL.delete_directory(DEST)
        except Exception:
            pass
    EAL.make_directory(DEST)


def run():
    if not os.path.isfile(SRC):
        unreal.log_error(f"missing {SRC}")
        return
    wipe()
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(SRC)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    try:
        params.replace_existing = True
    except Exception:
        pass
    try:
        params.destination_name = NAME
    except Exception:
        pass
    try:
        arr = unreal.Array(unreal.InterchangePipelineBase)
        arr.append(make_pipeline())
        params.set_editor_property("override_pipelines", arr)
    except Exception as e:
        unreal.log_warning(f"override_pipelines failed: {e}")
    result = mgr.import_asset(DEST, source, params)
    unreal.log_warning(f"import result={result}")

    # Rename any static mesh to WANT
    for p in list(EAL.list_assets(DEST, recursive=True, include_folder=False)):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            src_path = a.get_path_name().split(".")[0]
            if src_path != WANT:
                if EAL.does_asset_exist(WANT):
                    EAL.delete_asset(WANT)
                EAL.rename_asset(src_path, WANT)
                unreal.log_warning(f"renamed {src_path} -> {WANT}")

    # Force-save every asset under DEST
    EAL.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        EAL.save_asset(p, only_if_is_dirty=False)
        data = EAL.find_asset_data(p)
        unreal.log_warning(f"SAVED {p} class={data.asset_class_path.asset_name}")

    sm = unreal.load_asset(WANT)
    if isinstance(sm, unreal.StaticMesh):
        unreal.log_warning(f"READY {WANT} bounds={sm.get_bounds().sphere_radius} mats={len(sm.static_materials)}")
    else:
        unreal.log_error("READY FAILED: SM_SharedPlatform missing or not StaticMesh")


run()

