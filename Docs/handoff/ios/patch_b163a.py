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
    print('b163a ok', len(EDITS))
# b163a (Evan Nov 6 13:32 "Finish the sound scape"): ZoogiSoundscape (b163_files) ticks from ActionFun; ZoogiFeel::OnKnockOff adds the
# fall whoosh + the blast under the arena. NEWS B163.
import shutil
EDITS.clear()
FD = os.path.dirname(os.path.abspath(__file__)) + '/b163_files/'
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '''#include "ZoogiFlags.h"
''', '''#include "ZoogiFlags.h"
#include "ZoogiSoundscape.h"
''')
ed1(A, '''	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena
''', '''	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena
	ZoogiSoundscape::Tick(G, RealDt); // b163 (Evan): ambience per map, crowd build-up, final drumroll, miss groan
''')
F = 'Private/ZoogiFeel.cpp'
ed1(F, '''	void OnKnockOff(AZoogiBall* Ball)
	{
		const FTune& T = Tune();''', '''	void OnKnockOff(AZoogiBall* Ball)
	{
		ZoogiSoundscape::OnKnockOff(Ball); // b163: whoosh over the edge + the blast far below
		const FTune& T = Tune();''')
ed1(F, '''#include "HAL/PlatformTime.h"
''', '''#include "HAL/PlatformTime.h"
#include "ZoogiSoundscape.h"
''')
N = 'Private/ZoogiFront.cpp'
ed1(N, '\t\t\t{ TEXT("B162"),', \
'''\t\t\t{ TEXT("B163"), TEXT("ARENA SOUNDSCAPES"), TEXT("Every arena now has its own sound: meadow wind and birds, creaking ice, bubbling lava, the hum of space, a desert market, a chanting tomb, city traffic. The crowd builds up as the match goes on, a drumroll plays in the final moments, a missed shot gets an \\"awww\\" and a Zoogi falling off whooshes down and booms far below.") },
\t\t\t{ TEXT("B162"),''')
run()
if not DRY:
    shutil.copy(FD + 'ZoogiSoundscape.h', R + 'Public/ZoogiSoundscape.h'); shutil.copy(FD + 'ZoogiSoundscape.cpp', R + 'Private/ZoogiSoundscape.cpp'); print('files copied')
