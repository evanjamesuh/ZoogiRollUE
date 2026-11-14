# Oct 31 (Claude): Lyra rifle locomotion / actions (UE5 Manny skeleton) -> the generic UEFN mannequin hero (/Game/Characters/UEFN_Mannequin/Rifle/R_*).
import unreal
L = lambda s: unreal.log('RIFLERT: ' + str(s))
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
SRC_MESH = EAL.load_asset('/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny')
SRC_RIG = EAL.load_asset('/Game/Characters/Heroes/Mannequin/Rig/IK_Mannequin')
TGT_MESH = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
TGT_RIG = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin')
L('src %s %s tgt %s %s' % (SRC_MESH, SRC_RIG, TGT_MESH, TGT_RIG))
p = '/Game/Zoogis/Ultimates/Retarget/RTG_Manny_to_UEFN'
if EAL.does_asset_exist(p): EAL.delete_asset(p)
rt = AT.create_asset('RTG_Manny_to_UEFN', '/Game/Zoogis/Ultimates/Retarget', unreal.IKRetargeter, unreal.IKRetargetFactory())
tc = unreal.IKRetargeterController.get_controller(rt)
tc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, SRC_RIG); tc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, TGT_RIG)
tc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, SRC_MESH); tc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, TGT_MESH)
tc.add_default_ops(); tc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True); tc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
EAL.save_loaded_asset(rt)
paths = [a.split('.')[0] for a in EAL.list_assets('/Game/Characters/Heroes/Mannequin/Animations', recursive=True)]
ads = [EAL.find_asset_data(p) for p in paths]
ads = [a for a in ads if str(a.asset_class_path.asset_name) == 'AnimSequence']
L('source clips %d' % len(ads))
out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, SRC_MESH, TGT_MESH, rt, '', '', 'R_', '', False)
moved = 0
for a in out:
    src = str(a.package_name) if hasattr(a, 'package_name') else a.get_path_name().split('.')[0]
    np = '/Game/Characters/UEFN_Mannequin/Rifle/' + src.split('/')[-1]
    if EAL.does_asset_exist(np): EAL.delete_asset(np)
    if EAL.rename_asset(src, np): moved += 1
L('retargeted %d moved %d' % (len(out), moved))
rifle = EAL.load_asset('/Game/Weapons/Rifle/Mesh/SM_Rifle')
if rifle:
    L('rifle sockets: %s bounds %s' % ([str(s.socket_name) for s in rifle.get_editor_property('sockets')], rifle.get_bounds().box_extent))
    for i, m in enumerate(rifle.get_editor_property('static_materials')):
        mi = m.get_editor_property('material_interface'); L('rifle mat %d %s parent %s' % (i, mi, mi.get_editor_property('parent') if mi and hasattr(mi, 'parent') else None))
EAL.save_directory('/Game/Characters/UEFN_Mannequin', only_if_is_dirty=True, recursive=True)
L('DONE')
try: unreal.SystemLibrary.quit_editor()
except Exception as e: L('quit %s' % e)
