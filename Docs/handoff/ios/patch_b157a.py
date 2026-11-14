# b157a (Evan Nov 6 07:38 "Battle royal arena map starts with ancient meadows, ... the zoogi king sit on the throne which would be
# me but as a 3d model I create for myself"): the Battle Royale lap starts on ANCIENT MEADOWS (a classic round on the plaza; the
# Ancient -> Meadow redirect only applies outside Battle Royale) + the ZOOGI KING throne on the Sky Citadel (ZoogiThrone).
import os, sys, re, shutil
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
    here = os.path.dirname(os.path.abspath(__file__))
# b157a (Evan Nov 6 08:03 "Replace meadows with ancient meadows for trick shot"): the Daily Trick Shot rotation uses ANCIENT MEADOWS
# (the plaza) instead of the Meadow; the Ancient -> Meadow redirect is skipped for the trick shot too.
EDITS.clear()
T = 'Private/ZoogiTrickShot.cpp'
ed1(T, 'static const EZoogiArena A[] = { EZoogiArena::Meadow, EZoogiArena::Volcanic,', 'static const EZoogiArena A[] = { EZoogiArena::Ancient, EZoogiArena::Volcanic, // b157 (Evan): Ancient Meadows replaces the Meadow\n\t\t\t ')
G = 'Private/ZoogiGameMode.cpp'
ed1(G, 'if (!ZoogiRoyaleMix::Active() && !FParse::Param(Cl, TEXT("ZoogiAncientClassic"))', 'if (!ZoogiRoyaleMix::Active() && !ZoogiTrickShot::Active() && !FParse::Param(Cl, TEXT("ZoogiAncientClassic"))')
F = 'Private/ZoogiFront.cpp'
ed1(F, '			{ TEXT("B156"),', '			{ TEXT("B157"), TEXT("TRICK SHOT ON ANCIENT MEADOWS"), TEXT("The Daily Trick Shot now rotates Ancient Meadows (the plaza under the Zoogi King\'s throne), Volcano, Arabian Nights, Tomb and Neon - no more plain Meadow.") },\n			{ TEXT("B156"),')
run()
