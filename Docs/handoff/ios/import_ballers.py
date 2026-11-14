# Nov 9 b208 (Claude): Evan's Tripo Ballers -> /Game/Zoogis/Ballers/<Name>/SM_<Name>; the 2M-triangle ones are reduced to ~40k for phones
import unreal, glob, time
LOG = 'C:/dev/ios/import_ballers_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
M = [('Blue', 'pinpoint_tripo'), ('Frosti', 'blue_tripo'), ('Lumi', 'luma_tripo'), ('Vex', 'vex_tripo'), ('Hotstreak', 'bolt_tripo')]
tasks = []
for name, folder in M:
    t = unreal.AssetImportTask(); t.filename = glob.glob('C:/dev/art/%s/*.fbx' % folder)[0]
    t.destination_path = '/Game/Zoogis/Ballers/' + name; t.destination_name = 'SM_' + name
    t.automated = True; t.replace_existing = True; t.save = True
    o = unreal.FbxImportUI(); o.import_as_skeletal = False; o.import_animations = False; o.import_materials = True; o.import_textures = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.static_mesh_import_data.combine_meshes = True
    t.options = o; tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for (name, folder), t in zip(M, tasks):
    L('%s <- %s: %d objects' % (name, t.filename, len(t.imported_object_paths)))
    for a in reg.get_assets_by_path('/Game/Zoogis/Ballers/' + name, recursive=True):
        c = str(a.asset_class_path.asset_name)
        if c == 'Texture2D':
            tex = unreal.load_asset(str(a.package_name)); tex.set_editor_property('max_texture_size', 1024); unreal.EditorAssetLibrary.save_loaded_asset(tex)
        if c == 'StaticMesh':
            m = unreal.load_asset(str(a.package_name))
            try:
                v0 = sub.get_number_verts(m, 0); L('  %s verts %d' % (a.package_name, v0))
                if v0 > 60000:
                    rs = unreal.MeshReductionSettings(); rs.set_editor_property('percent_triangles', max(0.01, 40000.0 / (v0 * 2.0)))
                    try: rs.set_editor_property('termination_criterion', unreal.StaticMeshReductionTerimationCriterion.TRIANGLES)
                    except Exception as e: L('  crit %r' % e)
                    sub.set_lod_reduction_settings(m, 0, rs)
                    L('  reduced -> verts %d' % sub.get_number_verts(m, 0))
            except Exception as e: L('  reduce error %r' % e)
            try: m.set_editor_property('nanite_settings', unreal.MeshNaniteSettings())
            except Exception: pass
            b = m.get_bounding_box(); L('  bounds %s %s' % (b.min, b.max))
            unreal.EditorAssetLibrary.save_loaded_asset(m)
L('DONE'); log.close()
