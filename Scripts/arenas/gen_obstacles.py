# Generates Source/ZoogiRoll/Private/ZoogiObstacles.inl for the arenas-import obstacle sets (Meadow batch 6, Night Circuit rev 18.1).
# Hulls come straight from the GLB UCX_ nodes (glTF m, Y-up -> UE cm: X=x, Y=z, Z=y), placements from the packs' PLACEMENT files,
# trigger shapes/values from the packs' IMPORT.md tables (placeholders, untuned).
import json, math, trimesh, numpy as np
from scipy.spatial import ConvexHull
RI = '/workspace/zoogi-unreal/reskin-incoming'
B = '/workspace/zoogi-unreal/arena-import/bundle'
imp = json.load(open('/workspace/zoogi-unreal/arena-import/obstacles_import.json'))
THEME = {'meadow': 0, 'neon': 6}

def hulls(path):
    s = trimesh.load(path); out = []
    for node in s.graph.nodes_geometry:
        if not node.upper().startswith('UCX'): continue
        T, g = s.graph[node]
        v = trimesh.transform_points(s.geometry[g].vertices, T)
        v = np.stack([v[:, 0], v[:, 2], v[:, 1]], 1) * 100.0
        try: v = v[ConvexHull(v).vertices]
        except Exception: pass
        out.append((node, v))
    return out

meshes = []  # key -> (theme, visual paths, spent paths, hulls)
def mesh_idx(m, key, spent=None):
    for i, e in enumerate(meshes):
        if e['key'] == key: return i
    vis = [r['path'] for r in imp if r['map'] == m and r['file'] == key and not r['name'].upper().startswith(('UCX', 'TRIGGER'))]
    sp = [r['path'] for r in imp if r['map'] == m and r['file'] == spent and not r['name'].upper().startswith('UCX')] if spent else []
    assert vis, key
    meshes.append({'key': key, 'theme': THEME[m], 'vis': vis, 'spent': sp, 'hulls': hulls(f'{B}/{m}/{key}.glb')})
    return len(meshes) - 1

inst = []  # dict(theme, mesh, x,y,z,yaw,scale, collide, fric, rest, gain, maxout, trig=[...], name)
def T(kind, shape, c=(0,0,0), e=(0,0,0), d=(1,0), p=(0,0,0,0)):
    return dict(kind=kind, shape=shape, c=c, e=e, d=d, p=p)
# kinds: 1 Boost(p0 add cm/s, p1 mult, p2 cooldown, p3 min entry), 2 Launch(p0 fwd, p1 up, p2 cooldown, p3 min entry), 3 Energy(p0 amount, p1 recharge),
#        4 Zone(p0 decel abs (0=keep), p1 decel mult, p2 entry speed mult, p3 cooldown)
# shapes: 0 box (c centre, e half extents), 1 sphere (c, e.x r), 2 cylinder (c centre, e.x r, e.z half height), 3 arc (e = r0, r1, h; d = yaw0, yaw1 local deg)

# ---------------- Meadow batch 6 (PLACEMENT_batch6_obstacles_cm.json, IMPORT.md B6-2, SHAPE_CHANGE.md)
md = json.load(open(f'{RI}/01-meadow-arena/PLACEMENT_batch6_obstacles_cm.json'))
PM = {'grassy_bank': (0.7, 0.45), 'log_deflector': (0.6, 0.5), 'dirt_ramp': (0.9, 0.3), 'boulder_bumper': (0.5, 0.6),
      'hedge_arch': (0.8, 0.35), 'stump_launch': (0.7, 0.4), 'wet_clover': (0, 0), 'energy_orb': (0, 0)}
for p in md['placements']:
    key = p['mesh'].split('/')[-1][:-4]; k = p['kind']
    mi = mesh_idx('meadow', key, 'SM_MeadowEnergyOrb_Spent' if k == 'energy_orb' else None)
    f, r = PM[k]
    e = dict(theme=0, mesh=mi, x=p['location_cm'][0], y=p['location_cm'][1], z=p['location_cm'][2], yaw=float(p['yaw']), scale=1.0,
             collide=bool(p['solid']), fric=f, rest=r, gain=0.0, maxout=0.0, trig=[], name=f"{k}_{p['yaw']:g}")
    if k == 'boulder_bumper': e['gain'], e['maxout'] = 1.25, 1400.0
    if k == 'hedge_arch': e['trig'].append(T(1, 0, (0, 0, 100), (20, 80, 100), (1, 0), (600, 0, 1.0, 0)))           # TRG_HedgeBoost
    if k == 'stump_launch':
        s, el = 850.0, math.radians(35)
        e['trig'].append(T(2, 2, (0, 0, 60), (40, 0, 15), (1, 0), (s * math.cos(el), s * math.sin(el), 1.0, 0)))     # TRG_StumpLaunch
    if k == 'energy_orb': e['trig'].append(T(3, 1, (0, 0, 35), (45, 0, 0), (1, 0), (10, 20, 0, 0)))                    # TRG_EnergyOrb
    if k == 'wet_clover': e['trig'].append(T(4, 3, (0, 0, 0), (430, 560, 20), (-70, 70), (115, 0, 0, 0)))              # VOL_WetClover
    inst.append(e)

# ---------------- Night Circuit rev 18 / 18.1 (PLACEMENT.json rev 18 rows, IMPORT.md Rev 18)
nc = json.load(open(f'{RI}/07-night-circuit/PLACEMENT.json'))
NPM = {'SM_NC_PostBumper': (0.2, 0.8), 'SM_NC_BoostGate': (0.2, 0.8), 'SM_NC_Deflector': (0.2, 0.7), 'SM_NC_BankCurve': (0.2, 0.5),
       'SM_NC_ReentryHump': (0.3, 0.1)}
for p in nc['pieces']:
    if p.get('rev') != 18: continue
    key = p['mesh']; mi = mesh_idx('neon', key)
    f, r = NPM.get(key, (0, 0))
    gp = p['game_placeholders']
    e = dict(theme=6, mesh=mi, x=p['ue_cm'][0], y=p['ue_cm'][1], z=p['ue_cm'][2], yaw=float(p['yaw_deg']), scale=float(p['ue_scale'][0]),
             collide=bool(p['collision_enabled']) or key == 'SM_NC_BoostGate', fric=f, rest=r, gain=0.0, maxout=0.0, trig=[], name=p['id'])
    if key == 'SM_NC_PostBumper': e['gain'], e['maxout'] = gp['bounce_gain'], 1400.0
    if key == 'SM_NC_BoostGate':  # posts at local X +-117 (world), passage along local Y; Gate A clockwise (-Y), Gate B ccw (+Y)
        e['trig'].append(T(1, 0, (0, 0, 100), (85, 30, 100), (0, -1 if p['id'].endswith('A') else 1), (0, gp['boost_mult'], 1.0, 0)))
    if key == 'SM_NC_SpeedLane':
        e['trig'].append(T(4, 3, (0, 0, 0), (1290, 1410, 30), (0, 32), (0, gp['friction_mult'], gp['speed_bonus'], 1.0)))
    if key == 'SM_NC_JumpPad':   # 'aerial hop over the pocket line toward hub/orbs': forward = inward = actor -X (yaw = polar)
        e['trig'].append(T(2, 0, (0, 0, 20), (100, 80, 20), (-1, 0), (gp['launch_fwd_cm_s'], gp['launch_up_cm_s'], gp['cooldown_s'], 0)))
    if key == 'SM_NC_EnergyOrb': e['trig'].append(T(3, 1, (0, 0, 60), (45, 0, 0), (1, 0), (gp['episode_energy'], gp['respawn_s'], 0, 0)))
    inst.append(e)

def f3(v): return ', '.join(f'{x:.2f}f' for x in v)
L = ['// Generated by gen_obstacles.py (arenas-import, Oct 14): Meadow batch 6 + Night Circuit rev 18.1 obstacle sets.',
     '// Placements in world cm (actor yaw only, mesh scale Scale); hulls = the packs\' UCX_ nodes in mesh-local cm; triggers = IMPORT.md placeholders.',
     '#pragma once', 'namespace ZoogiObs', '{',
     '\tstruct FMeshDef { int32 Theme; const TCHAR* Key; int32 VisFirst, VisNum, SpentFirst, SpentNum, HullFirst, HullNum; };',
     '\tstruct FHull { int32 PtFirst, PtNum; };',
     '\tstruct FTrig { int32 Inst, Kind, Shape; float C[3], E[3], D[2], P[4]; };',
     '\tstruct FInst { int32 Theme, Mesh; const TCHAR* Name; float X, Y, Z, Yaw, Scale; bool bCollide; float Friction, Restitution, BounceGain, MaxOut; };']
paths, hl, pts, md_rows = [], [], [], []
for m in meshes:
    vf = len(paths); paths += m['vis']; sf = len(paths); paths += m['spent']; hf = len(hl)
    for _, v in m['hulls']:
        hl.append((len(pts), len(v))); pts += [tuple(x) for x in v]
    md_rows.append(f'\t\t{{ {m["theme"]}, TEXT("{m["key"]}"), {vf}, {len(m["vis"])}, {sf}, {len(m["spent"])}, {hf}, {len(m["hulls"])} }},')
L.append('\tinline const TCHAR* Paths[] = {'); L += [f'\t\tTEXT("{p}"),' for p in paths]; L.append('\t};')
L.append('\tinline const FMeshDef Meshes[] = {'); L += md_rows; L.append('\t};')
L.append('\tinline const FHull Hulls[] = {'); L += [f'\t\t{{ {a}, {b} }},' for a, b in hl]; L.append('\t};')
L.append('\tinline const float Pts[] = {')
for i in range(0, len(pts), 4): L.append('\t\t' + ' '.join(f'{x:.2f}f, {y:.2f}f, {z:.2f}f,' for x, y, z in pts[i:i + 4]))
L.append('\t};')
L.append('\tinline const FInst Insts[] = {')
trg = []
for i, e in enumerate(inst):
    L.append(f'\t\t{{ {e["theme"]}, {e["mesh"]}, TEXT("{e["name"]}"), {e["x"]:.1f}f, {e["y"]:.1f}f, {e["z"]:.1f}f, {e["yaw"]:.2f}f, {e["scale"]:.3f}f, {"true" if e["collide"] else "false"}, {e["fric"]:.2f}f, {e["rest"]:.2f}f, {e["gain"]:.2f}f, {e["maxout"]:.0f}.f }},')
    for t in e['trig']: trg.append((i, t))
L.append('\t};')
L.append('\tinline const FTrig Trigs[] = {')
for i, t in trg: L.append(f'\t\t{{ {i}, {t["kind"]}, {t["shape"]}, {{ {f3(t["c"])} }}, {{ {f3(t["e"])} }}, {{ {f3(t["d"])} }}, {{ {f3(t["p"])} }} }},')
L.append('\t};'); L.append('}')
open('/workspace/zoogi-unreal/arena-import/gen/ZoogiObstacles.inl', 'w', newline='\r\n').write('\n'.join(L) + '\n')
print(len(meshes), 'meshes', len(paths), 'paths', len(hl), 'hulls', len(pts), 'pts', len(inst), 'insts', len(trg), 'trigs')
for m in meshes: print(m['key'], len(m['vis']), len(m['spent']), len(m['hulls']), [len(v) for _, v in m['hulls']][:12])
