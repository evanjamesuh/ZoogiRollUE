# Nov 6 (Claude): inspect the recovered ithappy Winter scene (winter_location.glb) + animals: hierarchy, bounds, renders
import bpy, os, sys, json, math
from mathutils import Vector
D = sys.argv[sys.argv.index('--') + 1]
OUT = os.path.join(D, 'inspect'); os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
def bbox(objs):
    lo = Vector((1e9, 1e9, 1e9)); hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != 'MESH': continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi
info = {}
bpy.ops.import_scene.gltf(filepath=os.path.join(D, 'winter_location.glb'))
scene_objs = list(bpy.context.scene.objects)
meshes = [o for o in scene_objs if o.type == 'MESH']
lo, hi = bbox(meshes)
info['winter'] = {'objects': len(scene_objs), 'meshes': len(meshes), 'tris': sum(len(o.data.polygons) for o in meshes),
                  'bbox_lo': list(lo), 'bbox_hi': list(hi),
                  'materials': sorted({m.name for o in meshes for m in o.data.materials if m}),
                  'images': sorted({i.name + ' %dx%d' % tuple(i.size) for i in bpy.data.images})}
tops = [o for o in scene_objs if o.parent is None]
def walk(o, d=0, acc=None):
    acc.append({'name': o.name, 'type': o.type, 'depth': d, 'loc': [round(v, 2) for v in o.matrix_world.translation],
                'dims': [round(v, 2) for v in o.dimensions] if o.type == 'MESH' else None,
                'polys': len(o.data.polygons) if o.type == 'MESH' else 0})
    for c in o.children: walk(c, d + 1, acc)
    return acc
info['winter']['tree'] = []
for t in tops: walk(t, 0, info['winter']['tree'])
# renders
sc = bpy.context.scene
for eng in ('BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'):
    try: sc.render.engine = eng; break
    except Exception: pass
sc.render.resolution_x = 1600; sc.render.resolution_y = 1000; sc.view_settings.view_transform = 'Standard'
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs[0].default_value = (0.75, 0.85, 1.0, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
bpy.ops.object.light_add(type='SUN'); L = bpy.context.object; L.rotation_euler = (math.radians(55), 0, math.radians(-140)); L.data.energy = 3.5
bpy.ops.object.camera_add(); cam = bpy.context.object; sc.camera = cam; cam.data.clip_end = 5000
C = (lo + hi) / 2; S = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z)
def shot(name, loc, ortho=False, lens=30):
    cam.data.type = 'ORTHO' if ortho else 'PERSP'
    if ortho: cam.data.ortho_scale = S * 1.05
    cam.data.lens = lens
    cam.location = loc; cam.rotation_euler = (C - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True)
shot('winter_top', C + Vector((0, 0.001, S * 2)), ortho=True)
shot('winter_3q_south', C + Vector((0, -S * 0.9, S * 0.55)))
shot('winter_3q_east', C + Vector((S * 0.9, 0, S * 0.45)))
shot('winter_low', C + Vector((0, -S * 0.55, S * 0.12)), lens=24)
# animals side by side
for o in list(bpy.context.scene.objects):
    if o.type == 'MESH' or o.type == 'EMPTY': o.hide_render = True
info['animals'] = {}
x = 0.0
for nm in ('fox', 'polar_bear', 'penguin', 'walrus', 'fish_1', 'fish_2'):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(D, nm + '.glb'))
    new = [o for o in bpy.context.scene.objects if o not in before]
    l2, h2 = bbox(new)
    info['animals'][nm] = {'bbox_lo': list(l2), 'bbox_hi': list(h2), 'objects': len(new),
                           'anims': [a.name for a in bpy.data.actions]}
    sz = max(h2.x - l2.x, h2.y - l2.y, h2.z - l2.z) or 1
    for o in new:
        if o.parent is None: o.location.x += x - (l2.x + h2.x) / 2; o.location.y -= (l2.y + h2.y) / 2; o.location.z -= l2.z
    x += max(2.5, (h2.x - l2.x) * 1.4)
anims = [o for o in bpy.context.scene.objects if not o.hide_render and o.type == 'MESH']
la, ha = bbox(anims); C = (la + ha) / 2; S = max(ha.x - la.x, 1)
shot('animals', C + Vector((0, -S * 0.9, S * 0.3)), lens=40)
json.dump(info, open(os.path.join(OUT, 'info.json'), 'w'), indent=1, default=str)
print('INSPECT DONE')
