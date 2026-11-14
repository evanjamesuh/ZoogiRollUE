# Duplicates engine Niagara templates into /Game/FX, rescales them to Zoogi size (1 m marbles) and recolours
# them with UZoogiFXLibrary (module-input edits + recompile). Re-runnable.
import unreal
EAL = unreal.EditorAssetLibrary
L = unreal.ZoogiFXLibrary
T = '/Niagara/DefaultAssets/Templates/Systems/'
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Niagara'], True)

def dup(src, dst):
    if EAL.does_asset_exist(dst):
        EAL.delete_asset(dst)
    return EAL.duplicate_asset(T + src, dst)

BURST_SCALE = [('Uniform Sprite Size', 3.5), ('RandomRangeFloat.Maximum', 2.2), ('RandomRangeFloat.Minimum', 2.0),
               ('Speed Limit', 3.0), ('Lifetime Max', 0.55), ('Lifetime Min', 0.6), ('Sphere Radius', 1.5), ('Ribbon Width', 2.5)]
SPARK_SCALE = [('RandomRangeFloat002', 1.6), ('Speed Limit', 2.5), ('Lifetime Max', 0.4), ('Lifetime Min', 0.45)]
BOOM_SCALE = [('Uniform Sprite Size', 3.0), ('Sphere Radius', 3.5), ('RandomRangeFloat001', 3.0), ('RandomRangeFloat.', 3.0),
              ('Speed Limit', 3.0), ('Lifetime', 0.6)]

variants = {
    'NS_Explosion': ('SimpleExplosion', unreal.LinearColor(1.0, 0.45, 0.1, 1), 5.0, BOOM_SCALE),
    'NS_Sparks': ('DirectionalBurst', unreal.LinearColor(0.75, 0.9, 1.0, 1), 8.0, SPARK_SCALE),
    'NS_Burst_Blue': ('RadialBurst', unreal.LinearColor(0.35, 0.75, 1.0, 1), 10.0, BURST_SCALE),
    'NS_Burst_Gold': ('RadialBurst', unreal.LinearColor(1.0, 0.7, 0.15, 1), 10.0, BURST_SCALE),
    'NS_Burst_Red': ('RadialBurst', unreal.LinearColor(1.0, 0.2, 0.15, 1), 10.0, BURST_SCALE),
    'NS_Burst_Purple': ('RadialBurst', unreal.LinearColor(0.45, 0.1, 1.0, 1), 12.0, BURST_SCALE),
    'NS_Burst_Yellow': ('RadialBurst', unreal.LinearColor(1.0, 0.95, 0.3, 1), 12.0, BURST_SCALE),
    'NS_Burst_Green': ('RadialBurst', unreal.LinearColor(0.3, 1.0, 0.35, 1), 10.0, BURST_SCALE),
    'NS_Burst_White': ('RadialBurst', unreal.LinearColor(0.85, 0.9, 1.0, 1), 8.0, BURST_SCALE),
}
for dst, (src, col, inten, scales) in variants.items():
    path = '/Game/FX/' + dst
    a = dup(src, path)
    if not a:
        unreal.log_error('ZNIA dup failed ' + dst); continue
    msg = []
    for key, mul in scales:
        msg.append('%s=%d' % (key, L.scale_niagara_floats(a, key, mul)))
    n = L.tint_niagara(a, col, inten, False)
    EAL.save_asset(path, only_if_is_dirty=False)
    unreal.log('ZNIA %s tint=%d %s' % (dst, n, ' '.join(msg)))
unreal.log('ZNIA done')
