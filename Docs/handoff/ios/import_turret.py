# Nov 9 b202 (Claude): import Evan's Tripo models + the Dark ball skin
#   turret (cute robotic statue, retopo 9.2k tris)  -> /Game/Zoogis/Turret/SM_Turret
#   Dark Zoogi (stylized dragon warrior, retopo 9.9k) -> /Game/Zoogis/Dark/SM_DarkZoogi   (static for now - no rig)
#   Tally Golem (Meadow boss, 27k quads)             -> /Game/Zoogis/MeadowGolem/SM_TallyGolem
#   T_Ball_Dark.png                                   -> /Game/Zoogis/Balls/T_Ball_Dark
import unreal, glob, os, time
LOG = 'C:/dev/ios/import_turret_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
def task(src, dest, name):
    t = unreal.AssetImportTask(); t.filename = src; t.destination_path = dest; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True; return t
tasks = [task(glob.glob('C:/dev/art/turret_tripo/*.fbx')[0], '/Game/Zoogis/Turret', 'SM_Turret'),
         task(glob.glob('C:/dev/art/dark_zoogi_tripo/*.fbx')[0], '/Game/Zoogis/Dark', 'SM_DarkZoogi'),
         task(glob.glob('C:/dev/art/meadow_golem_tripo/*.fbx')[0], '/Game/Zoogis/MeadowGolem', 'SM_TallyGolem'),
         task('C:/dev/art/dark_zoogi_tripo/T_Ball_Dark.png', '/Game/Zoogis/Balls', 'T_Ball_Dark')]
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    L('%s/%s -> %d objects' % (t.destination_path, t.destination_name, len(t.imported_object_paths)))
    for p in t.imported_object_paths: L('   ' + p)
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ['/Game/Zoogis/Turret', '/Game/Zoogis/Dark', '/Game/Zoogis/MeadowGolem']:
    for a in reg.get_assets_by_path(folder, recursive=True):
        c = str(a.asset_class_path.asset_name); L('asset %s %s' % (c, a.package_name))
        if c == 'Texture2D':
            tex = unreal.load_asset(str(a.package_name)); tex.set_editor_property('max_texture_size', 1024); unreal.EditorAssetLibrary.save_loaded_asset(tex)
        if c == 'StaticMesh':
            m = unreal.load_asset(str(a.package_name)); b = m.get_bounding_box(); L('bounds %s %s' % (b.min, b.max))
L('DONE'); log.close()
