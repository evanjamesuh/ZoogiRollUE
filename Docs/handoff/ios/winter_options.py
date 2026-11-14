# Nov 6 (Claude): Frozen Ring x ithappy Winter merge OPTIONS (mock-up renders). The Frozen Ring itself is a stand-in disc textured
# with the in-game top-down capture (proofs/p5_style/lmap_frozen/arena_frozen_0_r2_top.png).
import bpy, os, sys, math, random
from mathutils import Vector, Matrix
D = sys.argv[sys.argv.index('--') + 1]
OUT = os.path.join(D, 'options'); os.makedirs(OUT, exist_ok=True)
TOP = r'C:\dev\proofs\p5_style\lmap_frozen\arena_frozen_0_r2_top.png'
R = 34.0
def reset(): bpy.ops.wm.read_factory_settings(use_empty=True)
def load():
    bpy.ops.import_scene.gltf(filepath=os.path.join(D, 'winter_location.glb'))
    return list(bpy.context.scene.objects)
def wbb(o):
    cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
    lo = Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))); hi = Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
    return lo, hi
def meshes(): return [o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.hide_render]
def centre():
    lo = Vector((1e9, 1e9, 0)); hi = Vector((-1e9, -1e9, 0))
    for o in meshes():
        a, b = wbb(o); lo.x = min(lo.x, a.x); lo.y = min(lo.y, a.y); hi.x = max(hi.x, b.x); hi.y = max(hi.y, b.y)
    return Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, 0))
def gz(x, y):
    dg = bpy.context.evaluated_depsgraph_get()
    hit, loc, *_ = bpy.context.scene.ray_cast(dg, Vector((x, y, 200)), Vector((0, 0, -1)))
    return loc.z if hit else None
def mat_img():
    m = bpy.data.materials.new('RingTop'); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(TOP)
    nt.links.new(t.outputs['Color'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.9
    nt.links.new(t.outputs['Color'], b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value = 0.25
    return m
def mat_col(name, rgb):
    m = bpy.data.materials.new(name); m.use_nodes = True; m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = rgb + (1,); return m
def disc(c, r, z, mat, uv=False):
    bpy.ops.mesh.primitive_circle_add(vertices=128, radius=r, fill_type='NGON', location=(c.x, c.y, z))
    o = bpy.context.object; o.data.materials.append(mat)
    if uv:
        me = o.data; uvl = me.uv_layers.new()
        for poly in me.polygons:
            for li in poly.loop_indices:
                v = me.vertices[me.loops[li].vertex_index].co
                uvl.data[li].uv = ((800 + v.x / r * 450) / 1600, 0.5 + v.y / r * 0.5)
    return o
def ring_at(c, z):
    disc(c, R + 6, z - 0.15, mat_col('Skirt', (0.93, 0.96, 1.0)))
    disc(c, R, z, mat_img(), uv=True)
def setup_render():
    sc = bpy.context.scene
    for eng in ('BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'):
        try: sc.render.engine = eng; break
        except Exception: pass
    sc.render.resolution_x = 1600; sc.render.resolution_y = 900; sc.view_settings.view_transform = 'Standard'
    w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.62, 0.78, 1.0, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
    bpy.ops.object.light_add(type='SUN'); L = bpy.context.object; L.rotation_euler = (math.radians(50), 0, math.radians(-150)); L.data.energy = 3.2
    bpy.ops.object.camera_add(); cam = bpy.context.object; sc.camera = cam; cam.data.clip_end = 6000
    return cam
def shoot(cam, tag, c, z):
    for nm, off, lens in (('high', (0, -R * 3.4, R * 2.3), 28), ('low', (0, -R * 1.55, R * 0.42), 24), ('top', (0, -0.01, R * 6.5), 35)):
        cam.location = Vector((c.x + off[0], c.y + off[1], z + off[2])); cam.data.lens = lens
        cam.rotation_euler = (Vector((c.x, c.y, z + (0 if nm == 'top' else 4))) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        bpy.context.scene.render.filepath = os.path.join(OUT, '%s_%s.png' % (tag, nm)); bpy.ops.render.render(write_still=True)
def ring_z(c, rr):
    zs = [gz(c.x + rr * math.cos(a), c.y + rr * math.sin(a)) for a in [i * math.pi / 12 for i in range(24)]]
    zs = sorted(z for z in zs if z is not None); return zs[len(zs) // 2] if zs else 0.0

# ---------- A: the ring in the middle of the Winter village (everything inside the ring area cleared)
reset(); load(); C = centre(); z0 = ring_z(C, R + 12)
for o in meshes():
    a, b = wbb(o); m = (a + b) / 2
    if (Vector((m.x, m.y, 0)) - C).length < R + 4: o.hide_render = True
ring_at(C, z0 + 0.2); cam = setup_render(); shoot(cam, 'A_village', C, z0)
print('A z0', z0, 'C', C)

# ---------- B: today's Frozen Ring + stands untouched; the Winter village pushed out as the far backdrop (x1.7, cleared to 80 m)
reset(); objs = load(); C = centre()
for o in objs:
    if o.parent is None: o.matrix_world = Matrix.Translation(C) @ Matrix.Scale(1.7, 4) @ Matrix.Translation(-C) @ o.matrix_world
bpy.context.view_layer.update()
z0 = ring_z(C, 95)
for o in meshes():
    a, b = wbb(o); m = (a + b) / 2
    if (Vector((m.x, m.y, 0)) - C).length < 80: o.hide_render = True
disc(C, 82, z0 - 0.3, mat_col('Snow', (0.95, 0.97, 1.0))); ring_at(C, z0 + 0.2); cam = setup_render(); shoot(cam, 'B_backdrop', C, z0)

# ---------- C: ice fortress courtyard (castle walls + towers ring the court, firs, igloos, snowmen, penguins)
reset(); objs = load()
tpl = {}
for o in meshes():
    s = o.name.split('.')[0]
    for k in ('wall', 'tower', 'tower_cap', 'arch', 'fir', 'igloo', 'snowman', 'penguin', 'flag_blue', 'flag_red', 'bonfire'):
        if (s == k or s.startswith(k + '_0')) and k not in tpl and not (k == 'tower' and s.startswith('tower_cap')): tpl[k] = o
for o in meshes(): o.hide_render = True
C = Vector((0, 0, 0)); z0 = 0.0
col = bpy.context.scene.collection
def place(k, pos, yaw):
    src = tpl.get(k)
    if not src: return None
    o = src.copy(); o.parent = None; col.objects.link(o); o.hide_render = False
    rs = src.matrix_world.to_3x3().to_4x4()
    o.matrix_world = Matrix.Rotation(yaw, 4, 'Z') @ rs
    bpy.context.view_layer.update()
    a, b = wbb(o); m = (a + b) / 2
    o.matrix_world = Matrix.Translation(Vector((pos.x - m.x, pos.y - m.y, pos.z - a.z))) @ o.matrix_world
    return o
disc(C, 140, -0.05, mat_col('Snow', (0.95, 0.97, 1.0)))
ring_at(C, 0.2)
N = 18
for i in range(N):
    a = 2 * math.pi * i / N - math.pi / 2           # i = 0 at the south (camera side)
    if abs(math.degrees(a + math.pi / 2)) < 25 or abs(math.degrees(a + math.pi / 2) - 360) < 25:
        if i == 0: place('arch', Vector((math.cos(a) * (R + 9), math.sin(a) * (R + 9), 0)), a + math.pi / 2)
        continue
    p = Vector((math.cos(a) * (R + 9), math.sin(a) * (R + 9), 0))
    if i % 2 == 0:
        t = place('tower', p, a); 
        if t: place('tower_cap', Vector((p.x, p.y, wbb(t)[1].z - 0.3)), a)
    else: place('wall', p, a + math.pi / 2)
random.seed(4)
for i in range(46):
    a = random.uniform(0, 2 * math.pi); r = random.uniform(R + 18, R + 50)
    place('fir', Vector((math.cos(a) * r, math.sin(a) * r, 0)), random.uniform(0, 6.3))
for k, n, r0, r1 in (('igloo', 4, R + 22, R + 30), ('snowman', 4, R + 14, R + 17), ('penguin', 10, R + 12, R + 16), ('bonfire', 2, R + 14, R + 16), ('flag_blue', 3, R + 13, R + 13), ('flag_red', 3, R + 13, R + 13)):
    for j in range(n):
        a = random.uniform(-math.pi * 0.8, math.pi * 0.8) + math.pi / 2   # behind and beside the court
        place(k, Vector((math.cos(a) * random.uniform(r0, r1), math.sin(a) * random.uniform(r0, r1), 0)), random.uniform(0, 6.3))
cam = setup_render(); shoot(cam, 'C_fortress', C, z0)
print('templates', sorted(tpl))
print('OPTIONS DONE')
