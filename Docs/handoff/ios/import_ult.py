# Oct 30 (Claude): import Evan's four Tripo ultimate forms (rigged Mixamo skeleton, 11 animations each) into /Game/Zoogis/Ultimates.
import unreal, glob, os, time
open('C:/dev/unreal.lock', 'w').write('Editor import of Tripo ultimate forms (Claude)')
LOG = 'C:/dev/ios/import_ult_log.txt'
log = open(LOG, 'w')
def L(s):
    log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
SRC = 'C:/dev/zoogi_ultimates'
tasks = []
for name in ['ult_ember_red', 'ult_fox_gold_A', 'ult_fox_gold_B', 'ult_frost_blue']:
    fbx = glob.glob(os.path.join(SRC, name, '*.fbx'))
    if not fbx: L('missing ' + name); continue
    t = unreal.AssetImportTask()
    t.filename = fbx[0]
    t.destination_path = '/Game/Zoogis/Ultimates/' + name
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
L('importing %d' % len(tasks))
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    L('%s -> %d objects' % (t.destination_path, len(t.imported_object_paths)))
    for p in t.imported_object_paths: L('   ' + p)
# mobile: cap the 4k Tripo textures at 1024
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for a in reg.get_assets_by_path('/Game/Zoogis/Ultimates', recursive=True):
    if str(a.asset_class_path.asset_name) == 'Texture2D':
        tex = unreal.load_asset(str(a.package_name))
        tex.set_editor_property('max_texture_size', 1024)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        L('capped ' + str(a.package_name))
L('DONE')
log.close()
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
