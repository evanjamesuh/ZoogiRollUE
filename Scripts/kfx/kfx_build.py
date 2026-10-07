# KriptoFX Realistic Effects Pack v4 -> Niagara port (Zoogi Roll). Re-runnable.
# Reads SourceArt/KriptoFX/spec.json (parsed Unity prefabs, see Scripts/kfx/parse_unity.py) and builds per prefab:
#   /Game/VFX/KriptoFX/<EffectN>/NS_KFX_<Prefab>  (+ MI_KFX_* material instances), from engine template emitters.
# Masters: /Game/VFX/KriptoFX/Materials/M_KFX_Add, M_KFX_Blend (unlit flipbook sprites/meshes), M_KFX_MeshLit (rocks/ice).
import unreal, json, os, re, math, statistics
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
L = unreal.ZoogiFXLibrary
ROOT = '/Game/VFX/KriptoFX'
SPEC = json.load(open(os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'KriptoFX', 'spec.json')))
TPL = '/Niagara/DefaultAssets/Templates/Emitters/'
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Niagara', ROOT], True)
REPORT = []
GSCALE = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fbx_scale.json')))
SKIP_MESH = ('Golem', 'Dragon')  # demo character / skinned dragon, not part of the effect
SKIP_MAT = ('Mask.mat', 'Sky.mat', 'MaskRock.mat')  # Unity stencil-mask tricks of the portal
def mesh_scale(mesh, name, s, cap):
    # Unity size = FBX native * meta globalScale; then clamp the final diameter (cm) for Zoogi readability
    gs = GSCALE.get(os.path.splitext(os.path.basename(name or ''))[0].replace(' ', '_'), 1.0)
    b = mesh.get_bounds(); d = max(1.0, 2.0 * max(b.box_extent.x, b.box_extent.y, b.box_extent.z))
    want = min(cap, max(5.0, d * gs * s))
    return want / d

def log(s): unreal.log('KFX ' + s)

# ------------------------------------------------------------ master materials
def fresh_material(name):
    p = ROOT + '/Materials/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    return AT.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())

def E(m, cls, x, y): return MEL.create_material_expression(m, cls, x, y)
def C(a, ao, b, bi): MEL.connect_material_expressions(a, ao, b, bi)
def scalar(m, name, v, x, y):
    e = E(m, unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def vec(m, name, v, x, y):
    e = E(m, unreal.MaterialExpressionVectorParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def tex(m, name, x, y, default='/Engine/EngineResources/WhiteSquareTexture'):
    e = E(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y); e.set_editor_property('parameter_name', name)
    e.set_editor_property('texture', unreal.load_asset(default)); return e
def op(m, cls, a, b, x, y, ao='', bo=''):
    e = E(m, cls, x, y)
    if a is not None: C(a, ao, e, 'A')
    if b is not None: C(b, bo, e, 'B')
    return e
MUL, ADD, DIV = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd, unreal.MaterialExpressionDivide

def usage(m):
    for prop in ('used_with_niagara_sprites', 'used_with_niagara_mesh_particles', 'used_with_niagara_ribbons'):
        try: m.set_editor_property(prop, True)
        except Exception as ex: log('usage %s failed %s' % (prop, ex))

def build_unlit(name, additive):
    m = fresh_material(name)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    usage(m)
    # flipbook: idx = floor(RelTime * TX*TY * 0.999); uv = (uv + (fmod(idx,TX), floor(idx/TX))) / (TX,TY); + Time*Pan
    tx = scalar(m, 'TilesX', 1.0, -1600, 0); ty = scalar(m, 'TilesY', 1.0, -1600, 100)
    rt = E(m, unreal.MaterialExpressionParticleRelativeTime, -1600, 200)
    fr = op(m, MUL, tx, ty, -1400, 50)
    k = E(m, unreal.MaterialExpressionConstant, -1400, 150); k.set_editor_property('r', 0.999)
    t1 = op(m, MUL, rt, k, -1250, 200); t2 = op(m, MUL, t1, fr, -1100, 150)
    idx = E(m, unreal.MaterialExpressionFloor, -950, 150); C(t2, '', idx, '')
    col = E(m, unreal.MaterialExpressionFmod, -800, 100); C(idx, '', col, 'A'); C(tx, '', col, 'B')
    rq = op(m, DIV, idx, tx, -800, 200); row = E(m, unreal.MaterialExpressionFloor, -650, 200); C(rq, '', row, '')
    off = E(m, unreal.MaterialExpressionAppendVector, -500, 150); C(col, '', off, 'A'); C(row, '', off, 'B')
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -800, -100)
    pu = scalar(m, 'PanU', 0.0, -1100, -250); pv = scalar(m, 'PanV', 0.0, -1100, -150)
    pan = E(m, unreal.MaterialExpressionAppendVector, -950, -200); C(pu, '', pan, 'A'); C(pv, '', pan, 'B')
    tm = E(m, unreal.MaterialExpressionTime, -950, -300)
    panT = op(m, MUL, tm, pan, -800, -250)
    uv0 = op(m, ADD, tc, panT, -650, -150)
    uv1 = op(m, ADD, uv0, off, -400, 0)
    tiles = E(m, unreal.MaterialExpressionAppendVector, -500, 300); C(tx, '', tiles, 'A'); C(ty, '', tiles, 'B')
    uv = op(m, DIV, uv1, tiles, -250, 50)
    t = tex(m, 'Tex', -50, 0); C(uv, '', t, 'UVs')
    mk = tex(m, 'MaskTex', -50, 300); C(tc, '', mk, 'UVs')
    pc = E(m, unreal.MaterialExpressionParticleColor, -50, 550)
    tint = vec(m, 'Tint', unreal.LinearColor(1, 1, 1, 1), -50, 700)
    inten = scalar(m, 'Intensity', 1.0, -50, 850)
    rgb1 = op(m, MUL, t, pc, 250, 0, 'RGB', 'RGB'); rgb2 = op(m, MUL, rgb1, tint, 400, 0, '', 'RGB'); rgb = op(m, MUL, rgb2, inten, 550, 0)
    a1 = op(m, MUL, t, mk, 250, 300, 'A', 'R'); a2 = op(m, MUL, a1, pc, 400, 300, '', 'A'); a = op(m, MUL, a2, tint, 550, 300, '', 'A')
    if additive:
        # KriptoFX additive textures are mostly black-backed RGB; alpha-less PNGs import with A=1, so RGB carries the shape.
        em = op(m, MUL, rgb, a, 750, 100)
        MEL.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        # Alpha-blended: opacity = alpha * clamp(luma*3) so RGB-only (no alpha) textures do not show a square.
        d = E(m, unreal.MaterialExpressionDotProduct, 250, 450); C(t, 'RGB', d, 'A')
        third = E(m, unreal.MaterialExpressionConstant3Vector, 100, 500); third.set_editor_property('constant', unreal.LinearColor(1.0, 1.0, 1.0, 1))
        C(third, '', d, 'B')
        sat = E(m, unreal.MaterialExpressionSaturate, 400, 450); C(d, '', sat, '')
        lum = scalar(m, 'LumaAlpha', 1.0, 400, 550)
        lerp = E(m, unreal.MaterialExpressionLinearInterpolate, 550, 500)
        one = E(m, unreal.MaterialExpressionConstant, 400, 650); one.set_editor_property('r', 1.0)
        C(one, '', lerp, 'A'); C(sat, '', lerp, 'B'); C(lum, '', lerp, 'Alpha')
        op_ = op(m, MUL, a, lerp, 750, 350)
        MEL.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        MEL.connect_material_property(op_, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def build_lit():
    m = fresh_material('M_KFX_MeshLit')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    usage(m)
    t = tex(m, 'Tex', -600, 0)
    pc = E(m, unreal.MaterialExpressionParticleColor, -600, 250)
    tint = vec(m, 'Tint', unreal.LinearColor(1, 1, 1, 1), -600, 400)
    b1 = op(m, MUL, t, tint, -300, 0, 'RGB', 'RGB'); b2 = op(m, MUL, b1, pc, -150, 0, '', 'RGB')
    MEL.connect_material_property(b2, '', unreal.MaterialProperty.MP_BASE_COLOR)
    r = scalar(m, 'Roughness', 0.7, -300, 200); MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    fr = E(m, unreal.MaterialExpressionFresnel, -600, 600)
    rim = vec(m, 'RimColor', unreal.LinearColor(0, 0, 0, 1), -600, 750)
    e1 = op(m, MUL, fr, rim, -300, 600, '', 'RGB')
    glow = vec(m, 'Glow', unreal.LinearColor(0, 0, 0, 1), -600, 900)
    e2 = op(m, MUL, t, glow, -300, 800, 'RGB', 'RGB')
    e3 = op(m, ADD, e1, e2, -100, 700)
    MEL.connect_material_property(e3, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

MASTER = {'add': build_unlit('M_KFX_Add', True), 'blend': build_unlit('M_KFX_Blend', False), 'lit': build_lit()}

# ------------------------------------------------------------ helpers
def tex_asset(eff, upath):
    if not upath or '/' not in upath: return None
    n = os.path.splitext(os.path.basename(upath))[0].replace(' ', '_')
    p = '%s/%s/Textures/T_%s' % (ROOT, eff, n)
    return unreal.load_asset(p) if EAL.does_asset_exist(p) else None
def mesh_asset(eff, upath):
    if not upath: return None
    n = os.path.splitext(os.path.basename(upath))[0].replace(' ', '_')
    p = '%s/%s/Meshes/SM_%s' % (ROOT, eff, n)
    return unreal.load_asset(p) if EAL.does_asset_exist(p) else None
_mi = {}
def make_mi(eff, prefab, label, master, params):
    key = (prefab, label)
    name = 'MI_KFX_%s_%s' % (prefab, re.sub(r'[^A-Za-z0-9]', '', label))[:60]
    path = '%s/%s/Materials/%s' % (ROOT, eff, name)
    if key in _mi: return _mi[key]
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    mi = AT.create_asset(name, '%s/%s/Materials' % (ROOT, eff), unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, MASTER[master])
    for k, v in params.items():
        if v is None: continue
        if isinstance(v, unreal.Texture): MEL.set_material_instance_texture_parameter_value(mi, k, v)
        elif isinstance(v, unreal.LinearColor): MEL.set_material_instance_vector_parameter_value(mi, k, v)
        else: MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
    EAL.save_loaded_asset(mi)
    _mi[key] = mi
    return mi

def rng(v, scale=1.0):
    """Unity MinMaxCurve (scalar / [min,max] / curve dict) -> (min,max) * scale."""
    if isinstance(v, (int, float)): return (v * scale, v * scale)
    if isinstance(v, list) and len(v) == 2 and all(isinstance(x, (int, float)) for x in v): return (min(v) * scale, max(v) * scale)
    if isinstance(v, dict):
        c = [x for x in (v.get('curve') or []) if isinstance(x, (int, float))]
        if c: return (min(c) * scale, max(c) * scale)
    return (0.0, 0.0)
def avg(v): a, b = rng(v); return (a + b) * 0.5
def burst_count(e):
    n = 0
    for b in e.get('bursts') or []:
        n += avg(b.get('n'))
    return n
def U2UE(p):  # Unity (x right, y up, z fwd, metres) -> UE (X fwd, Y right, Z up, cm)
    return unreal.Vector(p[2] * 100.0, p[0] * 100.0, p[1] * 100.0)
def lc(c, mul=1.0): return unreal.LinearColor(c[0] * mul, c[1] * mul, c[2] * mul, c[3] if len(c) > 3 else 1.0)
def tint_of(m):
    cs = (m or {}).get('colors') or {}
    return cs.get('_TintColor') or cs.get('_Color') or [1, 1, 1, 1]
def blend_of(m):
    kw = (m or {}).get('keywords') or ''
    t = tint_of(m)
    if '_BLENDMODE_ADD' in kw or 'BlendAdd' in kw: return 'add'
    if max(t[:3]) < 0.05: return 'blend'  # black smoke / black hole cores
    if '_BLENDMODE_BLEND' in kw or '_ALPHABLEND_ON' in kw or 'BlendAlpha' in kw: return 'blend'
    return 'add'
def is_distortion(m):
    return 'Distortion' in ((m or {}).get('shader') or '')

def addem(sysa, tpl, en):
    ok = L.add_emitter_from_asset(sysa, unreal.load_asset(TPL + tpl), en)
    L.set_emitter_local_space(sysa, en, True)  # component transform (scale/attach) applies to offsets, velocities, meshes
    L.set_niagara_float(sysa, en, 'EmitterState.MaxDistance', 1000000.0)  # template distance culling (5000) is measured from the player view
    if not ok: log('add emitter failed %s %s' % (tpl, en))
    return ok
def safe(n): return 'K' + (re.sub(r'[^A-Za-z0-9]', '', n)[:20] or 'E')  # prefix: an emitter named 'Particles' breaks the namespace

# ------------------------------------------------------------ per prefab system
def build_prefab(eff, pf):
    prefab = os.path.splitext(os.path.basename(pf['prefab']))[0]
    sysname = 'NS_KFX_' + prefab
    folder = '%s/%s' % (ROOT, eff)
    path = folder + '/' + sysname
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    sysa = AT.create_asset(sysname, folder, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    ems = [e for e in pf['emitters'] if e.get('active') and (e.get('render') or {}).get('enabled', 1)]
    # recentre: horizontal on the median emitter position, vertical from the lowest emitter
    if ems:
        mx = statistics.median([e['pos'][0] for e in ems]); mz = statistics.median([e['pos'][2] for e in ems]); my = min(e['pos'][1] for e in ems)
    else: mx = mz = my = 0.0
    def pos(p): return U2UE([p[0] - mx, max(0.0, p[1] - my) * 0.6, p[2] - mz])
    used = []; skipped = []
    names = set()
    def uname(base):
        n = safe(base); i = 1; o = n
        while o in names: i += 1; o = '%s%d' % (n, i)
        names.add(o); return o

    for e in ems:
        ren = e.get('render') or {}
        mode = ren.get('mode')
        m = (ren.get('mats') or [None])[0] or {}
        nm = e['name'].split('/')[-1]
        if mode == 5 or not m: skipped.append(nm + ' (trail/light-only)'); continue
        if is_distortion(m): skipped.append(nm + ' (heat distortion)'); continue
        life = rng(e['lifetime'])
        if life[1] <= 0: life = (1.0, 1.0)
        size = rng(e['size'], 100.0)
        speed = rng(e['speed'], 100.0)
        grav = avg(e.get('gravity')) * 980.0
        tint = tint_of(m)
        delay = max(0.0, avg(e.get('delay')))
        dur = float(e.get('duration') or 1.0)
        shape = e.get('shape') or {}
        radius = float(shape.get('radius') or 0.0) * 100.0 if shape.get('enabled') else 0.0
        flat = shape.get('enabled') and shape.get('type') in (10, 13)  # circle / edge
        nburst = burst_count(e)
        rate = avg(e.get('rate'))
        if rate <= 0 and avg(e.get('rateDist')) > 0: rate = avg(e.get('rateDist')) * 8.0  # emitted per metre travelled (projectiles): ~8 m/s
        loop = bool(e.get('loop'))
        if mode == 4:
            mesh = mesh_asset(eff, ren.get('mesh'))
            if not mesh: skipped.append(nm + ' (mesh missing)'); continue
            t = tex_asset(eff, (m.get('tex') or {}).get('_MainTex'))
            mi = make_mi(eff, prefab, 'Mesh_' + os.path.basename(m.get('path', 'x')), 'lit', {'Tex': t, 'Tint': lc(tint if max(tint[:3]) > 0.01 else [0.25, 0.25, 0.25, 1])})
            n = int(min(40, max(1, nburst if nburst > 0 else rate * min(dur, 2.0))))
            en = uname(nm)
            addem(sysa, 'UpwardMeshBurst', en)
            L.set_emitter_render(sysa, en, mi, mesh)
            L.set_niagara_int(sysa, en, 'SpawnBurst_Instantaneous.Spawn Count', n)
            L.set_niagara_float(sysa, en, 'SpawnBurst_Instantaneous.Spawn Time', delay)
            L.set_niagara_float(sysa, en, 'EmitterState.Loop Duration', max(1.0, delay + 0.5))
            L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', min(life[0], 4.0))
            L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', min(life[1], 4.0))
            s0, s1 = rng(e['size'])
            L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Min', mesh_scale(mesh, ren.get('mesh'), s0, 50.0)); L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Max', mesh_scale(mesh, ren.get('mesh'), max(s0, s1), 70.0))
            L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', speed[0]); L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', max(speed[1], speed[0]))
            L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, -grav))
            L.set_niagara_float(sysa, en, 'AddVelocity.Cone Angle', float(shape.get('angle') or 30.0) if shape.get('type') == 4 else 80.0)
            L.set_niagara_color(sysa, en, 'InitializeParticle.Color', unreal.LinearColor(1, 1, 1, 1))
            L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', pos(e['pos']))
            used.append('%s: %d x %s mesh burst' % (en, n, mesh.get_name()))
            continue
        # sprites (billboard / stretched -> billboard)
        t = tex_asset(eff, (m.get('tex') or {}).get('_MainTex'))
        if not t: t = unreal.load_asset('/Niagara/DefaultAssets/DefaultSpriteTexture') if EAL.does_asset_exist('/Niagara/DefaultAssets/DefaultSpriteTexture') else None
        fb = e.get('flipbook') or [1, 1]
        bl = blend_of(m)
        params = {'Tex': t, 'TilesX': fb[0] if fb[0] <= 16 else 1, 'TilesY': fb[1] if fb[1] <= 16 else 1, 'Intensity': 1.5 if bl == 'blend' else 3.0,
                  'MaskTex': tex_asset(eff, (m.get('tex') or {}).get('_CutoutTex'))}
        mi = make_mi(eff, prefab, '%s_%s_%s' % (bl, os.path.basename(m.get('path', 'x')), os.path.basename((m.get('tex') or {}).get('_MainTex') or '')), bl, params)
        col = lc(tint)
        sc = e.get('color')
        if isinstance(sc, list) and len(sc) == 4 and isinstance(sc[0], (int, float)): col = unreal.LinearColor(col.r * sc[0], col.g * sc[1], col.b * sc[2], col.a * sc[3])
        if bl == 'blend' and max(tint[:3]) < 0.05: col = unreal.LinearColor(0.02, 0.015, 0.012, min(1.0, max(0.5, col.a)))
        en = uname(nm)
        smin = max(2.0, size[0]); smax = max(smin, size[1])
        if mode == 1: smin = max(smin, 3.0); smax = max(smax, 5.0)   # stretched sparks: keep them visible as dots
        if loop and rate > 0 and nburst <= 0:
            addem(sysa, 'Fountain', en)
            L.set_niagara_float(sysa, en, 'SpawnRate.SpawnRate', min(rate, 150.0))
            L.set_niagara_float(sysa, en, 'EmitterState.Loop Duration', max(0.5, dur))
            L.set_niagara_float(sysa, en, 'EmitterState.Loop Delay', delay)
            L.set_niagara_float(sysa, en, 'AddVelocity.Cone Angle', float(shape.get('angle') or 25.0) if shape.get('type') == 4 else 180.0)
            kind = 'rate %.0f/s' % min(rate, 150.0)
        else:
            n = nburst if nburst > 0 else rate * min(dur, 2.0)
            if n <= 0 and rate <= 0 and not e.get('bursts'): n = 1   # emitter driven by a script/sub-emitter in Unity: one sprite
            n = int(min(250, max(1, round(n))))
            addem(sysa, 'OmnidirectionalBurst', en)
            L.set_niagara_int(sysa, en, 'SpawnBurst_Instantaneous.Spawn Count', n)
            L.set_niagara_float(sysa, en, 'SpawnBurst_Instantaneous.Spawn Time', delay)
            L.set_niagara_float(sysa, en, 'EmitterState.Loop Duration', max(1.0, delay + 0.5))
            L.set_niagara_float(sysa, en, 'Drag.Drag', 1.5 if speed[1] > 500 else 0.5)
            kind = 'burst %d' % n
        L.set_emitter_render(sysa, en, mi, None)
        L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', min(life[0], 5.0))
        L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', min(max(life[1], life[0]), 5.0))
        L.set_niagara_float(sysa, en, 'Uniform Sprite Size Min', smin)
        L.set_niagara_float(sysa, en, 'Uniform Sprite Size Max', smax)
        L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', min(speed[0], 3000.0))
        L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', min(max(speed[1], speed[0]), 3000.0))
        L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, -grav))
        L.set_niagara_float(sysa, en, 'ShapeLocation.Sphere Radius', max(1.0, radius))
        if flat: L.set_niagara_vector(sysa, en, 'ShapeLocation.Non Uniform Scale', unreal.Vector(1, 1, 0.05))
        L.set_niagara_color(sysa, en, 'InitializeParticle.Color', col)
        L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', pos(e['pos']))
        used.append('%s: %s %s %s tex=%s size=%.0f-%.0f life=%.1f-%.1f' % (en, kind, bl, mi.get_name(), t.get_name() if t else '-', smin, smax, life[0], life[1]))

    # static/decal meshes -> one mesh particle each (portal, tornado, dragon cracks, icicles, decals as flat planes)
    groups = {}
    for mm in pf['meshes']:
        if not mm.get('active') or not mm.get('enabled', 1): continue
        mat = (mm.get('mats') or [None])[0] or {}
        key = (mm.get('mesh') or 'PLANE', mat.get('path'))
        groups.setdefault(key, []).append(mm)
    for (mpath, matpath), lst in groups.items():
        mat = (lst[0].get('mats') or [None])[0] or {}
        is_decal = 'Decal' in (mat.get('shader') or '') or mpath == 'PLANE'
        if any(k in (mpath or '') for k in SKIP_MESH) or os.path.basename(matpath or '') in SKIP_MAT: skipped.append('%s %s (not part of effect / stencil)' % (mpath, matpath)); continue
        mesh = unreal.load_asset('/Engine/BasicShapes/Plane') if is_decal else mesh_asset(eff, mpath)
        if not mesh: skipped.append('%s (mesh missing)' % mpath); continue
        texs = mat.get('tex') or {}
        t = tex_asset(eff, texs.get('_MainTex')) or tex_asset(eff, texs.get('_Mask')) or tex_asset(eff, texs.get('_CutoutTex'))
        tint = tint_of(mat)
        rocky = 'rock' in (mpath or '').lower() or 'icicle' in (mpath or '').lower() or 'golem' in (mpath or '').lower()
        if rocky:
            icy = 'icicle' in mpath.lower()
            params = {'Tex': t, 'Tint': lc([0.75, 0.9, 1.0, 1] if icy else ([0.3, 0.3, 0.3, 1] if max(tint[:3]) < 0.05 or max(tint[:3]) > 1.5 else tint)),
                      'RimColor': lc([0.4, 0.75, 1.5, 1] if icy else ([1.5, 0.25, 0.05, 1] if eff in ('Effect17', 'Effect19', 'Effect3') else [0, 0, 0, 1])), 'Roughness': 0.15 if icy else 0.8}
            mi = make_mi(eff, prefab, 'Static_' + os.path.basename(matpath or 'x'), 'lit', params)
        else:
            bl = blend_of(mat) if not is_decal else ('add' if '_BLENDMODE_ADD' in (mat.get('keywords') or '') else 'blend')
            mi = make_mi(eff, prefab, 'Static_%s_%s' % (bl, os.path.basename(matpath or 'x')), bl,
                         {'Tex': t, 'MaskTex': tex_asset(eff, texs.get('_Mask')) if is_decal else None, 'Tint': lc(tint), 'Intensity': 2.0,
                          'PanV': 0.6 if 'tornado' in (mpath or '').lower() else 0.0, 'LumaAlpha': 0.0 if is_decal else 1.0})
        # place: icicles spread in a ring (8 groups), everything else at the recentred position
        cnt = len(lst)
        sc = lst[0].get('scale') or {'x': 1, 'y': 1, 'z': 1}
        s = max(abs(sc.get('x', 1)), abs(sc.get('z', 1))) if is_decal else max(abs(sc.get('x', 1)), abs(sc.get('y', 1)), abs(sc.get('z', 1)))
        if cnt > 12:
            # many copies (debris rocks / icicles): radial bursts of up to 24 around the centre
            ring = 6
            per = int(min(6, math.ceil(min(cnt, 36) / ring)))
            rad = statistics.median([math.hypot(x['pos'][0] - mx, x['pos'][2] - mz) for x in lst]) * 100.0
            for i in range(ring):
                a = 2 * math.pi * i / ring
                en = uname('%s%d' % (os.path.basename(mpath).split('.')[0], i))
                addem(sysa, 'UpwardMeshBurst', en)
                L.set_emitter_render(sysa, en, mi, mesh)
                L.set_niagara_int(sysa, en, 'SpawnBurst_Instantaneous.Spawn Count', per)
                L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', 2.0); L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', 3.0)
                cap = 140.0 if 'icicle' in mpath.lower() else 45.0
                L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Min', mesh_scale(mesh, mpath, s * 0.7, cap)); L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Max', mesh_scale(mesh, mpath, s * 1.2, cap))
                icy = 'icicle' in mpath.lower()
                L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', 0.0 if icy else 250.0); L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', 30.0 if icy else 650.0)
                L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, 0 if icy else -980))
                L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', unreal.Vector(math.cos(a) * max(60.0, rad), math.sin(a) * max(60.0, rad), 0))
                L.set_niagara_color(sysa, en, 'InitializeParticle.Color', unreal.LinearColor(1, 1, 1, 1))
            used.append('%s x%d -> %d radial mesh bursts of %d (%s)' % (os.path.basename(mpath), cnt, ring, per, mi.get_name()))
        else:
            for j, mm in enumerate(lst[:3]):
                en = uname((os.path.basename(mpath).split('.')[0] if mpath != 'PLANE' else 'Decal') + os.path.basename(matpath or '')[:6])
                addem(sysa, 'UpwardMeshBurst', en)
                L.set_emitter_render(sysa, en, mi, mesh)
                L.set_niagara_int(sysa, en, 'SpawnBurst_Instantaneous.Spawn Count', 1)
                L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', 2.5); L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', 2.5)
                us = min(8.0, max(0.05, s)) if is_decal else mesh_scale(mesh, mpath, s, 700.0)
                L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Min', us); L.set_niagara_float(sysa, en, 'Mesh Uniform Scale Max', us)
                L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', 0.0); L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', 0.0)
                L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, 0))
                L.set_niagara_vector(sysa, en, 'RandomRangeVector.Minimum', unreal.Vector(0, 0, 0)); L.set_niagara_vector(sysa, en, 'RandomRangeVector.Maximum', unreal.Vector(0, 0, 0))
                p = pos(mm['pos']); 
                if is_decal: p.z = 2.0
                L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', p)
                L.set_niagara_color(sysa, en, 'InitializeParticle.Color', unreal.LinearColor(1, 1, 1, 1))
                used.append('%s: static %s %s scale %.2f' % (en, 'decal plane' if is_decal else mesh.get_name(), mi.get_name(), us))
    rep = L.compile_report(sysa)
    EAL.save_loaded_asset(sysa)
    REPORT.append({'system': path, 'emitters': used, 'skipped': skipped, 'compile': rep})
    log('%s compile=%s emitters=%d skipped=%d' % (path, rep[:300], len(used), len(skipped)))

for eff, pfs in SPEC.items():
    for pf in pfs:
        try: build_prefab(eff, pf)
        except Exception as ex:
            import traceback; log('FAILED %s %s %s' % (eff, pf['prefab'], traceback.format_exc()))
            REPORT.append({'system': pf['prefab'], 'error': str(ex)})
json.dump(REPORT, open(os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'KriptoFX', 'build_report.json'), 'w'), indent=1)
log('done %d systems' % len(REPORT))
