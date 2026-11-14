# Oct 31 (Claude): generic stand-in hero = Epic UEFN mannequin. Retarget the crystal heroes' Tripo action clips (frost rig) onto it, and dump its material params.
import unreal
L = lambda s: unreal.log('GENRT: ' + str(s))
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
mi = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Materials/MI_UEFN_Mannequin_Blue')
for fn in ('get_vector_parameter_names', 'get_scalar_parameter_names', 'get_texture_parameter_names'):
    try: L('%s: %s' % (fn, [str(n) for n in getattr(unreal.MaterialEditingLibrary, fn)(mi)]))
    except Exception as e: L('%s err %s' % (fn, e))
try:
    for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mi): L('vec %s = %s' % (n, unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mi, n)))
except Exception as e: L('vec vals err %s' % e)
mesh = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
L('mesh slots: %s' % [str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('materials')])
SRC_MESH = EAL.load_asset('/Game/Zoogis/Ultimates/ult_frost_blue/ult_frost_blue')
SRC_RIG = EAL.load_asset('/Game/Zoogis/Ultimates/Retarget/IK_ult_frost_blue')
TGT_RIG = EAL.load_asset('/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin')
OUT = '/Game/Characters/UEFN_Mannequin/Actions'
p = '/Game/Zoogis/Ultimates/Retarget/RTG_frost_to_UEFN'
if EAL.does_asset_exist(p): EAL.delete_asset(p)
rt = AT.create_asset('RTG_frost_to_UEFN', '/Game/Zoogis/Ultimates/Retarget', unreal.IKRetargeter, unreal.IKRetargetFactory())
tc = unreal.IKRetargeterController.get_controller(rt)
tc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, SRC_RIG); tc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, TGT_RIG)
tc.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, SRC_MESH); tc.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, mesh)
tc.add_default_ops(); tc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True); tc.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
EAL.save_loaded_asset(rt)
pre = 'tripo_convert_c56d5f01-e4d4-43ca-98a0-612995efea9a_Anim_'
names = ['box_01', 'box_02', 'cast_a_spell', 'slash', 'front_kick_01', 'hit_to_body_01', 'cheer']
ads = [EAL.find_asset_data('/Game/Zoogis/Ultimates/ult_frost_blue/' + pre + n) for n in names]
out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(ads, SRC_MESH, mesh, rt, pre, 'GEN_', '', '', False)
moved = 0
for a in out:
    src = str(a.package_name) if hasattr(a, 'package_name') else a.get_path_name().split('.')[0]
    np = OUT + '/' + src.split('/')[-1]
    if EAL.does_asset_exist(np): EAL.delete_asset(np)
    if EAL.rename_asset(src, np): moved += 1
L('actions retargeted %d moved %d: %s' % (len(out), moved, [str(a.asset_name) if hasattr(a, 'asset_name') else a.get_name() for a in out]))
EAL.save_directory('/Game/Characters/UEFN_Mannequin', only_if_is_dirty=True, recursive=True)
L('DONE')
try: unreal.SystemLibrary.quit_editor()
except Exception as e: L('quit %s' % e)
