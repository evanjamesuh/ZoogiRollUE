# Nov 10 b217 (Claude): Evan "build animation and a skeleton for golem no exceptions i want fluent movements" - the Tripo golem,
# retopologised (18k), auto-rigged on the UE5 Mannequin skeleton, 9 Tripo animations -> /Game/Zoogis/MeadowGolemRig (SK_Golem + AS_Golem_<name>)
import unreal, time, glob
LOG = 'C:/dev/ios/golem_rig_import_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
DEST = '/Game/Zoogis/MeadowGolemRig'
try:
    fbx = sorted(glob.glob('C:/dev/art/golem_rig/*.fbx'))[0]
    t = unreal.AssetImportTask(); t.filename = fbx; t.destination_path = DEST; t.destination_name = 'SK_Golem'
    t.automated = True; t.replace_existing = True; t.save = True
    o = unreal.FbxImportUI(); o.import_as_skeletal = True; o.import_mesh = True; o.import_animations = True; o.import_materials = True; o.import_textures = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    o.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
    try: o.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
    except Exception as e: L('bone tracks %r' % e)
    t.options = o
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    L('imported from %s: %s' % (fbx, list(t.imported_object_paths)))
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    keys = ['look_around', 'hit_to_body_01', 'angry_01', 'defeat', 'idle', 'chop', 'slash', 'cheer', 'hug']
    for a in reg.get_assets_by_path(DEST, recursive=True):
        c = str(a.asset_class_path.asset_name); n = str(a.asset_name); L('  %s %s' % (c, n))
        if c == 'Texture2D':
            tex = unreal.load_asset(str(a.package_name)); tex.set_editor_property('max_texture_size', 1024); unreal.EditorAssetLibrary.save_loaded_asset(tex)
        if c == 'AnimSequence':
            k = next((k for k in keys if k in n.lower()), None)
            if k:
                new = '%s/AS_Golem_%s' % (DEST, k)
                if str(a.package_name) != new and not unreal.EditorAssetLibrary.does_asset_exist(new):
                    ok = unreal.EditorAssetLibrary.rename_asset(str(a.package_name), new); L('    -> %s %s' % (new, ok))
        if c == 'SkeletalMesh':
            m = unreal.load_asset(str(a.package_name)); b = m.get_bounds(); L('  bounds origin %s extent %s' % (b.origin, b.box_extent))
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
