# Nov 9 b214 (Claude): run INSIDE the full editor (-ExecutePythonScript): cut the 1.9M-face golem to ~45k triangles, save, quit
import unreal, time
LOG = 'C:/dev/ios/golem_reduce_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
try:
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    m = unreal.load_asset('/Game/Zoogis/MeadowGolem2/SM_PurpleGolem')
    if not m: L('missing')
    else:
        v0 = sub.get_number_verts(m, 0); L('verts %d' % v0)
        if v0 > 60000:
            rs = unreal.MeshReductionSettings()
            rs.set_editor_property('percent_triangles', max(0.01, 45000.0 / (v0 * 2.0)))
            try: rs.set_editor_property('percent_vertices', max(0.01, 23000.0 / v0))
            except Exception as e: L('  pv %r' % e)
            sub.set_lod_reduction_settings(m, 0, rs)
            L('  reduced -> verts %d' % sub.get_number_verts(m, 0))
        unreal.EditorAssetLibrary.save_loaded_asset(m)
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
unreal.SystemLibrary.quit_editor()
