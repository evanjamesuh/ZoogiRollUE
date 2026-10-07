# KriptoFX port step 1: import the referenced raw textures/meshes/wavs from SourceArt/KriptoFX/<EffectN> into /Game/VFX/KriptoFX/<EffectN>.
import unreal, os, json
ROOT = os.path.join(unreal.Paths.project_dir(), 'SourceArt', 'KriptoFX')
AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
tasks = []
for eff in sorted(os.listdir(ROOT)):
    d = os.path.join(ROOT, eff)
    if not os.path.isdir(d): continue
    for sub in ('Textures', 'Meshes', 'Audio'):
        sd = os.path.join(d, sub)
        if not os.path.isdir(sd): continue
        for f in sorted(os.listdir(sd)):
            dest = '/Game/VFX/KriptoFX/%s/%s' % (eff, sub)
            name = os.path.splitext(f)[0].replace(' ', '_')
            pre = {'Textures': 'T_', 'Meshes': 'SM_', 'Audio': 'S_'}[sub]
            if EAL.does_asset_exist(dest + '/' + pre + name): continue
            t = unreal.AssetImportTask()
            t.filename = os.path.join(sd, f); t.destination_path = dest; t.destination_name = pre + name
            t.automated = True; t.save = True; t.replace_existing = True
            if sub == 'Meshes':
                o = unreal.FbxImportUI()
                o.import_mesh = True; o.import_as_skeletal = False; o.import_materials = False; o.import_textures = False; o.import_animations = False
                o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
                o.static_mesh_import_data.combine_meshes = True
                o.static_mesh_import_data.set_editor_property('import_uniform_scale', 1.0)
                t.options = o
            tasks.append(t)
unreal.log('KFXIMP tasks %d' % len(tasks))
AT.import_asset_tasks(tasks)
bad = [t.filename for t in tasks if not t.imported_object_paths]
unreal.log('KFXIMP imported %d failed %d %s' % (len(tasks) - len(bad), len(bad), bad))
# tweak textures: no mips streaming issues; effect textures are masks -> keep sRGB except noise/normal
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.Texture2D):
            n = a.get_name().lower()
            if 'norm' in n:
                a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
            a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
            EAL.save_loaded_asset(a)
unreal.log('KFXIMP done')
