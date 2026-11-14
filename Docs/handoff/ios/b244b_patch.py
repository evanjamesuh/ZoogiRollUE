# Nov 11 (Claude): B244 part 2 - Evan 08:58 "zoogi and orbs still phase through walls and earth bending effect walls, fix it so the intended
# blocks actually work". Log proof: skill solids were removed the same frame they were made ("solid ... gone") - the Niagara rock wall is not
# active yet on its first frame, and the solid died with an inactive effect. Now a skill / court rock stays solid for its whole life
# (min 4.5 s), only an effect that is destroyed or truly finished (after 2 s) ends it early; it arms at 0.25 s (was 0.45).
import io, os, sys
p = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiEarthEvents.cpp'); t = io.open(p, encoding='utf-8-sig').read()
def rep(a, b):
    global t
    if t.count(a) != 1: print('FAIL', t.count(a), a[:100]); sys.exit(1)
    t = t.replace(a, b)
rep('double ArmAt = 0.0, Until = 0.0; bool bArmed = false, bHadFx = false; };', 'double ArmAt = 0.0, Until = 0.0, MadeAt = 0.0; bool bArmed = false, bHadFx = false; };')
rep('''				bool bAlive = A && Tn < Sd.Until && !(Sd.bHadFx && !Fx); // its effect was destroyed
				if (bAlive && Fx && Fx->FreezeAt <= 0.f && (!Fx->NC || !Fx->NC->IsActive())) bAlive = false; // the effect ended''',
'''				bool bAlive = A && Tn < Sd.Until;
				const double Age = Tn - Sd.MadeAt;
				static const bool bOldLife = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSolidOldLife"));
				if (bAlive && Age > (bOldLife ? 0.0 : 4.5) && Sd.bHadFx && !Fx) bAlive = false; // b244: its effect was destroyed (not before 4.5 s)
				if (bAlive && Age > (bOldLife ? 0.0 : 4.5) && Fx && Fx->FreezeAt <= 0.f && (!Fx->NC || !Fx->NC->IsActive())) bAlive = false; // b244: the effect ended (was checked on frame 1: walls died at once)''')
rep('		Sd.ArmAt = FPlatformTime::Seconds() + 0.45; Sd.Until = FPlatformTime::Seconds() + Life;',
    '		Sd.MadeAt = FPlatformTime::Seconds(); Sd.ArmAt = Sd.MadeAt + 0.25; Sd.Until = Sd.MadeAt + FMath::Max(Life, 4.5f); // b244: arms faster, lives its whole time')
io.open(p, 'w', encoding='utf-8', newline='').write(t)
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiFront.cpp'); f = io.open(S, encoding='utf-8-sig').read()
a = 'Watch them - then do it yourself: launch, then cast while you still have momentum.'
assert f.count(a) == 1
f = f.replace(a, a + ' FIX: earth-bending walls and rocks are solid now - Zoogis and orbs bounce off them instead of passing through (they used to switch off on their first frame).')
io.open(S, 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
