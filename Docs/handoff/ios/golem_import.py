# Nov 9 b214 (Claude): Evan's Tripo stone golem (body hue-shifted to purple) -> /Game/Zoogis/MeadowGolem2/SM_PurpleGolem (Meadow titan)
import unreal, time
LOG = 'C:/dev/ios/golem_import_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
# + the DARK REALM boss: Evan's Tripo crystal brute -> /Game/Zoogis/DarkRealmBoss/SM_CrystalBrute
J = [('C:/dev/art/golem_tripo/tripo_convert_1e3da918-15c1-4268-9ebc-ab0e522f8aba.fbx', '/Game/Zoogis/MeadowGolem2', 'SM_PurpleGolem'),
     ('C:/dev/art/brute_tripo/tripo_convert_2393b916-00f4-43fa-aa50-ccd1be2a6ba7.fbx', '/Game/Zoogis/DarkRealmBoss', 'SM_CrystalBrute')]
try:
  for fbx, dest, nm in J:
    t = unreal.AssetImportTask(); t.filename = fbx
    t.destination_path = dest; t.destination_name = nm
    t.automated = True; t.replace_existing = True; t.save = True
    o = unreal.FbxImportUI(); o.import_as_skeletal = False; o.import_animations = False; o.import_materials = True; o.import_textures = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.static_mesh_import_data.combine_meshes = True
    t.options = o
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    L('%s imported %d objects: %s' % (nm, len(t.imported_object_paths), list(t.imported_object_paths)))
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_path(dest, recursive=True):
      c = str(a.asset_class_path.asset_name); L('  %s %s' % (c, a.package_name))
      if c == 'Texture2D':
          tex = unreal.load_asset(str(a.package_name)); tex.set_editor_property('max_texture_size', 1024); unreal.EditorAssetLibrary.save_loaded_asset(tex)
      if c == 'StaticMesh':
          m = unreal.load_asset(str(a.package_name)); b = m.get_bounding_box(); L('  bounds %s %s' % (b.min, b.max))
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
