# Creates /Game/Materials/M_ZoogiArtifact: additive, unlit, two-sided energy overlay for infused Zoogis.
# Parameters (set by UZoogiArtifactComponent): Color, Strength, Pattern (artifact index 0..4), Mask (0..1 spread).
# Emissive = Color * Strength * Mask * (rim + moving bands); band frequency and speed depend on Pattern.
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

path = "/Game/Materials/M_ZoogiArtifact"
if EAL.does_asset_exist(path):
    EAL.delete_asset(path)
m = AT.create_asset("M_ZoogiArtifact", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)

def expr(cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)

def const(v, x, y):
    c = expr(unreal.MaterialExpressionConstant, x, y)
    c.set_editor_property("r", v)
    return c

def scalar(name, v, x, y):
    p = expr(unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", v)
    return p

def op(cls, a, b, x, y, ia="A", ib="B"):
    e = expr(cls, x, y)
    MEL.connect_material_expressions(a, "", e, ia)
    MEL.connect_material_expressions(b, "", e, ib)
    return e

color = expr(unreal.MaterialExpressionVectorParameter, -1400, -300)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.5, 0.1, 1.0))
strength = scalar("Strength", 4.0, -1400, -150)
pattern = scalar("Pattern", 0.0, -1800, 300)
mask = scalar("Mask", 1.0, -1400, 0)

# Rim glow.
fres = expr(unreal.MaterialExpressionFresnel, -1400, 150)
fres.set_editor_property("exponent", 2.0)
fres.set_editor_property("base_reflect_fraction", 0.08)

# Moving bands: frac(dot(localPos, dir) * freq - time * speed), sharpened.
wp = expr(unreal.MaterialExpressionWorldPosition, -2200, 450)
opos = expr(unreal.MaterialExpressionObjectPositionWS, -2200, 550)
rel = op(unreal.MaterialExpressionSubtract, wp, opos, -2000, 500)
dirv = expr(unreal.MaterialExpressionConstant3Vector, -2000, 620)
dirv.set_editor_property("constant", unreal.LinearColor(0.35, 0.2, 0.92, 0.0))
d = op(unreal.MaterialExpressionDotProduct, rel, dirv, -1850, 520)
freq = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, pattern, const(0.03, -1800, 400), -1650, 350), const(0.06, -1650, 430), -1500, 380)
speed = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, pattern, const(0.35, -1800, 700), -1650, 680), const(0.8, -1650, 760), -1500, 700)
t = expr(unreal.MaterialExpressionTime, -1650, 820)
ts = op(unreal.MaterialExpressionMultiply, t, speed, -1350, 760)
df = op(unreal.MaterialExpressionMultiply, d, freq, -1350, 520)
phase = op(unreal.MaterialExpressionSubtract, df, ts, -1200, 600)
fr = expr(unreal.MaterialExpressionFrac, -1050, 600)
MEL.connect_material_expressions(phase, "", fr, "")
# triangle wave -> sharpen
tri = op(unreal.MaterialExpressionSubtract, fr, const(0.5, -1050, 700), -920, 620)
ab = expr(unreal.MaterialExpressionAbs, -800, 620)
MEL.connect_material_expressions(tri, "", ab, "")
inv = op(unreal.MaterialExpressionSubtract, const(0.5, -800, 720), ab, -680, 640)
band = op(unreal.MaterialExpressionMultiply, inv, const(2.0, -680, 740), -560, 660)
pw = expr(unreal.MaterialExpressionPower, -440, 660)
MEL.connect_material_expressions(band, "", pw, "Base")
MEL.connect_material_expressions(const(6.0, -560, 760), "", pw, "Exp")

look = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, fres, const(0.8, -1200, 230), -1050, 180), op(unreal.MaterialExpressionAdd, pw, const(0.08, -440, 760), -300, 700), -250, 300)
cs = op(unreal.MaterialExpressionMultiply, color, strength, -1100, -250)
csm = op(unreal.MaterialExpressionMultiply, cs, mask, -900, -150)
e = op(unreal.MaterialExpressionMultiply, csm, look, -100, 0)
MEL.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.layout_material_expressions(m)
MEL.recompile_material(m)
EAL.save_loaded_asset(m)
unreal.log("[ZoogiSetup] created M_ZoogiArtifact")
