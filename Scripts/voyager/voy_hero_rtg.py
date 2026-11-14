# Nov 11 (Claude): Evan "start with the characters" - the Voyager hero (UE4 mannequin skeleton) drives the crystal Zoogi heroes
# (Tripo-rigged /Game/Zoogis/Ultimates/ult_*) at runtime. Builds RTG_VoyUE4_to_<rig> per hero (source = GASP's UE4 IK rig).
import unreal
L = lambda s: unreal.log('VOYRTG: ' + str(s))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SRC_RIG = EAL.load_asset('/Game/Voyager/Addons/GASP/Characters/UE4_Mannequin/Rigs/IK_UE4_Mannequin_Retarget')
SRC_MESH = EAL.load_asset('/Game/Voyager/Demo/Characters/Hero/SK_Hero')
L('src rig %s, src mesh %s' % (SRC_RIG, SRC_MESH))
try:
    rcs = unreal.IKRigController.get_controller(SRC_RIG)
    L('src chains: ' + ', '.join([str(c.get_editor_property('chain_name')) for c in rcs.get_retarget_chains()]) + ' root ' + str(rcs.get_retarget_root()))
    L('src rig mesh: %s' % rcs.get_skeletal_mesh())
except Exception as e: L('src chains: %s' % e)
OUT = '/Game/Zoogis/Ultimates/Retarget'
for r in ('ult_frost_blue', 'ult_ember_red', 'ult_fox_gold_A', 'ult_fox_gold_B'):
    mesh = EAL.load_asset('/Game/Zoogis/Ultimates/%s/%s' % (r, r)); rig = EAL.load_asset(OUT + '/IK_' + r)
    if not mesh or not rig: L('%s missing mesh %s rig %s' % (r, mesh, rig)); continue
    name = 'RTG_VoyUE4_to_' + r; p = OUT + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    rt = AT.create_asset(name, OUT, unreal.IKRetargeter, unreal.IKRetargetFactory())
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
    try:
        for c in ('Spine', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg', 'Head'): L('%s map %s <- %s' % (r, c, tc.get_source_chain(c)))
    except Exception as e: L('%s map dump: %s' % (r, e))
    try:
        b = mesh.get_editor_property('skeleton'); L('%s skeleton %s, bounds %s' % (r, b.get_name(), mesh.get_bounds().box_extent))
    except Exception as e: L('%s info %s' % (r, e))
    EAL.save_loaded_asset(rt); L('%s saved %s' % (r, p))
try: L('hero bounds %s' % SRC_MESH.get_bounds().box_extent)
except Exception as e: L(e)
L('DONE')
try:
    unreal.SystemLibrary.quit_editor()
except Exception as e:
    L('quit: %s' % e)
