# Oct 29 (Claude): the old-style Zoogi materials need "Used with Instanced Static Meshes" for the Frozen crowd (ISM fans render black otherwise).
import unreal
out = []
paths = unreal.EditorAssetLibrary.list_assets('/Game/Zoogis', recursive=True, include_folder=False)
done = set()
for p in paths:
    a = unreal.EditorAssetLibrary.load_asset(p)
    m = None
    if isinstance(a, unreal.Material):
        m = a
    elif isinstance(a, unreal.MaterialInstance):
        m = a.get_base_material()
    if not m or m.get_path_name() in done:
        continue
    done.add(m.get_path_name())
    was = m.get_editor_property('used_with_instanced_static_meshes')
    if not was:
        m.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(m)
        ok = unreal.EditorAssetLibrary.save_loaded_asset(m, False)
        out.append('SET %s saved=%s' % (m.get_path_name(), ok))
    else:
        out.append('ok  %s' % m.get_path_name())
open('C:/dev/ios/fixism.txt', 'w').write('\n'.join(out) + '\nFIXISM_DONE\n')
print('FIXISM_DONE', len(out))
