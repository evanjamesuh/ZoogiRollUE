# Imports the 8 Buzzy bible characters (Blue, Tavi, Vex, Moss, MasterOrin, Sera, Luma, DarkZoogy) into
# /Game/Characters/Bible/<Name>:
#   SM_<Name>        shell + eyes + trim + emissive core (one mesh, one material slot per part), radius 95.8 cm, pivot = centre
#   SM_<Name>_Nubs   the two visual-only flipper nubs (AZoogiBall::Nubs, toggle: [ZoogiRoll.Characters] ShowFlipperNubs)
#   Textures/T_<Name>_<Part>_<Map>   BC (sRGB), ORM (R=AO G=roughness B=metallic, linear masks), N (normal, green flipped), E
#   Materials/MI_<Name>_<Slot>       instances of the cel-friendly masters in /Game/Characters/Bible/Materials:
#       M_BibleZoogi        opaque, textured (BC/ORM/N/E) + anime rim (fresnel) + EmissiveBoost
#       M_BibleZoogiGlass   translucent (surface forward shading) for Blue / Luma: opacity = BC alpha * Opacity + rim
#       M_BibleZoogiMasked  masked (alpha-cut eye/face decals of Tavi / Master Orin)
#       M_BibleZoogiFactor  untextured trim / sockets / nubs (BaseColor, Metallic, Roughness, Emissive) + rim
#       M_BibleZoogiEyes(Masked)  eye / visor decal reading a 4x4 expression atlas (T_<Name>_EyesAtlas_BC/E); ExprIndex picks
#                           the cell (0 neutral .. 11 flourish), driven by UZoogiExpressionComponent at runtime
# Source files: SourceArt/Characters/Bible/<Name>/<Name>_UE.glb + T_*.png (made on the build box, see Docs/BIBLE_CHARACTERS.md).
# Run headless (no other editor open): .\Scripts\run_python.ps1 Scripts\import_bible_characters.py import_bible
# Args: --force (or env ZOOGI_BIBLE_FORCE=1, since run_python.ps1 passes no args) re-creates the master materials;
# names (e.g. Blue Vex, or env ZOOGI_BIBLE_ONLY=Blue,Vex) limit the import.
import os
import sys
import json
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
PROJ = unreal.Paths.project_dir()
SRC = os.path.join(PROJ, "SourceArt", "Characters", "Bible")
ROOT = "/Game/Characters/Bible"
MATF = ROOT + "/Materials"
ARGS = sys.argv[1:]
NAMES = ["Blue", "Tavi", "Vex", "Moss", "MasterOrin", "Sera", "Luma", "DarkZoogy"]
REPORT = os.path.join(PROJ, "Saved", "import_bible_report.json")
report = {"meshes": {}, "errors": [], "log": []}


def log(m):
    unreal.log("[ZoogiBible] " + str(m))
    report["log"].append(str(m))


def slots_json():
    with open(os.path.join(SRC, "bible_materials.json")) as f:
        return json.load(f)


# ------------------------------------------------------------------ master materials
def expr(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def sparam(m, name, v, x, y):
    p = expr(m, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", v)
    return p


def vparam(m, name, v, x, y):
    p = expr(m, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", unreal.LinearColor(*v))
    return p


def tparam(m, name, default_path, sampler, x, y):
    p = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("texture", EAL.load_asset(default_path))
    p.set_editor_property("sampler_type", sampler)
    return p


def op(m, cls, a, b, x, y, oa="", ob=""):
    e = expr(m, cls, x, y)
    if a is not None:
        MEL.connect_material_expressions(a, oa, e, "A")
    if b is not None:
        MEL.connect_material_expressions(b, ob, e, "B")
    return e


def rim(m, x, y):
    """anime rim light: RimColor * RimStrength * fresnel^RimPower (cel-friendly: a hard-ish edge, no extra lights)."""
    fres = expr(m, unreal.MaterialExpressionFresnel, x, y)
    pw = sparam(m, "RimPower", 3.0, x - 250, y + 80)
    MEL.connect_material_expressions(pw, "", fres, "ExponentIn")
    col = vparam(m, "RimColor", (1.0, 1.0, 1.0, 1.0), x - 250, y + 180)
    st = sparam(m, "RimStrength", 0.35, x - 250, y + 300)
    return op(m, unreal.MaterialExpressionMultiply, op(m, unreal.MaterialExpressionMultiply, fres, col, x + 200, y), st, x + 400, y)


FORCE = "--force" in ARGS or bool(os.environ.get("ZOOGI_BIBLE_FORCE"))
FLAT_NORMAL = MATF + "/T_Bible_FlatNormal"


ORM_DEFAULT = MATF + "/T_Bible_ORMDefault"


def import_defaults():
    """Our own default textures for the master parameters (the engine's DefaultNormal is not in the 5.6 asset registry,
    and the sRGB WhiteSquareTexture can't sit in a Masks / linear sampler):
    T_Bible_FlatNormal (8x8 flat normal) and T_Bible_ORMDefault (AO 1, roughness 0.5, metallic 0, TC_MASKS)."""
    for path, fn, comp in ((FLAT_NORMAL, "T_Bible_FlatNormal.png", unreal.TextureCompressionSettings.TC_NORMALMAP),
                           (ORM_DEFAULT, "T_Bible_ORMDefault.png", unreal.TextureCompressionSettings.TC_MASKS)):
        if EAL.does_asset_exist(path) and not FORCE:
            continue
        t = unreal.AssetImportTask()
        t.filename = os.path.join(SRC, fn)
        t.destination_path = MATF
        t.destination_name = path.split("/")[-1]
        t.replace_existing = True
        t.automated = True
        t.save = False
        AT.import_asset_tasks([t])
        tex = EAL.load_asset(path)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", comp)
        EAL.save_asset(path, only_if_is_dirty=False)
        log("default texture " + path)


def new_material(name):
    path = MATF + "/" + name
    if EAL.does_asset_exist(path):
        if not FORCE:
            return None
        # Rebuild the graph in place (instances keep pointing at the same asset).
        m = EAL.load_asset(path)
        MEL.delete_all_material_expressions(m)
        return m
    return AT.create_asset(name, MATF, unreal.Material, unreal.MaterialFactoryNew())


def make_textured(name, blend):
    m = new_material(name)
    if m is None:
        return
    m.set_editor_property("blend_mode", blend)
    if blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    S = unreal.MaterialSamplerType
    bc = tparam(m, "BaseColorTex", "/Engine/EngineResources/WhiteSquareTexture", S.SAMPLERTYPE_COLOR, -1400, -500)
    orm = tparam(m, "ORMTex", ORM_DEFAULT, S.SAMPLERTYPE_MASKS, -1400, -150)  # ORM textures import as TC_MASKS
    nrm = tparam(m, "NormalTex", FLAT_NORMAL, S.SAMPLERTYPE_NORMAL, -1400, 150)
    emt = tparam(m, "EmissiveTex", "/Engine/EngineResources/Black", S.SAMPLERTYPE_COLOR, -1400, 450)
    tint = vparam(m, "Tint", (1, 1, 1, 1), -1100, -650)
    base = op(m, unreal.MaterialExpressionMultiply, bc, tint, -900, -500, "RGB", "")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = op(m, unreal.MaterialExpressionMultiply, orm, sparam(m, "RoughnessScale", 1.0, -1100, -100), -900, -150, "G", "")
    metal = op(m, unreal.MaterialExpressionMultiply, orm, sparam(m, "MetallicScale", 1.0, -1100, 0), -900, -50, "B", "")
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    em = op(m, unreal.MaterialExpressionMultiply, emt, sparam(m, "EmissiveBoost", 1.0, -1100, 550), -900, 450, "RGB", "")
    em2 = op(m, unreal.MaterialExpressionAdd, em, rim(m, -900, 700), -500, 500)
    MEL.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        f = expr(m, unreal.MaterialExpressionFresnel, -900, 900)
        a = op(m, unreal.MaterialExpressionMultiply, bc, sparam(m, "Opacity", 1.0, -1100, 1000), -700, 900, "A", "")
        a2 = op(m, unreal.MaterialExpressionAdd, a, op(m, unreal.MaterialExpressionMultiply, f, sparam(m, "RimOpacity", 0.35, -1100, 1100), -700, 1050), -500, 950)
        sat = expr(m, unreal.MaterialExpressionSaturate, -300, 950)
        MEL.connect_material_expressions(a2, "", sat, "")
        MEL.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY)
    if blend == unreal.BlendMode.BLEND_MASKED:
        MEL.connect_material_property(bc, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("material " + name)


def make_factor():
    m = new_material("M_BibleZoogiFactor")
    if m is None:
        return
    base = vparam(m, "BaseColor", (0.8, 0.6, 0.3, 1), -900, -400)
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(sparam(m, "Metallic", 0.0, -900, -200), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(sparam(m, "Roughness", 0.4, -900, -100), "", unreal.MaterialProperty.MP_ROUGHNESS)
    em = op(m, unreal.MaterialExpressionMultiply, vparam(m, "EmissiveColor", (0, 0, 0, 1), -1100, 100),
            sparam(m, "EmissiveStrength", 0.0, -1100, 250), -900, 150)
    em2 = op(m, unreal.MaterialExpressionAdd, em, rim(m, -900, 400), -500, 250)
    MEL.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("material M_BibleZoogiFactor")


def make_eyes(name, masked):
    """Eye / face decal with the 4x4 expression atlas: cell = ExprIndex (row-major from the top-left), set at runtime by
    UZoogiExpressionComponent (blinks, phase expressions). AtlasUV = (clamp(UV) + (col, row)) / (Cols, Rows)."""
    m = new_material(name)
    if m is None:
        return
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE)
    # Unlit (anime eyes read flat): no shadow acne from the shell 0.4 % below the decal, and faces stay readable from the
    # 45-60 deg gameplay camera whatever the sun does. Brightness = FaceBrightness.
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    S = unreal.MaterialSamplerType
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -2400, 0)
    cl = expr(m, unreal.MaterialExpressionClamp, -2200, 0)
    cl.set_editor_property("min_default", 0.004)
    cl.set_editor_property("max_default", 0.996)
    MEL.connect_material_expressions(uv, "", cl, "")
    idx = sparam(m, "ExprIndex", 0.0, -2600, 300)
    cols = sparam(m, "Cols", 4.0, -2600, 420)
    rows = sparam(m, "Rows", 4.0, -2600, 540)
    half = expr(m, unreal.MaterialExpressionConstant, -2600, 640)
    half.set_editor_property("r", 0.5)
    ir = expr(m, unreal.MaterialExpressionFloor, -2200, 300)
    MEL.connect_material_expressions(op(m, unreal.MaterialExpressionAdd, idx, half, -2400, 300), "", ir, "")
    col = op(m, unreal.MaterialExpressionFmod, ir, cols, -2000, 300)
    row = expr(m, unreal.MaterialExpressionFloor, -1800, 420)
    MEL.connect_material_expressions(op(m, unreal.MaterialExpressionDivide, ir, cols, -2000, 420), "", row, "")
    cr = op(m, unreal.MaterialExpressionAppendVector, col, row, -1600, 300)
    cells = op(m, unreal.MaterialExpressionAppendVector, cols, rows, -1600, 450)
    auv = op(m, unreal.MaterialExpressionDivide, op(m, unreal.MaterialExpressionAdd, cl, cr, -1400, 150), cells, -1200, 200)
    bc = tparam(m, "BaseColorTex", "/Engine/EngineResources/WhiteSquareTexture", S.SAMPLERTYPE_COLOR, -1000, -300)
    emt = tparam(m, "EmissiveTex", "/Engine/EngineResources/Black", S.SAMPLERTYPE_COLOR, -1000, 300)
    MEL.connect_material_expressions(auv, "", bc, "UVs")
    MEL.connect_material_expressions(auv, "", emt, "UVs")
    tint = vparam(m, "Tint", (1, 1, 1, 1), -800, -450)
    face = op(m, unreal.MaterialExpressionMultiply, op(m, unreal.MaterialExpressionMultiply, bc, tint, -600, -300, "RGB", ""),
              sparam(m, "FaceBrightness", 0.85, -800, -200), -400, -250)
    em = op(m, unreal.MaterialExpressionMultiply, emt, sparam(m, "EmissiveBoost", 1.0, -800, 450), -600, 300, "RGB", "")
    em2 = op(m, unreal.MaterialExpressionMultiply, em, vparam(m, "GlowTint", (1, 1, 1, 1), -800, 600), -400, 350)
    MEL.connect_material_property(op(m, unreal.MaterialExpressionAdd, face, em2, -200, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if masked:
        MEL.connect_material_property(bc, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("material " + name)


# ------------------------------------------------------------------ textures
def import_textures(name):
    src = os.path.join(SRC, name)
    dest = "%s/%s/Textures" % (ROOT, name)
    tasks = []
    for f in sorted(os.listdir(src)):
        if not (f.startswith("T_") and f.lower().endswith(".png")):
            continue
        t = unreal.AssetImportTask()
        t.filename = os.path.join(src, f)
        t.destination_path = dest
        t.destination_name = os.path.splitext(f)[0]
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    AT.import_asset_tasks(tasks)
    out = {}
    for t in tasks:
        # Interchange doesn't always fill imported_object_paths: fall back to the known destination path.
        paths = list(t.imported_object_paths) or ["%s/%s" % (dest, t.destination_name)]
        for p in paths:
            if not EAL.does_asset_exist(p.split(".")[0]):
                report["errors"].append("texture missing after import: " + p)
                continue
            tex = EAL.load_asset(p)
            if not isinstance(tex, unreal.Texture2D):
                continue
            n = tex.get_name()
            if n.endswith("_ORM"):
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            elif n.endswith("_N"):
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("flip_green_channel", True)  # glTF / Blender (OpenGL) -> UE (DirectX)
            else:
                # Colour maps (BC / E / atlases): force sRGB + default (DXT) compression. The importer's normal-map
                # auto-detection flags bluish textures (Blue's eye atlas / shell, Sera's core) as BC5 normal maps, which drops
                # the blue channel (white -> yellow, sky blue -> green in game).
                tex.set_editor_property("srgb", True)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                if "_EyesAtlas_" in n:
                    # Faces must be sharp from the first frame and cells must not bleed: never stream, clamp addressing.
                    tex.set_editor_property("never_stream", True)
                    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
                    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
                    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
            report.setdefault("textures", {})[n] = str(tex.get_editor_property("compression_settings"))
            EAL.save_asset(p, only_if_is_dirty=False)
            out[n] = tex
    return out


# ------------------------------------------------------------------ meshes
def interchange_import(src, dest):
    mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(src)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    try:
        params.replace_existing = True
    except Exception:
        pass
    return mgr.import_asset(dest, source, params)


def import_mesh(name):
    folder = "%s/%s" % (ROOT, name)
    before = set(EAL.list_assets(folder, recursive=True, include_folder=False)) if EAL.does_directory_exist(folder) else set()
    interchange_import(os.path.join(SRC, name, name + "_UE.glb"), folder)
    after = set(EAL.list_assets(folder, recursive=True, include_folder=False))
    body = nubs = None
    for p in sorted(after):
        data = EAL.find_asset_data(p)
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        short = p.split("/")[-1].split(".")[0]
        if short in ("SM_" + name, "SM_" + name + "_Nubs"):
            continue
        target = folder + "/" + ("SM_%s_Nubs" % name if "Nubs" in short else "SM_" + name)
        if EAL.does_asset_exist(target):
            EAL.delete_asset(target)
        EAL.rename_asset(p.split(".")[0], target)
        log("mesh %s -> %s" % (short, target))
    # Interchange also brings the GLB's own materials / textures: drop them (we assign our instances below).
    for p in sorted(after - before):
        p0 = p.split(".")[0]
        if not EAL.does_asset_exist(p0):
            continue
        cls = str(EAL.find_asset_data(p0).asset_class_path.asset_name)
        if cls != "StaticMesh" and "/Textures/" not in p0 and "/Materials/" not in p0:
            EAL.delete_asset(p0)
    body = EAL.load_asset(folder + "/SM_" + name)
    nubs = EAL.load_asset(folder + "/SM_%s_Nubs" % name)
    for mesh in (body, nubs):
        no_nanite(mesh)
    return body, nubs


def no_nanite(mesh):
    """Interchange turns Nanite on; its position quantisation (~0.25 cm) eats the 0.4 % offset of the eye / face decal over
    the shell (holes in the eyes) and Nanite can't draw the glass shells. These are small meshes: plain static meshes."""
    if mesh is None:
        return
    ns = mesh.get_editor_property("nanite_settings")
    if not ns.get_editor_property("enabled"):
        return
    ns.set_editor_property("enabled", False)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if hasattr(sub, "set_nanite_settings"):
        sub.set_nanite_settings(mesh, ns, True)
    else:
        mesh.set_editor_property("nanite_settings", ns)
    EAL.save_loaded_asset(mesh, False)
    log("nanite off: " + mesh.get_name())


def make_mi(name, slot, spec, tex):
    folder = "%s/%s/Materials" % (ROOT, name)
    mi_name = "MI_%s_%s" % (name, slot)
    path = folder + "/" + mi_name
    mi = EAL.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset(
        mi_name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    parent = EAL.load_asset(MATF + "/" + spec["parent"])
    MEL.set_material_instance_parent(mi, parent)
    for k, v in spec.get("textures", {}).items():
        if v in tex:
            MEL.set_material_instance_texture_parameter_value(mi, k, tex[v])
    for k, v in spec.get("vectors", {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in spec.get("scalars", {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    # report what the instance really carries (texture / vector overrides), to catch a parameter that didn't take
    d = report.setdefault("mis", {}).setdefault(name, {})
    d[slot] = {"parent": parent.get_name() if parent else None,
               "tex": {k: (lambda t: t.get_name() if t else None)(MEL.get_material_instance_texture_parameter_value(mi, k))
                       for k in spec.get("textures", {})},
               "vec": {k: str(MEL.get_material_instance_vector_parameter_value(mi, k)) for k in spec.get("vectors", {})}}
    return mi


def assign(mesh, mis, name):
    if mesh is None:
        report["errors"].append(name + ": mesh missing")
        return []
    mats = list(mesh.get_editor_property("static_materials"))
    out = []
    for i, sm in enumerate(mats):
        slot = str(sm.get_editor_property("material_slot_name"))
        key = slot.split("_")[-1] if slot.startswith("M_") else slot
        if key in mis:
            sm.set_editor_property("material_interface", mis[key])
            mats[i] = sm
            out.append("%s=%s" % (slot, mis[key].get_name()))
        else:
            out.append("%s=?" % slot)
            report["errors"].append("%s: no instance for slot %s" % (name, slot))
    mesh.set_editor_property("static_materials", mats)
    EAL.save_loaded_asset(mesh, False)
    return out


def main():
    for d in (ROOT, MATF):
        if not EAL.does_directory_exist(d):
            EAL.make_directory(d)
    import_defaults()
    make_textured("M_BibleZoogi", unreal.BlendMode.BLEND_OPAQUE)
    make_textured("M_BibleZoogiGlass", unreal.BlendMode.BLEND_TRANSLUCENT)
    make_textured("M_BibleZoogiMasked", unreal.BlendMode.BLEND_MASKED)
    make_factor()
    make_eyes("M_BibleZoogiEyes", False)
    make_eyes("M_BibleZoogiEyesMasked", True)
    spec = slots_json()
    only = [a for a in ARGS if not a.startswith("--")] or [a for a in os.environ.get("ZOOGI_BIBLE_ONLY", "").split(",") if a]
    for name in NAMES:
        if only and name not in only:
            continue
        try:
            tex = import_textures(name)
            body, nubs = import_mesh(name)
            mis = {slot: make_mi(name, slot, s, tex) for slot, s in spec[name].items()}
            b = body.get_bounds() if body else None
            report["meshes"][name] = {
                "body": assign(body, mis, name), "nubs": assign(nubs, mis, name),
                "bounds_extent": [b.box_extent.x, b.box_extent.y, b.box_extent.z] if b else None,
                "nanite": bool(body.get_editor_property("nanite_settings").get_editor_property("enabled")) if body else None,
                "textures": sorted(tex.keys())}
            log("%s done" % name)
        except Exception as ex:
            report["errors"].append("%s: %s" % (name, ex))
            log("ERROR %s: %s" % (name, ex))
    EAL.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        json.dump(report, f, indent=1)
    log("ZOOGI_BIBLE_DONE errors=%d" % len(report["errors"]))


main()
