# Zoogi Roll Titan Awaken FX assets (Titan card stone shell). Re-runnable.
# Builds /Game/VFX/TitanFX/{T_TitanCrack, M_TitanStone, MI_TitanStone, M_TitanSprite, MI_TitanDust, MI_TitanEmber,
#   NS_TitanDust, NS_TitanEmbers} used by AZoogiTitanShell (Source/ZoogiRoll/Private/ZoogiTitanShell.cpp).
# Stone albedo reuses the KriptoFX Effect17 (golem) rock texture; the crack mask is the arena Titan glow crack.
import unreal, os
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
L = unreal.ZoogiFXLibrary
ROOT = '/Game/VFX/TitanFX'
HERE = os.path.dirname(os.path.abspath(__file__))
TPL = '/Niagara/DefaultAssets/Templates/Emitters/'
LC = unreal.LinearColor
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Niagara', '/Game/VFX'], True)
def log(s): unreal.log('TITANFX ' + s)

def fresh(name, cls=unreal.Material, factory=None):
    p = ROOT + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    return AT.create_asset(name, ROOT, cls, factory or unreal.MaterialFactoryNew())
def E(m, cls): return MEL.create_material_expression(m, cls, 0, 0)
def C(a, ao, b, bi):
    if not MEL.connect_material_expressions(a, ao, b, bi): log('CONNECT FAILED %s.%s -> %s.%s' % (a.get_class().get_name(), ao, b.get_class().get_name(), bi))
def S(m, n, v):
    e = E(m, unreal.MaterialExpressionScalarParameter); e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', v); return e
def V(m, n, v):
    e = E(m, unreal.MaterialExpressionVectorParameter); e.set_editor_property('parameter_name', n); e.set_editor_property('default_value', v); return e
def K(m, v):
    e = E(m, unreal.MaterialExpressionConstant); e.set_editor_property('r', v); return e
def TX(m, n, tex, uv=None):
    e = E(m, unreal.MaterialExpressionTextureSampleParameter2D); e.set_editor_property('parameter_name', n); e.set_editor_property('texture', tex)
    st = tex.get_editor_property('srgb') if tex else True
    e.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if st else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    if uv is not None: C(uv, '', e, 'UVs')
    return e
def O(m, cls, a, b, ao='', bo=''):
    e = E(m, cls)
    if a is not None: C(a, ao, e, 'A')
    if b is not None: C(b, bo, e, 'B')
    return e
MUL, ADD = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd
def finish(m):
    MEL.recompile_material(m); EAL.save_loaded_asset(m); return m

def import_tex():
    f = os.path.join(HERE, 'T_TitanCrack.png')
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', f); t.set_editor_property('destination_path', ROOT)
    t.set_editor_property('automated', True); t.set_editor_property('replace_existing', True); t.set_editor_property('save', False)
    AT.import_asset_tasks([t])
    a = unreal.load_asset(ROOT + '/T_TitanCrack')
    a.set_editor_property('srgb', False)
    EAL.save_loaded_asset(a)
    return a

def build_stone(crack):
    # Opaque lit stone. Vertex colour R = seam heat (slab sides / underside / rim), G = outer face.
    # Emissive = SeamColor * SeamGlow * R^1.5  +  SeamColor * CrackGlow * crack * G.  Base colour cools to charcoal where hot.
    m = fresh('M_TitanStone')
    rock = unreal.load_asset('/Game/VFX/KriptoFX/Effect17/Textures/T_Rock')
    tc = E(m, unreal.MaterialExpressionTextureCoordinate)
    t = TX(m, 'StoneTex', rock, O(m, MUL, tc, S(m, 'StoneTile', 0.8)))
    vc = E(m, unreal.MaterialExpressionVertexColor)
    tint = V(m, 'Tint', LC(0.80, 0.80, 0.80, 1))
    # grey stone like the card: desaturated KriptoFX golem rock, lifted
    lum = E(m, unreal.MaterialExpressionDotProduct); C(t, 'RGB', lum, 'A')
    w = E(m, unreal.MaterialExpressionConstant3Vector); w.set_editor_property('constant', LC(0.33, 0.33, 0.33, 1)); C(w, '', lum, 'B')
    stone = O(m, ADD, O(m, MUL, O(m, MUL, lum, K(m, 0.9)), tint), S(m, 'Lift', 0.10))
    # hot faces (slab sides / underside, vertex R > 0.5) read orange even under daylight
    hotbase = V(m, 'HotBase', LC(0.55, 0.12, 0.02, 1))
    ha = E(m, unreal.MaterialExpressionSaturate); C(O(m, MUL, O(m, unreal.MaterialExpressionSubtract, vc, K(m, 0.5), 'R', ''), K(m, 2.5)), '', ha, '')
    lerp = E(m, unreal.MaterialExpressionLinearInterpolate); C(stone, '', lerp, 'A'); C(hotbase, '', lerp, 'B'); C(ha, '', lerp, 'Alpha')
    MEL.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(S(m, 'Roughness', 0.88), '', unreal.MaterialProperty.MP_ROUGHNESS)
    seamc = V(m, 'SeamColor', LC(1.0, 0.30, 0.04, 1))
    heat = O(m, MUL, vc, vc, 'R', 'R')
    e1 = O(m, MUL, O(m, MUL, seamc, S(m, 'SeamGlow', 1.6)), heat)
    ck = TX(m, 'CrackTex', crack, O(m, MUL, tc, K(m, 1.0)))
    e2 = O(m, MUL, O(m, MUL, O(m, MUL, seamc, S(m, 'CrackGlow', 0.0)), ck, '', 'R'), vc, '', 'G')
    # card-like even lighting: a little self-lit stone so slabs in the ball's own shadow stay readable grey
    amb = O(m, MUL, O(m, MUL, stone, S(m, 'Ambient', 0.75)), vc, '', 'G')
    MEL.connect_material_property(O(m, ADD, O(m, ADD, e1, e2), amb), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    mi = fresh('MI_TitanStone', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, m)
    EAL.save_loaded_asset(mi)

def build_sprite():
    m = fresh('M_TitanSprite')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    for prop in ('used_with_niagara_sprites', 'used_with_niagara_mesh_particles'):
        m.set_editor_property(prop, True)
    # soft round puff: 1 - |uv - 0.5| * 2, squared
    tc = E(m, unreal.MaterialExpressionTextureCoordinate)
    half = E(m, unreal.MaterialExpressionConstant2Vector); half.set_editor_property('r', 0.5); half.set_editor_property('g', 0.5)
    d = O(m, unreal.MaterialExpressionSubtract, tc, half)
    ln = E(m, unreal.MaterialExpressionLength); C(d, '', ln, '')
    om = E(m, unreal.MaterialExpressionOneMinus); C(O(m, MUL, ln, K(m, 2.0)), '', om, '')
    sat = E(m, unreal.MaterialExpressionSaturate); C(om, '', sat, '')
    puff = O(m, MUL, sat, sat)
    pc = E(m, unreal.MaterialExpressionParticleColor)
    MEL.connect_material_property(O(m, MUL, O(m, MUL, pc, puff, 'RGB', ''), S(m, 'Glow', 1.0)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(O(m, MUL, O(m, MUL, pc, puff, 'A', ''), S(m, 'Opacity', 1.0)), '', unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    def mi(n, params):
        x = fresh(n, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(x, m)
        for k, v in params.items(): MEL.set_material_instance_scalar_parameter_value(x, k, v)
        EAL.save_loaded_asset(x)
        return x
    return mi('MI_TitanDust', {'Glow': 0.55, 'Opacity': 0.55, 'Softness': 1.4}), mi('MI_TitanEmber', {'Glow': 7.0, 'Opacity': 1.0, 'Softness': 2.5})

def system(name, tpl, mat, cfg):
    p = ROOT + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    s = AT.create_asset(name, ROOT, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    en = 'K' + name.split('_')[-1]
    if not L.add_emitter_from_asset(s, unreal.load_asset(TPL + tpl), en): log('add emitter FAILED ' + name); return
    L.set_emitter_local_space(s, en, False)
    L.set_niagara_float(s, en, 'EmitterState.MaxDistance', 1000000.0)
    L.set_emitter_render(s, en, mat, None)
    for k, v in cfg.items():
        if isinstance(v, int) and not isinstance(v, bool): n = L.set_niagara_int(s, en, k, v)
        elif isinstance(v, unreal.LinearColor): n = L.set_niagara_color(s, en, k, v)
        elif isinstance(v, unreal.Vector): n = L.set_niagara_vector(s, en, k, v)
        else: n = L.set_niagara_float(s, en, k, float(v))
        if not n: log('%s: input not found %s' % (name, k))
    log('%s compile: %s' % (name, L.compile_report(s)))
    EAL.save_loaded_asset(s)

def build_niagara(dust, ember):
    # dust ring: flat burst along the floor (component scale 1 = Zoogi-sized thump)
    system('NS_TitanDust', 'OmnidirectionalBurst', dust, {
        'SpawnBurst_Instantaneous.Spawn Count': 26, 'InitializeParticle.Lifetime Min': 0.6, 'InitializeParticle.Lifetime Max': 1.3,
        'Uniform Sprite Size Min': 30.0, 'Uniform Sprite Size Max': 70.0, 'InitializeParticle.Color': LC(0.62, 0.55, 0.47, 0.55),
        'RandomRangeFloat.Minimum': 120.0, 'RandomRangeFloat.Maximum': 340.0, 'GravityForce.Gravity': unreal.Vector(0, 0, 40), 'Drag.Drag': 3.5,
        'ShapeLocation.Sphere Radius': 45.0, 'ShapeLocation.Non Uniform Scale': unreal.Vector(1, 1, 0.08)})
    # embers: small orange sparks drifting off the seams
    system('NS_TitanEmbers', 'OmnidirectionalBurst', ember, {
        'SpawnBurst_Instantaneous.Spawn Count': 10, 'InitializeParticle.Lifetime Min': 0.5, 'InitializeParticle.Lifetime Max': 1.2,
        'Uniform Sprite Size Min': 2.0, 'Uniform Sprite Size Max': 5.0, 'InitializeParticle.Color': LC(1.0, 0.45, 0.1, 1),
        'RandomRangeFloat.Minimum': 40.0, 'RandomRangeFloat.Maximum': 160.0, 'GravityForce.Gravity': unreal.Vector(0, 0, 60), 'Drag.Drag': 1.5,
        'ShapeLocation.Sphere Radius': 12.0})

crack = import_tex()
build_stone(crack)
d, e = build_sprite()
build_niagara(d, e)
log('done')
