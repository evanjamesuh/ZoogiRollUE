# b80 (Claude; Evan via Grok, notes_for_claude/boltballs.txt): BOLT BALLS - picking it gives 3 bolt shots: lightning stems round the
# Zoogi (one per shot left), SUPERPOWER fires one straight missile per press while rolling (like Wolf Pack), each blasts like a wolf.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);\n',
  '''	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);
	/** b80 (Evan: Bolt Balls "works like Wolf Pack, the balls act like straight missiles, 3 shots, a lightning stem round the Zoogi
	 *  per shot left"): banked bolt shots per player; SUPERPOWER fires one while rolling. -ZoogiOldBoltBalls = the b66 redirect bolts. */
	ZOOGIROLL_API int32 BoltCount(const AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool BoltReady(const AZoogiGameMode* G);
	ZOOGIROLL_API bool BoltGo(AZoogiGameMode* G);
'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''		else if (Id == FName(TEXT("bolt_balls")))
		{
			// the old classic StaticShock''',
  '''		else if (Id == FName(TEXT("bolt_balls")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoltBalls")))
		{
			// b80: 3 bolt shots - the intro clip, then the lightning stems appear round the Zoogi
			auto Fx = [WG, Pi]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				ArmBolts(Gm, Pi, 3);
				ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 1.f, 1.3f);
				if (UZoogiFunSys* F = UZoogiFunSys::Get(Gm)) F->PublicCallout(TEXT("BOLT BALLS x3!"), TEXT("TAP SUPERPOWER WHILE ROLLING - ONE BOLT PER TAP"), FLinearColor(0.6f, 0.55f, 1.f), 2.f, 1.f);
			};
			if (AnS && AnS->PlayPowerIntro(TEXT("BoltBalls"), Pi, FLinearColor(0.6f, 0.55f, 1.f))) AnS->AfterCrash(Fx); else Fx();
		}
		else if (Id == FName(TEXT("bolt_balls")))
		{
			// the old classic StaticShock'''),
 ('''	void WolfTick(AZoogiGameMode* G)
	{''',
  '''	// ---- b80 bolt balls
	struct FBoltShot { TWeakObjectPtr<AActor> Vis; FVector P = FVector::ZeroVector, D = FVector::ForwardVector; float Age = 0.f, Spark = 0.f; int32 Owner = -1; };
	static TWeakObjectPtr<const AZoogiGameMode> GBoltG;
	static TMap<int32, int32> GBolts;
	static TArray<FBoltShot> GBoltShots;
	static float GStemT = 0.f;
	void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N)
	{
		if (GBoltG.Get() != G) { GBolts.Reset(); GBoltShots.Reset(); GBoltG = G; }
		GBolts.FindOrAdd(Player) += N;
		UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d has %d bolts"), Player + 1, GBolts[Player]);
	}
	int32 BoltCount(const AZoogiGameMode* G, int32 Player) { const int32* N = (G && GBoltG.Get() == G) ? GBolts.Find(Player) : nullptr; return N ? *N : 0; }
	bool BoltReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && BoltCount(G, B->PlayerIndex) > 0;
	}
	bool BoltGo(AZoogiGameMode* G)
	{
		if (!BoltReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		if (int32* N = GBolts.Find(B->PlayerIndex)) { if (--(*N) <= 0) GBolts.Remove(B->PlayerIndex); }
		UWorld* W = G->GetWorld();
		const FVector From = B->BallLocation();
		// straight at the nearest rival (else the nearest orb, else along the roll)
		AActor* Best = nullptr; float BestD = 1e12f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float D = FVector::DistSquared2D(O->BallLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		FVector Dir = Best ? (Best->GetActorLocation() - From).GetSafeNormal2D() : B->GetBallVelocity().GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = FRotator(0.f, B->AimYaw, 0.f).Vector();
		FBoltShot S; S.P = From + Dir * (ZoogiConst::ZoogiRadius + 20.f); S.D = Dir; S.Owner = B->PlayerIndex;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(S.P), SP))
		{
			A->SetMobility(EComponentMobility::Movable);
			UStaticMeshComponent* M = A->GetStaticMeshComponent();
			M->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetCastShadow(false);
			M->SetMaterial(0, ZoogiUtil::FxMID(A, FLinearColor(0.55f, 0.6f, 1.f), 12.f));
			A->SetActorScale3D(FVector(0.55f, 0.55f, 0.55f));
			S.Vis = A;
		}
		AZoogiFX::Stylized(W, TEXT("ElectricShock"), S.P, 0.6f);
		ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 1.5f);
		GBoltShots.Add(S);
		UE_LOG(LogZoogi, Log, TEXT("BOLT BALLS: P%d fires (%d left) at %s"), B->PlayerIndex + 1, BoltCount(G, B->PlayerIndex), Best ? *Best->GetName() : TEXT("ahead"));
		return true;
	}
	static void BoltBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, AZoogiBall* Struck)
	{
		UWorld* W = G->GetWorld();
		const float R = 430.f; // one wolf (AZoogiClone ShockR)
		for (AZoogiOrb* Ob : G->Orbs)
		{
			if (!Ob || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
			FVector D = Ob->OrbLocation() - At; D.Z = 0.f;
			const float Dist = D.Size(); if (Dist > R) continue;
			const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
			const float K = 1.f - Dist / R;
			const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
			Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K));
			Ob->LastHitBy = Owner;
		}
		if (Struck)
		{
			const FVector D = (Struck->BallLocation() - At).GetSafeNormal2D();
			Struck->LastHitPlayer = Owner;
			Struck->SetPlanarVelocity(Struck->GetBallVelocity() + D * 900.f, 120.f);
		}
		const FVector Ground(At.X, At.Y, At.Z - ZoogiConst::ZoogiRadius + 4.f);
		const FLinearColor Col(0.55f, 0.6f, 1.f);
		AZoogiFX::Spawn(W, At, FLinearColor(0.85f, 0.9f, 1.f), 130.f, 0.22f, false, 14.f);
		AZoogiFX::Spawn(W, Ground, Col, R, 0.55f, true, 10.f);
		UZoogiScreenFx::Shockwave(W, Ground, R + 60.f, Col);
		AZoogiFX::Stylized(W, TEXT("ElectricShock"), Ground, 1.2f);
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.25f);
	}
	static void BoltTick(AZoogiGameMode* G)
	{
		if (!G || GBoltG.Get() != G) return;
		UWorld* W = G->GetWorld();
		const float Dt = FMath::Min(W->GetDeltaSeconds(), 0.05f);
		// the lightning stems: one per bolt left, round the Zoogi that holds them
		GStemT += Dt;
		if (GStemT >= 0.3f)
		{
			GStemT = 0.f;
			for (const TPair<int32, int32>& Kv : GBolts)
				for (AZoogiBall* B : G->Balls)
					if (B && B->PlayerIndex == Kv.Key && !B->bOut)
						for (int32 k = 0; k < FMath::Min(Kv.Value, 3); ++k)
						{
							const float A = FMath::DegreesToRadians(W->GetTimeSeconds() * 90.f + k * 120.f);
							const FVector At = B->BallLocation() + FVector(FMath::Cos(A) * 95.f, FMath::Sin(A) * 95.f, -ZoogiConst::ZoogiRadius + 6.f);
							AZoogiFX::Stylized(W, TEXT("ElectricShock"), At, 0.45f);
						}
		}
		// missiles: straight line, 2600 cm/s, blast on the first rival / orb they reach (or the court edge / 1.4 s)
		const float KR = ZoogiConst::KnockoffRadius * (G->Arena ? G->Arena->GetActorScale3D().X : 1.f);
		const FVector Ctr = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
		for (int32 i = GBoltShots.Num() - 1; i >= 0; --i)
		{
			FBoltShot& S = GBoltShots[i];
			S.Age += Dt; S.Spark += Dt;
			S.P += S.D * 2600.f * Dt;
			if (AActor* V = S.Vis.Get()) V->SetActorLocation(S.P);
			if (S.Spark > 0.07f) { S.Spark = 0.f; AZoogiFX::Spawn(W, S.P, FLinearColor(0.6f, 0.65f, 1.f), 60.f, 0.25f, false, 8.f); }
			AZoogiBall* Hit = nullptr; bool bBoom = false;
			for (AZoogiBall* O : G->Balls) if (O && O->PlayerIndex != S.Owner && !O->bOut && FVector::Dist2D(O->BallLocation(), S.P) < ZoogiConst::ZoogiRadius + 40.f) { Hit = O; bBoom = true; break; }
			if (!bBoom) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), S.P) < 70.f) { bBoom = true; break; }
			if (!bBoom && (S.Age > 1.4f || FVector::Dist2D(S.P, Ctr) > KR + 150.f)) bBoom = true;
			if (bBoom)
			{
				BoltBlast(G, S.P, S.Owner, Hit);
				if (AActor* V = S.Vis.Get()) V->Destroy();
				GBoltShots.RemoveAt(i);
			}
		}
	}

	void WolfTick(AZoogiGameMode* G)
	{
		BoltTick(G);
		{	// the CPU fires its bolts through its own roll
			const AZoogiBall* Bc = G ? G->GetActiveBall() : nullptr;
			if (Bc && Bc->bCPU && BoltReady(G) && G->PhaseTime > 0.6f + 0.55f * (3 - BoltCount(G, Bc->PlayerIndex))) BoltGo(G);
		}'''),
], inc='#include "Engine/StaticMeshActor.h"', after='#include "ZoogiSkills.h"')
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API int32 BoltCount(const AZoogiGameMode* G, int32 Player);\n', '	ZOOGIROLL_API void ArmBolts(AZoogiGameMode* G, int32 Player, int32 N);\n	ZOOGIROLL_API int32 BoltCount(const AZoogiGameMode* G, int32 Player);\n'),
])
# SUPERPOWER: wolf -> bolt -> pack recall -> ultimate
ed(P + 'ZoogiClassicPad.cpp', [
 ('''				else if (bPower && WolfBanked(G) && G->Phase == EZoogiPhase::Aiming)''',
  '''				else if (bPower && ZoogiSkills::BoltReady(G)) ZoogiSkills::BoltGo(G); // b80: Bolt Balls - one straight bolt per press while rolling
				else if (bPower && (WolfBanked(G) || BoltBanked(G)) && G->Phase == EZoogiPhase::Aiming)'''),
 ('''			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::WolfReady(G)) : ReactionNeeded());''',
  '''			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::WolfReady(G) || ZoogiSkills::BoltReady(G)) : ReactionNeeded());'''),
 ('''	bool WolfBanked(const AZoogiGameMode* G) {''',
  '''	bool BoltBanked(const AZoogiGameMode* G) { const AZoogiBall* B = G ? G->GetActiveBall() : nullptr; return B && !B->bCPU && ZoogiSkills::BoltCount(G, B->PlayerIndex) > 0; }
	bool WolfBanked(const AZoogiGameMode* G) {'''),
 ('''			Label(bPower ? ((PadGM() && ZoogiSkills::WolfReady(PadGM())) ? TEXT("WOLF PACK!") : TEXT("SUPERPOWER")) :''',
  '''			Label(bPower ? ((PadGM() && ZoogiSkills::WolfReady(PadGM())) ? FString(TEXT("WOLF PACK!")) : ((PadGM() && ZoogiSkills::BoltReady(PadGM()) && PadGM()->GetActiveBall()) ? FString::Printf(TEXT("BOLT x%d!"), ZoogiSkills::BoltCount(PadGM(), PadGM()->GetActiveBall()->PlayerIndex)) : FString(TEXT("SUPERPOWER")))) :'''),
])
print('b80 ok')
