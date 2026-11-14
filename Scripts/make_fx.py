# Creates /Game/Materials/M_ZoogiFX: additive, unlit, two-sided glow with a fresnel rim (power shockwaves, clones, zaps).
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

path = "/Game/Materials/M_ZoogiFX"
if EAL.does_asset_exist(path):
    EAL.delete_asset(path)
m = AT.create_asset("M_ZoogiFX", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)

color = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -1000, -200)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.5, 0.1, 1.0))
strength = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1000, 0)
strength.set_editor_property("parameter_name", "Strength")
strength.set_editor_property("default_value", 4.0)
fres = MEL.create_material_expression(m, unreal.MaterialExpressionFresnel, -1000, 200)
fres.set_editor_property("exponent", 2.5)
fres.set_editor_property("base_reflect_fraction", 0.12)

ring_p = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -1000, 400)
ring_p.set_editor_property("parameter_name", "Ring")
ring_p.set_editor_property("default_value", 0.0)

# Sphere look: fresnel rim plus a faint body.
body = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -800, 200)
MEL.connect_material_expressions(fres, "", body, "A")
c015 = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -1000, 300)
c015.set_editor_property("r", 0.12)
MEL.connect_material_expressions(c015, "", body, "B")

# Ring look: brightness rises toward the edge of the flattened cylinder (distance from the object centre / object radius).
wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1600, 600)
op = MEL.create_material_expression(m, unreal.MaterialExpressionObjectPositionWS, -1600, 700)
sub = MEL.create_material_expression(m, unreal.MaterialExpressionSubtract, -1400, 650)
MEL.connect_material_expressions(wp, "", sub, "A")
MEL.connect_material_expressions(op, "", sub, "B")
cm = MEL.create_material_expression(m, unreal.MaterialExpressionComponentMask, -1250, 650)
cm.set_editor_property("r", True)
cm.set_editor_property("g", True)
cm.set_editor_property("b", False)
cm.set_editor_property("a", False)
MEL.connect_material_expressions(sub, "", cm, "")
zero2 = MEL.create_material_expression(m, unreal.MaterialExpressionConstant2Vector, -1250, 750)
dist = MEL.create_material_expression(m, unreal.MaterialExpressionDistance, -1100, 650)
MEL.connect_material_expressions(cm, "", dist, "A")
MEL.connect_material_expressions(zero2, "", dist, "B")
orad = MEL.create_material_expression(m, unreal.MaterialExpressionObjectRadius, -1100, 780)
div = MEL.create_material_expression(m, unreal.MaterialExpressionDivide, -950, 680)
MEL.connect_material_expressions(dist, "", div, "A")
MEL.connect_material_expressions(orad, "", div, "B")
sat = MEL.create_material_expression(m, unreal.MaterialExpressionSaturate, -820, 680)
MEL.connect_material_expressions(div, "", sat, "")
pw = MEL.create_material_expression(m, unreal.MaterialExpressionPower, -700, 680)
MEL.connect_material_expressions(sat, "", pw, "Base")
e5 = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -820, 780)
e5.set_editor_property("r", 5.0)
MEL.connect_material_expressions(e5, "", pw, "Exp")

mix = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -550, 300)
MEL.connect_material_expressions(body, "", mix, "A")
MEL.connect_material_expressions(pw, "", mix, "B")
MEL.connect_material_expressions(ring_p, "", mix, "Alpha")

c = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -700, -100)
MEL.connect_material_expressions(color, "", c, "A")
MEL.connect_material_expressions(strength, "", c, "B")
e = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 0)
MEL.connect_material_expressions(c, "", e, "A")
MEL.connect_material_expressions(mix, "", e, "B")
MEL.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.layout_material_expressions(m)
MEL.recompile_material(m)
EAL.save_loaded_asset(m)
unreal.log("[ZoogiSetup] created M_ZoogiFX blend=%s" % m.get_editor_property("blend_mode"))
