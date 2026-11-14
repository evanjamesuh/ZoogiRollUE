# b156d (Evan Nov 6 07:38 "Battle royal arena map starts with ancient meadows, ... the zoogi king sit on the throne which would be
# me but as a 3d model I create for myself"): the Battle Royale lap starts on ANCIENT MEADOWS (a classic round on the plaza; the
# Ancient -> Meadow redirect only applies outside Battle Royale) + the ZOOGI KING throne on the Sky Citadel (ZoogiThrone).
import os, sys, re, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
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
    here = os.path.dirname(os.path.abspath(__file__))
    shutil.copy(here + '/b156_files/ZoogiThrone.h', R + 'Public/ZoogiThrone.h')
    shutil.copy(here + '/b156_files/ZoogiThrone.cpp', R + 'Private/ZoogiThrone.cpp')
    print('b156d ok', len(EDITS))

X = 'Private/ZoogiRoyaleMix.cpp'
ed1(X, 'static const EZoogiArena Classic[8] = { EZoogiArena::Meadow, EZoogiArena::Frozen, EZoogiArena::Volcanic, EZoogiArena::Cosmic, EZoogiArena::Arabian, EZoogiArena::Tomb, EZoogiArena::Neon, EZoogiArena::Teaser };',
       'static const EZoogiArena Classic[8] = { EZoogiArena::Ancient, EZoogiArena::Meadow, EZoogiArena::Frozen, EZoogiArena::Volcanic, EZoogiArena::Cosmic, EZoogiArena::Arabian, EZoogiArena::Tomb, EZoogiArena::Neon }; return Classic[S % 8]; } // b153: classic-only lap; b156 (Evan): the lap opens on ANCIENT MEADOWS (the Zoogi King\'s throne)')
G = 'Private/ZoogiGameMode.cpp'
ed1(G, "if (!FParse::Param(Cl, TEXT(\"ZoogiAncientClassic\")) && !FParse::Param(Cl, TEXT(\"ZoogiAncientTest\")) && !FParse::Param(Cl, TEXT(\"ZoogiArenaShots\")))",
       "if (!ZoogiRoyaleMix::Active() && !FParse::Param(Cl, TEXT(\"ZoogiAncientClassic\")) && !FParse::Param(Cl, TEXT(\"ZoogiAncientTest\")) && !FParse::Param(Cl, TEXT(\"ZoogiArenaShots\"))) // b156: Battle Royale's opening round is on Ancient")
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '#include "ZoogiTrickShot.h"', '#include "ZoogiTrickShot.h"\n#include "ZoogiThrone.h"')
ed1(A, '	ZoogiTrickShot::Tick(G, RealDt); // b156: DAILY TRICK SHOT', '	ZoogiTrickShot::Tick(G, RealDt); // b156: DAILY TRICK SHOT\n	ZoogiThrone::Tick(G); // b156: the Zoogi King\'s throne on Ancient Meadows')
run()
