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
    print('b160a ok', len(EDITS))
# b160a (Evan Nov 6 12:25 "z flag on every map"): the b158 flags flew above the game camera (9.5 m poles) and only 2 of them, so
# they were mostly off screen. Now FOUR Zoogi flags round every arena (yaw 45/135/225/315), 6.2 m poles, a bigger 5 x 3.2 m flag
# hung lower, brighter, so one is in view from any angle.
EDITS.clear()
F = 'Private/ZoogiFlags.cpp'
ed1(F, 'constexpr float FW = 420.f, FH = 270.f, PoleH = 950.f;', 'constexpr float FW = 500.f, FH = 320.f, PoleH = 620.f; // b160: lower + bigger (was 420 x 270 on 950)')
ed1(F, "M->SetScalarParameterValue(TEXT(\"EmissiveStrength\"), 0.35f);", "M->SetScalarParameterValue(TEXT(\"EmissiveStrength\"), 0.6f); // b160: brighter")
ed1(F, '''			for (int32 k = 0; k < 2; ++k)
			{
				const float Yaw = 35.f + 180.f * k;''', '''			for (int32 k = 0; k < 4; ++k) // b160: four flags (was 2)
			{
				const float Yaw = 45.f + 90.f * k;''')
ed1(F, '''				AStaticMeshActor* Pole = Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Base + FVector(0, 0, PoleH * 0.5f), FVector(0.22f, 0.22f, PoleH / 100.f), Gold);''', '''				AStaticMeshActor* Pole = Shape(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Base + FVector(0, 0, PoleH * 0.5f), FVector(0.26f, 0.26f, PoleH / 100.f), Gold);''')
ed1(F, "FFlag F; F.Pole = Pole; F.Cloth = PM; F.Phase = k * 1.9f;", "FFlag F; F.Pole = Pole; F.Cloth = PM; F.Phase = k * 1.7f;")
run()
