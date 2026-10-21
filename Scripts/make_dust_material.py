# Creates /Game/Materials/M_ZoogiDust: lit, translucent, never emissive stone dust for AZoogiRock (Oct 4 realism pass).
# Soft sphere edge (view-facing falloff, param Soft 0..1), slow drifting noise breakup, soft depth fade where the puff
# meets the turf or the boulder (FadeDistance, cm). Vertex colour RGB tints, vertex alpha fades (rubble powder stain).
# Params set per puff from C++: Color, Opacity, Soft, FadeDistance. Re-runnable (deletes and recreates the asset).
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

path = "/Game/Materials/M_ZoogiDust"
if EAL.does_asset_exist(path):
    EAL.delete_asset(path)
m = AT.create_asset("M_ZoogiDust", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property("two_sided", False)
try:
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
except Exception as ex:  # older/newer enum naming: keep the default lighting mode
    unreal.log_warning("M_ZoogiDust: could not set translucency lighting mode: %s" % ex)


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


def binop(cls, a, b, x, y, a_out="", b_out=""):
    n = expr(cls, x, y)
    MEL.connect_material_expressions(a, a_out, n, "A")
    MEL.connect_material_expressions(b, b_out, n, "B")
    return n


# Parameters.
color = expr(unreal.MaterialExpressionVectorParameter, -1600, -400)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(0.3, 0.28, 0.24, 1.0))
opacity = scalar("Opacity", 0.4, -1600, -200)
soft = scalar("Soft", 1.0, -1600, 0)
fade = scalar("FadeDistance", 30.0, -1600, 900)
vcol = expr(unreal.MaterialExpressionVertexColor, -1600, -600)

# Base colour = Color * vertex RGB. Roughness 1 (dust is fully matte).
base = binop(unreal.MaterialExpressionMultiply, color, vcol, -1200, -500)
MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(const(1.0, -400, -300), "", unreal.MaterialProperty.MP_ROUGHNESS)

# Soft edge: lerp(1, saturate(N.V)^1.6, Soft).
nrm = expr(unreal.MaterialExpressionVertexNormalWS, -1600, 150)
cam = expr(unreal.MaterialExpressionCameraVectorWS, -1600, 250)
dot = binop(unreal.MaterialExpressionDotProduct, nrm, cam, -1400, 200)
sat = expr(unreal.MaterialExpressionSaturate, -1250, 200)
MEL.connect_material_expressions(dot, "", sat, "")
pw = expr(unreal.MaterialExpressionPower, -1100, 200)
MEL.connect_material_expressions(sat, "", pw, "Base")
MEL.connect_material_expressions(const(1.6, -1250, 300), "", pw, "Exp")
edge = expr(unreal.MaterialExpressionLinearInterpolate, -900, 150)
MEL.connect_material_expressions(const(1.0, -1100, 100), "", edge, "A")
MEL.connect_material_expressions(pw, "", edge, "B")
MEL.connect_material_expressions(soft, "", edge, "Alpha")

# Breakup: world-space noise drifting slowly upwards (0.3 .. 1).
wp = expr(unreal.MaterialExpressionWorldPosition, -1600, 450)
tm = expr(unreal.MaterialExpressionTime, -1600, 550)
up = expr(unreal.MaterialExpressionConstant3Vector, -1600, 620)
up.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 12.0, 0.0))
drift = binop(unreal.MaterialExpressionMultiply, tm, up, -1400, 580)
npos = binop(unreal.MaterialExpressionSubtract, wp, drift, -1250, 500)
noise = expr(unreal.MaterialExpressionNoise, -1100, 500)
noise.set_editor_property("scale", 0.012)
noise.set_editor_property("levels", 2)
noise.set_editor_property("output_min", 0.3)
noise.set_editor_property("output_max", 1.0)
MEL.connect_material_expressions(npos, "", noise, "Position")

# Depth fade against opaque geometry: saturate((SceneDepth - PixelDepth) / FadeDistance).
sd = expr(unreal.MaterialExpressionSceneDepth, -1600, 750)
pd = expr(unreal.MaterialExpressionPixelDepth, -1600, 830)
dd = binop(unreal.MaterialExpressionSubtract, sd, pd, -1400, 780)
ddiv = binop(unreal.MaterialExpressionDivide, dd, fade, -1250, 820)
dsat = expr(unreal.MaterialExpressionSaturate, -1100, 820)
MEL.connect_material_expressions(ddiv, "", dsat, "")

# Opacity = Opacity * vertex alpha * edge * noise * depth fade.
o1 = binop(unreal.MaterialExpressionMultiply, opacity, vcol, -900, -150, "", "A")
o2 = binop(unreal.MaterialExpressionMultiply, o1, edge, -750, 0)
o3 = binop(unreal.MaterialExpressionMultiply, o2, noise, -600, 200)
o4 = binop(unreal.MaterialExpressionMultiply, o3, dsat, -450, 400)
MEL.connect_material_property(o4, "", unreal.MaterialProperty.MP_OPACITY)

MEL.layout_material_expressions(m)
MEL.recompile_material(m)
EAL.save_loaded_asset(m)
unreal.log("M_ZoogiDust created (%d expressions)" % MEL.get_num_material_expressions(m))
