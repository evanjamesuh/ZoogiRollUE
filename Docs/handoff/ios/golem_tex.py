# Nov 10 b216 (Claude): Evan "change the meadow boss color back to its original color" - re-import the golem's ORIGINAL base colour over the purple one
import unreal, time
LOG = 'C:/dev/ios/golem_tex_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
try:
    t = unreal.AssetImportTask(); t.filename = 'C:/dev/art/golem_tripo/basecolor_original.jpg'
    t.destination_path = '/Game/Zoogis/MeadowGolem2'; t.destination_name = 'stone_golem_3d_model_basecolor'
    t.automated = True; t.replace_existing = True; t.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    L('imported %s' % list(t.imported_object_paths))
    tex = unreal.load_asset('/Game/Zoogis/MeadowGolem2/stone_golem_3d_model_basecolor')
    tex.set_editor_property('max_texture_size', 1024); unreal.EditorAssetLibrary.save_loaded_asset(tex)
    L('ok %s' % tex)
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
