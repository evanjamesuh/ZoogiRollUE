import io, os, sys, shutil
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
D = os.path.expanduser('~/mnt/dev/ios/cine')
shutil.copy(os.path.join(D, 'ZoogiWallCine.h'), os.path.join(S, 'Public', 'ZoogiWallCine.h'))
shutil.copy(os.path.join(D, 'ZoogiWallCine.cpp'), os.path.join(S, 'Private', 'ZoogiWallCine.cpp'))
def patch(rel, pairs):
    p = os.path.join(S, rel); t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        n = t.count(a)
        if n != 1: print('FAIL', rel, n, a[:90]); sys.exit(1)
        t = t.replace(a, b)
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)
patch('Private/ZoogiActionFun.cpp', [
('	ZoogiDemoReel::Tick(G, RealDt); // b213: the previs reel (-ZoogiDemoReel only)',
 '	ZoogiDemoReel::Tick(G, RealDt); // b213: the previs reel (-ZoogiDemoReel only)\n	ZoogiWallCine::Tick(G, RealDt); // b230: the wall-smash cinematic'),
('	if (ZoogiLoadScreen::Holding()) return; // b207: nothing over the loading screen',
 '	if (ZoogiLoadScreen::Holding()) return; // b207: nothing over the loading screen\n	if (ZoogiWallCine::Active()) { ZoogiWallCine::DrawHud(C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunText(C, Ts, Tx, Ty, Tp, Tc); }); return; } // b230'),
])
patch('Private/ZoogiHUD.cpp', [
('	if (ZoogiLoadScreen::Holding()) { UpdateAwakenWidget(G, true); return; } // b207: the loading screen covers the match',
 '	if (ZoogiLoadScreen::Holding()) { UpdateAwakenWidget(G, true); return; } // b207: the loading screen covers the match\n	if (ZoogiWallCine::Active()) { UpdateAwakenWidget(G, true); return; } // b230: the wall-smash cinematic plays clean'),
])
patch('Private/ZoogiPlayerController.cpp', [
('!ZoogiDemoReel::Holding()) UpdateHumanTest(Dt);', '!ZoogiDemoReel::Holding() && !ZoogiWallCine::Active()) UpdateHumanTest(Dt);'),
])
patch('Private/ZoogiFrozenFun.cpp', [
('if (SignSpin < -999.f) { SignSpin = 0.f;', 'if (SignSpin < -999.f) { SignSpin = 180.f; /* b230: the B229 signs read upside down */'),
])
patch('Private/ZoogiFront.cpp', [
("			{ TEXT(\"B229\"),", "			{ TEXT(\"B230\"), TEXT(\"SIGNS THE RIGHT WAY UP\"), TEXT(\"The fans' signs now read the right way up. Behind the scenes: the wall-smash cinematic (a Baller punches a rival back into a rock and the FIRE BLAST bursts out of it) is built - pick when it plays and it goes live.\") },\n			{ TEXT(\"B229\"),"),
])
for rel in ['Private/ZoogiActionFun.cpp', 'Private/ZoogiHUD.cpp', 'Private/ZoogiPlayerController.cpp']:
    p = os.path.join(S, rel); t = io.open(p, encoding='utf-8-sig').read()
    if '#include "ZoogiWallCine.h"' not in t:
        i = t.index('#include', t.index('#include') + 1)
        t = t[:i] + '#include "ZoogiWallCine.h" // b230\n' + t[i:]
        io.open(p, 'w', encoding='utf-8', newline='').write(t); print('inc', rel)
print('ALL OK')
