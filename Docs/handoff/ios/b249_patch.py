# Nov 11 (Claude): B249 - Evan 11:29 "You have enough tripo models to start with start with the characters":
# the Voyager hero becomes the crystal Zoogi (ult_frost_blue / ult_ember_red / ult_fox_gold_A / _B, picked from the equipped Zoogi),
# runtime-retargeted from Voyager's hidden UE4 hero. -ZoogiVoyNoCrystal = old Tripo-head dress. -ZoogiVoyHeroRig=<rig>, -ZoogiVoyHeroYaw=deg.
import io, os, sys, shutil, json
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
D = os.path.expanduser('~/mnt/dev/ios/b249/')
shutil.copy(D + 'ZoogiVoyHero.h', S + 'Public/ZoogiVoyHero.h'); shutil.copy(D + 'ZoogiVoyHero.cpp', S + 'Private/ZoogiVoyHero.cpp')
def patch(rel, pairs, incs=()):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    for inc in incs:
        if '#include "%s"' % inc not in t:
            k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b249\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Private/ZoogiVoyagerLevels.cpp', [(
'''			ZoogiVoyager::DressAs(Ch->GetMesh(), H.Name, 0, D ? D->Color : FLinearColor(0.2f, 0.6f, 1.f), D ? D->Secondary : FLinearColor::White, false);''',
'''			if (!ZoogiVoyHero::Apply(Ch, H.Name)) // b249: the crystal Zoogi hero
				ZoogiVoyager::DressAs(Ch->GetMesh(), H.Name, 0, D ? D->Color : FLinearColor(0.2f, 0.6f, 1.f), D ? D->Secondary : FLinearColor::White, false);''')],
	incs=('ZoogiVoyHero.h',))
patch('ZoogiRoll.Build.cs', [('"CommonUI", "CommonInput", "EnhancedInput"', '"CommonUI", "CommonInput", "EnhancedInput", "IKRig"')])
up = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/ZoogiRoll.uproject'); u = json.load(io.open(up, encoding='utf-8-sig'))
names = [p['Name'] for p in u['Plugins']]
for n in ('IKRig', 'ControlRig'):
    if n not in names: u['Plugins'].append({'Name': n, 'Enabled': True})
io.open(up, 'w', encoding='utf-8', newline='').write(json.dumps(u, indent='\t')); print('ok uproject')
f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B248"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B249"), TEXT("CRYSTAL ZOOGI HEROES"), TEXT("In Voyager mode you now play as a crystal Zoogi hero (blue frost, red ember or gold fox, picked from your equipped Zoogi) with every Voyager move: run, cover, aim, shoot, roll.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f); print('ALL OK')
