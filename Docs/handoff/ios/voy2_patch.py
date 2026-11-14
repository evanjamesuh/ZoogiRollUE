# Nov 11 (Claude): Voyager port step 2 - permanent: plugins, config merge, CommonUI viewport, SHOOTER cards -> Voyager levels
import io, os, sys, json, shutil, re
D = os.path.expanduser('~/mnt/dev/'); Z = D + 'ZoogiRollUnreal-p5style/'; S = Z + 'Source/ZoogiRoll/'; V = D + 'ios/voy2/'; VY = D + 'Voyager/'
def rd(p): return io.open(p, encoding='utf-8-sig').read()
def wr(p, t): io.open(p, 'w', encoding='utf-8', newline='').write(t)
def rep(t, a, b, what):
    if t.count(a) != 1: print('FAIL', what, t.count(a), a[:100]); sys.exit(1)
    return t.replace(a, b)
for f in ('Config/DefaultEngine.ini', 'Config/DefaultGameplayTags.ini', 'ZoogiRoll.uproject'):
    if not os.path.exists(Z + f + '.pre_voy2'): shutil.copy(Z + f, Z + f + '.pre_voy2')
# 1. plugins
up = json.loads(rd(Z + 'ZoogiRoll.uproject.pre_voy2')); vp = json.loads(rd(VY + 'VoyagerThirdPersonShooter.uproject'))
have = {p['Name'] for p in up['Plugins']}
for p in vp['Plugins']:
    if p['Name'] not in have: up['Plugins'].append(p)
wr(Z + 'ZoogiRoll.uproject', json.dumps(up, indent='\t') + '\n'); print('plugins', len(up['Plugins']))
# 2. engine ini
ve = rd(VY + 'Config/DefaultEngine.ini').replace('\r\n', '\n'); ze = rd(Z + 'Config/DefaultEngine.ini.pre_voy2').replace('\r\n', '\n')
def section(txt, name):
    m = re.search(r'^\[' + re.escape(name) + r'\]\n(.*?)(?=^\[|\Z)', txt, re.S | re.M); return m.group(1) if m else ''
coll = section(ve, '/Script/Engine.CollisionProfile'); cv = section(ve, '/Script/Engine.DataDrivenConsoleVariableSettings')
surf = '\n'.join(l for l in section(ve, '/Script/Engine.PhysicsSettings').split('\n') if l.startswith('+PhysicalSurfaces'))
assert coll.count('Profiles') > 10 and surf.count('+PhysicalSurfaces') == 4 and 'CVarsArray' in cv
ze = rep(ze, 'RestitutionCombineMode=Average\n', 'RestitutionCombineMode=Average\n; Nov 11 voyager port: Voyager impact surfaces\n' + surf + '\n', 'surf')
ze = rep(ze, '[/Script/Engine.RendererSettings]\n', '[/Script/Engine.RendererSettings]\n; Nov 11 voyager port (Voyager characters / outlines)\nr.CustomDepth=3\nr.GPUSkin.Support16BitBoneIndex=True\nr.GPUSkin.UnlimitedBoneInfluences=True\nr.SkinCache.CompileShaders=True\nSkeletalMesh.UseExperimentalChunking=1\n', 'rend')
ze = ze.rstrip('\n') + '\n\n; Nov 11 voyager port: Voyager collision channels + profiles (Zoogi had none)\n[/Script/Engine.CollisionProfile]\n' + coll.rstrip('\n') + '\n\n[/Script/Engine.DataDrivenConsoleVariableSettings]\n' + cv.rstrip('\n') + '\n\n[/Script/AIModule.CrowdManager]\nMaxAgents=100\n'
wr(Z + 'Config/DefaultEngine.ini', ze); print('engine ini ok')
# 3. gameplay tags
zt = rd(Z + 'Config/DefaultGameplayTags.ini.pre_voy2').replace('\r\n', '\n')
tags = [l for l in rd(VY + 'Config/DefaultGameplayTags.ini').replace('\r\n', '\n').split('\n') if l.startswith('+GameplayTagList') and l not in zt]
wr(Z + 'Config/DefaultGameplayTags.ini', zt.rstrip('\n') + '\n; Nov 11 voyager port\n' + '\n'.join(tags) + '\n'); print('tags', len(tags))
# 4. source
shutil.copy(V + 'ZoogiVoyagerLevels.h', S + 'Public/ZoogiVoyagerLevels.h'); shutil.copy(V + 'ZoogiVoyagerLevels.cpp', S + 'Private/ZoogiVoyagerLevels.cpp')
t = rd(S + 'ZoogiRoll.Build.cs'); t = rep(t, '"WebBrowser", "WebBrowserWidget"', '"WebBrowser", "WebBrowserWidget", "CommonUI", "CommonInput", "EnhancedInput"', 'build'); wr(S + 'ZoogiRoll.Build.cs', t)
t = rd(S + 'Public/ZoogiViewportClient.h')
t = rep(t, '#include "Engine/GameViewportClient.h"', '#include "Engine/GameViewportClient.h"\n#include "CommonGameViewportClient.h" // Nov 11 voyager port: Voyager\'s CommonUI menus need the CommonUI viewport', 'vp1')
t = rep(t, 'class ZOOGIROLL_API UZoogiViewportClient : public UGameViewportClient', 'class ZOOGIROLL_API UZoogiViewportClient : public UCommonGameViewportClient', 'vp2'); wr(S + 'Public/ZoogiViewportClient.h', t)
t = rd(S + 'Private/ZoogiModeSelect.cpp')
t = rep(t, '''	if (Id == ZoogiModeSel::ECardId::ShooterHold || Id == ZoogiModeSel::ECardId::ShooterRoyale || Id == ZoogiModeSel::ECardId::ShooterTeam)
	{''', '''	if ((Id == ZoogiModeSel::ECardId::ShooterHold || Id == ZoogiModeSel::ECardId::ShooterRoyale || Id == ZoogiModeSel::ECardId::ShooterTeam)
		&& ZoogiVoyLevels::Open(G, Id == ZoogiModeSel::ECardId::ShooterHold ? 1 : Id == ZoogiModeSel::ECardId::ShooterRoyale ? 2 : 3)) return; // Nov 11 (Evan): the REAL Voyager game
	if (Id == ZoogiModeSel::ECardId::ShooterHold || Id == ZoogiModeSel::ECardId::ShooterRoyale || Id == ZoogiModeSel::ECardId::ShooterTeam)
	{''', 'ms')
k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "ZoogiVoyagerLevels.h" // voyager port\n' + t[k:]
wr(S + 'Private/ZoogiModeSelect.cpp', t); print('ALL OK')
