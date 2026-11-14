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
    import shutil
    here = os.path.dirname(os.path.abspath(__file__))
    shutil.copy(here + '/b157_files/ZoogiFlags.h', R + 'Public/ZoogiFlags.h')
    shutil.copy(here + '/b157_files/ZoogiFlags.cpp', R + 'Private/ZoogiFlags.cpp')
    print('b157b ok', len(EDITS))
# b157b (Evan Nov 6):
#  08:25 "for trick shot build a version of ancient meadows without the most centralize bumpers" / 08:26 "leave enough to do bank
#   trick shots": in a Daily Trick Shot on Ancient Meadows the plaza props mesh is swapped for SM_Arena_PlazaPropsTrick (the ring
#   of 10 drums around the Z removed; curb ring, hazard drums, bollards, banner posts stay to bank off). Swapped back afterwards.
#  08:28 "you can bank off the map boss never phase threw it": the titan gets solid colliders on its hands, forearms, upper arms
#   and head while it is up on the rim (off while it hides, sweeps or sinks). -ZoogiBossNoCollide = old.
T = 'Private/ZoogiTrickShot.cpp'
ed1(T, '#include "Engine/Canvas.h"', '#include "Engine/Canvas.h"\n#include "Engine/StaticMesh.h"\n#include "Components/StaticMeshComponent.h"')
ed1(T, '''		void End(AZoogiGameMode* G)
		{''', '''		bool GPropsSwapped = false;
		void SwapProps(AZoogiGameMode* G, bool bTrick)
		{	// b157 (Evan): Ancient Meadows without the central drum ring for the trick shot
			if (!G || !G->Arena) return;
			static UStaticMesh* Orig = nullptr; static UStaticMesh* Trick = nullptr;
			if (!Orig) { Orig = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaProps.SM_Arena_PlazaProps"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (Orig) Orig->AddToRoot(); }
			if (!Trick) { Trick = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Env/AncientV14/SM_Arena_PlazaPropsTrick.SM_Arena_PlazaPropsTrick"), nullptr, LOAD_NoWarn | LOAD_Quiet); if (Trick) Trick->AddToRoot(); }
			if (!Orig || !Trick) { if (bTrick) UE_LOG(LogZoogi, Warning, TEXT("TRICK SHOT: plaza props swap unavailable (orig %d, trick %d)"), Orig ? 1 : 0, Trick ? 1 : 0); GPropsSwapped = bTrick; return; }
			TInlineComponentArray<UStaticMeshComponent*> Cs(G->Arena);
			int32 N = 0;
			for (UStaticMeshComponent* C : Cs)
			{
				if (!C) continue;
				if (bTrick && C->GetStaticMesh() == Orig) { C->SetStaticMesh(Trick); ++N; }
				else if (!bTrick && C->GetStaticMesh() == Trick) { C->SetStaticMesh(Orig); ++N; }
			}
			GPropsSwapped = bTrick;
			if (N) UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT: Ancient plaza props -> %s"), bTrick ? TEXT("no central drums (bank posts stay)") : TEXT("original"));
		}
		void End(AZoogiGameMode* G)
		{
			if (GPropsSwapped) SwapProps(G, false);''')
ed1(T, '		if (G->Phase == EZoogiPhase::Menu) { End(G); return; }', '''		if (G->Phase == EZoogiPhase::Menu) { End(G); return; }
		if (!GPropsSwapped && G->Arena && G->Arena->GetTheme() == EZoogiArena::Ancient && !S.bPlaced) SwapProps(G, true); // before the layout is placed''')
C = 'Private/ZoogiCosmosBoss.cpp'
ed1(C, '#include "Components/StaticMeshComponent.h"', '#include "Components/StaticMeshComponent.h"\n#include "Components/SphereComponent.h"')
ed1(C, '		float GCameo = 0.f; bool GWasShown = false; // b155a: a short appearance after a KO / combo', '''		float GCameo = 0.f; bool GWasShown = false; // b155a: a short appearance after a KO / combo
		TArray<TWeakObjectPtr<USphereComponent>> GCol; // b157: the titan is solid (bank off it)
		void BossCollision(AZoogiAurexPuppet* P, bool bOn)
		{	// b157 (Evan: "you can bank off the map boss never phase threw it")
			static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossNoCollide"));
			if (!P || !P->Body) return;
			struct FB { const TCHAR* Bone; float R; };
			static const FB Bs[] = { { TEXT("hand_l"), 130.f }, { TEXT("hand_r"), 130.f }, { TEXT("lowerarm_l"), 105.f }, { TEXT("lowerarm_r"), 105.f },
				{ TEXT("upperarm_l"), 120.f }, { TEXT("upperarm_r"), 120.f }, { TEXT("head"), 210.f } };
			if (GCol.Num() != (int32)UE_ARRAY_COUNT(Bs) || !GCol[0].IsValid() || GCol[0]->GetOwner() != P)
			{
				for (TWeakObjectPtr<USphereComponent>& Old : GCol) if (USphereComponent* Os = Old.Get()) Os->DestroyComponent();
				GCol.Reset();
				for (const FB& Bb : Bs)
				{
					USphereComponent* Sc = NewObject<USphereComponent>(P);
					Sc->SetSphereRadius(Bb.R);
					Sc->SetCollisionObjectType(ECC_WorldDynamic);
					Sc->SetCollisionResponseToAllChannels(ECR_Block);
					Sc->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
					Sc->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
					Sc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Sc->SetHiddenInGame(true);
					Sc->RegisterComponent();
					GCol.Add(Sc);
				}
				UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: %d solid colliders (hands, arms, head)"), GCol.Num());
			}
			const bool bWant = bOn && !bOff;
			for (int32 i = 0; i < GCol.Num(); ++i)
			{
				USphereComponent* Sc = GCol[i].Get(); if (!Sc) continue;
				if (bWant) Sc->SetWorldLocation(P->Body->GetBoneLocationByName(Bs[i].Bone, EBoneSpaces::WorldSpace), false, nullptr, ETeleportType::TeleportPhysics);
				const ECollisionEnabled::Type Want = bWant ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
				if (Sc->GetCollisionEnabled() != Want) Sc->SetCollisionEnabled(Want);
			}
		}''')
ed1(C, '			P->LookAt = Look;', '			P->LookAt = Look;\n			BossCollision(P, !GDefeated && !GSweepOn && B.Dip < 0.45f); // b157: solid while it is up on the rim')
F = 'Private/ZoogiFront.cpp'
ed1(F, '			{ TEXT("B156"),', '			{ TEXT("B157"), TEXT("BANK OFF THE TITAN + TRICK SHOT PLAZA"), TEXT("The map titan is solid now: its hands, arms and head stop Zoogis and orbs - bank shots off it (it never lets anything pass through). Daily Trick Shot on Ancient Meadows: the ring of drums around the centre is gone so shots run clean across the plaza; the curb ring, hazard drums and lantern posts stay for bank shots. Every arena now flies two purple-and-gold Z flags waving in the wind. The ON FIRE circles are thinner.") },\n			{ TEXT("B156"),')
#  08:29 "every map has the iconic purple and gold Z zoogi flag or banner waving": ZoogiFlags (2 waving flags per arena).
#  08:30 "thin the circle particle on player on fire effect": the ON FIRE bursts are smaller and half as frequent; the awakened ring
#   is fainter.
A = 'Private/ZoogiActionFun.cpp'
ed1(A, '#include "ZoogiThrone.h"', '#include "ZoogiThrone.h"\n#include "ZoogiFlags.h"')
ed1(A, '	ZoogiThrone::Tick(G); // b156', '	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena\n	ZoogiThrone::Tick(G); // b156')
ed1(A, 'AZoogiFX::Niagara(W, FMath::RandBool() ? TEXT("NS_Burst_Red") : TEXT("NS_Burst_Yellow"), L + FVector(0, 0, 30.f), FRotator::ZeroRotator, 0.32f);',
       'if (FMath::RandBool()) AZoogiFX::Niagara(W, FMath::RandBool() ? TEXT("NS_Burst_Red") : TEXT("NS_Burst_Yellow"), L + FVector(0, 0, 30.f), FRotator::ZeroRotator, 0.2f); // b157 (Evan: "thin the circle particle"): smaller, half as often')
ed1(A, '150.f, 0.4f, true, 8.f, B); // awakened aura', '150.f, 0.4f, true, 3.5f, B); // awakened aura (b157: fainter ring)')

#  b156 test fixes: a turn that ends before the trick shot placed its layout (the command-line start) is not scored as a try;
#  the coin packs read COINS ("STACK OF COINS"), not GOLD.
ed1(T, '''		if (G->TurnCounter == S.EvalTurn) return true;
		S.EvalTurn = G->TurnCounter;''', '''		if (G->TurnCounter == S.EvalTurn) return true;
		S.EvalTurn = G->TurnCounter;
		if (S.Try == 0) { S.bWaiting = true; S.WaitT = 0.4f; S.bDone = false; UE_LOG(LogZoogi, Log, TEXT("TRICK SHOT: a turn ended before the layout - placing it now")); return true; } // b157''')
ed1(F, 'const FString Nm0 = FString(Gp[i].Name).ToUpper();', 'const FString Nm0 = FString(Gp[i].Name).ToUpper();') if False else None
ed1(F, 'ZTextC(Out, G, L + 6, FString(Gp[i].Name).ToUpper(), ZFitFont(FString(Gp[i].Name).ToUpper(), 0, 26.f, 230.f)', 'ZTextC(Out, G, L + 6, FString(Gp[i].Name).ToUpper().Replace(TEXT("GOLD"), TEXT("COINS")), ZFitFont(FString(Gp[i].Name).ToUpper().Replace(TEXT("GOLD"), TEXT("COINS")), 0, 26.f, 230.f)')

run()
# chained (Evan 08:46): the trick shot becomes a start-up pop-up - patch_b157c applies right after this one
if not DRY:
    import subprocess as _sp
    _r = _sp.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'patch_b157c.py')], capture_output=True, text=True)
    print('b157c:', _r.stdout.strip(), _r.stderr.strip()[-800:])
