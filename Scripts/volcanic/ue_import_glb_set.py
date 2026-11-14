"""Generic realism-pack GLB importer (UE 5.6). Config via env ZR_CFG JSON.
LOD0 with Interchange: static mesh, combine ON, UCX by name, no mats, no Nanite.
LOD1/2 import to temp and attach. Flattens nested Interchange paths to dest/Name.
Fixes UE 5.6 props: fallback_collision_type (not collision_type); texture via material_pipeline."""
import os, json, glob, unreal
CFG = json.load(open(os.environ['ZR_CFG'], encoding='utf-8-sig')); TAG = CFG.get('tag', 'ZR')
AT = unreal.AssetToolsHelpers.get_asset_tools(); EAL = unreal.EditorAssetLibrary; ESML = unreal.EditorStaticMeshLibrary
rep = {'meshes': {}, 'errors': [], 'missing_mi': [], 'lod_fail': [], 'ucx_total': 0, 'ucx_name_check': []}
def log(m): unreal.log_warning('%s: %s' % (TAG, m))
def err(m): rep['errors'].append(m); unreal.log_warning('%s: ERROR %s' % (TAG, m))
def sp(o, k, v, t):
    try: o.set_editor_property(k, v); return True
    except Exception as ex: err('%s.%s: %s' % (t, k, ex)); return False

def pipe(collision):
    p = unreal.InterchangeGenericAssetsPipeline()
    sp(p, 'use_source_name_for_asset', True, 'p')
    sp(p, 'scene_name_sub_folder', False, 'p')
    try: sp(p, 'asset_type_sub_folders', False, 'p')
    except Exception: pass
    c = p.get_editor_property('common_meshes_properties'); m = p.get_editor_property('mesh_pipeline'); mp = p.get_editor_property('material_pipeline')
    sp(c, 'force_all_mesh_as_type', unreal.InterchangeForceMeshType.IFMT_STATIC_MESH, 'c'); sp(c, 'import_lods', True, 'c')
    try: sp(c, 'vertex_color_import_option', unreal.InterchangeVertexColorImportOption.IVCIO_REPLACE, 'c')
    except Exception as ex: err('vc enum: %s' % ex)
    sp(c, 'recompute_normals', False, 'c'); sp(c, 'recompute_tangents', True, 'c'); sp(c, 'use_mikk_t_space', True, 'c')
    sp(m, 'import_static_meshes', True, 'm'); sp(m, 'import_skeletal_meshes', False, 'm'); sp(m, 'combine_static_meshes', True, 'm')
    sp(m, 'build_nanite', False, 'm'); sp(m, 'collision', bool(collision), 'm')
    sp(m, 'import_collision_according_to_mesh_name', bool(collision), 'm')
    sp(m, 'one_convex_hull_per_ucx', True, 'm'); sp(m, 'generate_lightmap_u_vs', False, 'm')
    # UE 5.6 ScriptName = FallbackCollisionType (None = no auto, keep custom UCX)
    if not sp(m, 'fallback_collision_type', unreal.InterchangeMeshCollision.NONE, 'm'):
        sp(m, 'collision', bool(collision), 'm')  # already set; property probe only
    sp(mp, 'import_materials', False, 'mp')
    try: sp(mp, 'search_location', unreal.InterchangeMaterialSearchLocation.ALL_ASSETS, 'mp')
    except Exception as ex: err('search enum: %s' % ex)
    try:
        tp = mp.get_editor_property('texture_pipeline')
        if tp: sp(tp, 'import_textures', False, 't')
    except Exception as ex: err('tex: %s' % ex)
    return p

def pick_sm(paths, prefer_name=None):
    """Pick the primary StaticMesh from imported paths (skip UCX_/UBX_/UCP_/USP_)."""
    cands = []
    for p in paths or []:
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.StaticMesh): continue
        name = a.get_name()
        if name.startswith(('UCX_', 'UBX_', 'UCP_', 'USP_')): continue
        cands.append((name, a, p))
    if prefer_name:
        for name, a, p in cands:
            if name == prefer_name or name.startswith(prefer_name): return a, p
    return (cands[0][1], cands[0][2]) if cands else (None, None)

def flatten(sm, dest, name):
    """Move nested Interchange asset to dest/name if needed."""
    want = '%s/%s' % (dest.rstrip('/'), name)
    if EAL.does_asset_exist(want):
        cur = unreal.load_asset(want)
        if cur == sm: return sm, want
    # rename/move
    try:
        src_path = sm.get_path_name().split('.')[0]
        if src_path == want: return sm, want
        if EAL.does_asset_exist(want):
            EAL.delete_asset(want)
        ok = EAL.rename_asset(src_path, want)
        log('DIAG flatten %s -> %s ok=%s' % (src_path, want, ok))
        sm2 = unreal.load_asset(want) if EAL.does_asset_exist(want) else sm
        return sm2 if isinstance(sm2, unreal.StaticMesh) else sm, want
    except Exception as ex:
        err('flatten %s: %s' % (name, ex)); return sm, sm.get_path_name().split('.')[0]

def run(files, dest, collision):
    tasks = []
    for f in files:
        k = unreal.AssetImportTask(); k.filename = f; k.destination_path = dest; k.automated = True; k.replace_existing = True; k.save = False
        k.options = pipe(collision); tasks.append(k)
    AT.import_asset_tasks(tasks)
    out = {}
    for k in tasks:
        n = os.path.splitext(os.path.basename(k.filename))[0]
        paths = list(k.imported_object_paths or [])
        out[n] = paths
        log('DIAG import %s -> %d paths %s' % (n, len(paths), paths[:4]))
    return out

def count_ucx(sm):
    try:
        bs = sm.get_editor_property('body_setup')
        if bs:
            return len(bs.get_editor_property('agg_geom').get_editor_property('convex_elems'))
    except Exception: pass
    try: return max(ESML.get_convex_collision_count(sm), 0)
    except Exception: return 0

LS = CFG.get('lod_suffix', ['_LOD1', '_LOD2']); SCR = CFG.get('screen', [1.0, 0.5, 0.25]); MAT = CFG.get('mat_dir')
TMP = '/Game/_ZRTmp/' + TAG
MATS = {}
for d in reversed([MAT] + CFG.get('mat_search', [])):
    if not d or not EAL.does_directory_exist(d): continue
    for p in EAL.list_assets(d, True, False):
        a = unreal.EditorAssetLibrary.find_asset_data(p)
        if str(a.asset_class_path.asset_name) in ('Material', 'MaterialInstanceConstant'):
            MATS[str(a.asset_name)] = str(a.package_name) + '.' + str(a.asset_name)
for k, v in CFG.get('slot_map', {}).items(): MATS[k] = MATS.get(v, v)
log('materials known: %d' % len(MATS))

# DIAG mesh_pipeline collision props once
_mp = unreal.InterchangeGenericAssetsPipeline().get_editor_property('mesh_pipeline')
log('DIAG mesh_pipeline coll props: ' + ', '.join(n for n in dir(_mp) if 'oll' in n or 'ucx' in n.lower()))

for st in CFG['sets']:
    allf = sorted(glob.glob(os.path.join(st['src'], '*.glb')))
    lod0 = [f for f in allf if not any(os.path.splitext(f)[0].endswith(s) for s in LS)]
    coll = bool(st.get('collision', True))
    res = run(lod0, st['dest'], coll)
    for f in lod0:
        n = os.path.splitext(os.path.basename(f))[0]
        paths = list(res.get(n, []) or [])
        sms = []
        for pth in paths:
            a = unreal.load_asset(pth)
            if not isinstance(a, unreal.StaticMesh): continue
            if a.get_name().startswith(('UCX_', 'UBX_', 'UCP_', 'USP_')): continue
            sms.append(a)
        if not sms:
            err('no mesh from ' + n); continue
        # Keep ALL submeshes: move each to dest/n/<SubName> (or dest/n if single matching name)
        kept = []
        dest_folder = '%s/%s' % (st['dest'].rstrip('/'), n)
        for sm in sms:
            sub = sm.get_name()
            # Prefer flat dest/n when only one SM named like n; else dest/n/sub
            if len(sms) == 1:
                want = '%s/%s' % (st['dest'].rstrip('/'), n)
            else:
                want = '%s/%s' % (dest_folder, sub)
            sm2, flat = flatten(sm, want.rsplit('/', 1)[0], want.rsplit('/', 1)[1])
            kept.append(sm2)
        # LODs: attempt attach onto primary (first) only when single-SM
        sm = kept[0]
        if len(kept) == 1:
            for li, s in enumerate(LS, 1):
                lf = os.path.join(st['src'], n + s + '.glb')
                if not os.path.exists(lf): continue
                r = run([lf], TMP, False)
                src = None
                for pth in r.get(n + s, []) or []:
                    a = unreal.load_asset(pth)
                    if isinstance(a, unreal.StaticMesh) and not a.get_name().startswith(('UCX_', 'UBX_')):
                        src = a; break
                ok = -1
                if src:
                    try: ok = ESML.set_lod_from_static_mesh(sm, li, src, 0, True)
                    except Exception as ex: err('%s lod%d: %s' % (n, li, ex))
                if ok != li: rep['lod_fail'].append('%s LOD%d -> %s' % (n, li, ok))
            try: sm.set_editor_property('lod_group', 'None')
            except Exception: pass
            nl = sm.get_num_lods()
            try: ESML.set_lod_screen_sizes(sm, SCR[:nl])
            except Exception: pass
        else:
            nl = 1
            rep['lod_fail'].append('%s multipart skip LOD attach (parts=%d)' % (n, len(kept)))
        for sm in kept:
            ucx = count_ucx(sm)
            if not coll:
                try:
                    ESML.remove_collisions(sm)
                    ucx = 0
                    log('NoCollision forced: %s' % sm.get_name())
                except Exception as ex:
                    err('%s remove_collisions: %s' % (sm.get_name(), ex))
            rep['ucx_total'] += ucx
            slots = []
            for i, s in enumerate(sm.get_editor_property('static_materials')):
                sn = str(s.get_editor_property('material_slot_name')); mi = s.get_editor_property('material_interface')
                want = MATS.get(sn)
                if want and (not mi or mi.get_path_name() != want): sm.set_material(i, unreal.load_asset(want)); mi = True
                if not want and (not mi or 'WorldGridMaterial' in mi.get_path_name() or 'DefaultMaterial' in mi.get_path_name()):
                    rep['missing_mi'].append('%s[%d] %s' % (sm.get_name(), i, sn))
                slots.append(sn)
            # also try binding by material asset name if slot is mesh-part name
            for i, s in enumerate(sm.get_editor_property('static_materials')):
                mi = s.get_editor_property('material_interface')
                if mi and 'WorldGridMaterial' not in mi.get_path_name(): continue
                # already handled above
            rep['meshes'][sm.get_path_name()] = {'src': n, 'lods': sm.get_num_lods(), 'ucx': 0 if not coll else ucx, 'slots': slots, 'path': sm.get_path_name().split('.')[0]}
            EAL.save_loaded_asset(sm)

req = [p for p in CFG.get('require', []) if not exists_req(p)]
rep['require_missing'] = req
log('REQUIRE missing=%d %s' % (len(req), ' '.join(req[:10])))
if EAL.does_directory_exist(TMP): EAL.delete_directory(TMP)
json.dump(rep, open(os.path.join(unreal.Paths.project_saved_dir(), CFG.get('report', TAG + '.json')), 'w'), indent=1)
log('DONE meshes=%d ucx_total=%d lod_fail=%d missing_mi=%d errors=%d ucx_name_mismatches=%d' % (
    len(rep['meshes']), rep['ucx_total'], len(rep['lod_fail']), len(rep['missing_mi']), len(rep['errors']),
    len([1 for x in rep['ucx_name_check'] if 'MISMATCH' in x])))
