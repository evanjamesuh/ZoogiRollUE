"""
Headless content setup for Zoogi Roll (run with Scripts/run_python.ps1).
 1. Creates the shared materials (surface, glow, ground).
 2. Imports the Zoogi, orb and scenery GLBs from the web game (read-only) with Interchange.
 3. Renames the main static mesh of each import to SM_<Name>.
 4. Creates the Meadow map (the game mode builds the arena at runtime).
Writes Saved/import_report.json.
"""
import json
import os
import sys
import unreal

SRC = r"C:\dev\ZoogiRollUE\client\public\models"
# Zoogi GLBs need EXT_texture_webp removed first (Scripts/glb_unwebp.py) -> SourceArt\Zoogis
SRC_CONVERTED = r"C:\dev\ZoogiRollUnreal\SourceArt\Zoogis"
REPORT = r"C:\dev\ZoogiRollUnreal\Saved\import_report.json"
ARGS = sys.argv[1:]

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
report = {"imports": {}, "materials": [], "errors": [], "log": []}


def log(msg):
    unreal.log("[ZoogiSetup] " + str(msg))
    report["log"].append(str(msg))


# ---------------------------------------------------------------- materials
def new_material(name, folder="/Game/Materials"):
    path = folder + "/" + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    return AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def vparam(m, name, default, x, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*default))
    return e


def sparam(m, name, default, x, y):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    return e


def mul(m, a, b, x, y, a_out="", b_out=""):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, x, y)
    if a is not None:
        MEL.connect_material_expressions(a, a_out, e, "A")
    if b is not None:
        MEL.connect_material_expressions(b, b_out, e, "B")
    return e


def finish(m):
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m)
    report["materials"].append(m.get_path_name())


def make_surface():
    m = new_material("M_ZoogiSurface")
    color = vparam(m, "Color", (1, 1, 1, 1), -800, -300)
    rough = sparam(m, "Roughness", 0.6, -800, -100)
    metal = sparam(m, "Metallic", 0.0, -800, 0)
    emc = vparam(m, "EmissiveColor", (0, 0, 0, 1), -800, 150)
    ems = sparam(m, "EmissiveStrength", 0.0, -800, 350)
    em = mul(m, emc, ems, -400, 200)
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)


def make_glow():
    m = new_material("M_ZoogiGlow")
    color = vparam(m, "Color", (0.53, 0.81, 0.92, 1), -900, -200)
    strength = sparam(m, "Strength", 3.0, -900, 0)
    fres = MEL.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 200)
    fres.set_editor_property("exponent", 2.5)
    rim = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -650, 200)
    MEL.connect_material_expressions(fres, "", rim, "A")
    rim.set_editor_property("const_b", 2.0)
    rim_add = MEL.create_material_expression(m, unreal.MaterialExpressionAdd, -450, 200)
    MEL.connect_material_expressions(rim, "", rim_add, "A")
    rim_add.set_editor_property("const_b", 0.6)
    cs = mul(m, color, strength, -650, -100)
    em = mul(m, cs, rim_add, -250, 0)
    base = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, -300)
    MEL.connect_material_expressions(color, "", base, "A")
    base.set_editor_property("const_b", 0.35)
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -450, 400)
    r.set_editor_property("r", 0.15)
    MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


def make_ground():
    m = new_material("M_ZoogiGround")
    a = vparam(m, "ColorA", (0.18, 0.45, 0.12, 1), -1000, -400)
    b = vparam(m, "ColorB", (0.32, 0.62, 0.18, 1), -1000, -200)
    rough = sparam(m, "Roughness", 0.9, -1000, 300)
    sparam(m, "NoiseScale", 0.002, -1000, 450)
    wp = MEL.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1200, 0)
    n1 = MEL.create_material_expression(m, unreal.MaterialExpressionNoise, -900, 0)
    n1.set_editor_property("scale", 0.0025)
    n1.set_editor_property("levels", 4)
    n1.set_editor_property("output_min", 0.0)
    n1.set_editor_property("output_max", 1.0)
    MEL.connect_material_expressions(wp, "", n1, "Position")
    lerp = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -600, -200)
    MEL.connect_material_expressions(a, "", lerp, "A")
    MEL.connect_material_expressions(b, "", lerp, "B")
    MEL.connect_material_expressions(n1, "", lerp, "Alpha")
    n2 = MEL.create_material_expression(m, unreal.MaterialExpressionNoise, -900, 150)
    n2.set_editor_property("scale", 0.08)
    n2.set_editor_property("levels", 2)
    n2.set_editor_property("output_min", 0.8)
    n2.set_editor_property("output_max", 1.08)
    MEL.connect_material_expressions(wp, "", n2, "Position")
    base = mul(m, lerp, n2, -350, -100)
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


# ---------------------------------------------------------------- imports
IMPORTS = [
    # (file, destination folder, final mesh name)
    ("wolfgang.glb", "/Game/Zoogis/wolfgang", "SM_wolfgang"),
    ("hotstreak.glb", "/Game/Zoogis/hotstreak", "SM_hotstreak"),
    ("pinpoint.glb", "/Game/Zoogis/pinpoint", "SM_pinpoint"),
    ("bolt.glb", "/Game/Zoogis/bolt", "SM_bolt"),
    ("wraps.glb", "/Game/Zoogis/wraps", "SM_wraps"),
    ("lars.glb", "/Game/Zoogis/lars", "SM_lars"),
    ("nightshade.glb", "/Game/Zoogis/nightshade", "SM_nightshade"),
    ("orb.glb", "/Game/Orbs", "SM_Orb"),
    ("stylized_oak_tree.glb", "/Game/Env/OakTree", "SM_OakTree"),
    ("stylized_pine_tree.glb", "/Game/Env/PineTree", "SM_PineTree"),
    ("stylized_snow_pine.glb", "/Game/Env/SnowPine", "SM_SnowPine"),
    ("stylized_alien_crystal.glb", "/Game/Env/AlienCrystal", "SM_AlienCrystal"),
    ("stylized_desert_hoodoo.glb", "/Game/Env/DesertHoodoo", "SM_DesertHoodoo"),
]


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


def assets_in(folder):
    return [p for p in EAL.list_assets(folder, recursive=True, include_folder=False)]


def bounds_of(mesh):
    b = mesh.get_bounds()
    return {"origin": [b.origin.x, b.origin.y, b.origin.z], "extent": [b.box_extent.x, b.box_extent.y, b.box_extent.z]}


def do_import(fname, folder, mesh_name):
    src = os.path.join(SRC_CONVERTED, fname)
    if not os.path.exists(src):
        src = os.path.join(SRC, fname)
    entry = {"src": src, "folder": folder}
    if not os.path.exists(src):
        entry["error"] = "missing source"
        report["errors"].append(fname + ": missing")
        return entry
    final_path = folder + "/" + mesh_name
    if EAL.does_asset_exist(final_path) and "--force" not in ARGS:
        entry["skipped"] = True
        mesh = EAL.load_asset(final_path)
        entry["bounds"] = bounds_of(mesh)
        return entry
    log("importing " + fname)
    try:
        interchange_import(src, folder)
    except Exception as ex:
        entry["error"] = "interchange: %s" % ex
        report["errors"].append(fname + ": " + str(ex))
        return entry
    found = assets_in(folder)
    entry["assets"] = found
    meshes = []
    for p in found:
        data = unreal.EditorAssetLibrary.find_asset_data(p)
        cls = str(data.asset_class_path.asset_name)
        if cls in ("StaticMesh", "SkeletalMesh"):
            meshes.append((p, cls))
    entry["meshes"] = meshes
    statics = [p for p, c in meshes if c == "StaticMesh"]
    if statics:
        # choose the static mesh with the biggest bounds as the main one
        best, best_size = None, -1.0
        for p in statics:
            mesh = EAL.load_asset(p)
            b = mesh.get_bounds()
            size = b.box_extent.x + b.box_extent.y + b.box_extent.z
            if size > best_size:
                best, best_size = p, size
        src_path = best.split(".")[0]
        if src_path != final_path:
            if EAL.does_asset_exist(final_path):
                EAL.delete_asset(final_path)
            EAL.rename_asset(src_path, final_path)
        mesh = EAL.load_asset(final_path)
        entry["main"] = final_path
        entry["bounds"] = bounds_of(mesh)
        entry["materials"] = [str(mesh.get_material(i).get_path_name()) if mesh.get_material(i) else None
                              for i in range(mesh.get_num_sections(0))]
        entry["static_count"] = len(statics)
    else:
        entry["error"] = "no static mesh created"
        report["errors"].append(fname + ": no static mesh")
    EAL.save_directory(folder, only_if_is_dirty=False, recursive=True)
    return entry


def make_map():
    path = "/Game/Maps/Meadow"
    if EAL.does_asset_exist(path):
        log("map exists")
        return
    world = AT.create_asset("Meadow", "/Game/Maps", unreal.World, unreal.WorldFactory())
    EAL.save_loaded_asset(world)
    log("created map " + path)


def main():
    try:
        make_surface()
        make_glow()
        make_ground()
    except Exception as ex:
        report["errors"].append("materials: %s" % ex)
        log("material error %s" % ex)
    only = [a for a in ARGS if not a.startswith("--")]
    for fname, folder, mesh_name in IMPORTS:
        if only and fname not in only:
            continue
        try:
            report["imports"][fname] = do_import(fname, folder, mesh_name)
        except Exception as ex:
            report["errors"].append("%s: %s" % (fname, ex))
    try:
        make_map()
    except Exception as ex:
        report["errors"].append("map: %s" % ex)
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, "w") as f:
        json.dump(report, f, indent=1)
    log("done, errors=%d" % len(report["errors"]))


main()
