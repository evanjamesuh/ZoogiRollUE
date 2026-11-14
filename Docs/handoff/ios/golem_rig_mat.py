# Nov 10 b218 (Claude): the rigged golem rendered near-black with Tripo's FBX material - give SK_Golem a plain stone material:
# its own (retopo-baked) basecolor + normal, roughness 0.85, metallic 0, used with skeletal meshes.
import unreal, time
LOG = 'C:/dev/ios/golem_rig_mat_log.txt'
log = open(LOG, 'w')
def L(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
D = '/Game/Zoogis/MeadowGolemRig'
try:
    MEL = unreal.MaterialEditingLibrary
    old = unreal.load_asset(D + '/tripo_mat_553bc662')
    try:
        L('old material %s' % old)
        for p in ['blend_mode', 'shading_model', 'used_with_skeletal_mesh', 'two_sided']:
            try: L('  %s = %s' % (p, old.get_editor_property(p)))
            except Exception as e: L('  %s ? %r' % (p, e))
        L('  textures %s' % [str(t.get_name()) for t in MEL.get_used_textures(old)])
    except Exception as e: L('old inspect %r' % e)
    bc = unreal.load_asset(D + '/stone_golem_3d_model_basecolor')
    nm = unreal.load_asset(D + '/stone_golem_3d_model_normal')
    bc.set_editor_property('srgb', True); unreal.EditorAssetLibrary.save_loaded_asset(bc)
    nm.set_editor_property('srgb', False); nm.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); unreal.EditorAssetLibrary.save_loaded_asset(nm)
    path = D + '/M_GolemRig'
    if unreal.EditorAssetLibrary.does_asset_exist(path): m = unreal.load_asset(path); MEL.delete_all_material_expressions(m)
    else: m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GolemRig', D, unreal.Material, unreal.MaterialFactoryNew())
    t1 = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, -100); t1.set_editor_property('texture', bc)
    t1.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    MEL.connect_material_property(t1, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    t2 = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 250); t2.set_editor_property('texture', nm)
    t2.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    MEL.connect_material_property(t2, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 120); r.set_editor_property('r', 0.85)
    MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    k = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 180); k.set_editor_property('r', 0.0)
    MEL.connect_material_property(k, '', unreal.MaterialProperty.MP_METALLIC)
    m.set_editor_property('used_with_skeletal_mesh', True)
    MEL.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)
    sk = unreal.load_asset(D + '/SK_Golem')
    mats = sk.get_editor_property('materials'); out = []
    for sm in mats:
        sm.set_editor_property('material_interface', m); out.append(sm)
    sk.set_editor_property('materials', out); unreal.EditorAssetLibrary.save_loaded_asset(sk)
    L('SK_Golem now uses M_GolemRig on %d slot(s)' % len(out))
except Exception as e:
    L('ERROR %r' % e)
L('DONE'); log.close()
