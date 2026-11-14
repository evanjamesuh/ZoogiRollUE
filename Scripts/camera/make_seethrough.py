# b161 (Evan Nov 6 12:57: when the camera sits behind a barrier "I want only that specific barrier to be see-through so players can
# still launch and see as normal"): every opaque Ancient Collision Meadows material gets a cut-out around the line from the camera to
# the active Zoogi. MPC_ZoogiCam.Zoogi = (Zoogi x, y, z, on) is set every frame by ZoogiOcclusionFade::Tick. Only the pixels of a
# prop that sit in front of the Zoogi, above its feet and inside a narrow cone toward it vanish - the rest of the prop stays.
import unreal
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
def log(s): unreal.log("[ZoogiSeeThrough] " + s)
MPC_PATH = "/Game/Materials/MPC_ZoogiCam"
mpc = unreal.load_asset(MPC_PATH) if EAL.does_asset_exist(MPC_PATH) else None
if not mpc:
    mpc = AT.create_asset("MPC_ZoogiCam", "/Game/Materials", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
p = unreal.CollectionVectorParameter()
p.set_editor_property("parameter_name", "Zoogi")
p.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, -100000.0, 0.0))
mpc.set_editor_property("vector_parameters", [p])
EAL.save_loaded_asset(mpc)
log("MPC %s" % mpc.get_path_name())
CODE = r'''
float3 d = Z.xyz - C; float L = length(d);
if (Z.w <= 0.5 || L < 100.0) return 1.0;
float3 u = d / L; float t = dot(P - C, u);
if (t <= 0.0 || t > L - 60.0) return 1.0;
if (P.z < Z.z - 30.0) return 1.0;
float r = length((P - C) - u * t);
float R = max(55.0, 0.17 * t);
return r > R ? 1.0 : 0.0;
'''
SKIP = ("Water", "Portal", "Fall", "Mist", "Foam", "Wave", "Moat")
done = skipped = 0
for a in [x for d in ("/Game/Env/AncientV14", "/Game/Env/AncientV15") if EAL.does_directory_exist(d) for x in EAL.list_assets(d, recursive=True)]:  # b162: + v15
    m = unreal.load_asset(a)
    if not isinstance(m, unreal.Material): continue
    nm = m.get_name()
    if any(s in nm for s in SKIP): skipped += 1; continue
    if m.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE: skipped += 1; log("skip %s (not opaque)" % nm); continue
    if MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_OPACITY_MASK): skipped += 1; continue
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    cu = MEL.create_material_expression(m, unreal.MaterialExpressionCustom, -700, 900)
    cu.set_editor_property("code", CODE)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    cu.set_editor_property("description", "ZoogiSeeThrough")
    ins = []
    for n in ("P", "C", "Z"):
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    cu.set_editor_property("inputs", ins)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1000, 860)
    cp = MEL.create_material_expression(m, unreal.MaterialExpressionCameraPositionWS, -1000, 920)
    cv = MEL.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -1000, 980)
    cv.set_editor_property("collection", mpc)
    cv.set_editor_property("parameter_name", "Zoogi")
    ok = MEL.connect_material_expressions(wp, "", cu, "P") and MEL.connect_material_expressions(cp, "", cu, "C") and MEL.connect_material_expressions(cv, "", cu, "Z") \
        and MEL.connect_material_property(cu, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    done += 1
    log("cut-out %s ok=%s" % (nm, ok))
log("done: %d materials, %d skipped" % (done, skipped))
