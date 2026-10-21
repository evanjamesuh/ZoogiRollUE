import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Niagara'], True)
flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Niagara', 'NiagaraSystem'), unreal.TopLevelAssetPath('/Script/Niagara', 'NiagaraEmitter')], package_paths=['/Niagara'], recursive_paths=True)
for d in ar.get_assets(flt):
    unreal.log('ZNIA %s %s' % (d.asset_class_path.asset_name, d.package_name))
unreal.log('ZNIA done')
