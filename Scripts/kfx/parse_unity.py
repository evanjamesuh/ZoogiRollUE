# Parses KriptoFX Unity prefabs (YAML) + .meta GUIDs into spec.json (run on any machine with PyYAML; R = copy of the pack's Effects folder with .prefab/.mat/.meta).
import yaml,os,re,json,glob
R='src'
guid={}
for m in glob.glob(R+'/**/*.meta',recursive=True):
    g=re.search(r'guid: (\w+)',open(m,errors='ignore').read())
    if g: guid[g.group(1)]=m[len(R)+1:-5]
def load(p):
    t=open(p,errors='ignore').read()
    docs={}
    for chunk in re.split(r'^--- ',t,flags=re.M)[1:]:
        h,_,body=chunk.partition('\n')
        mm=re.match(r'!u!(\d+) &(-?\d+)',h)
        if not mm: continue
        try: d=yaml.load(body,Loader=yaml.CLoader)
        except Exception as e: continue
        k=list(d.keys())[0]; docs[int(mm.group(2))]=(int(mm.group(1)),k,d[k])
    return docs
def mmc(c):
    if not isinstance(c,dict): return c
    s=c.get('minMaxState',0); sc=c.get('scalar',0)
    if s==0: return sc
    if s==3: return [c.get('minScalar',0),sc]
    def cv(n): return [k['value']*sc for k in c.get(n,{}).get('m_Curve',[])]
    return {'curve':cv('maxCurve'),'min':cv('minCurve') if s==2 else None}
def col(c):
    s=c.get('minMaxState',0)
    f=lambda x:[round(x['r'],3),round(x['g'],3),round(x['b'],3),round(x['a'],3)]
    if s==0: return f(c['maxColor'])
    if s==2: return [f(c['minColor']),f(c['maxColor'])]
    return grad(c['maxGradient'])
def grad(g):
    nc=g.get('m_NumColorKeys',2); na=g.get('m_NumAlphaKeys',2)
    return {'c':[[round(g['ctime%d'%i]/65535,3),round(g['key%d'%i]['r'],3),round(g['key%d'%i]['g'],3),round(g['key%d'%i]['b'],3)] for i in range(nc)],
            'a':[[round(g['atime%d'%i]/65535,3),round(g['key%d'%i]['a'],3)] for i in range(na)]}
matcache={}
def items(x):
    if isinstance(x,dict): return list(x.items())
    o=[]
    for e in x:
        if 'first' in e: o.append((e['first']['name'],e['second']))
        else: o+=list(e.items())
    return o
def mat(g):
    if not g or g not in guid: return None
    p=guid[g]
    if g in matcache: return matcache[g]
    r={'path':p}
    if p.endswith('.mat'):
        d=list(load(R+'/'+p).values())[0][2]
        sh=d.get('m_Shader',{}).get('guid'); r['shader']=guid.get(sh,sh)
        r['keywords']=d.get('m_ShaderKeywords','')
        sp=d['m_SavedProperties']; r['tex']={}
        for n,v in items(sp.get('m_TexEnvs',[])):
            tg=v.get('m_Texture',{}).get('guid')
            if tg: r['tex'][n]=guid.get(tg,tg); 
            if tg and (v['m_Scale']!={'x':1,'y':1}): r['tex'][n+'_scale']=v['m_Scale']
        r['colors']={}
        for n,v in items(sp.get('m_Colors',[])):
            if n in('_TintColor','_Color','_EmissionColor','_MainColor','_FresnelColor','_RimColor'): r['colors'][n]=[round(v[x],3) for x in 'rgba']
        r['floats']={}
        for n,v in items(sp.get('m_Floats',[])):
            if any(s in n for s in('Blend','Mode','Intens','Boost','Speed','Distort','Cutout','InvFade','Fresnel')): r['floats'][n]=v
    matcache[g]=r; return r
def parse(p):
    D=load(p); out={'prefab':p[len(R)+1:],'emitters':[],'meshes':[],'audio':[],'lights':[],'scripts':[]}
    tr={};go={}
    for i,(c,k,d) in D.items():
        if c==4: tr[i]=d
        if c==1: go[i]=d
    def world(t):
        pos=[0,0,0];chain=[]
        while t in tr: chain.append(tr[t]); t=tr[t]['m_Father']['fileID']
        sc=1
        for x in chain:
            pos=[pos[0]+x['m_LocalPosition']['x'],pos[1]+x['m_LocalPosition']['y'],pos[2]+x['m_LocalPosition']['z']]
        return [round(v,3) for v in pos]
    def comp(g,cls):
        for c in go[g]['m_Component']:
            fid=list(c.values())[0]['fileID']
            if fid in D and D[fid][0]==cls: return fid,D[fid][2]
        return None,None
    def gname(d): return go.get(d['m_GameObject']['fileID'],{}).get('m_Name')
    def gpath(g):
        tid,t=comp(g,4); n=[]
        while t: 
            n.append(go[t['m_GameObject']['fileID']]['m_Name']); f=t['m_Father']['fileID']; t=tr.get(f)
        return '/'.join(reversed(n))
    allg=set(re.findall(r'guid: (\w{32})',open(p,errors='ignore').read()))
    out['all_refs']=sorted(set(guid[g] for g in allg if g in guid))
    for i,(c,k,d) in D.items():
        g=d.get('m_GameObject',{}).get('fileID') if isinstance(d,dict) else None
        if c==198:
            tid,t=comp(g,4); ini=d['InitialModule']; em=d['EmissionModule']; sh=d['ShapeModule']
            rid,rr=comp(g,199)
            e={'name':gpath(g),'active':go[g].get('m_IsActive'),'pos':world(tid),'rot':t['m_LocalRotation'],'scale':t['m_LocalScale'],
               'duration':d.get('lengthInSec'),'loop':d.get('looping'),'delay':mmc(d.get('startDelay')),
               'lifetime':mmc(ini['startLifetime']),'speed':mmc(ini['startSpeed']),'size':mmc(ini['startSize']),
               'size3D':ini.get('size3D'),'sizeY':mmc(ini.get('startSizeY')) if ini.get('size3D') else None,
               'rotation':mmc(ini.get('startRotation')),'color':col(ini['startColor']),'gravity':mmc(ini.get('gravityModifier')),'maxParticles':ini.get('maxNumParticles'),
               'sim_space':d.get('moveWithTransform'),
               'rate':mmc(em.get('rateOverTime',em.get('rate'))),'rateDist':mmc(em.get('rateOverDistance')),
               'bursts':[{'t':b.get('time'),'n':mmc(b.get('countCurve')) if 'countCurve' in b else [b.get('minCount'),b.get('maxCount')]} for b in em.get('m_Bursts',[])],
               'shape':{'enabled':sh['enabled'],'type':sh['type'],'angle':sh['angle'],'radius':sh['radius']['value'] if isinstance(sh['radius'],dict) else sh['radius'],'arc':sh.get('arc',{}).get('value') if isinstance(sh.get('arc'),dict) else sh.get('arc'),'boxThickness':sh.get('boxThickness'),'scale':sh.get('m_Scale'),'rot':sh.get('m_Rotation'),'mesh':guid.get((sh.get('m_Mesh') or {}).get('guid'))},
            }
            for mod,key in [('ColorModule','gradient'),]:
                m=d.get(mod,{})
                if m.get('enabled'): e['colorOverLife']=col(m[key])
            for mod in ['SizeModule','VelocityModule','ForceModule','RotationModule','NoiseModule','UVModule','CollisionModule','SubModule','TrailModule','LightsModule','ClampVelocityModule']:
                m=d.get(mod)
                if m and m.get('enabled'):
                    if mod=='SizeModule': e['sizeOverLife']=mmc(m['curve'])
                    elif mod=='VelocityModule': e['velOverLife']=[mmc(m['x']),mmc(m['y']),mmc(m['z'])]; e['velSpace']=m.get('inWorldSpace')
                    elif mod=='ForceModule': e['force']=[mmc(m['x']),mmc(m['y']),mmc(m['z'])]
                    elif mod=='RotationModule': e['rotOverLife']=mmc(m['curve'])
                    elif mod=='NoiseModule': e['noise']={'str':mmc(m['strength']),'freq':m.get('frequency')}
                    elif mod=='UVModule': e['flipbook']=[m['tilesX'],m['tilesY'],m.get('animationType'),mmc(m.get('cycles'))]
                    elif mod=='ClampVelocityModule': e['drag']=mmc(m.get('dampen'))
                    else: e.setdefault('mods',[]).append(mod)
            if rr:
                ms=[mat(x.get('guid')) for x in rr.get('m_Materials',[])]
                e['render']={'mode':rr.get('m_RenderMode'),'enabled':rr.get('m_Enabled'),'lengthScale':rr.get('m_LengthScale'),'velScale':rr.get('m_VelocityScale'),'mesh':guid.get((rr.get('m_Mesh') or {}).get('guid')),'mats':ms,'sortFudge':rr.get('m_SortingFudge')}
            out['emitters'].append(e)
        elif c in(23,137):
            tid,t=comp(g,4)
            mesh=None
            if c==137: mesh=d.get('m_Mesh')
            else:
                _,mf=comp(g,33); mesh=mf and mf.get('m_Mesh')
            out['meshes'].append({'name':gpath(g),'skinned':c==137,'enabled':d.get('m_Enabled'),'active':go[g].get('m_IsActive'),'pos':world(tid),'scale':t['m_LocalScale'],'mesh':guid.get((mesh or {}).get('guid')),'mesh_fid':(mesh or {}).get('fileID'),'mats':[mat(x.get('guid')) for x in d.get('m_Materials',[])]})
        elif c==82:
            out['audio'].append({'name':gname(d),'clip':guid.get((d.get('m_audioClip') or {}).get('guid')),'vol':d.get('m_Volume'),'loop':d.get('Loop')})
        elif c==108:
            out['lights'].append({'name':gname(d),'color':d.get('m_Color'),'intensity':d.get('m_Intensity'),'range':d.get('m_Range')})
        elif c==114:
            s=guid.get((d.get('m_Script') or {}).get('guid'))
            dd={k:v for k,v in d.items() if not k.startswith('m_') or k in()}
            out['scripts'].append({'go':gname(d),'script':s,'fields':json.loads(json.dumps(dd,default=str))})
    return out
want=['3','4','5','6','7','11','17','19','22']
res={}
for n in want:
    ps=glob.glob(R+'/Prefabs/**/Effect%s.prefab'%n,recursive=True)+glob.glob(R+'/Prefabs/**/Effect%s_*.prefab'%n,recursive=True)
    res['Effect'+n]=[parse(p) for p in ps]
json.dump(res,open('spec.json','w'),indent=1,default=str)
for k,v in res.items():
    print(k,[ (x['prefab'].split('/')[-1],len(x['emitters']),len(x['meshes']),len(x['audio'])) for x in v])
