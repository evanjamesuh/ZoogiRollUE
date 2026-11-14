# Oct 31 (Claude): retarget Epic's Game Animation Sample loops (UEFN mannequin) onto the crystal hero rigs (/Game/Zoogis/Ultimates/ult_*).
# Output: /Game/Zoogis/Ultimates/<rig>/GASP/<anim>  (+ IK rigs / retargeters in /Game/Zoogis/Ultimates/Retarget)
import unreal
L = lambda s: unreal.log('GASPRT: ' + str(s))
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
RIGS = ['ult_frost_blue', 'ult_ember_red', 'ult_fox_gold_A', 'ult_fox_gold_B']
SRC_MESH = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
SRC_RIG = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin')
L('src mesh %s rig %s' % (SRC_MESH, SRC_RIG))
L('IKRigController: ' + ', '.join([m for m in dir(unreal.IKRigController) if not m.startswith('_')]))
_m = [m for m in dir(unreal.IKRetargeterController) if not m.startswith('_')]
for i in range(0, len(_m), 12): L('IKRetargeterController: ' + ', '.join(_m[i:i+12]))
anims = [a for a in EAL.list_assets('/Game/Characters/UEFN_Mannequin/Animations', recursive=True) if 'M_Neutral' in a]
anims = [EAL.load_asset(a.split('.')[0]) for a in anims]
anims = [a for a in anims if isinstance(a, unreal.AnimSequence)]
L('anims %d' % len(anims))
OUT = '/Game/Zoogis/Ultimates/Retarget'
def make(name, cls, fac):
    p = OUT + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    return AT.create_asset(name, OUT, cls, fac)
for r in RIGS:
    mesh = EAL.load_asset('/Game/Zoogis/Ultimates/%s/%s' % (r, r))
    if not mesh: L('%s: mesh missing' % r); continue
    sk = mesh.get_editor_property('skeleton')
    if r == RIGS[0]:
        try:
            names = unreal.SkeletalMeshEditorSubsystem if False else None
        except Exception: pass
    rig = make('IK_' + r, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    rc = unreal.IKRigController.get_controller(rig)
    rc.set_skeletal_mesh(mesh)
    ok = False
    try: ok = rc.apply_auto_generated_retarget_definition()
    except Exception as e: L('%s auto retarget def failed: %s' % (r, e))
    try: rc.apply_auto_fbik()
    except Exception as e: L('%s auto fbik: %s' % (r, e))
    try:
        chains = rc.get_retarget_chains()
        L('%s auto def %s, chains: %s' % (r, ok, ', '.join([str(c.get_editor_property('chain_name')) for c in chains])))
    except Exception as e: L('%s chains: %s' % (r, e))
    try: L('%s retarget root %s' % (r, rc.get_retarget_root()))
    except Exception as e: L('root: %s' % e)
    rt = make('RTG_UEFN_to_' + r, unreal.IKRetargeter, unreal.IKRetargetFactory())
    tc = unreal.IKRetargeterController.get_controller(rt)
    for fn, args in (('set_ik_rig', (unreal.RetargetSourceOrTarget.SOURCE, SRC_RIG)), ('set_ik_rig', (unreal.RetargetSourceOrTarget.TARGET, rig)),
                     ('set_preview_mesh', (unreal.RetargetSourceOrTarget.SOURCE, SRC_MESH)), ('set_preview_mesh', (unreal.RetargetSourceOrTarget.TARGET, mesh)),
                     ('add_default_ops', ()), ('assign_ik_rig_to_all_ops', (unreal.RetargetSourceOrTarget.SOURCE, SRC_RIG)), ('assign_ik_rig_to_all_ops', (unreal.RetargetSourceOrTarget.TARGET, rig))):
        if hasattr(tc, fn):
            try: res = getattr(tc, fn)(*args); L('%s %s ok %s' % (r, fn, res))
            except Exception as e: L('%s %s FAILED %s' % (r, fn, e))
        else: L('%s no %s' % (r, fn))
    for fn, args in (('auto_map_chains', (unreal.AutoMapChainType.FUZZY, True)), ('auto_align_all_bones', (unreal.RetargetSourceOrTarget.TARGET,))):
        if hasattr(tc, fn):
            try: getattr(tc, fn)(*args); L('%s %s ok' % (r, fn))
            except Exception as e: L('%s %s FAILED %s' % (r, fn, e))
    EAL.save_loaded_asset(rig); EAL.save_loaded_asset(rt)
    try:
        ads = [EAL.find_asset_data(a.get_path_name().split('.')[0]) for a in anims]
        out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, SRC_MESH, mesh, rt, '', '', 'GASP_', '', False)
        moved = 0
        for a in out:
            p = (str(a.package_name) if hasattr(a, 'package_name') else a.get_path_name().split('.')[0])
            np = '/Game/Zoogis/Ultimates/%s/GASP/%s' % (r, p.split('/')[-1])
            if EAL.does_asset_exist(np): EAL.delete_asset(np)
            if EAL.rename_asset(p, np): moved += 1
        L('%s retargeted %d anims (%d moved to GASP/)' % (r, len(out), moved))
    except Exception as e: L('%s batch retarget FAILED %s' % (r, e))
EAL.save_directory('/Game/Zoogis/Ultimates', only_if_is_dirty=True, recursive=True)
L('DONE')
try:
    unreal.SystemLibrary.quit_editor()
except Exception as e:
    L('quit: %s' % e)
