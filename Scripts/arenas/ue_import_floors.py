import unreal, os, glob
SRC = os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'Floors')
AT = unreal.AssetToolsHelpers.get_asset_tools()
for f in sorted(glob.glob(os.path.join(SRC, 'T_*.png'))):
    n = os.path.splitext(os.path.basename(f))[0]
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = '/Game/Env/Arenas'; t.destination_name = n
    t.replace_existing = True; t.automated = True; t.save = False
    AT.import_asset_tasks([t])
    tex = unreal.load_asset('/Game/Env/Arenas/' + n)
    glow = n.endswith('Glow')
    tex.set_editor_property('srgb', not glow)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE if glow else unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP); tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_asset('/Game/Env/Arenas/' + n)
    unreal.log('FLOOR imported ' + n)
unreal.log('FLOOR done')
