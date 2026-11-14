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
    print('b161b ok', len(EDITS))
# b161b (Evan Nov 6 12:57 "only that specific barrier to be see-through so players can still launch and see as normal"): every frame
# MPC_ZoogiCam.Zoogi = (active Zoogi location, 1) so the Ancient materials (Scripts/camera/make_seethrough.py) cut a hole only in the
# part of a prop that sits between the camera and the Zoogi. -ZoogiNoSeeThrough = off.
EDITS.clear()
F = 'Private/ZoogiOcclusionFade.cpp'
ed1(F, '''#include "UObject/UObjectIterator.h"
''', '''#include "UObject/UObjectIterator.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
''')
ed1(F, '''		if (GWorld.Get() != W) { GFaded.Reset(); GCandidates.Reset(); GWorld = W; GRescan = 0.f; }
		AZoogiBall* Me = (G && Enabled() && PC->PlayerCameraManager) ? G->GetActiveBall() : nullptr;
''', '''		if (GWorld.Get() != W) { GFaded.Reset(); GCandidates.Reset(); GWorld = W; GRescan = 0.f; }
		{	// b161: the per-pixel see-through hole (Ancient materials read MPC_ZoogiCam.Zoogi)
			static const bool bNo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSeeThrough"));
			static TWeakObjectPtr<UMaterialParameterCollection> GMpc; static bool bTried = false;
			if (!bTried) { bTried = true; GMpc = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_ZoogiCam.MPC_ZoogiCam"), nullptr, LOAD_NoWarn | LOAD_Quiet); UE_LOG(LogZoogi, Log, TEXT("SEE-THROUGH: MPC_ZoogiCam %s"), GMpc.IsValid() ? TEXT("loaded") : TEXT("missing")); }
			if (UMaterialParameterCollection* Mpc = GMpc.Get())
			{
				AZoogiBall* Act = (!bNo && G && PC->PlayerCameraManager) ? G->GetActiveBall() : nullptr;
				const FVector L = Act ? Act->GetActorLocation() : FVector(0.f, 0.f, -100000.f);
				UKismetMaterialLibrary::SetVectorParameterValue(W, Mpc, TEXT("Zoogi"), FLinearColor(L.X, L.Y, L.Z, Act ? 1.f : 0.f));
			}
		}
		AZoogiBall* Me = (G && Enabled() && PC->PlayerCameraManager) ? G->GetActiveBall() : nullptr;
''')
F2 = 'Private/ZoogiFront.cpp'
ed1(F2, '\t\t\t{ TEXT("B160"),', '''\t\t\t{ TEXT("B161"), TEXT("ANCIENT MEADOWS: CLEAR VIEW"), TEXT("The red-and-yellow drums in the middle of Ancient Collision Meadows are gone, and any prop between the camera and your Zoogi turns see-through right where it blocks you. New Ancient map picture.") },
\t\t\t{ TEXT("B160"),''')
run()
