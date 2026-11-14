# Nov 6 b163 (Claude): imports the soundscape (Scripts/synth_ambience.py -> SourceArt/Audio/Amb) as /Game/Audio/Sounds/Amb/SW_<Name>.
# *_Loop waves loop; everything else is a one-shot.
import os
import unreal
SRC = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Audio", "Amb")
DEST = "/Game/Audio/Sounds/Amb"
tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith(".wav"): continue
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, f); t.destination_path = DEST; t.destination_name = "SW_" + os.path.splitext(f)[0]
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
n = 0
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SoundWave):
            a.set_editor_property("looping", "_Loop" in a.get_name())
            unreal.EditorAssetLibrary.save_loaded_asset(a); n += 1
        unreal.log("[ZoogiAmb] %s -> %s" % (os.path.basename(t.filename), p))
unreal.log("[ZoogiAmb] import done: %d sound waves" % n)
