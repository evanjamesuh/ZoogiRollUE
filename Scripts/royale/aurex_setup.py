# Oct 31 (Claude): (1) Evan's 8 buzzy ball forms -> /Game/Zoogis/Balls (make_ball_kit.py), (2) Fab "Mecha Robot: Aurex" becomes the
# hero model for every Zoogi: IK rig on its UE4-mannequin skeleton, RTG_UEFN_to_Aurex, the Royale hero clips (actions / GASP
# locomotion / rifle set) retargeted from the UEFN mannequin into /Game/Mecha_Aurex/Anims/<same sub-folders, same names>,
# (3) logs Aurex's material parameters so per-Zoogi colours can be set in code.
import unreal, os
L = lambda s: unreal.log('AUREX: ' + str(s))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
P = unreal.Paths.project_dir()
# ---- 1. ball kit
try:
    exec(open(os.path.join(P, 'Scripts', 'royale', 'make_ball_kit.py')).read().replace('unreal.SystemLibrary.quit_editor()', 'pass'))
except Exception as e: L('ballkit error %s' % e)
# ---- 2. Aurex rig + retarget
SRC_MESH = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
SRC_RIG = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin')
TGT_MESH = EAL.load_asset('/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex')
L('src %s %s tgt %s' % (SRC_MESH, SRC_RIG, TGT_MESH))
RD = '/Game/Mecha_Aurex/Rig'
for n in ('IK_Mecha_Aurex', 'RTG_UEFN_to_Aurex'):
    if EAL.does_asset_exist(RD + '/' + n): EAL.delete_asset(RD + '/' + n)
rig = AT.create_asset('IK_Mecha_Aurex', RD, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
rc = unreal.IKRigController.get_controller(rig)
rc.set_skeletal_mesh(TGT_MESH); rc.apply_auto_generated_retarget_definition()
try: rc.apply_auto_fbik()
except Exception as e: L('fbik %s' % e)
EAL.save_loaded_asset(rig)
L('aurex chains %s' % [str(c.get_editor_property('chain_name')) for c in rc.get_retarget_chains()] if hasattr(rc, 'get_retarget_chains') else 'chains ?')
rt = AT.create_asset('RTG_UEFN_to_Aurex', RD, unreal.IKRetargeter, unreal.IKRetargetFactory())
tc = unreal.IKRetargeterController.get_controller(rt)
tc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, SRC_RIG); tc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, rig)
tc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, SRC_MESH); tc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, TGT_MESH)
tc.add_default_ops(); tc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True); tc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
EAL.save_loaded_asset(rt)
U = '/Game/Characters/UEFN_Mannequin'
want = []
for c in ('idle', 'run', 'jump', 'fall', 'cast_a_spell', 'box_01', 'box_02', 'front_kick_01', 'slash', 'hit_to_body_01', 'cheer'):
    want.append((U + '/Actions/GEN_' + c, 'Actions'))
gasp = ['Stand_Idle_Loop', 'Sprint_Loop_F', 'Jump_Loop_Fall', 'Jump_F_Start_Run_Lfoot', 'Jump_F_Start_Stand_Lfoot', 'Jump_F_Land_Run_Light_Lfoot',
        'Jump_F_Land_Stand_Light_Lfoot', 'Jump_F_Land_Stand_Heavy_Lfoot', 'Jump_F_Land_Roll_Lfoot']
for d in ('F', 'B', 'LL', 'RR', 'FL', 'FR', 'BL', 'BR'): gasp += ['Walk_Loop_' + d, 'Run_Loop_' + d]
for g in gasp:
    sub = 'Idle' if g.startswith('Stand_Idle') else 'Walk' if g.startswith('Walk') else 'Run' if g.startswith('Run') else 'Sprint' if g.startswith('Sprint') else 'Jump'
    want.append((U + '/Animations/%s/M_Neutral_%s' % (sub, g), 'Animations/' + sub))
for r in ('MM_Rifle_Idle_Hipfire', 'MM_Rifle_Idle_ADS', 'MM_Rifle_Jog_Fwd', 'MM_Rifle_Jog_Bwd', 'MM_Rifle_Jog_Left', 'MM_Rifle_Jog_Right', 'MM_Rifle_Walk_Fwd',
          'MM_Rifle_Walk_Bwd', 'MM_Rifle_Walk_Left', 'MM_Rifle_Walk_Right', 'MM_Rifle_Jump_Start', 'MM_Rifle_Jump_Fall_Loop', 'MM_Rifle_Jump_Fall_Land',
          'MM_Rifle_Fire', 'MM_Rifle_Melee', 'MM_HitReact_Front_Lgt_01', 'MF_Emote_FingerGuns', 'MM_Dash_Forward'):
    want.append((U + '/Rifle/R_' + r, 'Rifle'))
ok = 0
for src, sub in want:
    if not EAL.does_asset_exist(src): L('missing ' + src); continue
    ad = EAL.find_asset_data(src)
    out = unreal.IKRetargetBatchOperation.duplicate_and_retarget([ad], SRC_MESH, TGT_MESH, rt, '', '', 'AXTMP_', '', False)
    for a in out:
        made = str(a.package_name) if hasattr(a, 'package_name') else a.get_path_name().split('.')[0]
        dst = '/Game/Mecha_Aurex/Anims/%s/%s' % (sub, src.split('/')[-1])
        if EAL.does_asset_exist(dst): EAL.delete_asset(dst)
        if EAL.rename_asset(made, dst): ok += 1
L('retargeted %d / %d' % (ok, len(want)))
# ---- 3. material parameters
for mp in ('/Game/Mecha_Aurex/Materials/MI_MechaAurex_CustomColor', '/Game/Mecha_Aurex/Materials/MI_MechaAurex_Color_01', '/Game/Mecha_Aurex/Materials/M_MechaAurex_Master'):
    m = EAL.load_asset(mp)
    if not m: continue
    try:
        MEL = unreal.MaterialEditingLibrary
        L('%s vectors %s' % (mp, [str(n) for n in MEL.get_vector_parameter_names(m)]))
        L('%s scalars %s' % (mp, [str(n) for n in MEL.get_scalar_parameter_names(m)]))
        L('%s textures %s' % (mp, [str(n) for n in MEL.get_texture_parameter_names(m)]))
        if isinstance(m, unreal.MaterialInstanceConstant):
            for n in MEL.get_vector_parameter_names(m): L('  %s = %s' % (n, MEL.get_material_instance_vector_parameter_value(m, n)))
    except Exception as e: L('params %s %s' % (mp, e))
sk = TGT_MESH
for i, m in enumerate(sk.get_editor_property('materials')): L('mesh slot %d %s %s' % (i, m.get_editor_property('material_slot_name'), m.get_editor_property('material_interface')))
L('bounds %s' % sk.get_bounds().box_extent)
EAL.save_directory('/Game/Mecha_Aurex', only_if_is_dirty=True, recursive=True)
L('DONE')
try: unreal.SystemLibrary.quit_editor()
except Exception as e: L('quit %s' % e)
