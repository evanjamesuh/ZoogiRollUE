# Checklist item 2: per-artifact overlay and guard material instances from the existing procedural materials.
#   /Game/Materials/Artifacts/MI_Artifact_<Key>  parent M_ZoogiArtifact (Color, Pattern, Strength, Mask): character overlay + shell
#   /Game/Materials/Artifacts/MI_Guard_<Key>     parent M_ZoogiFX (Color, Strength, Ring=0): guard bubble
# Colours come from the game's artifact table (UZoogiArtifactLibrary), so the instances match what the code used to set on MIDs.
# UZoogiArtifactComponent makes its dynamic instances from these when present (GLB materials are never touched).
# Run headless (no other editor open): .\Scripts\run_python.ps1 Scripts\make_artifact_instances.py artifact_instances
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.ZoogiArtifactLibrary
FOLDER = "/Game/Materials/Artifacts"
ARTIFACTS = [unreal.ZoogiArtifact.EMBER, unreal.ZoogiArtifact.STORM, unreal.ZoogiArtifact.FROST,
             unreal.ZoogiArtifact.TITAN, unreal.ZoogiArtifact.VOID]

overlay_parent = EAL.load_asset("/Game/Materials/M_ZoogiArtifact")
guard_parent = EAL.load_asset("/Game/Materials/M_ZoogiFX")
if overlay_parent is None or guard_parent is None:
    raise RuntimeError("ZOOGI_MI missing parent material (run make_artifact_material.py / make_fx.py first)")
if not EAL.does_directory_exist(FOLDER):
    EAL.make_directory(FOLDER)

def make_mi(name, parent, vectors, scalars):
    path = FOLDER + "/" + name
    if EAL.does_asset_exist(path):
        mi = EAL.load_asset(path)
    else:
        mi = AT.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    unreal.log("ZOOGI_MI %s parent=%s %s %s" % (path, parent.get_name(), vectors, scalars))
    return mi

n = 0
for i, art in enumerate(ARTIFACTS):
    key = LIB.get_artifact_key(art)
    color = LIB.get_artifact_color(art)
    pattern = float(i + 1)  # EZoogiArtifact index (Ember 1 .. Void 5), same as the runtime Pattern parameter
    make_mi("MI_Artifact_" + key, overlay_parent, {"Color": color}, {"Pattern": pattern, "Strength": 2.0, "Mask": 1.0})
    make_mi("MI_Guard_" + key, guard_parent, {"Color": color}, {"Strength": 1.0, "Ring": 0.0})
    n += 2

unreal.log("ZOOGI_MI_DONE %d material instances" % n)
