import unreal
L = unreal.ZoogiFXLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
T = '/Niagara/DefaultAssets/Templates/Emitters/'
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Niagara'], True)
p = '/Game/VFX/KriptoFX/_Scratch/NS_Inspect'
if EAL.does_asset_exist(p): EAL.delete_asset(p)
s = AT.create_asset('NS_Inspect', '/Game/VFX/KriptoFX/_Scratch', unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
for e in ['Fountain', 'OmnidirectionalBurst', 'SimpleSpriteBurst', 'SingleLoopingParticle', 'UpwardMeshBurst', 'HangingParticulates', 'BlowingParticles', 'DirectionalBurst', 'Minimal']:
    em = unreal.load_asset(T + e)
    unreal.log('KFXINS add %s %s' % (e, L.add_emitter_from_asset(s, em, e)))
unreal.log('KFXINS compile ' + L.compile_report(s))
for line in L.describe_niagara(s).splitlines():
    unreal.log('KFXINS ' + line)
EAL.delete_directory('/Game/VFX/KriptoFX/_Scratch')
