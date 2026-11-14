# Nov 9 b208 (Claude): run INSIDE the full editor (-ExecutePythonScript): cut the 2M-triangle Tripo Ballers to ~40k for phones, save, quit
import unreal, time
LOG = 'C:/dev/ios/reduce_ballers_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
try:
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    L('subsystem %r' % sub)
    for name in ['Blue', 'Frosti', 'Lumi', 'Vex', 'Hotstreak']:
        p = '/Game/Zoogis/Ballers/%s/SM_%s' % (name, name)
        m = unreal.load_asset(p)
        if not m: L('%s missing' % p); continue
        v0 = sub.get_number_verts(m, 0)
        L('%s verts %d' % (name, v0))
        if v0 > 60000:
            rs = unreal.MeshReductionSettings()
            rs.set_editor_property('percent_triangles', max(0.01, 40000.0 / (v0 * 2.0)))
            try: rs.set_editor_property('percent_vertices', max(0.01, 20000.0 / v0))
            except Exception as e: L('  pv %r' % e)
            sub.set_lod_reduction_settings(m, 0, rs)
            L('  reduced -> verts %d' % sub.get_number_verts(m, 0))
            unreal.EditorAssetLibrary.save_loaded_asset(m)
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
unreal.SystemLibrary.quit_editor()
