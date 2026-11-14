"""Cosmic R2 mesh import (UE 5.6). Legacy FBX (IMPORT.md fallback) - Interchange drops UCX+LOD FBX.
UCX-by-name, LODs on, no mats/textures created, no auto collision, no Nanite.
Writes Saved/cosmic_r2_import.json. Env COSMIC_R2_PACK = pack dir."""
import os, sys, json, glob, unreal
PACK = sys.argv[-1] if len(sys.argv) > 1 and os.path.isdir(sys.argv[-1]) else os.environ.get('COSMIC_R2_PACK', '')
DEST = '/Game/Cosmic/R2'; MDIR = DEST + '/Meshes'; MATDIR = DEST + '/Materials'
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
rep = {'meshes': {}, 'ucx_total': 0, 'errors': [], 'bad_slots': [], 'lod_issues': [], 'mat_check': {}, 'ucx_name_check': []}
def log(m): unreal.log_warning('COSMICR2: ' + m)
def err(m): rep['errors'].append(m); unreal.log_warning('COSMICR2: ERROR ' + m)

# ---- DIAG: UCX names vs LOD group names (from pack verify JSON)
verify = os.path.join(PACK, 'checks', 'fbx_lod_verify.json')
mismatch = 0; ucx_expect = 0
if os.path.isfile(verify):
    V = json.load(open(verify))
    ucx_expect = int(V.get('ucx_total', 0))
    for piece, p in V.get('pieces', {}).items():
        lodg = (p.get('lod_group_nodes') or [None])[0] or piece
        ucxs = list((p.get('ucx') or {}).keys())
        for u in ucxs:
            base = u[4:] if u.startswith('UCX_') else u
            base = base.rsplit('_', 1)[0] if base[-3:-2] == '_' and base[-2:].isdigit() else base
            # UCX_NAME_00 -> strip _NN
            parts = u.split('_')
            if parts and parts[-1].isdigit():
                base = '_'.join(parts[1:-1])  # drop UCX_ prefix and trailing index
            else:
                base = '_'.join(parts[1:]) if parts[0] == 'UCX' else u
            ok = (base == lodg) or (base == piece)
            line = 'UCX %s base=%s lod_group=%s piece=%s %s' % (u, base, lodg, piece, 'OK' if ok else 'MISMATCH')
            rep['ucx_name_check'].append(line)
            if not ok: mismatch += 1
    log('DIAG ucx_name_check: expect_ucx=%d pieces=%d mismatches=%d' % (ucx_expect, len(V.get('pieces', {})), mismatch))
    for line in rep['ucx_name_check'][:5]: log('DIAG ' + line)
    if mismatch: log('DIAG WARNING: %d UCX/LOD-group name mismatches' % mismatch)
else:
    log('DIAG no fbx_lod_verify.json at %s' % verify)

# Legacy FBX - Interchange scripted pipeline fails to finish UCX+LOD FBX (0 of 63).
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX false')
log('DIAG using legacy FbxImportUI (Interchange.FeatureFlags.Import.FBX false)')

def ui():
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    o.set_editor_property('import_animations', False); o.set_editor_property('automated_import_should_detect_type', False)
    s = o.get_editor_property('static_mesh_import_data')
    for k, v in (('combine_meshes', False), ('import_mesh_lods', True), ('auto_generate_collision', False),
                 ('one_convex_hull_per_ucx', True), ('build_nanite', False), ('compute_weighted_normals', False),
                 ('convert_scene', True), ('force_front_x_axis', False), ('convert_scene_unit', True),
                 ('import_uniform_scale', 1.0), ('bake_pivot_in_vertex', False)):
        try: s.set_editor_property(k, v)
        except Exception as ex: err('fbx opt %s: %s' % (k, ex))
    try: s.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    except Exception as ex: err('normals: %s' % ex)
    t = o.get_editor_property('texture_import_data')
    try: t.set_editor_property('material_search_location', unreal.MaterialSearchLocation.ALL_ASSETS)
    except Exception as ex: err('search loc: %s' % ex)
    return o

files = sorted(glob.glob(os.path.join(PACK, 'meshes', '*.fbx')))
log('%d FBX from %s' % (len(files), PACK))
tasks = []
for f in files:
    k = unreal.AssetImportTask(); k.filename = f; k.destination_path = MDIR; k.automated = True; k.replace_existing = True; k.save = False
    k.options = ui(); tasks.append(k)
AT.import_asset_tasks(tasks)
for k in tasks:
    paths = list(k.imported_object_paths or [])
    log('DIAG import %s -> %s' % (os.path.basename(k.filename), paths[:6]))
    if not paths: err('nothing imported from ' + os.path.basename(k.filename))
    elif len(paths) != 1: rep['lod_issues'].append('%s -> %s' % (os.path.basename(k.filename), paths))

for f in files:
    n = os.path.splitext(os.path.basename(f))[0]; p = '%s/%s' % (MDIR, n)
    sm = unreal.load_asset(p) if EAL.does_asset_exist(p) else None
    if not isinstance(sm, unreal.StaticMesh): err('missing mesh ' + p); continue
    try: sm.set_editor_property('lod_group', 'None')
    except Exception as ex: err('%s lod_group: %s' % (n, ex))
    nl = sm.get_num_lods()
    want = [1.0, 0.35, 0.12][:nl]
    ok = False
    if hasattr(SMS, 'set_lod_screen_sizes'):
        try: SMS.set_lod_screen_sizes(sm, want); ok = True
        except Exception as ex: rep['lod_issues'].append('%s set_lod_screen_sizes: %s' % (n, ex))
    if not ok: rep['lod_issues'].append('%s: screen sizes not set' % n)
    got = []
    try: got = [round(x, 3) for x in SMS.get_lod_screen_sizes(sm)]
    except Exception as ex: got = ['?%s' % ex]
    ucx = 0
    try:
        bs = sm.get_editor_property('body_setup')
        if bs:
            ucx = len(bs.get_editor_property('agg_geom').get_editor_property('convex_elems'))
    except Exception as ex:
        try: ucx = max(unreal.EditorStaticMeshLibrary.get_convex_collision_count(sm), 0)
        except Exception as ex2: err('%s ucx: %s / %s' % (n, ex, ex2)); ucx = 0
    rep['ucx_total'] += ucx
    slots = []
    for i, s in enumerate(sm.get_editor_property('static_materials')):
        sn = str(s.get_editor_property('material_slot_name')); mi = s.get_editor_property('material_interface')
        mp = mi.get_path_name() if mi else ''
        want_p = '%s/%s.%s' % (MATDIR, sn, sn)
        if mp != want_p:
            m2 = unreal.load_asset('%s/%s' % (MATDIR, sn)) if EAL.does_asset_exist('%s/%s' % (MATDIR, sn)) else None
            rep['bad_slots'].append('%s[%d] %s had %s -> %s' % (n, i, sn, mp or 'None', 'fixed' if m2 else 'NO MI'))
            if m2: sm.set_material(i, m2)
        slots.append(sn)
    b = sm.get_bounding_box()
    expect_lod = 1 if n in ('SM_HaloRing_R2','SM_Planet_R2','SM_CosmicFloorDisc_R2') else 3
    rep['lod_counts'] = rep.get('lod_counts', 0) + (1 if nl == expect_lod else 0)
    rep['meshes'][n] = {'lods': nl, 'screen': got, 'ucx': ucx, 'slots': slots,
        'min': [round(b.min.x), round(b.min.y), round(b.min.z)], 'max': [round(b.max.x), round(b.max.y), round(b.max.z)]}
    EAL.save_loaded_asset(sm)

for n1, n2 in (('SM_EmitterPylon_R1/SM_EmitterPylon_R1/StaticMeshes/SM_EmitterPylon', 'SM_EmitterPylon_R2'),
               ('SM_ZArch_R1/SM_ZArch_R1/StaticMeshes/SM_ZArch_Frame', 'SM_ZArch_R2')):
    p = '/Game/Env/CosmicR1/' + n1
    if EAL.does_asset_exist(p):
        b = unreal.load_asset(p).get_bounding_box()
        rep.setdefault('r1_bounds', {})[n2] = [round(b.min.x), round(b.min.y), round(b.min.z), round(b.max.x), round(b.max.y), round(b.max.z)]

J = json.load(open(os.path.join(PACK, 'materials', 'materials.json')))['instances']
picked = {}
for name, mi in J.items(): picked.setdefault(mi['parent'], name)
for k in ('MI_Cosmic_Paint_R2', 'MI_Cosmic_Satin_R2', 'MI_Cosmic_Metal_R2'): picked[k] = k
for par, name in picked.items():
    a = unreal.load_asset('%s/%s' % (MATDIR, name)); d = {}
    if not a: err('MI missing ' + name); continue
    d['parent'] = a.get_editor_property('parent').get_name() if a.get_editor_property('parent') else None
    for s in J[name]['scalars']: d[s] = round(MEL.get_material_instance_scalar_parameter_value(a, s), 3)
    for t in J[name]['textures']: tx = MEL.get_material_instance_texture_parameter_value(a, t); d[t] = tx.get_name() if tx else None
    pm = unreal.load_asset('%s/%s' % (MATDIR, J[name]['parent']))
    try: d['parent_expr'] = MEL.get_num_material_expressions(pm) if pm else 0
    except Exception as ex: d['parent_expr'] = str(ex)
    rep['mat_check'][name] = d
nmi = len([1 for n in J if EAL.does_asset_exist('%s/%s' % (MATDIR, n))])
npar = len([1 for n in set(v['parent'] for v in J.values()) if EAL.does_asset_exist('%s/%s' % (MATDIR, n))])
rep['mi_count'] = nmi; rep['parent_count'] = npar; rep['ucx_expect'] = ucx_expect; rep['ucx_name_mismatches'] = mismatch
if EAL.does_directory_exist('/Game/Env/CosmicR1'):
    rep['r1_deleted'] = EAL.delete_directory('/Game/Env/CosmicR1')
out = os.path.join(unreal.Paths.project_saved_dir(), 'cosmic_r2_import.json'); json.dump(rep, open(out, 'w'), indent=1)
log('LODOK=%d/51' % rep.get('lod_counts', 0))
log('DONE meshes=%d parents=%d mis=%d ucx_total=%d ucx_expect=%d bad_slots=%d lod_issues=%d errors=%d r1_deleted=%s' % (
    len(rep['meshes']), npar, nmi, rep['ucx_total'], ucx_expect, len(rep['bad_slots']), len(rep['lod_issues']), len(rep['errors']), rep.get('r1_deleted')))
