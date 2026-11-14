# b157 (Evan Nov 6 08:25 "for trick shot build a version of ancient meadows without the most centralize bumpers", 08:26 "leave
# enough to do bank trick shots"): Blender (headless) - SM_Arena_PlazaProps minus the ring of 10 red/white drums around the Z
# medallion (r7.2 m). Everything else stays for banks: the bumper curb ring r17.05, hazard drums r13.4, lantern bollards r14.6,
# banner posts, lamps, floor tiles. Writes pieces_v14_trick\SM_Arena_PlazaPropsTrick.fbx (same export settings as build_arena_v14).
import bpy, bmesh, os, math
A = r"C:\dev\art\ancient_meadows\grok_design\blender\arena"
SRC = os.path.join(A, "pieces_v14", "SM_Arena_PlazaProps.fbx")
OUTD = os.path.join(A, "pieces_v14_trick"); os.makedirs(OUTD, exist_ok=True)
OUT = os.path.join(OUTD, "SM_Arena_PlazaPropsTrick.fbx")
LOG = r"C:\dev\ios\trickprops_blender.txt"
def L(s):
    with open(LOG, "a") as f: f.write(s + "\n")
open(LOG, "w").close()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC, global_scale=1.0, axis_forward='-Y', axis_up='Z')
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
L("imported %d mesh object(s)" % len(objs))
bpy.ops.object.select_all(action='DESELECT')
for o in objs: o.select_set(True)
bpy.context.view_layer.objects.active = objs[0]
if len(objs) > 1: bpy.ops.object.join()
o = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
L("faces %d" % len(bm.faces))
# islands (linked faces)
seen = set(); islands = []
for f in bm.faces:
    if f.index in seen: continue
    st = [f]; seen.add(f.index); isl = []
    while st:
        g = st.pop(); isl.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h.index not in seen: seen.add(h.index); st.append(h)
    islands.append(isl)
def box(isl):
    vs = {v for f in isl for v in f.verts}
    xs = [v.co.x for v in vs]; ys = [v.co.y for v in vs]; zs = [v.co.z for v in vs]
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, min(zs), max(zs), max(max(xs) - min(xs), max(ys) - min(ys))
info = [box(i) for i in islands]
L("islands %d" % len(islands))
best = None
for c in ((0.0, 16.0), (0.0, -16.0), (0.0, 0.0), (16.0, 0.0), (-16.0, 0.0)):
    n = sum(1 for (x, y, z0, z1, w) in info if 6.0 <= math.hypot(x - c[0], y - c[1]) <= 8.6 and z1 > 0.15)
    L("centre %s: %d tall parts at r6-8.6" % (str(c), n))
    if best is None or n > best[1]: best = (c, n)
c = best[0]
kill = [islands[k] for k, (x, y, z0, z1, w) in enumerate(info) if 6.0 <= math.hypot(x - c[0], y - c[1]) <= 8.6 and z1 > 0.15 and w < 3.0]
nf = sum(len(i) for i in kill)
L("plaza centre %s: removing %d drum part(s), %d faces" % (str(c), len(kill), nf))
bmesh.ops.delete(bm, geom=[f for i in kill for f in i], context='FACES')
loose = [v for v in bm.verts if not v.link_faces]
bmesh.ops.delete(bm, geom=loose, context='VERTS')
bm.to_mesh(o.data); bm.free()
o.name = "SM_Arena_PlazaPropsTrick"; o.data.name = "SM_Arena_PlazaPropsTrick"
bpy.ops.object.select_all(action='DESELECT'); o.select_set(True)
bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
    axis_forward='-Y', axis_up='Z', object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False)
L("faces after %d -> %s" % (len(o.data.polygons), OUT))
L("DONE")
