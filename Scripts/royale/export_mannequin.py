# Oct 31 (Claude): export the UEFN mannequin (mesh + skeleton) to FBX so Blender can build the heroes on the exact game skeleton.
import unreal
L = lambda s: unreal.log('MANNEXP: ' + str(s))
m = unreal.EditorAssetLibrary.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
t = unreal.AssetExportTask(); t.object = m; t.filename = 'C:/dev/art/hero_build/SKM_UEFN_Mannequin.fbx'
t.automated = True; t.replace_identical = True; t.prompt = False
o = unreal.FbxExportOption(); o.export_morph_targets = False; o.level_of_detail = False; o.collision = False
t.options = o
L('export ok %s' % unreal.Exporter.run_asset_export_task(t))
seq = unreal.EditorAssetLibrary.load_asset('/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F')
t2 = unreal.AssetExportTask(); t2.object = seq; t2.filename = 'C:/dev/art/hero_build/M_Run_F.fbx'; t2.automated = True; t2.replace_identical = True; t2.prompt = False
L('anim export ok %s' % unreal.Exporter.run_asset_export_task(t2))
try:
    unreal.SystemLibrary.quit_editor()
except Exception as e:
    L('quit %s' % e)
