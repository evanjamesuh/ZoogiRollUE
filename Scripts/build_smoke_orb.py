# Builds /Game/Env/Orbs/M_ZoogiSmokeOrb (+ MI_ZoogiSmokeOrb_Blue / _Gold) from SourceArt/Orbs/T_Smoke_*.png
# following the build-box MATERIAL_PLAN (flow-mapped smoke, patches, 3-stop gradient, fresnel rim, emissive, unlit).
import os, json, unreal
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.project_dir(); SRC = os.path.join(PROJ, "SourceArt", "Orbs"); ROOT = "/Game/Env/Orbs"
REP = {"errors": [], "log": []}
def log(m): unreal.log("ZOOGI_ORB " + m); REP["log"].append(m)
def E(m, cls, x, y): return MEL.create_material_expression(m, cls, x, y)
def S(m, n, v, x, y):
    p = E(m, unreal.MaterialExpressionScalarParameter, x, y); p.set_editor_property("parameter_name", n); p.set_editor_property("default_value", v); return p
def V(m, n, v, x, y):
    p = E(m, unreal.MaterialExpressionVectorParameter, x, y); p.set_editor_property("parameter_name", n); p.set_editor_property("default_value", unreal.LinearColor(*v)); return p
def C(m, v, x, y):
    p = E(m, unreal.MaterialExpressionConstant, x, y); p.set_editor_property("r", v); return p
def O(m, cls, a, b, x, y, oa="", ob=""):
    n = E(m, cls, x, y); MEL.connect_material_expressions(a, oa, n, "A"); MEL.connect_material_expressions(b, ob, n, "B"); return n
def U(m, cls, a, x, y, oa=""):
    n = E(m, cls, x, y); MEL.connect_material_expressions(a, oa, n, ""); return n
def M(m, a, x, y, oa="", r=True, g=True, b=False):
    n = E(m, unreal.MaterialExpressionComponentMask, x, y)
    n.set_editor_property("r", r); n.set_editor_property("g", g); n.set_editor_property("b", b); n.set_editor_property("a", False)
    MEL.connect_material_expressions(a, oa, n, ""); return n
Mul, Add, Sub = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract
def T(m, tex, uv, x, y, sampler=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR):
    s = E(m, unreal.MaterialExpressionTextureSample, x, y); s.set_editor_property("texture", tex); s.set_editor_property("sampler_type", sampler)
    MEL.connect_material_expressions(uv, "", s, "UVs"); return s
def Lerp(m, a, b, t, x, y, oa="", ob=""):
    n = E(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, oa, n, "A"); MEL.connect_material_expressions(b, ob, n, "B"); MEL.connect_material_expressions(t, "", n, "Alpha"); return n

def import_tex():
    out = {}; tasks = []
    for f in sorted(os.listdir(SRC)):
        if f.startswith("T_Smoke_") and f.endswith(".png"):
            t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, f); t.destination_path = ROOT + "/Textures"
            t.destination_name = f[:-4]; t.replace_existing = True; t.automated = True; t.save = False; tasks.append(t)
    AT.import_asset_tasks(tasks)
    for t in tasks:
        p = ROOT + "/Textures/" + t.destination_name
        tex = EAL.load_asset(p)
        if not tex: REP["errors"].append("missing " + p); continue
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        EAL.save_asset(p, only_if_is_dirty=False); out[t.destination_name] = tex
    return out

def rot_uv(m, uv, ang, x, y):
    r = E(m, unreal.MaterialExpressionRotator, x, y); r.set_editor_property("speed", 0.0)
    MEL.connect_material_expressions(uv, "", r, "Coordinate")
    t = C(m, ang / 6.2831853, x - 150, y + 80); MEL.connect_material_expressions(t, "", r, "Time"); return r

def flow_sample(m, tex, flow, uv, speed, strength, x, y, offset=0.0):
    """Two-phase flow-mapped sample (triangle crossfade)."""
    tm = E(m, unreal.MaterialExpressionTime, x - 600, y)
    ph0 = U(m, unreal.MaterialExpressionFrac, O(m, Mul, tm, speed, x - 450, y), x - 300, y)
    ph1 = U(m, unreal.MaterialExpressionFrac, O(m, Add, ph0, C(m, 0.5, x - 450, y + 60), x - 350, y + 60), x - 300, y + 60)
    f = T(m, flow, uv, x - 600, y + 160)
    fv = O(m, Mul, O(m, Sub, O(m, Mul, M(m, f, x - 520, y + 160), C(m, 2.0, x - 500, y + 220), x - 450, y + 160), C(m, 1.0, x - 400, y + 260), x - 350, y + 160), strength, x - 250, y + 160)
    s0 = T(m, tex, O(m, Add, uv, O(m, Mul, fv, ph0, x - 150, y), x - 50, y), x + 50, y)
    s1 = T(m, tex, O(m, Add, uv, O(m, Mul, fv, ph1, x - 150, y + 100), x - 50, y + 100), x + 50, y + 100)
    w = U(m, unreal.MaterialExpressionAbs, O(m, Sub, O(m, Mul, ph0, C(m, 2.0, x - 150, y + 260), x - 100, y + 240), C(m, 1.0, x - 100, y + 300), x, y + 240), x + 80, y + 240)
    return Lerp(m, s0, s1, w, x + 250, y, "R", "R")

def build_master(tx):
    name = "M_ZoogiSmokeOrb"; path = ROOT + "/" + name
    if EAL.does_asset_exist(path):
        m = EAL.load_asset(path); MEL.delete_all_material_expressions(m)
    else:
        m = AT.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    tile = S(m, "TileA", 3.0, -3200, 0); pan = M(m, V(m, "PanA", (0.01, 0.004, 0, 0), -3350, 100), -3200, 100)
    fs = S(m, "FlowSpeed", 0.12, -3200, 200); fst = S(m, "FlowStrength", 0.08, -3200, 300)
    off = S(m, "UVOffset", 0.0, -3200, 400)
    uv0 = O(m, Add, E(m, unreal.MaterialExpressionTextureCoordinate, -3400, -100), off, -3050, -100)
    tm = E(m, unreal.MaterialExpressionTime, -3200, 500)
    uvA = O(m, Add, O(m, Mul, uv0, tile, -2900, 0), O(m, Mul, tm, pan, -2900, 150), -2750, 0)
    a = flow_sample(m, tx["T_Smoke_A"], tx["T_Smoke_Flow"], uvA, fs, fst, -2300, -400)
    uvB = rot_uv(m, O(m, Mul, uv0, C(m, 5.0, -2900, 600), -2800, 600), 0.4, -2650, 600)
    b = T(m, tx["T_Smoke_B"], O(m, Add, uvB, O(m, Mul, tm, pan, -2600, 700), -2450, 600), -2300, 600)
    uvC = rot_uv(m, O(m, Mul, uv0, C(m, 7.0, -2900, 900), -2800, 900), 1.1, -2650, 900)
    c = flow_sample(m, tx["T_Smoke_C"], tx["T_Smoke_Flow"], uvC, O(m, Mul, fs, C(m, 1.37, -2500, 1000), -2400, 1000), fst, -2000, 900)
    d = O(m, Add, O(m, Add, a, O(m, Mul, b, C(m, 0.35, -1900, 700), -1800, 600, "R", ""), -1650, 0),
          O(m, Mul, c, C(m, 0.4, -1700, 950), -1600, 900), -1450, 0)
    uvP = O(m, Add, uv0, O(m, Mul, tm, M(m, V(m, "PanPatches", (0.003, -0.002, 0, 0), -2150, 1300), -2000, 1300), -1900, 1250), -1800, 1200)
    p = T(m, tx["T_Smoke_Patches"], uvP, -1650, 1200)
    pr = U(m, unreal.MaterialExpressionSaturate, O(m, Mul, O(m, Sub, p, C(m, 0.35, -1500, 1300), -1450, 1200, "R", ""), C(m, 2.5, -1400, 1300), -1350, 1200), -1250, 1200)
    pm = O(m, Add, C(m, 0.25, -1150, 1300), O(m, Mul, pr, C(m, 0.75, -1150, 1350), -1100, 1200), -1000, 1200)
    den = O(m, Mul, d, pm, -1250, 0)
    deep = M(m, V(m, "DeepColor", (0.02, 0.12, 0.55, 1), -1250, -500), -1100, -500, b=True); bright = M(m, V(m, "BrightColor", (0.35, 0.75, 1.0, 1), -1250, -400), -1100, -400, b=True)
    s1 = S(m, "Stop1", 0.32, -1100, -300); s2 = S(m, "Stop2", 0.62, -1100, -250)
    t1 = U(m, unreal.MaterialExpressionSaturate, O(m, Divide, den, s1, -950, -200), -850, -200)
    c1 = Lerp(m, O(m, Mul, deep, C(m, 0.25, -900, -600), -800, -600), deep, t1, -700, -500)
    t2 = U(m, unreal.MaterialExpressionSaturate, O(m, Divide, O(m, Sub, den, s1, -950, -100), O(m, Sub, s2, s1, -950, -50), -850, -100), -750, -100)
    col = Lerp(m, c1, bright, t2, -550, -400)
    fr = E(m, unreal.MaterialExpressionFresnel, -700, 200); fr.set_editor_property("exponent", 2.5); fr.set_editor_property("base_reflect_fraction", 0.2)
    col2 = Lerp(m, col, M(m, V(m, "RimColor", (0.47, 0.99, 1.0, 1), -850, 100), -700, 100, b=True), fr, -400, -200)
    emi = O(m, Mul, col2, S(m, "EmissiveIntensity", 8.0, -400, 0), -250, -200)
    MEL.connect_material_property(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.layout_material_expressions(m); MEL.recompile_material(m); EAL.save_loaded_asset(m); log("master built")
    return m
Divide = unreal.MaterialExpressionDivide

def mi(parent, name, vec, sc):
    path = ROOT + "/" + name
    x = EAL.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset(name, ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(x, parent)
    for k, v in vec.items(): MEL.set_material_instance_vector_parameter_value(x, k, unreal.LinearColor(*v))
    for k, v in sc.items(): MEL.set_material_instance_scalar_parameter_value(x, k, v)
    MEL.update_material_instance(x); EAL.save_loaded_asset(x, False); log("mi " + name)

def main():
    for d in (ROOT, ROOT + "/Textures"):
        if not EAL.does_directory_exist(d): EAL.make_directory(d)
    tx = import_tex()
    if len(tx) < 5: REP["errors"].append("textures: %s" % list(tx)); 
    else:
        m = build_master(tx)
        mi(m, "MI_ZoogiSmokeOrb_Blue", {"DeepColor": (0.02, 0.12, 0.55, 1), "BrightColor": (0.35, 0.75, 1.0, 1), "RimColor": (0.47, 0.99, 1.0, 1)}, {"EmissiveIntensity": 8.0})
        mi(m, "MI_ZoogiSmokeOrb_Gold", {"DeepColor": (0.42, 0.10, 0.004, 1), "BrightColor": (1.0, 0.52, 0.10, 1), "RimColor": (1.0, 0.62, 0.22, 1)},
           {"EmissiveIntensity": 5.0, "Stop1": 0.34, "Stop2": 0.68})  # r4: was deep(0.6,0.26,0.02)/bright(1,0.9,0.5)@9 -> clipped flat yellow
    json.dump(REP, open(os.path.join(PROJ, "Saved", "build_smoke_orb_report.json"), "w"), indent=1)
    log("ZOOGI_ORB_DONE errors=%d" % len(REP["errors"]))
main()
