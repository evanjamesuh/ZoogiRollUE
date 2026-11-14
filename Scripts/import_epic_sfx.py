# Nov 3 b103 (Claude): imports the modal-synthesis realistic layers (SourceArt/Audio/Real, Scripts/synth_real_sfx.py) as /Game/Audio/Sounds/Epic/SW_<Name>.
import os
import unreal

SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Audio", "Epic")
DEST = "/Game/Audio/Sounds/Epic"
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".wav") or not f.startswith(("Epic", "Debris", "RockSmash", "Rumble")):
        continue  # b103: the literal war-drum takes stay out
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = DEST
    t.destination_name = "SW_" + os.path.splitext(f)[0]
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
n = 0
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SoundWave):
            a.set_editor_property("looping", False)  # roll loop repeats
            unreal.EditorAssetLibrary.save_loaded_asset(a)
            n += 1
        unreal.log("[ZoogiEpicSFX] %s -> %s" % (os.path.basename(t.filename), p))
unreal.log("[ZoogiEpicSFX] import done: %d sound waves" % n)
