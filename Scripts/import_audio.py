# Imports SourceArt/Audio/*.wav into /Game/Audio (music loops). Run with Scripts/run_python.ps1.
import os
import unreal

SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Audio")
DEST = "/Game/Audio"
MUSIC = {"MenuMusic", "MatchMusic"}

tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".wav"):
        continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f)
    t.destination_path = DEST
    t.destination_name = "S_" + os.path.splitext(f)[0]
    t.replace_existing = True
    t.automated = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        name = a.get_name()[2:]
        if isinstance(a, unreal.SoundWave):
            if name in MUSIC:
                a.set_editor_property("looping", True)
            unreal.EditorAssetLibrary.save_loaded_asset(a)
        unreal.log("[ZoogiSetup] audio %s -> %s" % (os.path.basename(t.filename), p))
unreal.log("[ZoogiSetup] audio import done: %d files" % len(tasks))
