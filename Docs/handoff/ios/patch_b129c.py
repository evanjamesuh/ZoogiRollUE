# b129c (Evan 03:03, Cosmic rune marker screenshot: "make this stage prop appear purple and arise without smoke and only start to appear
# on round 2. Every one of those that spawn increase in size the next round in order they summoned"):
#  - purple (lit purple surface + a soft purple glow), no smoke, purple rise ring (was red)
#  - the first marker rises at the start of round 2 (none in round 1)
#  - every round each marker already standing grows +20% (cap 2x), one after another in the order they rose, then the new one rises
#  -ZoogiOldCosmicMarkers = the b89 markers.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
F = 'Private/ZoogiCosmicMarkers.cpp'
ed1(F, '''		struct FMarker { TWeakObjectPtr<AActor> A; FVector Up = FVector::ZeroVector; float Drop = 0.f, T = 0.f, Dur = 1.6f; };''',
'''		struct FMarker { TWeakObjectPtr<AActor> A; FVector Up = FVector::ZeroVector; float Drop = 0.f, T = 0.f, Dur = 1.6f;
			float BaseSc = 1.f, Grow = 1.f, GrowFrom = 1.f, GrowTo = 1.f, GrowT = -1.f, GrowDelay = 0.f; }; // b129: grows each round
		const FLinearColor MarkerPurple(0.55f, 0.2f, 1.f);
		bool Old() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldCosmicMarkers")); return b; }
		// b129: sit the (scaled) marker's base on its floor spot
		void Seat(FMarker& M, AActor* A, const FVector& At)
		{
			A->SetActorLocation(At);
			if (UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(A->GetRootComponent())) { const float MinZ = Sm->Bounds.GetBox().Min.Z; A->SetActorLocation(At + FVector(0, 0, M.Up.Z - MinZ)); }
		}''')
ed1(F, '''			for (FMarker& M : Markers)
			{
				AActor* A = M.A.Get(); if (!A || M.T >= M.Dur) continue;''', '''			for (FMarker& M : Markers)
			{
				AActor* A = M.A.Get();
				if (A && M.GrowT >= 0.f)
				{	// b129: the round's growth, one marker after another
					bAny = true;
					if (M.GrowDelay > 0.f) { M.GrowDelay -= Dt; continue; }
					M.GrowT = FMath::Min(M.GrowT + Dt, 0.8f);
					const float K = M.GrowT / 0.8f, E = 1.f - FMath::Pow(1.f - K, 3.f);
					M.Grow = FMath::Lerp(M.GrowFrom, M.GrowTo, E);
					if (UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(A->GetRootComponent())) Sm->SetRelativeScale3D(FVector(M.BaseSc * M.Grow));
					Seat(M, A, FVector(M.Up.X, M.Up.Y, A->GetActorLocation().Z));
					if (M.GrowT >= 0.8f) { M.GrowT = -1.f; AZoogiFX::Spawn(A->GetWorld(), M.Up + FVector(0, 0, 8.f), MarkerPurple, 220.f * M.Grow, 0.5f, true, 8.f); }
					continue;
				}
				if (!A || M.T >= M.Dur) continue;''')
ed1(F, '''				if (M.T >= M.Dur) AZoogiFX::Spawn(A->GetWorld(), M.Up + FVector(0, 0, 8.f), FLinearColor(1.f, 0.25f, 0.15f), 260.f, 0.6f, true, 8.f);''',
'''				if (M.T >= M.Dur) AZoogiFX::Spawn(A->GetWorld(), M.Up + FVector(0, 0, 8.f), Old() ? FLinearColor(1.f, 0.25f, 0.15f) : MarkerPurple, 260.f, 0.6f, true, 8.f);''')
ed1(F, '''			if (Gm->RoundNumber <= 1 && Gm->Phase != EZoogiPhase::Menu) OnRoundStart(Gm);''', '''			if (Old() && Gm->RoundNumber <= 1 && Gm->Phase != EZoogiPhase::Menu) OnRoundStart(Gm); // b129: none in round 1 (the first rises with round 2)''')
ed1(F, '''		if (Off() || !G || !G->Arena || G->Arena->GetTheme() != EZoogiArena::Cosmic || G->bRoyaleMode) return;
		UWorld* W = G->GetWorld(); if (!W) return;
		Markers.RemoveAll([](const FMarker& M) { return !M.A.IsValid(); });''', '''		if (Off() || !G || !G->Arena || G->Arena->GetTheme() != EZoogiArena::Cosmic || G->bRoyaleMode) return;
		if (!Old() && G->RoundNumber < 2) return; // b129 (Evan): "only start to appear on round 2"
		UWorld* W = G->GetWorld(); if (!W) return;
		Markers.RemoveAll([](const FMarker& M) { return !M.A.IsValid(); });
		float NewDelay = 0.f;
		if (!Old())
		{	// b129 (Evan: "every one of those that spawn increase in size the next round in order they summoned"): +20% each (max 2x)
			for (FMarker& M : Markers)
			{
				if (M.Grow >= 2.f) continue;
				M.GrowFrom = M.Grow; M.GrowTo = FMath::Min(2.f, M.Grow * 1.2f); M.GrowT = 0.f; M.GrowDelay = NewDelay; NewDelay += 0.55f;
			}
			if (NewDelay > 0.f && !MarkerTick.IsValid()) MarkerTick = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float Dt) { return Step(Dt); }));
			if (NewDelay > 0.f) UE_LOG(LogZoogi, Log, TEXT("COSMIC MARKER: %d marker(s) grow for round %d, in the order they rose"), Markers.Num(), G->RoundNumber);
		}''')
ed1(F, '''		M->SetRelativeScale3D(FVector(Sc));''', '''		M->SetRelativeScale3D(FVector(Sc));
		if (!Old()) for (int32 s = 0; s < M->GetNumMaterials(); ++s) M->SetMaterial(s, ZoogiUtil::SurfaceMID(A, MarkerPurple, 0.35f, MarkerPurple, 1.2f, 0.25f)); // b129: purple''')
ed1(F, '''		FMarker Mk; Mk.A = A; Mk.Up = Spot; Mk.Drop = MarkerH + 20.f;''', '''		FMarker Mk; Mk.A = A; Mk.Up = Spot; Mk.Drop = MarkerH + 20.f; Mk.BaseSc = Sc;
		if (!Old()) { Mk.T = -NewDelay; } // b129: the new marker rises after the old ones have grown''')
ed1(F, '''		AZoogiFX::Spawn(W, Spot + FVector(0, 0, 4.f), FLinearColor(1.f, 0.2f, 0.1f), 320.f, 1.2f, true, 10.f);
		AZoogiFX::Stylized(W, TEXT("Smoke_v2"), Spot, 0.6f);''', '''		AZoogiFX::Spawn(W, Spot + FVector(0, 0, 4.f), Old() ? FLinearColor(1.f, 0.2f, 0.1f) : MarkerPurple, 320.f, 1.2f, true, 10.f);
		if (Old()) AZoogiFX::Stylized(W, TEXT("Smoke_v2"), Spot, 0.6f); // b129: no smoke''')
print('b129c ok')
