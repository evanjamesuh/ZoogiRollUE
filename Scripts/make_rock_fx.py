# Oct 4: dust + debris Niagara for destructible rocks (AZoogiRock). Same approach as make_niagara.py:
# duplicate engine templates into /Game/FX, rescale, recolour (low intensity so dust doesn't glow). Re-runnable.
import unreal
EAL = unreal.EditorAssetLibrary
L = unreal.ZoogiFXLibrary
T = '/Niagara/DefaultAssets/Templates/Systems/'
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Niagara'], True)

def dup(src, dst):
    if EAL.does_asset_exist(dst):
        EAL.delete_asset(dst)
    return EAL.duplicate_asset(T + src, dst)

DUST = [('Uniform Sprite Size', 6.0), ('RandomRangeFloat.Maximum', 1.4), ('RandomRangeFloat.Minimum', 1.2),
        ('Speed Limit', 1.2), ('Lifetime Max', 2.6), ('Lifetime Min', 2.4), ('Sphere Radius', 2.5)]
DEBRIS = [('RandomRangeFloat002', 1.3), ('Speed Limit', 1.6), ('Lifetime Max', 1.2), ('Lifetime Min', 1.2), ('Uniform Sprite Size', 2.0)]
variants = {
    'NS_Dust': ('RadialBurst', unreal.LinearColor(0.55, 0.45, 0.33, 1), 0.9, DUST),
    'NS_Debris': ('DirectionalBurst', unreal.LinearColor(0.42, 0.36, 0.3, 1), 0.8, DEBRIS),
}
for dst, (src, col, inten, scales) in variants.items():
    path = '/Game/FX/' + dst
    a = dup(src, path)
    if not a:
        unreal.log_error('ZROCKFX dup failed ' + dst); continue
    msg = ['%s=%d' % (k, L.scale_niagara_floats(a, k, m)) for k, m in scales]
    n = L.tint_niagara(a, col, inten, False)
    EAL.save_asset(path, only_if_is_dirty=False)
    unreal.log('ZROCKFX %s tint=%d %s' % (dst, n, ' '.join(msg)))
unreal.log('ZROCKFX done')
