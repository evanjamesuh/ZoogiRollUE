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
SKIP_MAT = ('Mask.mat', 'Sky.mat', 'MaskRock.mat', 'Fringe.mat', 'Portal1.mat', 'DecalCollision1.mat')  # + portal surface (renders solid white without stencil), atlas decal (ring grid)  # Unity stencil-mask tricks of the portal
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
    # soft radial edge (kills square sprite borders on alpha-less textures); RadialMask=0 for meshes/decals
    half = E(m, unreal.MaterialExpressionConstant2Vector, -800, 900); half.set_editor_property('r', 0.5); half.set_editor_property('g', 0.5)
    dv = op(m, unreal.MaterialExpressionSubtract, tc, half, -650, 900)
    ln = E(m, unreal.MaterialExpressionLength, -500, 900); C(dv, '', ln, '')
    two = E(m, unreal.MaterialExpressionConstant, -500, 1000); two.set_editor_property('r', 2.0)
    l2 = op(m, MUL, ln, two, -350, 950)
    oneR = E(m, unreal.MaterialExpressionOneMinus, -200, 950); C(l2, '', oneR, '')
    satR = E(m, unreal.MaterialExpressionSaturate, -50, 950); C(oneR, '', satR, '')
    rm = scalar(m, 'RadialMask', 1.0, -50, 1050)
    oneC = E(m, unreal.MaterialExpressionConstant, -50, 1150); oneC.set_editor_property('r', 1.0)
    rad = E(m, unreal.MaterialExpressionLinearInterpolate, 100, 1000); C(oneC, '', rad, 'A'); C(satR, '', rad, 'B'); C(rm, '', rad, 'Alpha')
    tint = vec(m, 'Tint', unreal.LinearColor(1, 1, 1, 1), -50, 700)
    inten = scalar(m, 'Intensity', 1.0, -50, 850)
    rgb1 = op(m, MUL, t, pc, 250, 0, 'RGB', 'RGB'); rgb2 = op(m, MUL, rgb1, tint, 400, 0, '', 'RGB'); rgb = op(m, MUL, rgb2, inten, 550, 0)
    a1 = op(m, MUL, t, mk, 250, 300, 'A', 'R'); a2 = op(m, MUL, a1, pc, 400, 300, '', 'A'); a3 = op(m, MUL, a2, tint, 550, 300, '', 'A'); a4 = op(m, MUL, a3, rad, 650, 300); fade = scalar(m, 'Fade', 1.0, 550, 450); a = op(m, MUL, a4, fade, 700, 350)
    if additive:
        # KriptoFX additive textures are mostly black-backed RGB; alpha-less PNGs import with A=1, so RGB carries the shape.
        em = op(m, MUL, rgb, a, 750, 100)
        MEL.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        # Alpha-blended: opacity = alpha * clamp(luma*3) so RGB-only (no alpha) textures do not show a square.
        d = E(m, unreal.MaterialExpressionDotProduct, 250, 450); C(t, 'RGB', d, 'A')
        third = E(m, unreal.MaterialExpressionConstant3Vector, 100, 500); third.set_editor_property('constant', unreal.LinearColor(1.0, 1.0, 1.0, 1))
        C(third, '', d, 'B')
        thr = E(m, unreal.MaterialExpressionConstant, 250, 550); thr.set_editor_property('r', 0.15)
        d2 = op(m, unreal.MaterialExpressionSubtract, d, thr, 330, 450)
        sat = E(m, unreal.MaterialExpressionSaturate, 400, 450); C(d2, '', sat, '')
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


def luma(m, t, x, y):
    d = E(m, unreal.MaterialExpressionDotProduct, x, y); C(t, 'RGB', d, 'A')
    w = E(m, unreal.MaterialExpressionConstant3Vector, x - 150, y + 50); w.set_editor_property('constant', unreal.LinearColor(0.33, 0.33, 0.33, 1)); C(w, '', d, 'B')
    return d
def const(m, v, x, y):
    c = E(m, unreal.MaterialExpressionConstant, x, y); c.set_editor_property('r', v); return c
def sat(m, a, x, y):
    e = E(m, unreal.MaterialExpressionSaturate, x, y); C(a, '', e, ''); return e

def build_decal(name, surf):
    """Deferred decal. Glow: emissive + opacity only (lava cracks, fire ring). Surf: also base colour (scorch, frost).
       Reveal (0..1, driven at runtime) grows the decal from the bright parts of its cutout mask, like KriptoFX's animated cutout."""
    m = fresh_material(name)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    t = tex(m, 'Tex', -900, 0); mk = tex(m, 'MaskTex', -900, 300)
    tint = vec(m, 'Tint', unreal.LinearColor(1, 1, 1, 1), -900, 600)
    inten = scalar(m, 'Intensity', 2.0, -900, 750)
    fade = scalar(m, 'Fade', 1.0, -900, 850); rev = scalar(m, 'Reveal', 1.0, -900, 950)
    gain = scalar(m, 'LumaGain', 2.0, -900, 1050)
    # reveal = saturate((mask - (1 - Reveal)) * 4): grows from the bright parts of the cutout mask; mask 0 stays hidden
    omr = E(m, unreal.MaterialExpressionOneMinus, -750, 950); C(rev, '', omr, '')
    mr1 = op(m, unreal.MaterialExpressionSubtract, mk, omr, -550, 900, 'R', '')
    revf = sat(m, op(m, MUL, mr1, const(m, 4.0, -450, 1000), -300, 900), -150, 900)
    # soft round edge so the projection box never shows
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -1100, 1200)
    half = E(m, unreal.MaterialExpressionConstant2Vector, -1100, 1300); half.set_editor_property('r', 0.5); half.set_editor_property('g', 0.5)
    dv = op(m, unreal.MaterialExpressionSubtract, tc, half, -950, 1200)
    ln = E(m, unreal.MaterialExpressionLength, -800, 1200); C(dv, '', ln, '')
    om = E(m, unreal.MaterialExpressionOneMinus, -500, 1200); C(op(m, MUL, ln, const(m, 2.0, -700, 1300), -650, 1200), '', om, '')
    radial = sat(m, op(m, MUL, om, const(m, 3.0, -500, 1300), -350, 1200), -200, 1200)
    lu = luma(m, t, -600, 400)
    lshape = sat(m, op(m, MUL, lu, gain, -400, 400), -250, 400)
    la = scalar(m, 'LumaAlpha', 1.0, -400, 550)
    shp = E(m, unreal.MaterialExpressionLinearInterpolate, -150, 450); C(t, 'A', shp, 'A'); C(lshape, '', shp, 'B'); C(la, '', shp, 'Alpha')
    o0 = op(m, MUL, shp, radial, -50, 600)
    o1 = op(m, MUL, o0, revf, 0, 500); o2 = op(m, MUL, o1, fade, 150, 500); o3 = op(m, MUL, o2, tint, 300, 500, '', 'A')
    MEL.connect_material_property(o3, '', unreal.MaterialProperty.MP_OPACITY)
    e1 = op(m, MUL, t, tint, -300, 0, 'RGB', 'RGB'); e2 = op(m, MUL, e1, inten, -150, 0); e3 = op(m, MUL, e2, fade, 0, 0)
    MEL.connect_material_property(e3, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if surf:
        bt = vec(m, 'BaseTint', unreal.LinearColor(0.03, 0.03, 0.03, 1), -300, 200)
        b = op(m, MUL, t, bt, -150, 200, 'RGB', 'RGB')
        MEL.connect_material_property(b, '', unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(const(m, 0.9, -150, 300), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def build_ice():
    """Clear refractive ice (Effect22 icicles): translucent lit, fresnel-weighted opacity and cold rim glow."""
    m = fresh_material('M_KFX_Ice')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    try: m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE)
    except Exception as ex: log('ice tlm %s' % ex)
    t = tex(m, 'Tex', -900, 0)
    base = vec(m, 'Tint', unreal.LinearColor(0.35, 0.5, 0.65, 1), -900, 250)
    b = op(m, MUL, t, base, -600, 0, 'RGB', 'RGB')
    MEL.connect_material_property(b, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(const(m, 0.08, -600, 150), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(const(m, 1.0, -600, 220), '', unreal.MaterialProperty.MP_SPECULAR)
    fr = E(m, unreal.MaterialExpressionFresnel, -900, 450); fr.set_editor_property('exponent', 2.5)
    lo = const(m, 0.18, -750, 550); hi = const(m, 0.8, -750, 620)
    lp = E(m, unreal.MaterialExpressionLinearInterpolate, -550, 500); C(lo, '', lp, 'A'); C(hi, '', lp, 'B'); C(fr, '', lp, 'Alpha')
    fade = scalar(m, 'Fade', 1.0, -550, 650)
    MEL.connect_material_property(op(m, MUL, lp, fade, -350, 550), '', unreal.MaterialProperty.MP_OPACITY)
    rim = vec(m, 'RimColor', unreal.LinearColor(0.22, 0.4, 0.8, 1), -900, 750)
    e1 = op(m, MUL, fr, rim, -550, 750, '', 'RGB')
    amb = vec(m, 'Glow', unreal.LinearColor(0.01, 0.02, 0.035, 1), -900, 900)
    e2 = op(m, ADD, e1, amb, -400, 800, '', 'RGB')
    MEL.connect_material_property(op(m, MUL, e2, fade, -250, 800), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    try: MEL.connect_material_property(const(m, 1.25, -400, 950), '', unreal.MaterialProperty.MP_REFRACTION)
    except Exception as ex: log('ice refraction %s' % ex)
    usage(m)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def build_portal():
    """Effect5 portal disc without Unity's stencil trick: additive swirl (two counter-rotating noise layers), bright rim, star sparkles."""
    m = fresh_material('M_KFX_Portal')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    r1 = E(m, unreal.MaterialExpressionRotator, -1200, -100); r1.set_editor_property('speed', 0.6); C(tc, '', r1, 'Coordinate')
    r2 = E(m, unreal.MaterialExpressionRotator, -1200, 100); r2.set_editor_property('speed', -0.35); C(tc, '', r2, 'Coordinate')
    n1 = tex(m, 'Tex', -1000, -100); C(r1, '', n1, 'UVs')
    st = tex(m, 'StarTex', -1000, 150)
    sc = op(m, MUL, r2, const(m, 3.0, -1100, 250), -1000, 300)
    C(sc, '', st, 'UVs')
    half = E(m, unreal.MaterialExpressionConstant2Vector, -1400, 400); half.set_editor_property('r', 0.5); half.set_editor_property('g', 0.5)
    dv = op(m, unreal.MaterialExpressionSubtract, tc, half, -1250, 400)
    ln = E(m, unreal.MaterialExpressionLength, -1100, 400); C(dv, '', ln, '')
    r = op(m, MUL, ln, const(m, 2.0, -1100, 500), -950, 450)
    # rim = saturate(1 - abs(r - 0.86) * 9)
    d = op(m, unreal.MaterialExpressionSubtract, r, const(m, 0.86, -950, 550), -800, 450)
    ab = E(m, unreal.MaterialExpressionAbs, -650, 450); C(d, '', ab, '')
    k9 = op(m, MUL, ab, const(m, 9.0, -650, 550), -500, 450)
    om1 = E(m, unreal.MaterialExpressionOneMinus, -350, 520); C(k9, '', om1, '')
    rim = sat(m, om1, -200, 520)
    # inner = saturate((1 - r) * 3)
    om2 = E(m, unreal.MaterialExpressionOneMinus, -800, 650); C(r, '', om2, '')
    inner = sat(m, op(m, MUL, om2, const(m, 3.0, -800, 750), -650, 650), -500, 650)
    swirl = op(m, MUL, n1, inner, -300, 0, 'R', '')
    stars = op(m, MUL, st, inner, -300, 200, 'R', '')
    s1 = op(m, ADD, op(m, MUL, rim, const(m, 1.3, -100, 600), 0, 520), op(m, MUL, swirl, const(m, 0.8, -150, 50), 0, 0), 150, 300)
    s2 = op(m, ADD, s1, op(m, MUL, stars, const(m, 2.5, -150, 250), 0, 200), 300, 300)
    tint = vec(m, 'Tint', unreal.LinearColor(1.0, 0.45, 2.6, 1), 300, 450)
    fade = scalar(m, 'Fade', 1.0, 300, 550)
    e = op(m, MUL, op(m, MUL, s2, tint, 450, 300, '', 'RGB'), fade, 600, 300)
    MEL.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    usage(m)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def build_meshadd():
    """Additive panned-noise mesh (Effect11 tornado): no particle inputs, soft fade at both ends of the mesh's V."""
    m = fresh_material('M_KFX_MeshAdd')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -1300, 0)
    pu = scalar(m, 'PanU', 0.3, -1300, 150); pv = scalar(m, 'PanV', 0.8, -1300, 250)
    pan = E(m, unreal.MaterialExpressionAppendVector, -1150, 200); C(pu, '', pan, 'A'); C(pv, '', pan, 'B')
    tm = E(m, unreal.MaterialExpressionTime, -1150, 300)
    uv = op(m, ADD, tc, op(m, MUL, tm, pan, -1000, 250), -850, 100)
    t = tex(m, 'Tex', -700, 0); C(uv, '', t, 'UVs')
    t2 = tex(m, 'Tex2', -700, 250)
    uv2 = op(m, MUL, op(m, ADD, tc, op(m, MUL, tm, pan, -1000, 400), -850, 350), const(m, 1.7, -850, 450), -750, 400)
    C(uv2, '', t2, 'UVs')
    mixv = op(m, MUL, t, t2, -450, 100, 'R', 'R')
    vv = E(m, unreal.MaterialExpressionComponentMask, -1000, 600); vv.set_editor_property('r', False); vv.set_editor_property('g', True); vv.set_editor_property('b', False); vv.set_editor_property('a', False)
    C(tc, '', vv, '')
    omv = E(m, unreal.MaterialExpressionOneMinus, -850, 650); C(vv, '', omv, '')
    vf = sat(m, op(m, MUL, op(m, MUL, vv, omv, -700, 600), const(m, 6.0, -700, 700), -550, 600), -400, 600)
    tint = vec(m, 'Tint', unreal.LinearColor(1, 0.16, 0.8, 1), -450, 300)
    inten = scalar(m, 'Intensity', 3.0, -450, 400); fade = scalar(m, 'Fade', 1.0, -450, 480)
    e = op(m, MUL, op(m, MUL, op(m, MUL, op(m, MUL, mixv, vf, -250, 150), tint, -100, 200, '', 'RGB'), inten, 50, 200), fade, 200, 200)
    MEL.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    usage(m)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def build_distort():
    """Heat distortion sprites (KriptoFX UberDistortion): screen-space 2D refraction offset from the texture, masked by particle alpha."""
    m = fresh_material('M_KFX_Distort')
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    usage(m)
    mode2d = False
    for nm in ('RM_2D_OFFSET', 'RM_2DOFFSET', 'RM_2D_OFFSET_'):
        try:
            m.set_editor_property('refraction_method', getattr(unreal.RefractionMode, nm)); mode2d = True; break
        except Exception: pass
    t = tex(m, 'Tex', -900, 0)
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -1300, 0)
    tm = E(m, unreal.MaterialExpressionTime, -1300, 150)
    pn = op(m, MUL, tm, const(m, 0.15, -1300, 250), -1150, 150)
    uv = op(m, ADD, tc, pn, -1050, 50); C(uv, '', t, 'UVs')
    half = E(m, unreal.MaterialExpressionConstant2Vector, -1300, 400); half.set_editor_property('r', 0.5); half.set_editor_property('g', 0.5)
    dv = op(m, unreal.MaterialExpressionSubtract, tc, half, -1150, 400)
    ln = E(m, unreal.MaterialExpressionLength, -1000, 400); C(dv, '', ln, '')
    om = E(m, unreal.MaterialExpressionOneMinus, -700, 400); C(op(m, MUL, ln, const(m, 2.0, -900, 500), -850, 400), '', om, '')
    rad = sat(m, om, -550, 400)
    pc = E(m, unreal.MaterialExpressionParticleColor, -900, 600)
    stren = scalar(m, 'Strength', 0.03, -900, 750)
    mask = op(m, MUL, op(m, MUL, rad, pc, -400, 450, '', 'A'), stren, -250, 450)
    rg = E(m, unreal.MaterialExpressionComponentMask, -700, 0); rg.set_editor_property('r', True); rg.set_editor_property('g', True); rg.set_editor_property('b', False); rg.set_editor_property('a', False)
    C(t, 'RGB', rg, '')
    off = op(m, unreal.MaterialExpressionSubtract, rg, half, -550, 0)
    if mode2d:
        MEL.connect_material_property(op(m, MUL, off, mask, -100, 100), '', unreal.MaterialProperty.MP_REFRACTION)
    else:
        lu = op(m, ADD, const(m, 1.0, -250, 300), op(m, MUL, mask, const(m, 3.0, -400, 600), -250, 600), -100, 300)
        MEL.connect_material_property(lu, '', unreal.MaterialProperty.MP_REFRACTION)
    MEL.connect_material_property(op(m, MUL, mask, const(m, 0.02, -250, 700), -100, 700), '', unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(const(m, 0.0, -100, 800), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    log('distort refraction 2D offset=%s' % mode2d)
    return m

MASTER = {'add': build_unlit('M_KFX_Add', True), 'blend': build_unlit('M_KFX_Blend', False), 'lit': build_lit(),
          'decal_glow': build_decal('M_KFX_DecalGlow', False), 'decal_surf': build_decal('M_KFX_DecalSurf', True),
          'ice': build_ice(), 'portal': build_portal(), 'distort': build_distort(), 'meshadd': build_meshadd()}


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
EXTRAS = {}
ROCK_STYLE = {  # (glow (emissive x texture), rim) per effect, from the demo look
    'Effect7': ([0.5, 1.4, 5.0, 1], [0.6, 1.2, 4.0, 1]),
    'Effect17': ([4.0, 0.15, 0.1, 1], [4.0, 0.2, 0.1, 1]),
    'Effect19': ([0.35, 0.04, 0.0, 1], [2.2, 0.35, 0.05, 1]),
    'Effect5': ([0.0, 0.0, 0.0, 1], [0.7, 0.35, 1.8, 1]),
    'Effect11': ([0.6, 0.08, 0.5, 1], [2.0, 0.3, 1.6, 1]),
}
def pinkify(t):  # Effect11: the demo tornado is magenta although the shipped material data is orange
    m = max(t[:3]); return [m, m * 0.16, m * 0.8, t[3] if len(t) > 3 else 1.0]
def objpath(a): p = a.get_path_name(); return p
import random

def build_prefab(eff, pf):
    prefab = os.path.splitext(os.path.basename(pf['prefab']))[0]
    sysname = 'NS_KFX_' + prefab
    folder = '%s/%s' % (ROOT, eff)
    path = folder + '/' + sysname
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    sysa = AT.create_asset(sysname, folder, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    ems = [e for e in pf['emitters'] if e.get('active') and (e.get('render') or {}).get('enabled', 1)]
    if ems:
        mx = statistics.median([e['pos'][0] for e in ems]); mz = statistics.median([e['pos'][2] for e in ems]); my = min(e['pos'][1] for e in ems)
    else: mx = mz = my = 0.0
    def pos(p): return U2UE([p[0] - mx, max(0.0, p[1] - my) * 0.6, p[2] - mz])
    used = []; skipped = []
    names = set()
    ex = EXTRAS.setdefault(prefab, {'decals': [], 'lights': [], 'meshes': [], 'systems': []})
    rnd = random.Random(hash(prefab) & 0xffff)
    def uname(base):
        n = safe(base); i = 1; o = n
        while o in names: i += 1; o = '%s%d' % (n, i)
        names.add(o); return o
    small = prefab.startswith('Effect6') or prefab.endswith('_Hand')

    for e in ems:
        ren = e.get('render') or {}
        mode = ren.get('mode')
        m = (ren.get('mats') or [None])[0] or {}
        nm = e['name'].split('/')[-1]
        if mode == 5 or not m: skipped.append(nm + ' (trail/light-only: light added as a point light)'); continue
        life = rng(e['lifetime'])
        if life[1] <= 0: life = (1.0, 1.0)
        size = rng(e['size'], 100.0)
        speed = rng(e['speed'], 100.0)
        grav = avg(e.get('gravity')) * 980.0
        tint = list(tint_of(m))
        if eff == 'Effect11': tint = pinkify(tint)
        delay = min(0.6, max(0.0, avg(e.get('delay'))) * 0.3)   # Unity cast delays (2 s windup) compressed for gameplay
        dur = float(e.get('duration') or 1.0)
        shape = e.get('shape') or {}
        stype = shape.get('type') if shape.get('enabled') else None
        radius = float(shape.get('radius') or 0.0) * 100.0 if shape.get('enabled') else 0.0
        flat = stype in (10, 13)
        nburst = burst_count(e)
        rate = avg(e.get('rate'))
        rotator = 'Rotator' in e['name'] and avg(e.get('rateDist')) > 0
        if rate <= 0 and avg(e.get('rateDist')) > 0: rate = avg(e.get('rateDist')) * 8.0
        loop = bool(e.get('loop'))
        if mode == 4:
            mesh = mesh_asset(eff, ren.get('mesh'))
            if not mesh: skipped.append(nm + ' (mesh missing)'); continue
            t = tex_asset(eff, (m.get('tex') or {}).get('_MainTex'))
            gl, rim = ROCK_STYLE.get(eff, ([0, 0, 0, 1], [0, 0, 0, 1]))
            mi = make_mi(eff, prefab, 'Mesh_' + os.path.basename(m.get('path', 'x')), 'lit', {'Tex': t, 'Tint': lc(tint if 0.01 < max(tint[:3]) < 1.5 else [0.25, 0.25, 0.25, 1]), 'Glow': lc(gl), 'RimColor': lc(rim)})
            n = int(min(12 if eff == 'Effect19' else 40, max(1, nburst if nburst > 0 else rate * min(dur, 2.0))))
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
            L.set_niagara_float(sysa, en, 'AddVelocity.Cone Angle', float(shape.get('angle') or 30.0) if stype == 4 else 80.0)
            L.set_niagara_color(sysa, en, 'InitializeParticle.Color', unreal.LinearColor(1, 1, 1, 1))
            L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', pos(e['pos']))
            used.append('%s: %d x %s mesh burst' % (en, n, mesh.get_name()))
            continue
        t = tex_asset(eff, (m.get('tex') or {}).get('_MainTex'))
        if not t: t = unreal.load_asset('/Niagara/DefaultAssets/DefaultSpriteTexture') if EAL.does_asset_exist('/Niagara/DefaultAssets/DefaultSpriteTexture') else None
        fb = e.get('flipbook') or [1, 1]
        if is_distortion(m):
            bl = 'distort'
            params = {'Tex': t, 'Strength': 0.035}
        else:
            bl = blend_of(m)
            if small and bl == 'blend' and max(tint[:3]) < 0.05: bl = 'add'; tint = [0.7, 0.4, 1.4, 1]
            if eff == 'Effect17' and max(tint[:3]) < 0.05: bl = 'add'; tint = [1.4, 0.12, 0.08, 1]
            if eff == 'Effect3' and max(tint[:3]) < 0.3: tint = [1.6, 0.55, 0.12, 1]
            if eff in ('Effect3', 'Effect4'): bl = 'add'
            params = {'Tex': t, 'TilesX': fb[0] if fb[0] <= 16 else 1, 'TilesY': fb[1] if fb[1] <= 16 else 1, 'Intensity': 1.5 if bl == 'blend' else (6.0 if small else 3.0),
                      'MaskTex': tex_asset(eff, (m.get('tex') or {}).get('_CutoutTex'))}
        mi = make_mi(eff, prefab, '%s_%s_%s' % (bl, os.path.basename(m.get('path', 'x')), os.path.basename((m.get('tex') or {}).get('_MainTex') or '')), bl, params)
        col = lc(tint)
        sc = e.get('color')
        if isinstance(sc, list) and len(sc) == 4 and isinstance(sc[0], (int, float)): col = unreal.LinearColor(col.r * sc[0], col.g * sc[1], col.b * sc[2], col.a * sc[3])
        if bl == 'blend' and max(tint[:3]) < 0.05: col = unreal.LinearColor(0.02, 0.015, 0.012, min(1.0, max(0.5, col.a)))
        if bl == 'blend' and col.a < 0.35: col = unreal.LinearColor(col.r, col.g, col.b, min(1.0, col.a * 2.5 + 0.1))  # Unity soft smoke reads stronger (alpha^pow + HDR) than plain alpha
        if bl == 'distort': col = unreal.LinearColor(1, 1, 1, 1)
        smin = max(2.0, size[0]); smax = max(smin, size[1])
        if mode == 1: smin = max(smin, 3.0); smax = max(smax, 5.0)
        if small: smin = max(12.0, smin * 2.0); smax = max(smin * 1.5, smax * 2.0); life = (max(0.8, life[0]), max(1.0, life[1]))
        if eff == 'Effect3': smin = max(smin, 40.0); smax = max(smax, 80.0)
        smax = min(smax, 260.0); smin = min(smin, smax)
        # placements: ring-shaped emitters (circle edge, wide cones, flames on a rotating arm) become K emitters around the ring
        base = pos(e['pos'])
        ring_r = 0.0
        if (flat and radius >= 80.0) or (stype == 4 and radius >= 150.0): ring_r = radius
        if rotator:
            ring_r = math.hypot(e['pos'][0] - mx, e['pos'][2] - mz) * 100.0
            base = pos([mx, e['pos'][1], mz])
        K = 8 if ring_r > 0 else 1
        target = sysa
        if K > 1:
            # Niagara's ShapeLocation discards InitializeParticle's offset, so ring points are separate small systems placed by AZoogiKFXExtras
            subname = 'NS_KFX_%s_Ring_%s' % (prefab, re.sub(r'[^A-Za-z0-9]', '', nm)[:16])
            sp = folder + '/Rings/' + subname
            if EAL.does_asset_exist(sp): EAL.delete_asset(sp)
            target = AT.create_asset(subname, folder + '/Rings', unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
            for k in range(K):
                a = 2 * math.pi * k / K
                ex['systems'].append((objpath(target), base.x + math.cos(a) * ring_r, base.y + math.sin(a) * ring_r, base.z, 0.0, 3.0))
        for k in range(1):
            en = uname(nm) if K == 1 else safe(nm)
            off = unreal.Vector(base.x, base.y, base.z) if K == 1 else unreal.Vector(0, 0, 0)
            subr = max(1.0, radius) if K == 1 else max(12.0, ring_r * 0.28)
            sysa_main = sysa; sysa = target
            if loop and rate > 0 and nburst <= 0:
                addem(sysa, 'Fountain', en)
                L.set_niagara_float(sysa, en, 'SpawnRate.SpawnRate', max(4.0, min(rate, 150.0) / (K * 0.5 if K > 1 else 1)))
                L.set_niagara_float(sysa, en, 'EmitterState.Loop Duration', max(0.5, dur))
                L.set_niagara_float(sysa, en, 'EmitterState.Loop Delay', delay)
                L.set_niagara_float(sysa, en, 'AddVelocity.Cone Angle', float(shape.get('angle') or 25.0) if stype == 4 else 180.0)
                kind = 'rate'
            else:
                n = nburst if nburst > 0 else rate * min(dur, 2.0)
                if n <= 0 and rate <= 0 and not e.get('bursts'): n = 1
                n = int(min(250, max(1, round(n / (K * 0.5 if K > 1 else 1)))))
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
            L.set_niagara_float(sysa, en, 'ShapeLocation.Sphere Radius', subr)
            if flat or K > 1: L.set_niagara_vector(sysa, en, 'ShapeLocation.Non Uniform Scale', unreal.Vector(1, 1, 0.1))
            L.set_niagara_color(sysa, en, 'InitializeParticle.Color', col)
            L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', off)
            sysa = sysa_main
        if K > 1:
            rep_r = L.compile_report(target); EAL.save_loaded_asset(target)
            if rep_r != 'OK': log('ring system %s compile=%s' % (target.get_name(), rep_r[:200]))
        used.append('%s x%d: %s %s %s size=%.0f-%.0f life=%.1f-%.1f ring=%.0f' % (nm, K, kind, bl, mi.get_name(), smin, smax, life[0], life[1], ring_r))

    # --- extra particle parts the Unity prefab got from skinned meshes / scripts
    def column(en, texp, tint, down, height, rate, sizes, speed):
        t = tex_asset(eff, texp)
        if not t: return
        mi = make_mi(eff, prefab, 'add_column_' + en, 'add', {'Tex': t, 'TilesX': 8, 'TilesY': 8, 'Intensity': 3.0})
        en = uname(en)
        addem(sysa, 'Fountain', en)
        L.set_emitter_render(sysa, en, mi, None)
        L.set_niagara_float(sysa, en, 'SpawnRate.SpawnRate', rate)
        L.set_niagara_float(sysa, en, 'EmitterState.Loop Duration', 0.9)
        L.set_niagara_float(sysa, en, 'AddVelocity.Cone Angle', 5.0)
        L.set_niagara_vector(sysa, en, 'AddVelocity.Cone Axis', unreal.Vector(0, 0, -1 if down else 1))
        L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', speed); L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', speed * 1.3)
        L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, 0))
        L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', height / speed * 0.8); L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', height / speed)
        L.set_niagara_float(sysa, en, 'Uniform Sprite Size Min', sizes[0]); L.set_niagara_float(sysa, en, 'Uniform Sprite Size Max', sizes[1])
        L.set_niagara_float(sysa, en, 'ShapeLocation.Sphere Radius', 15.0)
        L.set_niagara_color(sysa, en, 'InitializeParticle.Color', lc(tint))
        L.set_niagara_vector(sysa, en, 'InitializeParticle.Position Offset', unreal.Vector(0, 0, height if down else 0))
        used.append('%s: fire column (%s)' % (en, 'dragon breath' if down else 'pillar'))
    if prefab == 'Effect3': column('KDragonBreath', 'Textures/Fire1.png', [2.2, 0.8, 0.2, 1], True, 650.0, 90.0, (60.0, 130.0), 1400.0)
    if prefab == 'Effect19': column('KLavaPillar', 'Textures/Fire1.png', [2.4, 0.35, 0.08, 1], False, 450.0, 60.0, (70.0, 140.0), 700.0)
    if prefab == 'Effect6':
        # projectile trail (Unity trail renderer): world-space soft sprites left behind the rolling ball
        t = tex_asset(eff, 'Textures/Smoke1.png')
        if t:
            mi = make_mi(eff, prefab, 'add_trail', 'add', {'Tex': t, 'Intensity': 4.0})
            en = uname('KTrail')
            addem(sysa, 'Fountain', en)
            L.set_emitter_local_space(sysa, en, False)
            L.set_emitter_render(sysa, en, mi, None)
            L.set_niagara_float(sysa, en, 'SpawnRate.SpawnRate', 90.0)
            L.set_niagara_float(sysa, en, 'RandomRangeFloat.Minimum', 0.0); L.set_niagara_float(sysa, en, 'RandomRangeFloat.Maximum', 10.0)
            L.set_niagara_vector(sysa, en, 'GravityForce.Gravity', unreal.Vector(0, 0, 0))
            L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Min', 0.3); L.set_niagara_float(sysa, en, 'InitializeParticle.Lifetime Max', 0.45)
            L.set_niagara_float(sysa, en, 'Uniform Sprite Size Min', 35.0); L.set_niagara_float(sysa, en, 'Uniform Sprite Size Max', 55.0)
            L.set_niagara_color(sysa, en, 'InitializeParticle.Color', unreal.LinearColor(0.9, 0.75, 1.6, 0.8))
            used.append('KTrail: world-space trail')

    # --- lights (Unity point lights of the prefab)
    lights = sorted(pf.get('lights') or [], key=lambda l: -float(l.get('intensity') or 0))[:2]
    if prefab == 'Effect22' and not lights: lights = [{'color': {'r': 0.55, 'g': 0.75, 'b': 1.0}, 'intensity': 2.5, 'range': 25}]
    if prefab == 'Effect17': lights = [{'color': {'r': 1, 'g': 0.1, 'b': 0.08}, 'intensity': 2, 'range': 15}]
    for i, l in enumerate(lights):
        c = l.get('color') or {}
        rgb = [float(c.get('r', 1)), float(c.get('g', 1)), float(c.get('b', 1))]
        if eff == 'Effect11': rgb = pinkify(rgb + [1])[:3]
        ex['lights'].append((rgb[0], rgb[1], rgb[2], min(20.0, 4.0 * float(l.get('intensity') or 1)), min(700.0, max(250.0, float(l.get('range') or 10) * 22.0)),
                             0.0, 0.0, 80.0 + 60.0 * i, 0.0, 2.6))

    # --- static meshes & decals of the prefab -> AZoogiKFXExtras (real UE decals / placed meshes)
    groups = {}
    for mm in pf['meshes']:
        if not mm.get('active') or not mm.get('enabled', 1): continue
        mat = (mm.get('mats') or [None])[0] or {}
        key = (mm.get('mesh') or 'PLANE', mat.get('path'))
        groups.setdefault(key, []).append(mm)
    root = pos([mx, my, mz]); root.z = 0.0
    for (mpath, matpath), lst in groups.items():
        mat = (lst[0].get('mats') or [None])[0] or {}
        matb = os.path.basename(matpath or '')
        texs = mat.get('tex') or {}
        tint = list(tint_of(mat))
        if eff == 'Effect11': tint = pinkify(tint)
        kw = mat.get('keywords') or ''
        is_decal = 'Decal' in (mat.get('shader') or '') or mpath == 'PLANE'
        if 'Portal.FBX' in mpath and matb == 'Portal1.mat':
            sw = unreal.load_asset('/Game/VFX/KriptoFX/Effect11/Textures/T_TornadoNoise') or tex_asset(eff, 'Textures/Explosion1.png')
            mi = make_mi(eff, prefab, 'Portal_disc', 'portal', {'Tex': sw, 'StarTex': tex_asset(eff, 'Textures/TeleportParticles.png'), 'Tint': unreal.LinearColor(1.1, 0.45, 2.8, 1)})
            p = pos(lst[0]['pos'])
            ex['meshes'].append(('/Engine/BasicShapes/Plane.Plane', objpath(mi), 0.0, 0.0, 190.0, 2.8, 2.8, 1.0, 15.0, 90.0, 0.0, 0.0, 0.35, 3.0, 0.0, 0.0, 0, 1))
            used.append('portal disc (M_KFX_Portal, replaces the Unity stencil portal)')
            continue
        if any(k in (mpath or '') for k in SKIP_MESH) or re.sub(r'[^A-Za-z0-9.]', '', matb) in SKIP_MAT:
            skipped.append('%s %s (demo character / stencil-only)' % (mpath, matpath)); continue
        if is_decal:
            if prefab == 'Effect3' and matb.startswith('DecalCollision'): skipped.append(matb + ' (flipbook wave decal)'); continue
            t = tex_asset(eff, texs.get('_MainTex'))
            mkp = texs.get('_CutoutTex') if 'USE_CUTOUT_TEX' in kw else (texs.get('_Mask') or texs.get('_CutoutTex'))
            mk = tex_asset(eff, mkp)
            if not t: t = mk or tex_asset(eff, texs.get('_Mask'))
            if not t: skipped.append(matb + ' (decal texture missing)'); continue
            bright = max(tint[:3]) > 0.3
            if not bright:
                mi = make_mi(eff, prefab, 'Decal_' + matb, 'decal_surf', {'Tex': t, 'MaskTex': mk, 'Tint': unreal.LinearColor(1, 1, 1, 0.85), 'Intensity': 0.0, 'BaseTint': unreal.LinearColor(0.05, 0.045, 0.04, 1), 'LumaGain': 2.5, 'LumaAlpha': 0.0})
            elif '_BLENDMODE_MUL' in kw:
                mi = make_mi(eff, prefab, 'Decal_' + matb, 'decal_surf', {'Tex': t, 'MaskTex': mk, 'Tint': unreal.LinearColor(tint[0] * 0.15, tint[1] * 0.15, tint[2] * 0.15, 0.75), 'Intensity': 1.0, 'BaseTint': unreal.LinearColor(0.55, 0.65, 0.78, 1), 'LumaGain': 1.2, 'LumaAlpha': 1.0})
            else:
                mi = make_mi(eff, prefab, 'Decal_' + matb, 'decal_glow', {'Tex': t, 'MaskTex': mk, 'Tint': unreal.LinearColor(tint[0], tint[1], tint[2], 1), 'Intensity': 1.6 if eff in ('Effect3', 'Effect19') else 0.6, 'LumaGain': 2.0, 'LumaAlpha': 1.0})
            sc = lst[0].get('scale') or {'x': 1, 'z': 1}
            half = max(abs(sc.get('x', 1)), abs(sc.get('z', 1))) * 50.0
            if '_BLENDMODE_MUL' in kw and bright: half *= 0.45   # frost: 14 m in Unity, keep it around the icicles
            p = pos(lst[0]['pos'])
            reveal = 0.8 if eff in ('Effect3', 'Effect19') else 0.35
            ex['decals'].append((objpath(mi), p.x, p.y, half, 0.05, reveal, 3.0))
            used.append('decal %s half=%.0f (%s)' % (matb, half, mi.get_name()))
            continue
        mesh = mesh_asset(eff, mpath)
        if not mesh: skipped.append('%s (mesh missing)' % mpath); continue
        b = mesh.get_bounds(); D = max(1.0, 2.0 * max(b.box_extent.x, b.box_extent.y, b.box_extent.z))
        mname = os.path.splitext(os.path.basename(mpath))[0]
        gs = GSCALE.get(mname, 1.0)
        if 'Icicle' in mname:
            t = tex_asset(eff, texs.get('_MainTex'))
            mi = make_mi(eff, prefab, 'Ice', 'ice', {'Tex': t})
            sc = lst[0].get('scale') or {}
            if mname == 'IceIcicles1':
                p = pos(lst[0]['pos']); s3 = (gs * sc.get('x', 1), gs * sc.get('z', 1), gs * sc.get('y', 1)); dl = 0.0
            else:
                p = root; s = (gs if mname == 'IceIcicles2' else 1.0) * float(sc.get('x', 1)); s3 = (s, s, s); dl = 0.06
            ex['meshes'].append((objpath(mesh), objpath(mi), p.x, p.y, 0.0, s3[0], s3[1], s3[2], 0.0, 0.0, 0.0, dl, 0.28, 3.4, 0.0, 0.0, 0, 1))
            used.append('ice mesh %s (combined fracture, scale %.2f)' % (mname, s3[0]))
            continue
        if 'Tornado' in mname:
            t = tex_asset(eff, texs.get('_MainTex'))
            mi = make_mi(eff, prefab, 'TornadoMesh', 'meshadd', {'Tex': t, 'Tex2': t, 'Tint': lc([1.0, 0.12, 0.7, 1]), 'Intensity': 1.8, 'PanU': 0.45, 'PanV': 1.1})
            sc = lst[0].get('scale') or {}
            s3 = (2.6 * gs * sc.get('x', 1), 2.6 * gs * sc.get('z', 1), gs * sc.get('y', 1))
            ex['meshes'].append((objpath(mesh), objpath(mi), 0.0, 0.0, 0.0, s3[0], s3[1], s3[2], 0.0, 0.0, 0.0, 0.0, 0.45, 3.0, 420.0, 0.0, 0, 1))
            used.append('tornado mesh (spinning, panned noise, magenta)')
            continue
        if 'Rock' in mname:
            gl, rim = ROCK_STYLE.get(eff, ([0, 0, 0, 1], [0.3, 0.3, 0.3, 1]))
            t = tex_asset(eff, texs.get('_MainTex'))
            dark = eff == 'Effect19'
            mi = make_mi(eff, prefab, 'Rocks_' + matb, 'lit', {'Tex': t, 'Tint': lc([0.16, 0.15, 0.15, 1] if dark else [0.55, 0.55, 0.6, 1]), 'Glow': lc(gl), 'RimColor': lc(rim), 'Roughness': 0.75})
            pick = lst if len(lst) <= 24 else [lst[int(i * len(lst) / 24)] for i in range(24)]
            for j, mm in enumerate(pick):
                p = pos(mm['pos'])
                ang = math.degrees(math.atan2(p.y, p.x))
                sc = mm.get('scale') or {}
                sx, sy, sz = abs(sc.get('x', 1)), abs(sc.get('y', 1)), abs(sc.get('z', 1))
                if eff == 'Effect19':   # erupting spikes: tall, tilted outward, rising out of the ground
                    p = unreal.Vector(p.x * 0.5, p.y * 0.5, 0); w = 26.0 * (sy + sz) / 2.8; h = 140.0 * sx / 2.28
                    ex['meshes'].append((objpath(mesh), objpath(mi), p.x, p.y, 0.0, w / D, w / D, h / D * 1.4, ang, -28.0, rnd.uniform(-20, 20), rnd.uniform(0, 0.12), 0.16, 3.0, 0.0, h * 0.9, 0, 0))
                elif eff == 'Effect17':  # glowing shards flung out
                    L3 = 60.0 * sx / 2.1; w = 18.0
                    ex['meshes'].append((objpath(mesh), objpath(mi), p.x, p.y, max(5.0, p.z * 0.3), L3 / D, w / D, w / D, ang + rnd.uniform(-30, 30), rnd.uniform(-35, 10), rnd.uniform(0, 180), 0.0, 0.3, 3.0, 0.0, 0.0, 1, 0))
                else:                    # Effect7 blue glowing debris
                    sz_ = (34.0 if 'Rock2' in mname else 26.0) * max(sx, sy, sz) / 0.7
                    sz_ = min(60.0, sz_)
                    ex['meshes'].append((objpath(mesh), objpath(mi), p.x, p.y, min(160.0, p.z * 0.15), sz_ / D, sz_ / D, sz_ / D * 0.8, rnd.uniform(0, 360), rnd.uniform(-40, 40), rnd.uniform(0, 360), 0.0, 0.3, 3.0, 0.0, 0.0, 1, 0))
            used.append('%s x%d placed rocks (%s)' % (mname, len(pick), mi.get_name()))
            continue
        skipped.append('%s (unhandled static mesh)' % mpath)
    rep = L.compile_report(sysa)
    EAL.save_loaded_asset(sysa)
    REPORT.append({'system': path, 'emitters': used, 'skipped': skipped, 'compile': rep, 'extras': {k: len(v) for k, v in ex.items()}})
    log('%s compile=%s emitters=%d skipped=%d extras=%s' % (path, rep[:300], len(used), len(skipped), {k: len(v) for k, v in ex.items()}))

def write_extras():
    def f(v): return ('%.3ff' % v)
    out = ['// Generated by Scripts/kfx/kfx_build.py - do not edit. KriptoFX prefab decals / lights / placed meshes for AZoogiKFXExtras.']
    rows = []
    for pfb, ex in EXTRAS.items():
        if not any(ex.values()): continue
        names = {}
        if ex['decals']:
            out.append('static const FKFXDecalDef GKFX_%s_Decals[] = {' % pfb)
            for d in ex['decals']: out.append('\t{ TEXT("%s"), %s },' % (d[0], ', '.join(f(x) for x in d[1:])))
            out.append('};'); names['d'] = ('GKFX_%s_Decals' % pfb, len(ex['decals']))
        if ex['lights']:
            out.append('static const FKFXLightDef GKFX_%s_Lights[] = {' % pfb)
            for d in ex['lights']: out.append('\t{ %s },' % ', '.join(f(x) for x in d))
            out.append('};'); names['l'] = ('GKFX_%s_Lights' % pfb, len(ex['lights']))
        if ex['meshes']:
            out.append('static const FKFXMeshDef GKFX_%s_Meshes[] = {' % pfb)
            for d in ex['meshes']: out.append('\t{ TEXT("%s"), TEXT("%s"), %s, %d, %d },' % (d[0], d[1], ', '.join(f(x) for x in d[2:16]), d[16], d[17]))
            out.append('};'); names['m'] = ('GKFX_%s_Meshes' % pfb, len(ex['meshes']))
        if ex['systems']:
            out.append('static const FKFXSystemDef GKFX_%s_Systems[] = {' % pfb)
            for d in ex['systems']: out.append('\t{ TEXT("%s"), %s },' % (d[0], ', '.join(f(x) for x in d[1:])))
            out.append('};'); names['s'] = ('GKFX_%s_Systems' % pfb, len(ex['systems']))
        def ref(k): return '%s, %d' % names[k] if k in names else 'nullptr, 0'
        rows.append('\t{ TEXT("%s"), %s, %s, %s, %s },' % (pfb, ref('d'), ref('l'), ref('m'), ref('s')))
    out.append('static const FKFXExtrasDef GKFXExtras[] = {'); out += rows; out.append('};')
    p = os.path.join(unreal.Paths.project_dir(), 'Source', 'ZoogiRoll', 'Private', 'ZoogiKFXExtrasData.inl')
    open(p, 'w').write('\n'.join(out) + '\n')
    log('extras written %s (%d prefabs)' % (p, len(rows)))


for eff, pfs in SPEC.items():
    for pf in pfs:
        try: build_prefab(eff, pf)
        except Exception as ex:
            import traceback; log('FAILED %s %s %s' % (eff, pf['prefab'], traceback.format_exc()))
            REPORT.append({'system': pf['prefab'], 'error': str(ex)})
write_extras()
json.dump(REPORT, open(os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'KriptoFX', 'build_report.json'), 'w'), indent=1)
log('done %d systems' % len(REPORT))
