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
    print('b161a ok', len(EDITS))
# b161a (Evan Nov 6 12:57 "for this particular stage those center pillars, yellow and red remove"): Ancient Collision Meadows uses
# the plaza props without the ring of red/yellow drums (SM_Arena_PlazaPropsTrick) in every mode. -ZoogiAncientDrums = the old props.
EDITS.clear()
F = 'Private/ZoogiArenaAncientV14.cpp'
ed1(F, '''		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/AncientV14/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
''', '''		UStaticMesh* M = nullptr;
		if (FCString::Strcmp(P.Name, TEXT("SM_Arena_PlazaProps")) == 0 && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAncientDrums"))) // b161: no centre drums
			M = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaPropsTrick.SM_Arena_PlazaPropsTrick"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Env/AncientV14/%s.%s"), P.Name, P.Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
''')
ed1(F, '''#include "Materials/MaterialInterface.h"
''', '''#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
''')
run()
