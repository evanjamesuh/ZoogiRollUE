# Frozen rev_realism_v2: textures + M_RR2_Atlas / M_RR2_Shared masters + MIs for the swapped heroes (IMPORT.md step 3).
import os, unreal
PACK = os.environ['FROZ_PACK']            # .../rev_realism_v2
TD, MD = '/Game/Frozen/RR2/Textures', '/Game/Frozen/RR2/Materials'
EAL, MEL = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
def log(s): unreal.log_warning('FROZRR2M: ' + s)
def imp(path):
    n = os.path.splitext(os.path.basename(path))[0]
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = TD; t.destination_name = n
    t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t]); a = EAL.load_asset(TD + '/' + n)
    if a is None: log('ERROR texture ' + n); return None
    k = n.rsplit('_', 1)[1]
    if k == 'N': a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('flip_green_channel', True); a.set_editor_property('srgb', False)
    elif k in ('ORM', 'AO'): a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
    else: a.set_editor_property('srgb', True)
    EAL.save_loaded_asset(a); return a
def master(name, shared):
    p = MD + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    m = AT.create_asset(name, MD, unreal.Material, unreal.MaterialFactoryNew())
    def tp(pn, x, y, st):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        e.set_editor_property('parameter_name', pn); e.set_editor_property('sampler_type', st); return e
    bc = tp('BC', -800, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nn = tp('N', -800, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = tp('ORM', -800, 400, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    if shared:
        vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, -800, -550)
        mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, -300)
        MEL.connect_material_expressions(bc, 'RGB', mul, 'A'); MEL.connect_material_expressions(vc, '', mul, 'B')
        MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        MEL.connect_material_property(bc, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
        e = tp('E', -800, 700, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        e.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/Black'))
        s = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -800, 950)
        s.set_editor_property('parameter_name', 'EmissiveStrength'); s.set_editor_property('default_value', 0.0)
        me = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 750)
        MEL.connect_material_expressions(e, 'RGB', me, 'A'); MEL.connect_material_expressions(s, '', me, 'B')
        MEL.connect_material_property(me, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(nn, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(orm, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(orm, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    mt = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 550)
    MEL.connect_material_property(mt, '', unreal.MaterialProperty.MP_METALLIC)
    MEL.recompile_material(m); EAL.save_loaded_asset(m); return m
def mi(name, parent, tex, strength):
    p = MD + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    i = AT.create_asset(name, MD, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(i, parent)
    for k, t in tex.items(): MEL.set_material_instance_texture_parameter_value(i, k, t)
    if strength is not None: MEL.set_material_instance_scalar_parameter_value(i, 'EmissiveStrength', strength)
    EAL.save_loaded_asset(i); return i
B = os.path.join(PACK, 'bakes'); T = os.path.join(PACK, 'textures')
heroes = {'MI_RR2_Frozen_ResonancePillar': ('T_RR2_SM_Frozen_ResonancePillar', 0.791), 'MI_RR2_Frozen_IceObelisk_s0': ('T_RR2_SM_Frozen_IceObelisk_s0', 0.758),
          'MI_RR2_Frozen_CrystalBumper': ('T_RR2_SM_Frozen_CrystalBumper', 0.335), 'MI_RR2_FrozenOrbPocket': ('T_RR2_SM_FrozenOrbPocket', 0.0), 'MI_RR2_FrozenPortal': ('T_RR2_FrozenPortal', 1.0)}
nt = 0; ma = master('M_RR2_Atlas', False); ms = master('M_RR2_Shared', True)
for n, (stem, s) in heroes.items():
    tex = {}
    for k in ('BC', 'N', 'ORM', 'E'):
        f = os.path.join(B, stem + '_' + k + '.png')
        if os.path.exists(f):
            a = imp(f); nt += a is not None
            if a: tex[k] = a
    mi(n, ma, tex, s); log('MI %s tex=%s E=%s' % (n, ','.join(sorted(tex)), s))
tex = {k: imp(os.path.join(T, 'T_RR_Shared_Ice_' + k + '.png')) for k in ('BC', 'N', 'ORM')}; nt += 3
mi('MI_RR2_Shared_Ice', ms, tex, None)
log('DONE textures=%d mis=%d' % (nt, len(heroes) + 1))
