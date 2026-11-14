# b119c (Evan 16:20: "Wolf Pack can be activated while aiming or rolling; remove the slow down; summoned wolves stay with the player
# until used or knocked out"):
#  - casting Wolf Pack summons its orbs onto the field around the player (purple Energy Buff glow, one loop); they follow the player
#  - the WOLF button works while aiming or rolling; no slow-down (PRECISION volleys keep theirs)
#  - each picked target sends the closest waiting wolf; wolves not sent stay with the player (and stay banked) for later
#  - the player knocked out = its waiting wolves vanish
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
WH = 'Public/ZoogiWolfPack.h'; W = 'Private/ZoogiWolfPack.cpp'; C = 'Private/ZoogiClone.cpp'; S = 'Private/ZoogiSkills.cpp'
ed1(WH, '''	bool bFieldPack = false; // b119 (Evan): the pack is on the field from the summon; each picked target sends the closest wolf''',
'''	bool bFieldPack = false; // b119 (Evan): the pack is on the field from the summon; each picked target sends the closest wolf
	/** b119c: wolves waiting with each player (owner -> orbs that follow it until sent or the player is knocked out) */
	TMap<int32, TArray<TWeakObjectPtr<AZoogiClone>>> FieldWolves;
	/** b119c: tops the player's waiting wolves up to Want (Wolf Pack cast) */
	void SummonField(AZoogiGameMode* G, AZoogiBall* B, int32 Want);
	void TickField(AZoogiGameMode* G, float RealDt);''')
# --- SummonField + TickField
ed1(W, '''void UZoogiWolfPackSys::BeginPrecision(AZoogiGameMode* G, AZoogiBall* B, int32 N)''', '''void UZoogiWolfPackSys::SummonField(AZoogiGameMode* G, AZoogiBall* B, int32 Want)
{
	if (!G || !B || B->bOut || !GetWorld()) return;
	TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(B->PlayerIndex);
	F.RemoveAll([](const TWeakObjectPtr<AZoogiClone>& Wk) { return !Wk.IsValid(); });
	const int32 Need = FMath::Clamp(Want, 0, 5) - F.Num();
	const FLinearColor Pu(0.62f, 0.3f, 1.f);
	const FVector L = B->BallLocation();
	for (int32 k = 0; k < Need; ++k)
	{
		const int32 Idx = F.Num();
		const FVector At = L + FRotator(0.f, B->AimYaw + 90.f + 120.f * Idx, 0.f).Vector() * (ZoogiConst::ZoogiRadius + 130.f) + FVector(0, 0, 8.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AZoogiClone* Cl = GetWorld()->SpawnActor<AZoogiClone>(AZoogiClone::StaticClass(), FTransform(FRotator::ZeroRotator, At), SP);
		if (!Cl) continue;
		Cl->InitClone(nullptr, nullptr, B->PlayerIndex, 1750.f, Pu);
		Cl->PackIdx = Idx; Cl->bAlpha = Idx == 0;
		Cl->HitMul = (Cl->bAlpha ? 1.5f : 1.f) * ZoogiSkills::SkillPow();
		Cl->BlastMul = (Cl->bAlpha ? 1.4f : 1.f) * ZoogiSkills::SkillPow();
		if (Cl->bAlpha) Cl->SetBodyScale(1.3f);
		Cl->MakeOrb(); Cl->Hold();
		G->Clones.Add(Cl); F.Add(Cl);
		if (AZoogiFabFX* Ef = AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedToonVFX3/Effects/NS_Energy_buff.NS_Energy_buff"), At - FVector(0, 0, 40.f), Cl->bAlpha ? 0.17f : 0.13f, 3.f, Cl, FVector(0, 0, -40.f)))
		{
			Ef->StopAfter = 1.2f; // one loop
			if (Ef->NC) { Ef->NC->SetVariableLinearColor(FName(TEXT("User.Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("Color")), Pu); Ef->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), Pu); }
		}
		AZoogiFX::Spawn(GetWorld(), At, Pu, 110.f, 0.45f, false, 8.f);
	}
	if (Need > 0) { Howl(1.f, 0.7f); UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d summons %d (now %d waiting)"), B->PlayerIndex + 1, Need, F.Num()); }
}

void UZoogiWolfPackSys::TickField(AZoogiGameMode* G, float RealDt)
{
	for (auto It = FieldWolves.CreateIterator(); It; ++It)
	{
		TArray<TWeakObjectPtr<AZoogiClone>>& F = It.Value();
		F.RemoveAll([](const TWeakObjectPtr<AZoogiClone>& Wk) { return !Wk.IsValid() || !Wk->bWaiting; });
		if (!F.Num()) continue;
		AZoogiBall* B = nullptr;
		for (AZoogiBall* X : G->Balls) if (X && X->PlayerIndex == It.Key()) B = X;
		if (!B || B->bOut || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)
		{	// the player is knocked out (or the match is over): its waiting wolves go
			for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (AZoogiClone* Cw = Wk.Get()) Cw->Pop();
			UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d's %d waiting wolves vanish"), It.Key() + 1, F.Num());
			F.Reset(); continue;
		}
		const FVector L = B->BallLocation();
		const float Spin = GetWorld()->GetRealTimeSeconds() * 40.f;
		for (int32 i = 0; i < F.Num(); ++i)
			if (AZoogiClone* Cw = F[i].Get())
			{	// they circle the player and keep up with it
				const FVector Slot = L + FRotator(0.f, Spin + 360.f * i / F.Num(), 0.f).Vector() * (ZoogiConst::ZoogiRadius + 130.f) + FVector(0.f, 0.f, 10.f * FMath::Sin(Spin * 0.1f + i));
				Cw->SetActorLocation(FMath::VInterpTo(Cw->GetActorLocation(), Slot, RealDt, 7.f));
			}
	}
}

void UZoogiWolfPackSys::BeginPrecision(AZoogiGameMode* G, AZoogiBall* B, int32 N)''')
ed1(W, '''	if (!G || !ZoogiWolf::On()) return;
	ApplyBloodMoon(G);''', '''	if (!G || !ZoogiWolf::On()) return;
	TickField(G, RealDt); // b119c
	ApplyBloodMoon(G);''')
# --- summon: no slow-down for the pack; use the waiting wolves
ed1(W, '''	SetDil(0.2f); // b97 (Evan: "go back to how we had it"): slow-mo, not a pause''', '''	if (bShot) SetDil(0.2f); // b97 slow-mo; b119c (Evan: "remove the slow down"): only a PRECISION volley slows the world''')
ed1(W, '''	if (bBirdPick) { PaintLimit = 30.f; SetDil(0.05f); StartBird();''', '''	if (bBirdPick) { PaintLimit = 30.f; if (bShot) SetDil(0.05f); StartBird();''')
ed1(W, '''	bFieldPack = !bShot && !bLateSpawn; // b119 (Evan): "make the wolves summon on the field with the purple Energy Buff effect"
	if (bFieldPack) SpawnPack();''', '''	bFieldPack = !bShot && !bLateSpawn; // b119 (Evan): "make the wolves summon on the field with the purple Energy Buff effect"
	if (bFieldPack) { SummonField(G, B, NumWolves); NumWolves = FieldWolves.FindOrAdd(Owner).Num(); PaintMax = FMath::Min(PaintMax, FMath::Max(1, NumWolves)); } // b119c: the wolves already waiting with the player''')
# --- pick: the closest waiting wolf of this player
ed1(W, '''		AZoogiClone* Near = nullptr; float NearD = 1e12f;
		for (const TWeakObjectPtr<AZoogiClone>& Wk : Wolves) if (AZoogiClone* Cw = Wk.Get()) if (Cw->bWaiting) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(Best)); if (Dd < NearD) { NearD = Dd; Near = Cw; } }
		if (Near) Near->Strike(Best);''', '''		AZoogiClone* Near = nullptr; float NearD = 1e12f;
		for (const TWeakObjectPtr<AZoogiClone>& Wk : FieldWolves.FindOrAdd(Owner)) if (AZoogiClone* Cw = Wk.Get()) if (Cw->bWaiting) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(Best)); if (Dd < NearD) { NearD = Dd; Near = Cw; } }
		if (Near) { FieldWolves.FindOrAdd(Owner).Remove(Near); Wolves.Add(Near); WolfHits.Add(0); WolfPopped.Add(0); Near->Strike(Best); }''')
# --- release: CPU sends everything; a person keeps the unsent wolves (re-banked for the WOLF button)
ed1(W, '''		TArray<AActor*> Open = Targets(Caster.Get());
		for (const TWeakObjectPtr<AActor>& P : Painted) Open.Remove(P.Get());
		for (const TWeakObjectPtr<AZoogiClone>& Wk : Wolves)
		{
			AZoogiClone* Cw = Wk.Get(); if (!Cw || !Cw->bWaiting) continue;
			TArray<AActor*> Pool = Open.Num() ? Open : Targets(Caster.Get());
			AActor* Pick = nullptr; float PickD = 1e12f;
			for (AActor* A : Pool) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(A)); if (Dd < PickD) { PickD = Dd; Pick = A; } }
			if (Pick) { Open.Remove(Pick); Cw->Strike(Pick); } else Cw->Pop();
		}
		bFieldPack = false;''', '''		TArray<TWeakObjectPtr<AZoogiClone>>& F = FieldWolves.FindOrAdd(Owner);
		if (!bHumanOwner)
		{	// a CPU sends its whole pack at the nearest open targets
			TArray<AActor*> Open = Targets(Caster.Get());
			for (const TWeakObjectPtr<AActor>& P : Painted) Open.Remove(P.Get());
			TArray<TWeakObjectPtr<AZoogiClone>> Send = F; F.Reset();
			for (const TWeakObjectPtr<AZoogiClone>& Wk : Send)
			{
				AZoogiClone* Cw = Wk.Get(); if (!Cw || !Cw->bWaiting) continue;
				TArray<AActor*> Pool = Open.Num() ? Open : Targets(Caster.Get());
				AActor* Pick = nullptr; float PickD = 1e12f;
				for (AActor* A : Pool) { const float Dd = FVector::DistSquared2D(Cw->GetActorLocation(), WolfPos(A)); if (Dd < PickD) { PickD = Dd; Pick = A; } }
				if (Pick) { Open.Remove(Pick); Wolves.Add(Cw); WolfHits.Add(0); WolfPopped.Add(0); Cw->Strike(Pick); } else F.Add(Cw);
			}
		}
		// b119c (Evan: "summoned wolves stay with the player until used"): the rest stay - and stay on the WOLF button
		int32 Kept = 0;
		for (const TWeakObjectPtr<AZoogiClone>& Wk : F) if (Wk.IsValid()) { ZoogiSkills::ArmWolf(G, Owner); ++Kept; }
		if (Kept) UE_LOG(LogZoogi, Log, TEXT("WOLF field: P%d keeps %d wolves waiting"), Owner + 1, Kept);
		bFieldPack = false;''')
ed1(W, '''void UZoogiWolfPackSys::Release()
{
	EndBird();''', '''void UZoogiWolfPackSys::Release()
{
	AZoogiGameMode* G = GM();
	EndBird();''')
# icons: waiting wolves too
ed1(W, '''		for (const TWeakObjectPtr<AZoogiClone>& Wk : Wolves)
		{
			const AZoogiClone* Cl = Wk.Get();
			if (!Cl || Cl->IsHidden() || bShot) continue; // b119: missiles carry no wolf icon''', '''		for (const TWeakObjectPtr<AZoogiClone>& Wk : [this]() { TArray<TWeakObjectPtr<AZoogiClone>> All = Wolves; for (const auto& Kv : FieldWolves) All.Append(Kv.Value); return All; }())
		{
			const AZoogiClone* Cl = Wk.Get();
			if (!Cl || Cl->IsHidden() || (bShot && !Cl->bWaiting)) continue; // b119: missiles carry no wolf icon; b119c: waiting wolves always do''')
# --- clone: a waiting wolf floats (no physics) and is placed by the pack; Strike wakes it
ed1(C, '''	bWaiting = true; FormUntil = 0.f; Age = 0.f;
	if (Sphere) Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);''', '''	bWaiting = true; FormUntil = 0.f; Age = 0.f;
	if (Sphere) { Sphere->SetSimulatePhysics(false); Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); } // b119c: it floats with the player''')
ed1(C, '''	bWaiting = false; Age = 0.f; FormUntil = 0.f;
	AActor* Tg = Target.Get();''', '''	bWaiting = false; Age = 0.f; FormUntil = 0.f;
	if (Sphere) { Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Sphere->SetSimulatePhysics(true); }
	AActor* Tg = Target.Get();''')
ed1(C, '''		Age = 0.f;
		const FVector Vw = Sphere->GetPhysicsLinearVelocity();
		Sphere->SetPhysicsLinearVelocity(FVector(0.f, 0.f, FMath::Min(0.f, Vw.Z)));
		if (MID)''', '''		Age = 0.f;
		if (MID)''')
# --- ZoogiSkills: the WOLF button while aiming too; the cast summons the field wolves
ed1(S, '''		if (!(B && !B->bOut && G->Phase == EZoogiPhase::Rolling && WolfArmed(G, B->PlayerIndex))) return false;''', '''		const bool bAimNow = G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle; // b119c (Evan): "activated while aiming or rolling"
		if (!(B && !B->bOut && (G->Phase == EZoogiPhase::Rolling || bAimNow) && WolfArmed(G, B->PlayerIndex))) return false;''')
ed1(S, '''			for (int32 k = 0; k < Give; ++k) ArmWolf(G, Pi);''', '''			for (int32 k = 0; k < Give; ++k) ArmWolf(G, Pi);
			if (UZoogiWolfPackSys* WPf = UZoogiWolfPackSys::Get(G)) WPf->SummonField(G, Caster, WolfCount(G, Pi)); // b119c (Evan): the wolves come onto the field now and stay with the player''')
print('b119c ok')
