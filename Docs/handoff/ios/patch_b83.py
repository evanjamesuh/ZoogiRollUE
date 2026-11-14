# b83 (Evan 10:07): GUARD - once per turn; when that turn ends the Zoogi is sealed in a solid barrier until its next turn; the barrier
# cracks more with every PERFECT hit (1) and CRITICAL hit (1.5) and shatters at 6 (3 perfect + 2 critical)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);\n',
  '''	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);
	/** b83 (Evan: GUARD "can only be selected once; once the turn ends it activates and the Zoogi is covered by a solid barrier until
	 *  its next turn; it cracks more and more over three perfect hits and two critical hits"). -ZoogiOldGuard = the card only. */
	ZOOGIROLL_API bool GuardBusy(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void GuardArm(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API void GuardHit(AZoogiGameMode* G, AZoogiBall* Victim, const FVector& From, float Damage, const TCHAR* Why);
'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''	void WolfTick(AZoogiGameMode* G)
	{
		BoltTick(G);''',
  '''	// ---- b83 GUARD barrier
	struct FGuardDome { int32 ArmTurn = -1; bool bUp = false; int32 UpTurn = -1; float Dmg = 0.f; TWeakObjectPtr<AActor> A; TWeakObjectPtr<AZoogiBall> Ball; float OldMass = 0.f, OldLin = 0.f, OldAng = 0.f; };
	static TWeakObjectPtr<const AZoogiGameMode> GGuardG;
	static TMap<int32, FGuardDome> GGuards;
	constexpr float GuardHP = 6.f; // 3 perfect (1 each) + 2 critical (1.5 each)
	static bool GuardOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGuard")); return b; }
	static void GuardReset(const AZoogiGameMode* G) { if (GGuardG.Get() != G) { for (auto& Kv : GGuards) if (AActor* A = Kv.Value.A.Get()) A->Destroy(); GGuards.Reset(); GGuardG = G; } }
	bool GuardBusy(const AZoogiGameMode* G, int32 Player) { if (!GuardOn()) return false; GuardReset(G); return GGuards.Contains(Player); }
	void GuardArm(AZoogiGameMode* G, int32 Player)
	{
		if (!GuardOn() || !G) return;
		GuardReset(G);
		if (GGuards.Contains(Player)) return;
		FGuardDome D; D.ArmTurn = G->TurnCounter;
		GGuards.Add(Player, D);
		if (const AZoogiBall* B = G->GetActiveBall()) G->PublicPopup(TEXT("GUARD SET  -  BARRIER WHEN YOUR TURN ENDS"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.55f, 0.85f, 1.f));
		UE_LOG(LogZoogi, Log, TEXT("GUARD armed P%d (turn %d)"), Player + 1, G->TurnCounter);
	}
	static void GuardCrackDome(FGuardDome& D, const FVector& From, int32 Arms)
	{
		AActor* A = D.A.Get(); AZoogiBall* B = D.Ball.Get();
		if (!A || !B) return;
		const FVector C = B->BallLocation();
		const float Rd = ZoogiConst::ZoogiRadius + 36.f;
		FVector N = (From - C).GetSafeNormal(); if (N.IsNearlyZero()) N = FVector(1.f, 0.f, 0.f);
		N = (N + FVector(0.f, 0.f, 0.25f)).GetSafeNormal();
		FVector Ua = FVector::CrossProduct(N, FVector::UpVector).GetSafeNormal(); if (Ua.IsNearlyZero()) Ua = FVector(0.f, 1.f, 0.f);
		const FVector Va = FVector::CrossProduct(N, Ua).GetSafeNormal();
		UMaterialInterface* M = ZoogiUtil::FxMID(A, FLinearColor(0.9f, 0.97f, 1.f), 14.f);
		UStaticMesh* Cube = ZoogiUtil::BasicShape(TEXT("Cube"));
		auto OnDome = [&](const FVector2D& P2) { return C + (N * Rd + Ua * P2.X + Va * P2.Y).GetSafeNormal() * (Rd + 1.5f); };
		auto Seg = [&](const FVector2D& P0, const FVector2D& P1, float Th)
		{
			const FVector W0 = OnDome(P0), W1 = OnDome(P1), Dd = W1 - W0; const float L = Dd.Size(); if (L < 1.f) return;
			UStaticMeshComponent* Cm = NewObject<UStaticMeshComponent>(A);
			Cm->SetStaticMesh(Cube); Cm->SetCollisionEnabled(ECollisionEnabled::NoCollision); Cm->SetCastShadow(false);
			Cm->SetupAttachment(A->GetRootComponent()); Cm->RegisterComponent(); Cm->SetMaterial(0, M);
			const FVector Nm = ((W0 + W1) * 0.5f - C).GetSafeNormal();
			Cm->SetWorldLocationAndRotation((W0 + W1) * 0.5f, FRotationMatrix::MakeFromXZ(Dd / L, Nm).Rotator());
			Cm->SetWorldScale3D(FVector((L + Th) / 100.f, Th / 100.f, 0.01f));
		};
		for (int32 k = 0; k < Arms; ++k)
		{
			FVector2D At(0.f, 0.f); float Ang = FMath::FRandRange(0.f, UE_TWO_PI), Left = FMath::FRandRange(40.f, 85.f), Th = 3.2f;
			while (Left > 5.f)
			{
				const float St = FMath::Min(Left, FMath::FRandRange(8.f, 16.f));
				Ang += FMath::DegreesToRadians(FMath::FRandRange(-28.f, 28.f));
				const FVector2D Nx = At + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * St;
				Seg(At, Nx, Th); At = Nx; Left -= St; Th = FMath::Max(1.2f, Th * 0.88f);
			}
		}
	}
	static void GuardDown(AZoogiGameMode* G, int32 Player, bool bShatter)
	{
		FGuardDome* D = GGuards.Find(Player);
		if (!D) return;
		if (AZoogiBall* B = D->Ball.Get())
			if (B->Sphere && D->OldMass > 0.f)
			{
				B->Sphere->SetMassOverrideInKg(NAME_None, D->OldMass, true);
				B->Sphere->SetLinearDamping(D->OldLin); B->Sphere->SetAngularDamping(D->OldAng);
			}
		if (AActor* A = D->A.Get())
		{
			const FVector C = A->GetActorLocation();
			if (bShatter)
			{
				UWorld* W = A->GetWorld();
				AZoogiFX::Spawn(W, C, FLinearColor(0.8f, 0.95f, 1.f), 260.f, 0.5f, false, 14.f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_White"), C, FRotator::ZeroRotator, 1.4f);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), C, FRotator::ZeroRotator, 1.0f);
				ZoogiUtil::PlaySfx(G, TEXT("RockCrack"), 1.f, 1.2f);
				G->PublicPopup(TEXT("GUARD SHATTERED!"), C + FVector(0, 0, 170.f), FLinearColor(0.7f, 0.9f, 1.f));
			}
			A->Destroy();
		}
		UE_LOG(LogZoogi, Log, TEXT("GUARD barrier %s for P%d (damage %.1f / %.0f)"), bShatter ? TEXT("SHATTERED") : TEXT("down"), Player + 1, D->Dmg, GuardHP);
		GGuards.Remove(Player);
	}
	void GuardHit(AZoogiGameMode* G, AZoogiBall* Victim, const FVector& From, float Damage, const TCHAR* Why)
	{
		if (!GuardOn() || !G || !Victim) return;
		GuardReset(G);
		FGuardDome* D = GGuards.Find(Victim->PlayerIndex);
		if (!D || !D->bUp) return;
		D->Dmg += Damage;
		UE_LOG(LogZoogi, Log, TEXT("GUARD P%d barrier takes a %s hit: %.1f / %.0f"), Victim->PlayerIndex + 1, Why, D->Dmg, GuardHP);
		if (D->Dmg >= GuardHP - 0.01f) { GuardDown(G, Victim->PlayerIndex, true); return; }
		GuardCrackDome(*D, From, Damage > 1.2f ? 5 : 3);
		ZoogiUtil::PlaySfx(G, TEXT("HitCrack"), 0.9f, 1.1f);
		AZoogiFX::Spawn(G->GetWorld(), Victim->BallLocation(), FLinearColor(0.7f, 0.9f, 1.f), 160.f, 0.3f, false, 10.f);
		if (UMaterialInstanceDynamic* Mi = D->A.IsValid() ? Cast<UMaterialInstanceDynamic>(Cast<AStaticMeshActor>(D->A.Get()) ? Cast<AStaticMeshActor>(D->A.Get())->GetStaticMeshComponent()->GetMaterial(0) : nullptr) : nullptr)
			Mi->SetScalarParameterValue(TEXT("Strength"), 0.35f + 1.2f * (D->Dmg / GuardHP)); // brighter / whiter as it fails
	}
	static void GuardTick(AZoogiGameMode* G)
	{
		if (!GuardOn() || !G) return;
		GuardReset(G);
		TArray<int32> Down;
		for (auto& Kv : GGuards)
		{
			FGuardDome& D = Kv.Value;
			AZoogiBall* B = nullptr; for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex == Kv.Key) B = O;
			if (!B || B->bOut) { Down.Add(Kv.Key); continue; }
			if (!D.bUp && G->TurnCounter > D.ArmTurn && G->ActivePlayer != Kv.Key)
			{	// the guarding Zoogi's turn is over: up goes the barrier
				D.bUp = true; D.UpTurn = G->TurnCounter; D.Ball = B;
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* A = G->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(B->BallLocation()), SP))
				{
					A->SetMobility(EComponentMobility::Movable);
					UStaticMeshComponent* M = A->GetStaticMeshComponent();
					M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
					M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetCastShadow(false);
					M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.45f, 0.75f, 1.f), 0.35f));
					A->SetActorScale3D(FVector(2.f * (ZoogiConst::ZoogiRadius + 36.f) / 100.f));
					D.A = A;
				}
				if (B->Sphere)
				{	// solid: rivals bounce off, the guarded Zoogi does not budge
					D.OldMass = B->Sphere->GetMass(); D.OldLin = B->Sphere->GetLinearDamping(); D.OldAng = B->Sphere->GetAngularDamping();
					B->Sphere->SetMassOverrideInKg(NAME_None, D.OldMass * 40.f, true);
					B->Sphere->SetLinearDamping(8.f); B->Sphere->SetAngularDamping(8.f);
				}
				AZoogiFX::Spawn(G->GetWorld(), B->BallLocation(), FLinearColor(0.55f, 0.85f, 1.f), 220.f, 0.5f, false, 12.f);
				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.7f);
				G->PublicPopup(TEXT("GUARD BARRIER UP"), B->BallLocation() + FVector(0, 0, 170.f), FLinearColor(0.55f, 0.85f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("GUARD barrier up for P%d (turn %d)"), Kv.Key + 1, G->TurnCounter);
			}
			else if (D.bUp)
			{
				if (AActor* A = D.A.Get()) A->SetActorLocation(B->BallLocation());
				if (G->ActivePlayer == Kv.Key && G->TurnCounter > D.UpTurn) Down.Add(Kv.Key); // its next turn: the barrier drops
			}
		}
		for (int32 P : Down) GuardDown(G, P, false);
	}

	void WolfTick(AZoogiGameMode* G)
	{
		BoltTick(G);
		GuardTick(G);'''),
])
ed(P + 'ZoogiP5Command.cpp', [
 ('		case Guard: { const int32 Gs = GuardSlot(G, B); if (Gs >= 0) G->MenuAction(TEXT("CardPlay"), Gs); Dismiss(G); break; }',
  '		case Guard: { const int32 Gs = GuardSlot(G, B); if (Gs >= 0 && !ZoogiSkills::GuardBusy(G, B->PlayerIndex)) { G->MenuAction(TEXT("CardPlay"), Gs); ZoogiSkills::GuardArm(G, B->PlayerIndex); } Dismiss(G); break; } // b83: once, then the barrier'),
 ('		Set(Guard, TEXT("GUARD"), TEXT("Defend yourself"), GuardSlot(G, B) >= 0, 330.f, 5.f, 5.f, 280.f);',
  '		Set(Guard, TEXT("GUARD"), ZoogiSkills::GuardBusy(G, B->PlayerIndex) ? TEXT("Barrier set") : TEXT("Defend yourself"), GuardSlot(G, B) >= 0 && !ZoogiSkills::GuardBusy(G, B->PlayerIndex), 330.f, 5.f, 5.f, 280.f); // b83: one GUARD at a time'),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''	const FVector At = Hit ? Hit->GetActorLocation() : FVector::ZeroVector;
	FPendMul PM; PM.A = bParry ? (AActor*)Def : Hit;''',
  '''	const FVector At = Hit ? Hit->GetActorLocation() : FVector::ZeroVector;
	if (!bParry && Gr == 2) if (AZoogiBall* Gv = Cast<AZoogiBall>(Hit)) if (const AZoogiBall* Sh = G->GetActiveBall()) ZoogiSkills::GuardHit(G, Gv, Sh->BallLocation(), 1.f, TEXT("PERFECT")); // b83
	FPendMul PM; PM.A = bParry ? (AActor*)Def : Hit;'''),
])
ed(P + 'ZoogiAwaken.cpp', [
 ('''	EmitAnime(bLastHitCrit ? TEXT("Critical") : TEXT("PoweredHit"), Shooter, A, OB ? (AActor*)OB : Other, Mid, Mul, true, OB ? OB->PlayerIndex : -1, bRewardShatter);''',
  '''	EmitAnime(bLastHitCrit ? TEXT("Critical") : TEXT("PoweredHit"), Shooter, A, OB ? (AActor*)OB : Other, Mid, Mul, true, OB ? OB->PlayerIndex : -1, bRewardShatter);
	if (bLastHitCrit && OB) ZoogiSkills::GuardHit(this, OB, Shooter->BallLocation(), 1.5f, TEXT("CRITICAL")); // b83: the GUARD barrier cracks'''),
], inc='#include "ZoogiSkills.h"', after='#include "ZoogiGameMode.h"')
print('b83 ok')
