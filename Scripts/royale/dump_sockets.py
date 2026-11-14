import unreal
L = lambda s: unreal.log('SOCK: ' + str(s))
EAL = unreal.EditorAssetLibrary
for p in ('/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny', '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'):
    m = EAL.load_asset(p)
    if not m: L('%s missing' % p); continue
    sk = m.get_editor_property('skeleton')
    for owner in (m, sk):
        try:
            for s in owner.get_editor_property('sockets'):
                L('%s %s socket %s bone %s loc %s rot %s scale %s' % (p.split('/')[-1], owner.get_class().get_name(), s.get_editor_property('socket_name'), s.get_editor_property('bone_name'), s.get_editor_property('relative_location'), s.get_editor_property('relative_rotation'), s.get_editor_property('relative_scale')))
        except Exception as e: L('%s sockets err %s' % (owner.get_name(), e))
    try:
        ref = m.get_editor_property('skeleton')
        L('%s hand_r ref pose: %s' % (p.split('/')[-1], unreal.SkeletalMeshEditorSubsystem))
    except Exception: pass
r = EAL.load_asset('/Game/Weapons/Rifle/Mesh/SM_Rifle')
if r:
    L('rifle bounds origin %s extent %s' % (r.get_bounds().origin, r.get_bounds().box_extent))
    for s in r.get_editor_property('sockets'): L('rifle socket %s loc %s rot %s' % (s.get_editor_property('socket_name'), s.get_editor_property('relative_location'), s.get_editor_property('relative_rotation')))
L('DONE')
try: unreal.SystemLibrary.quit_editor()
except Exception as e: L('quit %s' % e)
