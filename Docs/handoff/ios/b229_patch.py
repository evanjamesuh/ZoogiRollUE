import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(rel, pairs):
    p = os.path.join(S, rel)
    t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        n = t.count(a)
        if n != 1: print('FAIL', rel, n, a[:90]); sys.exit(1)
        t = t.replace(a, b)
    io.open(p, 'w', encoding='utf-8', newline='').write(t)
    print('ok', rel)

# ---- signs: plain components (M_CosmicPlanet has no ISM usage flag -> default black material)
patch('Public/ZoogiFrozenFun.h', [
('	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ToonISMs;',
 '	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ToonISMs;\n	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SignComps; // b229: signs are plain meshes (the sign material has no ISM usage)'),
])
patch('Private/ZoogiFrozenFun.cpp', [
('''	NSign = 0;
	for (int32 s = 0; s < 8 && SignBase; ++s)''',
'''	NSign = 0; TArray<UMaterialInstanceDynamic*> SignMats; // b229
	for (int32 s = 0; s < 8 && SignBase; ++s)'''),
('''		NewISM(TEXT("Plane"), Mi); ++NSign;''',
'''		SignMats.Add(Mi); ++NSign;'''),
('''			M.BoardI = ToonISMs[NCol + 3 + M.Sign]->AddInstance(Z, true);''',
'''			UStaticMeshComponent* SignC = NewObject<UStaticMeshComponent>(this); // b229: a plain mesh per sign
			SignC->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Plane"))); SignC->SetMobility(EComponentMobility::Movable); SignC->SetupAttachment(Root);
			SignC->SetCollisionEnabled(ECollisionEnabled::NoCollision); SignC->SetCastShadow(false); SignC->SetCanEverAffectNavigation(false); SignC->SetGenerateOverlapEvents(false);
			SignC->RegisterComponent(); SignC->SetMaterial(0, SignMats[M.Sign]); SignC->SetWorldTransform(Z);
			M.BoardI = SignComps.Add(SignC);'''),
('''			PutX(NCol + 3 + M.Sign, M.BoardI, FVector(12.f, 30.f, 62.f + Lift), FVector(0.92f, 0.46f, 1.f), Bq);''',
'''			if (SignComps.IsValidIndex(M.BoardI) && SignComps[M.BoardI]) SignComps[M.BoardI]->SetWorldTransform(FTransform(Bq, At(FVector(12.f, 30.f, 62.f + Lift)), FVector(0.92f, 0.46f, 1.f) * Sc)); // b229'''),
('''void AZoogiFrozenCrowd::TickToon(float Dt)
{
	Clock3 += Dt;''',
'''void AZoogiFrozenCrowd::TickToon(float Dt)
{
	Clock3 += Dt;
	static const bool bCrowdCam = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrowdCam")); // b229 preview: a close camera on a sign-holding fan, cheering
	if (bCrowdCam)
	{
		static TWeakObjectPtr<ACameraActor> CCam; static int32 CIx = -1;
		if (CIx < 0) for (int32 i = 0; i < Members.Num(); ++i) if (Members[i].Sign >= 0 && Members[i].Base.Y < 0.f) { CIx = i; break; }
		if (CIx < 0) for (int32 i = 0; i < Members.Num(); ++i) if (Members[i].Sign >= 0) { CIx = i; break; }
		if (Members.IsValidIndex(CIx))
		{
			const FVector Tg = Members[CIx].Base + FVector(0.f, 0.f, 70.f);
			const FVector In = (FVector(Look.X, Look.Y, Tg.Z) - Tg).GetSafeNormal();
			const FVector Cp = Tg + In * 700.f + FVector(0.f, 0.f, 120.f);
			if (!CCam.IsValid()) { FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; CCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator::ZeroRotator, Cp), Sp); }
			if (ACameraActor* Ca = CCam.Get())
			{
				Ca->SetActorLocationAndRotation(Cp, (Tg - Cp).Rotation());
				if (APlayerController* Pc = GetWorld()->GetFirstPlayerController()) if (Pc->GetViewTarget() != Ca) Pc->SetViewTarget(Ca);
			}
			Cheer = FMath::Max(Cheer, 0.5f + 0.5f * FMath::Sin(Clock3 * 1.3f));
		}
	}'''),
])

# ---- ORB RELAY: MaxLeft before the copy; preview hook aims at orbs, stops at 0, no restart spam once the ball is out
patch('Private/ZoogiSkills.cpp', [
('''			DashRuns.Add(R);
			R.MaxLeft = R.Left;''',
'''			R.MaxLeft = R.Left; // b229: set before the copy goes in (refunds capped at the real count)
			DashRuns.Add(R);'''),
('''				if (bDashTest && Tn >= R.NextCpu) { R.NextCpu = Tn + 0.7; R.Tap = Db->BallLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 500.f;''',
'''				if (bDashTest && R.Left > 0 && Tn >= R.NextCpu)
				{	// b229: toward the nearest orb (shows the refund), else a random spot
					const AZoogiOrb* No = nullptr; float Nd = 1e9f;
					for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float Dd = FVector::Dist2D(O->OrbLocation(), Db->BallLocation()); if (Dd < Nd) { Nd = Dd; No = O; } }
					R.NextCpu = Tn + 0.7; R.Tap = No ? No->OrbLocation() : Db->BallLocation() + FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * 500.f;'''),
('''AZoogiFX::Spawn(W, R.Tap, GatGold, 90.f, 0.35f, true, 8.f); DashOnce(G, R, R.Tap); continue; }''',
'''AZoogiFX::Spawn(W, R.Tap, GatGold, 90.f, 0.35f, true, 8.f); DashOnce(G, R, R.Tap); continue;
				}
				if (bDashTest) continue;'''),
('''				if (bDashT && Hb && !Hb->bCPU && G->Phase''',
'''				static TWeakObjectPtr<AZoogiBall> DashTDone; static int32 DashTRound = -1; // b229: once per roll (no restart spam)
				if (bDashT && Hb && !Hb->bCPU && !Hb->bOut && (DashTDone.Get() != Hb || DashTRound != G->RoundNumber) && G->Phase'''),
('''return X.B.Get() == Hb; })) DashStart(G, Hb);''',
'''return X.B.Get() == Hb; })) { DashStart(G, Hb); DashTDone = Hb; DashTRound = G->RoundNumber; }'''),
])

# ---- chest items: seed the starting counts first (a chest before the first match was ignored) + log
patch('Private/ZoogiFrontProfile.cpp', [
('''		TMap<FName, int32> GotI; int32 Full = 0;''',
'''		TMap<FName, int32> GotI; int32 Full = 0;
		if (!D.bItemsSeeded)
		{	// b229: the starting items (as ZoogiItems::Ensure) before the chest adds to them
			for (const TCHAR* K : Its) { const FName Kn(K); int32& Cn = D.ItemCounts.FindOrAdd(Kn); Cn = FMath::Max(Cn, (Kn == FName(TEXT("sword")) || Kn == FName(TEXT("staff")) || Kn == FName(TEXT("apple"))) ? 3 : 5); }
			D.bItemsSeeded = true; D.ItemCounts.FindOrAdd(FName(TEXT("seed_v2"))) = 1; D.ItemCounts.FindOrAdd(FName(TEXT("seed_v3"))) = 1;
		}'''),
('''		for (const TPair<FName, int32>& P : GotI) Out.Add({ FName(TEXT("item")), P.Key, P.Value });''',
'''		for (const TPair<FName, int32>& P : GotI) { Out.Add({ FName(TEXT("item")), P.Key, P.Value }); UE_LOG(LogZoogi, Log, TEXT("CHEST item: %s +%d (now %d)"), *P.Key.ToString(), P.Value, D.ItemCounts.FindRef(P.Key)); }
		if (Full > 0) UE_LOG(LogZoogi, Log, TEXT("CHEST item: %d full -> %d gold"), Full, 25 * Full);'''),
])

# ---- ITEM tab pill: solid gold (option A mockup), dark number
patch('Private/ZoogiFront.cpp', [
('''			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f));
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), FLinearColor(0.06f, 0.05f, 0.16f, 1.f));
			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? Paper : FLinearColor(0.6f, 0.6f, 0.65f));''',
'''			ZRect(Out, G, L + 6, Pt - FVector2D(3.f, 3.f), FVector2D(Pw + 6.f, Ph + 6.f), FLinearColor(0.06f, 0.05f, 0.16f, 1.f)); // b229: dark rim
			ZRect(Out, G, L + 7, Pt, FVector2D(Pw, Ph), I.Count > 0 ? FLinearColor(1.f, 0.77f, 0.24f, 1.f) : FLinearColor(0.45f, 0.45f, 0.5f, 1.f)); // b229: solid gold pill (option A)
			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? FLinearColor(0.06f, 0.05f, 0.16f, 1.f) : FLinearColor(0.8f, 0.8f, 0.85f));'''),
('''			{ TEXT("B228"),''',
'''			{ TEXT("B229"), TEXT("CROWD SIGNS FIXED"), TEXT("The fans' signs now show their words (they were black boards). The item count is a solid gold pill. Chest items now add to your starting items instead of being lost. ORB RELAY refunds are capped at your real dash count.") },
			{ TEXT("B228"),'''),
])
print('ALL OK')
