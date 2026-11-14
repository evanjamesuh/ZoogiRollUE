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
    print('b164r ok', len(EDITS))
# b164r (Evan Nov 6 14:55 "put ancient meadows back the way it was" -> picked "Normal size, no centre drums"): the v14 arena (r17.5 m ring)
# is the default again; the b162 twice-as-big ring stays available with -ZoogiAncientV15. Centre drums stay removed, flags + see-through stay.
EDITS.clear()
L = 'Private/ZoogiAncientLayout.h'
ed1(L, '''	inline bool V15() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV14")); return b; }''', '''	inline bool V15() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientV15")); return b; } // b164 (Evan): back to v14 by default''')
N = 'Private/ZoogiFront.cpp'
ed1(N, '\t\t\t{ TEXT("B163"), TEXT("ANIME DIRECTOR (PC)")', \
'''\t\t\t{ TEXT("B164"), TEXT("ANCIENT MEADOWS: BACK TO NORMAL SIZE"), TEXT("Ancient Collision Meadows is back to its original size. The red-and-yellow centre drums stay gone, and the Z flags and see-through props stay.") },
\t\t\t{ TEXT("B163"), TEXT("ANIME DIRECTOR (PC)")''')
run()
