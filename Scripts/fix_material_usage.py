# Sets the instanced-static-mesh usage flag on the shared Zoogi materials (Meadow grass/flowers and star
# fields are ISMs; without the flag they fall back to the default material, and cooked builds show it).
import unreal

PATHS = ["/Game/Materials/M_ZoogiSurface", "/Game/Materials/M_ZoogiGlow", "/Game/Materials/M_ZoogiFX"]
for p in PATHS:
    m = unreal.EditorAssetLibrary.load_asset(p)
    if not m:
        unreal.log_warning("fix_material_usage: missing " + p)
        continue
    before = m.get_editor_property("used_with_instanced_static_meshes")
    if not before:
        m.set_editor_property("used_with_instanced_static_meshes", True)
        unreal.MaterialEditingLibrary.recompile_material(m)
        unreal.EditorAssetLibrary.save_asset(p, only_if_is_dirty=False)
    unreal.log("fix_material_usage: %s ISM usage %s -> True" % (p, before))
