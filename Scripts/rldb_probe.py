import unreal, json, os
out = []
def P(*a): out.append(' '.join(str(x) for x in a))
reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = reg.get_assets_by_path('/Game/RLDB', recursive=True)
P('RLDB assets', len(assets))
bad = 0
for a in assets:
    p = str(a.package_name)
    if '/Maps/' in p: continue
    o = unreal.EditorAssetLibrary.load_asset(p)
    if o is None: bad += 1; P('LOADFAIL', p)
P('load failures', bad)
for dtn in ['/Game/RLDB/Data/Cards/DT_Cards']:
    dt = unreal.EditorAssetLibrary.load_asset(dtn)
    P('DT', dtn, 'struct', dt.get_editor_property('row_struct').get_path_name())
    rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
    P('rows', len(rows), [str(r) for r in rows][:40])
    js = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt)
    open(os.path.join(unreal.Paths.project_saved_dir(), 'DT_Cards.json'), 'w').write(js)
for sn in ['FCard','FUseCost','FUseRule','FDeck','FHeroDeck','FCardEffect']:
    s = unreal.EditorAssetLibrary.load_asset('/Game/RLDB/Blueprints/Structs/' + sn)
    P('STRUCT', sn, s.get_class().get_name() if s else None)
open(os.path.join(unreal.Paths.project_saved_dir(), 'rldb_probe.txt'), 'w').write('\n'.join(out))
