# Generates Source/ZoogiRoll/Private/ZoogiObstacles.inl for the arenas-import obstacle sets (Meadow batch 6, Night Circuit rev 18.1).
# Hulls come straight from the GLB UCX_ nodes (glTF m, Y-up -> UE cm: X=x, Y=z, Z=y), placements from the packs' PLACEMENT files,
# trigger shapes/values from the packs' IMPORT.md tables (placeholders, untuned).
import json, math, trimesh, numpy as np
from scipy.spatial import ConvexHull
RI = '/workspace/zoogi-unreal/reskin-incoming'
B = '/workspace/zoogi-unreal/arena-import/bundle'
import os
imp = json.load(open('/workspace/zoogi-unreal/arena-import/obstacles_import.json'))
if os.path.exists('/workspace/zoogi-unreal/arena-import/imp2.json'): imp += json.load(open('/workspace/zoogi-unreal/arena-import/imp2.json'))
THEME = {'meadow': 0, 'neon': 6, 'volcanic': 2, 'cosmic': 3, 'arabian': 4, 'tomb': 5}
B2 = '/workspace/zoogi-unreal/arena-import/bundle2'

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
    meshes.append({'key': key, 'theme': THEME[m], 'vis': vis, 'spent': sp, 'hulls': hulls(f'{B}/{m}/{key}.glb' if os.path.exists(f'{B}/{m}/{key}.glb') else f'{B2}/{m}/{key}.glb')})
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


# kinds added for the r2015 sets: 5 heading snap (p0 world yaw), 6 set min speed along dir (p0 speed, p1 one-way), 7 two-way mult (p0 mult, p1 min speed)
# shape 4: world arc around the arena centre (e = r0, r1, h; d = world yaw0, yaw1)
def loc(e, wx, wy, wz):  # world point -> instance local (yaw only)
    a = math.radians(-e['yaw']); dx, dy = wx - e['x'], wy - e['y']
    return (dx * math.cos(a) - dy * math.sin(a), dx * math.sin(a) + dy * math.cos(a), wz - e['z'])
def polar(x, y): return math.degrees(math.atan2(y, x)) % 360
def trig_box(glb):
    s = trimesh.load(glb)
    for node in s.graph.nodes_geometry:
        if node.upper().startswith('TRIGGER'):
            T, g = s.graph[node]; v = trimesh.transform_points(s.geometry[g].vertices, T)
            v = np.stack([v[:, 0], v[:, 2], v[:, 1]], 1) * 100.0
            lo, hi = v.min(0), v.max(0); return tuple((lo + hi) / 2), tuple((hi - lo) / 2)
def add(theme, m, key, x, y, z, yaw, collide, f, r, name, gain=0.0, maxout=0.0, scale=1.0, spent=None):
    e = dict(theme=theme, mesh=mesh_idx(m, key, spent), x=x, y=y, z=z, yaw=float(yaw) % 360, scale=scale, collide=collide, fric=f, rest=r,
             gain=gain, maxout=maxout, trig=[], name=name)
    inst.append(e); return e

# ---------------- Volcanic batch 12.1 (PLACEMENT.json batch 12 rows + play_collider_v121)
vp = json.load(open(f'{RI}/03-volcanic-pit/PLACEMENT.json'))
VPM = {'BoostArch': (0.3, 0.4), 'ObsidianBumper': (0.2, 1.0), 'BasaltDeflector': (0.2, 0.6), 'BankWall': (0.5, 0.4), 'MagmaVent': (0.3, 0.3), 'EnergyOrb': (0.3, 0.4)}
for i in vp['instances']:
    if 'play_collider_v121' in i:
        pc = i['play_collider_v121']
        add(2, 'volcanic', pc['mesh'], pc['location'][0], pc['location'][1], 0, pc['yaw'], True, 0.3, 0.55, 'Outcrop01_PlayBumper_v121')
    if i.get('batch') != 12: continue
    kind = next((k for k in VPM if k in i['mesh']), None)
    f, r = VPM.get(kind, (0, 0)); L = i['location']
    e = add(2, 'volcanic', i['mesh'], L[0], L[1], L[2], i['yaw'], kind is not None, f, r, i['id'],
            gain=1.3 if kind == 'ObsidianBumper' else 0.0, maxout=1400.0 if kind == 'ObsidianBumper' else 0.0)
    t = i.get('trigger') or {}
    n = t.get('name', '')
    if n == 'TRG_ArchBoost': e['trig'].append(T(1, 0, tuple(t['center_local_cm']), tuple(t['half_extent_cm']), (0, 1), (900, 0, 1.0, 0)))
    if n == 'TRG_VentLaunch':
        ly = math.radians(t['aim']['line_yaw_deg'] - e['yaw'])
        e['trig'].append(T(2, 2, (0, 0, 30), (72, 0, 30), (math.cos(ly), math.sin(ly)), (150, math.sqrt(2 * 980 * 250), 4.0, 0)))
    if n == 'TRG_EnergyPickup': e['trig'].append(T(3, 1, (0, 0, 100), (60, 0, 0), (1, 0), (10, 12, 0, 0)))
    if n == 'TRG_SlickZone': e['trig'].append(T(4, 4, (0, 0, 0), (1370, 1530, 60), (e['yaw'] - 14, e['yaw'] + 14), (115, 0, 0, 0)))

# ---------------- Cosmic obstacles (trimmed r2015) + batch 3 rim pylon visual swap (UCX query-only = no collider; rim cylinders stay)
cp = json.load(open(f'{RI}/04-cosmic-platform/obstacles/PLACEMENT.json'))
CPM = {'RockPM': (0.2, 0.8), 'FieldPM': (0.0, 0.35), 'BankPM': (0.2, 0.3), 'FloorPM': (0.3, 0.1), 'CrystalPM': (0.1, 0.9)}
byname = {}
for a in cp['actors']:
    pm = a['pm'].split()[0]; f, r = CPM.get(pm, (0, 0)); L = a['location_cm']
    e = add(3, 'cosmic', a['mesh'], L[0], L[1], L[2], a['rotation_pyr_deg'][1], 'UCX' in a['collision'], f, r, a['name'],
            gain=1.15 if pm == 'CrystalPM' else 0.0, maxout=1400.0 if pm == 'CrystalPM' else 0.0)
    byname[a['name']] = e
for t in cp['trigger_volumes']:
    nm = t['name']; c = t['centre_cm']; p = t['params']
    if nm.startswith('TRG_PortalBoost'):
        e = byname['RingPortal_' + nm.split('_')[-1]]; lc = loc(e, *c)
        e['trig'].append(T(1, 0, lc, tuple(x / 2 for x in t['extent_cm']), (1, 0), (0, 1 + p['BoostPct'] / 100, p['Cooldown_s'], p['MinEntrySpeed_cms'])))
    elif nm.startswith('TRG_KickerAim'):
        e = byname['JumpKicker_' + nm.split('_')[-1]]; lc = loc(e, *c)
        e['trig'].append(T(5, 0, lc, tuple(x / 2 for x in t['extent_cm']), (1, 0), (p['HeadingSnapToYaw'] % 360, 0, 0.5, 0)))
    elif nm.startswith('TRG_OuterOrb'):
        e = byname['OuterOrb_' + nm.split('_')[-1]]; lc = loc(e, *c)
        e['trig'].append(T(3, 1, lc, (t.get('radius_cm', 55), 0, 0), (1, 0), (p['EpisodeEnergy'], p['Recharge_s'], 0, 0)))
    elif nm.startswith('PMR_StarDust'):
        e = byname['StarDustLane_' + nm.split('_')[-1]]; lc = loc(e, *c)
        e['trig'].append(T(4, 0, lc, tuple(x / 2 for x in t['extent_cm']), (1, 0), (p['RollingDecel_cms2'], 0, 0, 0)))
    elif nm == 'TRG_LowG_Hub':  # hub lob: 600-1600 cm/s entries get +250 up (placeholder; axis lock not modelled)
        e = add(3, 'cosmic', 'SM_Cosmic_OuterOrb', 0, 0, -400, 0, False, 0, 0, 'LowG_Hub')  # carrier only (mesh hidden below the floor)
        e['hidden'] = True
        e['trig'].append(T(2, 2, (0, 0, 485), (250, 0, 85), (1, 0), (-1, p['LaunchVz_cms'], 0.6, p['MinEntrySpeed_cms'])))
for yaw, (x, y) in [(32, (1472, 920)), (70, (600, 1649)), (152, (-1521, 809)), (196, (-1681, -482)), (250, (-591, -1625)), (332, (1538, -818))]:
    add(3, 'cosmic', 'SM_EmitterPylon_B3', x, y, 0, yaw, False, 0, 0, f'EmitterPylon_B3_{yaw}')

# ---------------- Arabian Nights rev 3.1 (PLACEMENT_6orb.json obstacle_* rows)
ap = json.load(open(f'{RI}/05-arabian-nights/PLACEMENT_6orb.json'))
APM = {'SM_AN_BoostArch': (0.3, 0.4), 'SM_AN_RubyBumper': (0.2, 0.9), 'SM_AN_Deflector_L': (0.2, 0.6), 'SM_AN_Deflector_S': (0.2, 0.6), 'SM_AN_BankWall': (0.4, 0.4)}
for k, p in enumerate(q for q in ap['pieces'] if str(q.get('group', '')).startswith('obstacle')):
    f, r = APM.get(p['mesh'], (0, 0)); L = p['ue_cm']
    e = add(4, 'arabian', p['mesh'], L[0], L[1], L[2], p['yaw_deg'], bool(p['collision_enabled']), f, r, f"{p['group']}_{p['polar_deg']:g}",
            gain=1.2 if 'Ruby' in p['mesh'] else 0.0, maxout=1400.0 if 'Ruby' in p['mesh'] else 0.0, scale=float(p['scale']))
    t = p.get('trigger') or {}; ph = t.get('placeholder', {})
    if t.get('kind') == 'boost':
        e['trig'].append(T(6, 0, loc(e, *t['centre_cm']), tuple(t['half_extent_cm']), (1, 0), (ph['boost_speed_cm_s'], 1, ph['cooldown_s'], 0)))
    elif t.get('kind') == 'speed_zone':
        for bx in t['boxes']:
            lc = loc(e, *bx['centre_cm']); a = math.radians(bx['yaw_deg'] - e['yaw'])
            e['trig'].append(T(4, 0, lc, tuple(bx['half_extent_cm']), (math.cos(a), math.sin(a)), (0, 0.4, 1.15, 1.0)))
    elif t.get('kind') == 'popup':
        e['trig'].append(T(2, 2, loc(e, *t['centre_cm']), (70, 0, 20), (1, 0), (-1, ph['launch_speed_cm_s'], ph['cooldown_s'], 0)))
    elif t.get('kind') == 'pickup':
        e['trig'].append(T(3, 1, loc(e, *t['centre_cm']), (60, 0, 0), (1, 0), (ph['episode_energy'], ph['respawn_s'], 0, 0)))

# ---------------- Pharaoh's Tomb r26.1 (PLACEMENT.json instances with r26_obstacle, world_A)
tp = json.load(open(f'{RI}/06-pharaohs-tomb/PLACEMENT.json')); TM = f'{RI}/06-pharaohs-tomb/meshes'
TPM = {'SM_TombUrnBumper': (0.2, 0.9), 'SM_TombObeliskPost': (0.3, 0.5), 'SM_TombDeflector': (0.2, 0.6), 'SM_TombBankWall': (0.3, 0.4),
       'SM_TombReentryHump': (0.3, 0.2), 'SM_TombPyramidJump': (0.3, 0.2)}
for i in tp['instances']:
    if 'r26_obstacle' not in i: continue
    o = i['r26_obstacle']; w = i['world_A']; m = i['mesh']; f, r = TPM.get(m, (0, 0))
    e = add(5, 'tomb', m, w['x'], w['y'], w['z'], w['yaw'], m in TPM, f, r, o['id'],
            gain=1.25 if 'Urn' in m else 0.0, maxout=1400.0 if 'Urn' in m else 0.0)
    tr = o.get('trigger', {})
    if m == 'SM_TombBoostPortal':
        c, h = trig_box(f'{TM}/SM_TombBoostPortal_Trigger.glb'); e['trig'].append(T(7, 0, c, h, (1, 0), (tr['boost_multiplier'], tr['min_exit_speed_cm_s'], 1.0, 0)))
    if m == 'SM_TombPyramidJump':
        c, h = trig_box(f'{TM}/SM_TombPyramidJump_Trigger.glb'); e['trig'].append(T(2, 0, c, h, (1, 0), (tr['impulse_inward_cm_s'], tr['impulse_up_cm_s'], 1.0, tr['min_entry_speed_cm_s'])))
    if m == 'SM_TombEnergyOrb': e['trig'].append(T(3, 1, (0, 0, 60), (45, 0, 0), (1, 0), (10, 12, 0, 0)))
    if m == 'SM_TombSandSlide':
        pa = polar(w['x'], w['y']); e['trig'].append(T(4, 4, (0, 0, 0), (1310, 1430, 20), (pa - 12, pa + 12), (115, 0, 0, 0)))

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
    L.append(f'\t\t{{ {e["theme"]}, {e["mesh"]}, TEXT("{e["name"]}"), {e["x"]:.1f}f, {e["y"]:.1f}f, {e["z"]:.1f}f, {e["yaw"]:.2f}f, {(-1 if e.get('hidden') else 1)*e["scale"]:.3f}f, {"true" if e["collide"] else "false"}, {e["fric"]:.2f}f, {e["rest"]:.2f}f, {e["gain"]:.2f}f, {e["maxout"]:.0f}.f }},')
    for t in e['trig']: trg.append((i, t))
L.append('\t};')
L.append('\tinline const FTrig Trigs[] = {')
for i, t in trg: L.append(f'\t\t{{ {i}, {t["kind"]}, {t["shape"]}, {{ {f3(t["c"])} }}, {{ {f3(t["e"])} }}, {{ {f3(t["d"])} }}, {{ {f3(t["p"])} }} }},')
L.append('\t};'); L.append('}')
open('/workspace/zoogi-unreal/arena-import/gen/ZoogiObstacles.inl', 'w', newline='\r\n').write('\n'.join(L) + '\n')
print(len(meshes), 'meshes', len(paths), 'paths', len(hl), 'hulls', len(pts), 'pts', len(inst), 'insts', len(trg), 'trigs')
for m in meshes: print(m['key'], len(m['vis']), len(m['spent']), len(m['hulls']), [len(v) for _, v in m['hulls']][:12])
