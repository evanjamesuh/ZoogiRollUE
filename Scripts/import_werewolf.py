# Nov 1 (Evan: "Make the wolf's this I bought in the past"): imports Bitgem's Micro Werewolf (Micro Monster & Heroes Pack,
# Evan's Unity Asset Store purchase) for the Wolf Pack clones: SK_MicroWerewolf + clip anims (Bitgem frame list) +
# T_MicroWerewolf + M_MicroWerewolf (Tex, Tint, TintAmt, Glow).
import os
import unreal

SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Characters", "MicroWerewolf")
FBX = os.path.join(SRC, "micro_werewolf.fbx")
DEST = "/Game/Characters/MicroWerewolf"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
def log(s): unreal.log("[ZoogiWerewolf] " + s)

# 1) skeletal mesh
ui = unreal.FbxImportUI()
ui.set_editor_property("import_mesh", True)
ui.set_editor_property("import_as_skeletal", True)
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property("import_animations", False)
ui.set_editor_property("import_materials", False)
ui.set_editor_property("import_textures", False)
ui.set_editor_property("create_physics_asset", False)
t = unreal.AssetImportTask()
t.filename = FBX; t.destination_path = DEST; t.destination_name = "SK_MicroWerewolf"
t.replace_existing = True; t.automated = True; t.save = True; t.options = ui
AT.import_asset_tasks([t])
sk = None
for p in t.imported_object_paths:
    a = unreal.load_asset(p)
    log("mesh import: %s (%s)" % (p, type(a).__name__))
    if isinstance(a, unreal.SkeletalMesh): sk = a
if not sk:
    sk = unreal.load_asset(DEST + "/SK_MicroWerewolf")
skel = sk.get_editor_property("skeleton") if sk else None
log("skeletal mesh %s skeleton %s" % (sk, skel))
if sk:
    b = sk.get_bounds()
    log("BOUNDS origin %s extent %s radius %.1f" % (b.origin, b.box_extent, b.sphere_radius))

# 2) texture
tt = unreal.AssetImportTask()
tt.filename = os.path.join(SRC, "T_MicroWerewolf.png"); tt.destination_path = DEST; tt.destination_name = "T_MicroWerewolf"
tt.replace_existing = True; tt.automated = True; tt.save = True
AT.import_asset_tasks([tt])
tex = unreal.load_asset(DEST + "/T_MicroWerewolf")
log("texture %s" % tex)

# 3) material: BaseColor = Tex * lerp(1, Tint, TintAmt); Emissive = Tex * Tint * Glow
mpath = DEST + "/M_MicroWerewolf"
if EAL.does_asset_exist(mpath): EAL.delete_asset(mpath)
m = AT.create_asset("M_MicroWerewolf", DEST, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("used_with_skeletal_mesh", True)
ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
ts.set_editor_property("parameter_name", "Tex"); ts.set_editor_property("texture", tex)
tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -900, 300)
tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
amt = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 450)
amt.set_editor_property("parameter_name", "TintAmt"); amt.set_editor_property("default_value", 0.0)
glow = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 550)
glow.set_editor_property("parameter_name", "Glow"); glow.set_editor_property("default_value", 0.0)
one = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -650, 200)
one.set_editor_property("constant", unreal.LinearColor(1, 1, 1, 1))
lp = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -450, 250)
MEL.connect_material_expressions(one, "", lp, "A"); MEL.connect_material_expressions(tint, "", lp, "B"); MEL.connect_material_expressions(amt, "", lp, "Alpha")
mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 100)
MEL.connect_material_expressions(ts, "RGB", mul, "A"); MEL.connect_material_expressions(lp, "", mul, "B")
MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
# hand-painted look: part of the painted colour is self-lit (BaseLit, default 0.45) + Glow for charge
blit = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -900, 650)
blit.set_editor_property("parameter_name", "BaseLit"); blit.set_editor_property("default_value", 0.45)
m2 = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -450, 450)
MEL.connect_material_expressions(blit, "", m2, "A"); MEL.connect_material_expressions(glow, "", m2, "B")
m3 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 400)
MEL.connect_material_expressions(mul, "", m3, "A"); MEL.connect_material_expressions(m2, "", m3, "B")
MEL.connect_material_property(m3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 600)
rough.set_editor_property("r", 0.85)
MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
spec = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 700)
spec.set_editor_property("r", 0.25)
MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m)
EAL.save_loaded_asset(m)
if sk:
    mats = sk.get_editor_property("materials")
    for sm in mats: sm.set_editor_property("material_interface", m)
    sk.set_editor_property("materials", mats)
    EAL.save_loaded_asset(sk)
    log("material slots: %d" % len(mats))

# 4) animation clips from the single take (Bitgem frame list, _micro_wolf_animations.txt)
CLIPS = [("walk", 145, 205), ("bite", 215, 245), ("idle", 6, 65), ("dizzy", 615, 645), ("laugh", 715, 745),
         ("jump", 395, 399), ("up_loop", 399, 407), ("fall_idle", 419, 427), ("land", 427, 433), ("die", 445, 490),
         ("panic", 325, 385), ("attack2", 545, 605), ("hit_front", 255, 275), ("getup", 515, 535), ("talk", 655, 705)]
n = 0
for name, a0, a1 in CLIPS if skel else []:
    final = DEST + "/A_MW_" + name
    if EAL.does_asset_exist(final): EAL.delete_asset(final)
    tmp = DEST + "/_tmp_" + name
    au = unreal.FbxImportUI()
    au.set_editor_property("import_mesh", False)
    au.set_editor_property("import_as_skeletal", True)
    au.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    au.set_editor_property("import_animations", True)
    au.set_editor_property("import_materials", False)
    au.set_editor_property("import_textures", False)
    au.set_editor_property("skeleton", skel)
    ad = au.get_editor_property("anim_sequence_import_data")
    ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_SET_RANGE)
    ad.set_editor_property("frame_import_range", unreal.Int32Interval(a0, a1))
    ad.set_editor_property("import_bone_tracks", True)
    at = unreal.AssetImportTask()
    at.filename = FBX; at.destination_path = tmp; at.destination_name = "A_MW_" + name
    at.replace_existing = True; at.automated = True; at.save = True; at.options = au
    AT.import_asset_tasks([at])
    got = None
    for p in list(at.imported_object_paths) + list(EAL.list_assets(tmp, recursive=True)):
        o = unreal.load_asset(p)
        if isinstance(o, unreal.AnimSequence): got = o; break
    if got:
        EAL.rename_asset(got.get_path_name().split(".")[0], final)
        o = unreal.load_asset(final)
        log("anim %s frames %d-%d -> %s len %.2fs" % (name, a0, a1, final, o.get_play_length() if o else -1))
        n += 1
    else:
        log("anim %s FAILED (paths %s)" % (name, list(at.imported_object_paths)))
    if EAL.does_directory_exist(tmp): EAL.delete_directory(tmp)
log("done: %d anims" % n)
