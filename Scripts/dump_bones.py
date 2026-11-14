# Nov 1 b58 (Claude): dump bone names + reference component-space positions for the Aurex + ultimate form meshes (puppet poses).
import unreal, json
OUT = r'C:\dev\ZoogiRollUnreal-p5style\Saved\bones_dump.json'
paths = ['/Game/Mecha_Aurex/Meshes/SK_Mecha_Aurex']
for f in unreal.EditorAssetLibrary.list_assets('/Game/Zoogis/Ultimates', recursive=False, include_folder=True):
    pass
for a in unreal.EditorAssetLibrary.list_assets('/Game/Zoogis/Ultimates', recursive=True):
    d = unreal.EditorAssetLibrary.find_asset_data(a)
    if str(d.asset_class_path.asset_name) == 'SkeletalMesh':
        paths.append(str(d.package_name))
res = {}
for p in paths:
    m = unreal.load_asset(p)
    if not m: continue
    try:
        sk = m.skeleton
        pose = unreal.AnimPoseExtensions.get_reference_pose(sk)
        names = unreal.AnimPoseExtensions.get_bone_names(pose)
        bones = []
        for n in names:
            t = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD)
            l = t.translation
            bones.append([str(n), round(l.x, 1), round(l.y, 1), round(l.z, 1)])
        b = m.get_bounds()
        res[p] = {'bones': bones, 'extent': [b.box_extent.x, b.box_extent.y, b.box_extent.z]}
    except Exception as e:
        res[p] = {'error': str(e)}
open(OUT, 'w').write(json.dumps(res, indent=1))
unreal.log('BONES_DUMP %d meshes' % len(res))
