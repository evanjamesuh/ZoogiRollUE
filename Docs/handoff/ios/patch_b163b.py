import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b163b ok', len(EDITS))
# b163b (Evan Nov 6 13:34/13:35): the Training arena is the LEAGUE TRAINING COURT (b163_files/ZoogiArenaLeagueCourt.cpp). No titan in the
# story's practice spar. NEWS.
import shutil
EDITS.clear()
FD = os.path.dirname(os.path.abspath(__file__)) + '/b163_files/'
H = 'Public/ZoogiArena.h'
ed1(H, '''	void BuildAncientV14();''', '''	void BuildAncientV14();
	bool BuildLeagueCourt(); // b163: the Training arena = the League Training Court (ZoogiArenaLeagueCourt.cpp; -ZoogiOldTraining = old)
	bool bLeagueCourt = false;''')
A = 'Private/ZoogiArena.cpp'
ed1(A, '''	Theme = InTheme;
	ActiveOrbGlow = OrbFloorGlowFor(InTheme);''', '''	Theme = InTheme;
	bLeagueCourt = false; // b163
	ActiveOrbGlow = OrbFloorGlowFor(InTheme);''')
ed1(A, '''	{ extern void ZoogiRealismApply(AZoogiArena*, EZoogiArena); ZoogiRealismApply(this, InTheme); }''', '''	if (!bLeagueCourt) { extern void ZoogiRealismApply(AZoogiArena*, EZoogiArena); ZoogiRealismApply(this, InTheme); } // b163: the league court is finished art''')
ed1(A, '''	if (InTheme != EZoogiArena::Meadow) PlaceIncoming();''', '''	if (bLeagueCourt) { StandBlockR = GetKnockoffRadius() + 260.f; UE_LOG(LogZoogi, Log, TEXT("LEAGUE COURT: out past r %.0f (bleachers)"), StandBlockR); } // b163: a Zoogi / orb in the stands is out
	if (InTheme != EZoogiArena::Meadow && !bLeagueCourt) PlaceIncoming(); // b163: no old builder pack on the league court''')
I = 'Private/ZoogiArenaIncoming.cpp'
ed1(I, '''void AZoogiArena::BuildTraining()
{''', '''void AZoogiArena::BuildTraining()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldTraining")) && BuildLeagueCourt()) return; // b163 (Evan): the League Training Court''')
B = 'Private/ZoogiCosmosBoss.cpp'
ed1(B, '''			if (A == EZoogiArena::Ancient || ZoogiTrickShot::Active()) return false; // b156: no titan in the trick shot''', '''			if (A == EZoogiArena::Ancient || ZoogiTrickShot::Active()) return false; // b156: no titan in the trick shot
			if (A == EZoogiArena::Training && ZoogiStory::InMission()) return false; // b163: the EP1 practice spar is just two friends''')
ed1(B, '''#include "Components/SphereComponent.h"
''', '''#include "Components/SphereComponent.h"
#include "ZoogiStory.h"
''')
N = 'Private/ZoogiFront.cpp'
ed1(N, '\t\t\t{ TEXT("B163"),', \
'''\t\t\t{ TEXT("B163"), TEXT("LEAGUE TRAINING COURT"), TEXT("The practice field is now the Zoogi League Training Court: the Ancient Meadows ring at dawn with bleachers, a PRACTICE scoreboard, light towers, banners and pink target pads. Episode 1's practice spar has no titan any more.") },
\t\t\t{ TEXT("B163"),''')
run()
if not DRY:
    shutil.copy(FD + 'ZoogiArenaLeagueCourt.cpp', R + 'Private/ZoogiArenaLeagueCourt.cpp'); print('court file copied')
