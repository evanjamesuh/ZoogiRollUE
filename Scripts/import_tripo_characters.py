# Imports the 15 game-ready Tripo Zoogis (build box pipeline: ~35k-tri shell, 2048 baked BC/ORM/N, swappable 12-expression
# eye decal, radius 0.97 m, facing +X) into /Game/Characters/Tripo/<Name>:
#   SM_<Name> (slots M_<Name>_Shell, M_<Name>_Eyes) + SM_<Name>_Nubs (M_<Name>_Nub)
#   MI_<Name>_Shell / _Nub -> M_ZoogiClearcoat (clear-coat M_BibleZoogi graph; Shell + separate Nub BC/ORM/N), MI_<Name>_Eyes -> M_BibleZoogiEyesMasked with T_<Name>_EyesAtlas_BC
#   (4x4 cells 512x256, 0 neutral .. 11 flourish, 12-15 neutral) so UZoogiExpressionComponent drives ExprIndex as for Bible.
# Reuses every helper of import_bible_characters.py (masters are NOT rebuilt). Sources: SourceArt/Characters/Tripo/<Name>/.
# Run headless: .\Scripts\run_python.ps1 Scripts\import_tripo_characters.py import_tripo   (env ZOOGI_TRIPO_ONLY=Blue,Vex)
import os, json, unreal
_here = os.path.join(unreal.Paths.project_dir(), "Scripts", "import_bible_characters.py")
_src = open(_here).read().rsplit("\nmain()", 1)[0]
G = {"__name__": "bible_lib"}
exec(compile(_src, _here, "exec"), G)
G["SRC"] = os.path.join(G["PROJ"], "SourceArt", "Characters", "Tripo")
G["ROOT"] = "/Game/Characters/Tripo"
G["REPORT"] = os.path.join(G["PROJ"], "Saved", "import_tripo_report.json")
G["FORCE"] = False
EAL, report, log = G["EAL"], G["report"], G["log"]
NAMES = ["Blue", "Tavi", "Vex", "Moss", "Wolfgang", "Hotstreak", "Lars", "Pinpoint", "Bolt", "Nightshade", "Wraps",
         "MasterOrin", "Sera", "Luma", "DarkZoogy"]



# ---- realistic rebuild (Oct 22): clearcoat master + separate Nub textures ----
CC_MASTER = "M_ZoogiClearcoat"


def make_clearcoat_master():
    """M_BibleZoogi graph (BC/ORM/N/Emissive/Rim) on the Clear Coat shading model, rebuilt every run."""
    old = G["FORCE"]; G["FORCE"] = True
    try:
        G["make_textured"](CC_MASTER, unreal.BlendMode.BLEND_OPAQUE)
    finally:
        G["FORCE"] = old
    m = EAL.load_asset(G["MATF"] + "/" + CC_MASTER)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    cc = G["sparam"](m, "ClearCoat", 1.0, -1100, 1200)
    ccr = G["sparam"](m, "ClearCoatRoughness", 0.06, -1100, 1300)
    MP = unreal.MaterialProperty
    props = [k for k in dir(MP) if "CUSTOM" in k.upper()]
    p0 = next((getattr(MP, k) for k in props if k.upper().rstrip("_").endswith("0")), None)
    p1 = next((getattr(MP, k) for k in props if k.upper().rstrip("_").endswith("1")), None)
    log("clear coat pins: %s" % props)
    if p0 is not None:
        G["MEL"].connect_material_property(cc, "", p0)
    if p1 is not None:
        G["MEL"].connect_material_property(ccr, "", p1)  # unconnected pins fall back to UE defaults (coat 1.0, rough 0.1)
    G["MEL"].recompile_material(m)
    EAL.save_loaded_asset(m)
    log("material " + CC_MASTER + " (clear coat)")


# Interchange keeps old slot names on reimport; Blender may suffix duplicates (M_Blue_Shell_001): normalise before matching.
import re as _re
_bible_assign = G["assign"]


def _assign(mesh, mis, name):
    if mesh is not None:
        mats = list(mesh.get_editor_property("static_materials"))
        for sm in mats:
            s = str(sm.get_editor_property("material_slot_name")); t = _re.sub(r"[._]\d{3}$", "", s)
            if t != s:
                sm.set_editor_property("material_slot_name", unreal.Name(t))
        mesh.set_editor_property("static_materials", mats)
    return _bible_assign(mesh, mis, name)


G["assign"] = _assign


def spec(n):
    def painted(part, cc_rough):
        return {"parent": CC_MASTER,
                "textures": {"BaseColorTex": "T_%s_%s_BC" % (n, part), "ORMTex": "T_%s_%s_ORM" % (n, part),
                             "NormalTex": "T_%s_%s_N" % (n, part)},
                "vectors": {"RimColor": [1, 1, 1, 1]},
                "scalars": {"RimStrength": 0.12, "RimPower": 3.0, "ClearCoat": 1.0, "ClearCoatRoughness": cc_rough}}
    # Realistic rebuild: painted PBR clearcoat shell; the fins (Nub slot) carry their own T_<Name>_Nub_* textures.
    return {"Shell": painted("Shell", 0.05), "Nub": painted("Nub", 0.08),
            "Eyes": {"parent": "M_BibleZoogiEyesMasked", "textures": {"BaseColorTex": "T_%s_EyesAtlas_BC" % n},
                     "scalars": {"ExprIndex": 0.0, "Cols": 4.0, "Rows": 4.0, "FaceBrightness": 0.9, "EmissiveBoost": 0.0}}}


def main():
    make_clearcoat_master()
    for m in (CC_MASTER, "M_BibleZoogiEyesMasked"):
        if not EAL.does_asset_exist(G["MATF"] + "/" + m):
            report["errors"].append("master material missing: " + m)
    if report["errors"]:
        log("ZOOGI_TRIPO_DONE errors=%d" % len(report["errors"])); return
    if not EAL.does_directory_exist(G["ROOT"]):
        EAL.make_directory(G["ROOT"])
    only = [a for a in os.environ.get("ZOOGI_TRIPO_ONLY", "").split(",") if a]
    for n in NAMES:
        if only and n not in only:
            continue
        try:
            tex = G["import_textures"](n)
            body, nubs = G["import_mesh"](n)
            mis = {slot: G["make_mi"](n, slot, s, tex) for slot, s in spec(n).items()}
            b = body.get_bounds() if body else None
            report["meshes"][n] = {"body": G["assign"](body, mis, n), "nubs": G["assign"](nubs, mis, n),
                                   "bounds_extent": [b.box_extent.x, b.box_extent.y, b.box_extent.z] if b else None,
                                   "tris": body.get_num_triangles(0) if body else None}
            try:
                G["drop_glb_leftovers"](n)
            except Exception as ex:
                log("leftover cleanup %s: %s" % (n, ex))
            log("%s done" % n)
        except Exception as ex:
            report["errors"].append("%s: %s" % (n, ex)); log("ERROR %s: %s" % (n, ex))
    EAL.save_directory(G["ROOT"], only_if_is_dirty=False, recursive=True)
    os.makedirs(os.path.dirname(G["REPORT"]), exist_ok=True)
    json.dump(report, open(G["REPORT"], "w"), indent=1)
    log("ZOOGI_TRIPO_DONE errors=%d" % len(report["errors"]))


main()
