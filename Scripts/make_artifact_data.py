# Checklist item 7: moves the Infuse / Guard table into DataAssets.
# Creates /Game/Data/Artifacts/DA_Artifact_<Key> (UZoogiArtifactData) for Ember, Storm, Frost, Titan, Void, each holding a copy of
# the built-in C++ row (UZoogiArtifactLibrary.get_built_in_artifact), so behaviour is identical until someone edits a DataAsset.
# The game loads these at runtime and falls back to the C++ table when one is missing (ZoogiArtifacts.cpp logs which source it used).
# Run headless (no other editor open): .\Scripts\run_python.ps1 Scripts\make_artifact_data.py artifact_data
import re
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.ZoogiArtifactLibrary
FOLDER = "/Game/Data/Artifacts"
ARTIFACTS = [unreal.ZoogiArtifact.EMBER, unreal.ZoogiArtifact.STORM, unreal.ZoogiArtifact.FROST,
             unreal.ZoogiArtifact.TITAN, unreal.ZoogiArtifact.VOID]

FIELDS = ["id", "element", "display_name", "infuse_text", "guard_text", "core_color", "rim_color",
          "launch_speed_mul", "mass_mul", "impact_radius", "impact_push", "chain_count", "pull_time", "pull_accel",
          "status_turns", "guard_knockback_mul", "guard_mass_mul", "guard_reflect_deg", "guard_attacker_mul",
          "impact_niagara", "aura_niagara", "anime_key", "affinity_zoogi"]

def same(a, b):
    for f in FIELDS:
        va, vb = a.get_editor_property(f), b.get_editor_property(f)
        # str() of a struct includes its memory address; compare the values only.
        if re.sub(r"\(0x[0-9A-Fa-f]+\)", "", str(va)) != re.sub(r"\(0x[0-9A-Fa-f]+\)", "", str(vb)):
            unreal.log_warning("ZOOGI_DATA mismatch %s: %s vs %s" % (f, va, vb))
            return False
    return True

if not EAL.does_directory_exist(FOLDER):
    EAL.make_directory(FOLDER)

ok = 0
for art in ARTIFACTS:
    key = LIB.get_artifact_key(art)
    name = "DA_Artifact_" + key
    path = FOLDER + "/" + name
    row = LIB.get_built_in_artifact(art)
    if EAL.does_asset_exist(path):
        da = EAL.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ZoogiArtifactData)
        da = AT.create_asset(name, FOLDER, unreal.ZoogiArtifactData, factory)
    if da is None:
        unreal.log_error("ZOOGI_DATA could not create " + path)
        continue
    da.set_editor_property("row", row)
    EAL.save_loaded_asset(da, False)
    back = EAL.load_asset(path).get_editor_property("row")
    if same(back, row):
        ok += 1
    unreal.log("ZOOGI_DATA %s radius=%.1f push=%.1f mass=%.2f guardKB=%.2f -> %s" % (
        name, row.impact_radius, row.impact_push, row.mass_mul, row.guard_knockback_mul, path))

unreal.log("ZOOGI_DATA_DONE %d/%d DataAssets identical to the C++ table" % (ok, len(ARTIFACTS)))
