# Oct 7 (branch `music`), fixed Oct 14 (branch `music2`): imports the adaptive arena themes and the announcer VO.
#   SourceArt/Audio/Music/<Slug>/Music_<Slug>_<Part>.wav -> /Game/Audio/Music/<Slug>/S_Music_<Slug>_<Part>
#   SourceArt/Audio/VO/VO_<Line>_<n>.wav               -> /Game/Audio/VO/S_VO_<Line>_<n>
# Layers (Base / Battle / Shift) loop, play when silent (so muted layers stay in sync) and use the same settings.
# Run headless: .\Scripts\music\run_python_wt.ps1 Scripts\import_music.py -LogName music_import
#
# Oct 14 fix: the first run exited 1 because the headless commandlet (-NullRHI, no audio device) has not loaded the
# Bink Audio decoder, and the project default compression is BINKA: the SoundFactory's first decode hit
# "Ensure condition failed: Factory ... Decoder for AudioFormat 'BINKA' not found" (one handled ensure = 42 Error lines
# with the callstack), and any logged error makes the commandlet return 1. The assets themselves were written.
# Now: load the Bink + ADPCM decoder modules before importing, store every music/VO wave as ADPCM (same codec on all three stems,
# as INTEGRATION.md asks; no Bink), and finish with a validation pass that fails loudly if any asset is missing/empty.
import os
import unreal

ROOT = os.path.join(unreal.Paths.project_dir(), "SourceArt", "Audio")
LAYERS = ("_Base", "_Battle", "_Shift")

for m in ("BinkAudioDecoder", "AdpcmAudioDecoder"):  # headless commandlets register no audio decoders
    try:
        unreal.load_module(m)
        unreal.log("[ZoogiMusic] loaded module %s" % m)
    except Exception as e:
        unreal.log("[ZoogiMusic] module %s not loadable (%s); relying on ADPCM" % (m, e))


def gather():
    jobs = []
    mdir = os.path.join(ROOT, "Music")
    if os.path.isdir(mdir):
        for slug in sorted(os.listdir(mdir)):
            d = os.path.join(mdir, slug)
            if not os.path.isdir(d):
                continue
            for f in sorted(os.listdir(d)):
                if f.lower().endswith(".wav"):
                    jobs.append((os.path.join(d, f), "/Game/Audio/Music/" + slug))
    vdir = os.path.join(ROOT, "VO")
    if os.path.isdir(vdir):
        for f in sorted(os.listdir(vdir)):
            if f.lower().endswith(".wav"):
                jobs.append((os.path.join(vdir, f), "/Game/Audio/VO"))
    return jobs


def setp(a, prop, value):
    try:
        a.set_editor_property(prop, value)
        return True
    except Exception as e:  # property names differ between engine versions; log and keep going
        unreal.log("[ZoogiMusic] %s: cannot set %s (%s)" % (a.get_name(), prop, e))
        return False


jobs = gather()
tasks = []
for src, dest in jobs:
    t = unreal.AssetImportTask()
    t.filename = src
    t.destination_path = dest
    t.destination_name = "S_" + os.path.splitext(os.path.basename(src))[0]
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

ADPCM = getattr(getattr(unreal, "SoundAssetCompressionType", None), "ADPCM", None)
n_layers = n_other = 0
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.SoundWave):
            continue
        name = a.get_name()
        is_layer = name.startswith("S_Music_") and name.endswith(LAYERS)
        if ADPCM is not None:
            setp(a, "sound_asset_compression_type", ADPCM)
        setp(a, "looping", is_layer)
        if is_layer:
            setp(a, "virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
            if hasattr(unreal, "SoundWaveLoadingBehavior"):
                setp(a, "loading_behavior", unreal.SoundWaveLoadingBehavior.RETAIN_ON_LOAD)
            n_layers += 1
        else:
            n_other += 1
        unreal.EditorAssetLibrary.save_loaded_asset(a, False)
        unreal.log("[ZoogiMusic] %s -> %s (%.2f s%s)" % (os.path.basename(t.filename), p, a.get_editor_property("duration"), ", loop" if is_layer else ""))

# Validation: every source WAV must have a saved, loadable, non-empty SoundWave.
bad = []
n_music = n_vo = 0
for src, dest in jobs:
    path = dest + "/S_" + os.path.splitext(os.path.basename(src))[0]
    a = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not isinstance(a, unreal.SoundWave) or a.get_editor_property("duration") <= 0.0:
        bad.append(path)
    elif "/Music/" in path:
        n_music += 1
    else:
        n_vo += 1
unreal.log("[ZoogiMusic] import done: %d tasks, %d looping layers, %d stingers/VO" % (len(tasks), n_layers, n_other))
unreal.log("[ZoogiMusic] VALIDATE: music %d valid, VO %d valid, %d bad%s" % (n_music, n_vo, len(bad), (": " + ", ".join(bad)) if bad else ""))
if bad:
    raise RuntimeError("music import: %d invalid assets" % len(bad))
