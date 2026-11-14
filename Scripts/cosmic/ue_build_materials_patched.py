"""rev_realism2 - UE 5.6 editor Python: import the 9 textures, build the 5 parent materials and the 47 MIs listed in materials.json.
Run in the editor (Tools > Execute Python Script) BEFORE importing meshes/*.fbx, so the importer binds every slot by name.
Self-contained: reads only this pack (textures folder + materials.json beside this script). Destination: DEST below.
NOT RUN IN UNREAL BY THE AUTHOR (box has no Unreal) - parent graphs follow materials.json 'parents' text; check one MI per parent visually."""
import os, json, unreal
DEST = '/Game/Cosmic/R2'
HERE = os.environ.get('COSMIC_R2_MATDIR') or os.path.dirname(os.path.abspath(__file__)); PACK = os.path.dirname(HERE)  # ZOOGI patch: env fallback
MJ = json.load(open(os.path.join(HERE, 'materials.json')))
AT = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; EAL = unreal.EditorAssetLibrary
# ---------------------------------------------------------------- textures
CMP = {'Masks': unreal.TextureCompressionSettings.TC_MASKS, 'Normalmap': unreal.TextureCompressionSettings.TC_NORMALMAP, 'Default': unreal.TextureCompressionSettings.TC_DEFAULT}
tasks = []
for name, t in MJ['textures'].items():
    k = unreal.AssetImportTask(); k.filename = os.path.join(PACK, t['file']); k.destination_path = DEST + '/Textures'; k.destination_name = name
    k.automated = True; k.replace_existing = True; k.save = False; tasks.append(k)
AT.import_asset_tasks(tasks)
TEX = {}
for name, t in MJ['textures'].items():
    tx = unreal.load_asset(f'{DEST}/Textures/{name}'); key = t['compression'].split(' ')[0]
    tx.set_editor_property('srgb', bool(t['srgb'])); tx.set_editor_property('compression_settings', CMP.get(key, CMP['Default']))
    if key == 'Normalmap': tx.set_editor_property('flip_green_channel', False)   # maps are already DirectX (green down)
    EAL.save_loaded_asset(tx); TEX[name] = tx
# ---------------------------------------------------------------- helpers
def new_mat(name):
    p = f'{DEST}/Materials/{name}'
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    return AT.create_asset(name, f'{DEST}/Materials', unreal.Material, unreal.MaterialFactoryNew())
def E(m, cls, x, y, **props):
    e = MEL.create_material_expression(m, cls, x, y)
    for k, v in props.items(): e.set_editor_property(k, v)
    return e
def S(m, name, v, x, y): return E(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)
def V(m, name, v, x, y): return E(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*v))
def T(m, name, tex, st, x, y, uv=None):
    e = E(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=tex, sampler_type=st)
    if uv is not None: MEL.connect_material_expressions(uv, '', e, 'UVs')
    return e
def C(a, ao, b, bi): MEL.connect_material_expressions(a, ao, b, bi)
def P(e, out, prop): MEL.connect_material_property(e, out, prop)
def op(m, cls, a, ao, b, bo, x, y, ia='A', ib='B'):
    e = E(m, cls, x, y); C(a, ao, e, ia); C(b, bo, e, ib); return e
MP = unreal.MaterialProperty; ST = unreal.MaterialSamplerType; Mul = unreal.MaterialExpressionMultiply; Lerp = unreal.MaterialExpressionLinearInterpolate; Add = unreal.MaterialExpressionAdd
def finish(m): MEL.layout_material_expressions(m); MEL.recompile_material(m); EAL.save_loaded_asset(m)
# ---------------------------------------------------------------- M_Cosmic_Trim_R2
m = new_mat('M_Cosmic_Trim_R2'); m.set_editor_property('two_sided', True)
bc = T(m, 'BaseColorTex', TEX['T_Cosmic_Trim_BaseColor_R2'], ST.SAMPLERTYPE_COLOR, -1400, -400)
orm = T(m, 'ORMTex', TEX['T_Cosmic_Trim_ORM_R2'], ST.SAMPLERTYPE_MASKS, -1400, 0)
nrm = T(m, 'NormalTex', TEX['T_Cosmic_Trim_N_R2'], ST.SAMPLERTYPE_NORMAL, -1400, 400)
vc = E(m, unreal.MaterialExpressionVertexColor, -1400, 800)
tint = S(m, 'BaseTint', 1.0, -1100, -250); rs = S(m, 'RoughScale', 1.0, -1100, 100); met = S(m, 'Metallic', 0.3, -1100, 200)
wa = S(m, 'WearAmount', 0.05, -1100, 300); wr_ = S(m, 'WearRoughness', 0.3, -1100, 350); wmet = S(m, 'WearMetallic', 1.0, -1100, 400)
glass = S(m, 'GlassRoughness', -1.0, -1100, 450); useg = S(m, 'UseGlass', 0.0, -1100, 500); wcol = V(m, 'WearColor', (0.58, 0.55, 0.70, 1), -1100, -150)
# wear mask = smoothstep(0.42, 0.55, VertexColor.a x Noise(WorldPos))
wp = E(m, unreal.MaterialExpressionWorldPosition, -1400, 1000)
nz = E(m, unreal.MaterialExpressionNoise, -1200, 1000, scale=0.18, levels=6, output_min=0.0, output_max=1.0); C(wp, '', nz, 'Position')
am = op(m, Mul, vc, 'A', nz, '', -1000, 900)
ss = E(m, unreal.MaterialExpressionSmoothStep, -850, 900, const_min=0.42, const_max=0.55); C(am, '', ss, 'Value')
b1 = op(m, Mul, bc, 'RGB', vc, '', -900, -400); b2 = op(m, Mul, b1, '', tint, '', -750, -400)
wb = op(m, Mul, ss, '', wa, '', -750, -100)
base = E(m, Lerp, -550, -400); C(b2, '', base, 'A'); C(wcol, '', base, 'B'); C(wb, '', base, 'Alpha'); P(base, '', MP.MP_BASE_COLOR)
r1 = op(m, Mul, orm, 'G', rs, '', -900, 100)
lum = E(m, unreal.MaterialExpressionDotProduct, -900, 600); C(vc, '', lum, 'A'); C(E(m, unreal.MaterialExpressionConstant3Vector, -1050, 650, constant=unreal.LinearColor(0.2126, 0.7152, 0.0722, 1)), '', lum, 'B')
gm = E(m, unreal.MaterialExpressionSmoothStep, -750, 600, const_min=0.01, const_max=0.03); C(lum, '', gm, 'Value')
gl = E(m, Lerp, -600, 450); C(glass, '', gl, 'A'); C(r1, '', gl, 'B'); C(gm, '', gl, 'Alpha')
rg = E(m, Lerp, -450, 200); C(r1, '', rg, 'A'); C(gl, '', rg, 'B'); C(useg, '', rg, 'Alpha')
rough = E(m, Lerp, -300, 200); C(rg, '', rough, 'A'); C(wr_, '', rough, 'B'); C(ss, '', rough, 'Alpha'); P(rough, '', MP.MP_ROUGHNESS)
metal = E(m, Lerp, -300, 350); C(met, '', metal, 'A'); C(wmet, '', metal, 'B'); C(ss, '', metal, 'Alpha'); P(metal, '', MP.MP_METALLIC)
P(orm, 'R', MP.MP_AMBIENT_OCCLUSION); P(nrm, 'RGB', MP.MP_NORMAL); finish(m); PARENT = {'M_Cosmic_Trim_R2': m}
# ---------------------------------------------------------------- M_Cosmic_Seating_R2
m = new_mat('M_Cosmic_Seating_R2'); m.set_editor_property('two_sided', True)
P(T(m, 'BaseColorTex', TEX['T_BowlSeating_R2'], ST.SAMPLERTYPE_COLOR, -600, 0), 'RGB', MP.MP_BASE_COLOR)
P(S(m, 'Metallic', 0.2, -600, 300), '', MP.MP_METALLIC); P(S(m, 'Roughness', 0.6, -600, 350), '', MP.MP_ROUGHNESS); P(S(m, 'Specular', 0.3, -600, 400), '', MP.MP_SPECULAR)
finish(m); PARENT['M_Cosmic_Seating_R2'] = m
# ---------------------------------------------------------------- M_Cosmic_Glow_R2 / M_Cosmic_GlowTranslucent_R2
for name, trans in (('M_Cosmic_Glow_R2', False), ('M_Cosmic_GlowTranslucent_R2', True)):
    m = new_mat(name); m.set_editor_property('two_sided', True)
    if trans:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
        P(S(m, 'Opacity', 0.4, -600, 600), '', MP.MP_OPACITY)
    P(V(m, 'BaseColor', (1, 1, 1, 1), -600, 0), '', MP.MP_BASE_COLOR)
    P(S(m, 'Metallic', 0.0, -600, 200), '', MP.MP_METALLIC); P(S(m, 'Roughness', 0.5, -600, 250), '', MP.MP_ROUGHNESS); P(S(m, 'Specular', 0.5, -600, 300), '', MP.MP_SPECULAR)
    e1 = op(m, Mul, V(m, 'EmissiveColor', (0, 0, 0, 1), -800, 400), '', S(m, 'EmissiveStrength', 1.0, -800, 500), '', -600, 400)
    P(op(m, Mul, e1, '', S(m, 'GlowStrength', 1.0, -800, 550), '', -450, 450), '', MP.MP_EMISSIVE_COLOR); finish(m); PARENT[name] = m
# ---------------------------------------------------------------- M_Cosmic_Floor_R2
m = new_mat('M_Cosmic_Floor_R2')
uv = E(m, unreal.MaterialExpressionTextureCoordinate, -1500, 0, coordinate_index=0)
dt = S(m, 'DetailTiling', 24.0, -1500, 500); duv = op(m, Mul, uv, '', dt, '', -1300, 500)
P(T(m, 'BaseColorTex', TEX['T_CosmicFloor_BaseColor_R2'], ST.SAMPLERTYPE_COLOR, -900, -300, uv), 'RGB', MP.MP_BASE_COLOR)
orm = T(m, 'ORMTex', TEX['T_CosmicFloor_ORM_R2'], ST.SAMPLERTYPE_MASKS, -900, 0, uv)
P(orm, 'R', MP.MP_AMBIENT_OCCLUSION); P(orm, 'G', MP.MP_ROUGHNESS); P(orm, 'B', MP.MP_METALLIC)
n1 = T(m, 'NormalTex', TEX['T_CosmicFloor_N_R2'], ST.SAMPLERTYPE_NORMAL, -900, 300, uv); n2 = T(m, 'DetailNormalTex', TEX['T_CosmicFloor_DetailN_R2'], ST.SAMPLERTYPE_NORMAL, -900, 600, duv)
bf = E(m, unreal.MaterialExpressionMaterialFunctionCall, -600, 400); _mf = unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals')
try: bf.set_material_function(_mf)
except Exception: bf.set_editor_property('material_function', _mf)  # ZOOGI patch: fallback if set_material_function isn't exposed
C(n1, 'RGB', bf, 'BaseNormal'); C(n2, 'RGB', bf, 'AdditionalNormal'); P(bf, '', MP.MP_NORMAL)
gt = T(m, 'GlowTex', TEX['T_CosmicGlow_R2'], ST.SAMPLERTYPE_MASKS, -900, 900, uv)
ga = op(m, Mul, gt, 'R', V(m, 'GlowColorA', (0.258183, 0.107023, 1, 1), -1100, 1000), '', -700, 900)
gb = op(m, Mul, gt, 'G', V(m, 'GlowColorB', (0.033105, 0.745404, 1, 1), -1100, 1100), '', -700, 1000)
g = op(m, Add, ga, '', gb, '', -550, 950); g2 = op(m, Mul, g, '', S(m, 'EmissiveStrength', 1.2, -700, 1150), '', -400, 950)
P(op(m, Mul, g2, '', S(m, 'GlowStrength', 1.0, -550, 1200), '', -250, 1000), '', MP.MP_EMISSIVE_COLOR); finish(m); PARENT['M_Cosmic_Floor_R2'] = m
# ---------------------------------------------------------------- MIs
for name, mi in MJ['instances'].items():
    p = f'{DEST}/Materials/{name}'
    inst = unreal.load_asset(p) if EAL.does_asset_exist(p) else AT.create_asset(name, f'{DEST}/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(inst, PARENT[mi['parent']])
    for k, v in mi['scalars'].items(): MEL.set_material_instance_scalar_parameter_value(inst, k, float(v))
    if mi['parent'] == 'M_Cosmic_Trim_R2': MEL.set_material_instance_scalar_parameter_value(inst, 'UseGlass', 1.0 if mi['scalars'].get('GlassRoughness', -1) >= 0 else 0.0)
    for k, v in mi['vectors'].items(): MEL.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(*v))
    for k, v in mi['textures'].items(): MEL.set_material_instance_texture_parameter_value(inst, k, TEX[v])
    MEL.update_material_instance(inst); EAL.save_loaded_asset(inst)
unreal.log(f'rev_realism2: {len(MJ["textures"])} textures, {len(PARENT)} parents, {len(MJ["instances"])} MIs in {DEST}/Materials')
