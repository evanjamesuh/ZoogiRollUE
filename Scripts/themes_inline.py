# b185: the two match themes load whole (ForceInline) instead of streaming in 39 chunks - on the phone they stayed silent
import unreal
for i in (1, 2):
    p = "/Game/Audio/Music/Themes/S_Theme_Arena%d" % i
    a = unreal.load_asset(p)
    if not a:
        unreal.log_warning("[ZoogiThemes] missing " + p); continue
    try:
        a.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    except Exception as e:
        unreal.log_warning("[ZoogiThemes] loading_behavior: %r" % e)
    a.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(a)
    unreal.log("[ZoogiThemes] %s -> %s" % (p, a.get_editor_property("loading_behavior")))
