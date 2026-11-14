// -ZoogiEpisodeTest=A|B|E|F: Titan Meteor test director (classic mode, P1's first turn). It only sets up the start
// state and gives P1's inputs through the same calls the HUD uses (placement, Awaken Infuse Titan, lock / free aim,
// confirm); everything after the launch is real physics. Then it logs one TITAN_TEST line comparing what the
// presentation depicted with what the simulation produced.
//   A: Titan heavy hit that pushes the target inward (expected: no ring-out)  -> meteor, "STILL STANDING"
//   B: full-power Titan hit toward the edge (expected: ring-out)              -> meteor, "RING OUT"
//   E: Titan shot that misses everything                                     -> no meteor, lower tier
//   F: B in Competitive mode                                                 -> short meteor, same result as B
//   S: B with a Storm Infuse instead of Titan (no archetype)                 -> tier 3/4 panels + Storm power colours
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"

namespace
{
	float SegDist2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float L2 = AB.SizeSquared();
		const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * T);
	}
}

void UZoogiEpisodeSubsystem::TickTest()
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W || G->bComboMode) return;
	if (TestQuitAt > 0.0 && RealClock > TestQuitAt)
	{
		TestQuitAt = 0.0;
		UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST done, quitting"));
		UKismetSystemLibrary::QuitGame(W, W->GetFirstPlayerController(), EQuitPreference::Quit, false);
		return;
	}
	if (W->IsPaused()) return;
	const bool bMiss = TestVariant == TEXT("E");
	const bool bInward = TestVariant == TEXT("A") || bMiss;
	switch (TestStep)
	{
	case 0:
	{
		if (G->Phase != EZoogiPhase::Aiming || G->Stage != EZoogiStage::Draw || G->ActivePlayer != 0 || G->Balls.Num() < 2 || !G->Arena) return;
		AZoogiBall* S = G->Balls[0];
		AZoogiBall* T = G->Balls[TestTarget];
		if (!S || !T || !S->Artifact) return;
		if (TestVariant == TEXT("N") || TestVariant == TEXT("R") || TestVariant == TEXT("K")) { TestSetupSimple(); return; }
		const float R = AZoogiArena::ActiveKnockoffR;
		const float Z0 = S->BallLocation().Z, Z1 = T->BallLocation().Z;
		// scan layout orientations until the lane and every spot are clear of orbs and rocks
		float BestTheta = 0.f;
		bool bFound = false;
		FVector2D SP(0.f, 0.f), TP(0.f, 0.f), AimEnd(0.f, 0.f);
		TArray<FVector2D> OthersP;
		for (int32 k = 0; k < 24 && !bFound; ++k)
		{
			const float Th = FMath::DegreesToRadians(15.f * k);
			const FVector2D U(FMath::Cos(Th), FMath::Sin(Th)), V(-U.Y, U.X);
			if (bInward) { TP = U * (0.50f * R); SP = U * (0.80f * R); AimEnd = TP - U * 0.15f * R; }
			else { TP = V * (0.62f * R) + U * (0.38f * R); SP = TP - U * 650.f; AimEnd = TP + U * 0.25f * R; }
			if (bMiss) AimEnd = SP + (TP - SP).GetSafeNormal().GetRotated(35.f) * 520.f;
			OthersP.Reset();
			for (int32 i = 1; i < G->Balls.Num(); ++i) if (i != TestTarget) OthersP.Add(-V * (0.55f * R) + U * (OthersP.Num() == 0 ? 420.f : -420.f));
			bool bOk = true;
			TArray<FVector2D> Spots = { SP, TP };
			Spots.Append(OthersP);
			for (const FVector2D& P : Spots) if (G->Arena->IsOffCourt(P) || !G->Arena->IsSpotClear(P, 60.f)) bOk = false;
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !bOk) continue;
				const FVector2D OP(O->OrbLocation());
				if (SegDist2D(OP, SP, bMiss ? AimEnd : TP) < 160.f || (!bInward && SegDist2D(OP, TP, AimEnd) < 160.f)) bOk = false;
				for (const FVector2D& P : Spots) if (FVector2D::Distance(OP, P) < 150.f) bOk = false;
			}
			for (const AZoogiRock* Rk : G->Arena->GetRocks())
			{
				if (!Rk || Rk->GetState() == EZoogiRockState::Shattered || !bOk) continue;
				const FVector2D RP(Rk->GetActorLocation());
				if (SegDist2D(RP, SP, AimEnd) < Rk->GetRadius() + 80.f) bOk = false;
				for (const FVector2D& P : Spots) if (FVector2D::Distance(RP, P) < Rk->GetRadius() + 90.f) bOk = false;
			}
			// small arena pieces (crates, posts, obstacles) on the lane or a spot; big rim walls / stands are ignored
			for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
			{
				if (!Pc || !bOk || Pc->GetPieceState() == EZoogiPieceState::Broken) continue;
				FVector C, Ext;
				Pc->GetActorBounds(true, C, Ext);
				const float PR = FVector2D(Ext.X, Ext.Y).Size();
				if (PR < 1.f || PR > 450.f) continue;
				const FVector2D PC2(C.X, C.Y);
				if (SegDist2D(PC2, SP, AimEnd) < PR + 70.f) bOk = false;
				for (const FVector2D& P : Spots) if (FVector2D::Distance(PC2, P) < PR + 80.f) bOk = false;
			}
			if (bOk) { bFound = true; BestTheta = 15.f * k; }
		}
		if (!bFound) UE_LOG(LogZoogi, Warning, TEXT("TITAN_TEST no fully clear lane found, using the last layout"));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(TP.Y - SP.Y, TP.X - SP.X));
		S->PlaceAt(FVector(SP, Z0), Yaw);
		T->PlaceAt(FVector(TP, Z1), Yaw + 180.f);
		int32 Oi = 0;
		for (int32 i = 1; i < G->Balls.Num(); ++i)
			if (i != TestTarget && G->Balls[i] && OthersP.IsValidIndex(Oi)) { G->Balls[i]->PlaceAt(FVector(OthersP[Oi++], G->Balls[i]->BallLocation().Z), 0.f); }
		S->bCPU = false;                              // P1's inputs come from this director this turn
		S->Artifact->Drawn = TestVariant == TEXT("S") ? EZoogiArtifact::Storm : EZoogiArtifact::Titan; // the drawn artifact this turn
		TestAimPoint = FVector(AimEnd, Z0);
		TestStep = 1;
		UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST setup variant=%s mode=%s lane_theta=%.0f clear=%d shooter=P1 (%.0f,%.0f) target=P%d (%.0f,%.0f) knockoffR=%.0f decel=%.0f"),
			*TestVariant, ZoogiEpisode::ModeName(Mode), BestTheta, bFound ? 1 : 0, SP.X, SP.Y, TestTarget + 1, TP.X, TP.Y, R, G->Arena->GetRollingDecel());
		break;
	}
	case 1:
		if (G->Stage == EZoogiStage::Awaken && G->StageTime > 0.25f && TestSimple())
		{
			const bool bOk = G->AwakenSave();                // simple shots: no Infuse (save the artifact)
			UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST awaken save=%d"), bOk ? 1 : 0);
			TestStep = 2;
		}
		else if (G->Stage == EZoogiStage::Awaken && G->StageTime > 0.25f)
		{
			const bool bOk = G->AwakenInfuse(0);
			UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST awaken infuse %s=%d"), TestVariant == TEXT("S") ? TEXT("storm") : TEXT("titan"), bOk ? 1 : 0);
			TestStep = 2;
		}
		break;
	case 2:
		if (G->IsAimStep() && G->StepTime > 0.3f)
		{
			AZoogiBall* S = G->Balls[0];
			AZoogiBall* T = G->Balls[TestTarget];
			if (TestVariant == TEXT("A"))
			{
				// free aim past the target: the shot reaches it at a moderate speed (v^2 = 2 a (D - gap))
				const float Decel = G->Arena->GetRollingDecel();
				const FVector2D D = FVector2D(T->BallLocation() - S->BallLocation());
				const float Gap = D.Size() - 2.f * ZoogiConst::ZoogiRadius;
				const float VImp = 520.f;
				const FVector2D P = FVector2D(S->BallLocation()) + D.GetSafeNormal() * (Gap + VImp * VImp / (2.f * Decel));
				G->SetFreeAim(FVector(P, S->BallLocation().Z));
			}
			else if (TestSimple()) G->SetFreeAim(TestAimPoint);
			else if (bMiss) G->SetFreeAim(TestAimPoint);
			else { G->LockOn(T, false); G->AimPowerStep = 3; }
			const bool bOk = G->CommitLock();
			UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST aim variant=%s committed=%d"), *TestVariant, bOk ? 1 : 0);
			TestStep = 3;
		}
		break;
	case 3:
		if (G->Phase == EZoogiPhase::Rolling)
		{
			G->Balls[0]->bCPU = true; // back to the CPU for the rest of the match
			G->AimPowerStep = 0;
			TestActionId = Truth.ActionId;
			TestStep = 4;
		}
		break;
	default:
		break;
	}
}

void UZoogiEpisodeSubsystem::TestFinalize()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const FZoogiEpisodeRecognitionRecord& R = LiveRec;
	const bool bTargetOut = Truth.RingOuts.Contains(TestTarget);
	const bool bGameOut = G->Balls.IsValidIndex(TestTarget) && G->Balls[TestTarget] && G->Balls[TestTarget]->bOut;
	const bool bDepictOut = Plan.Epilogue == TEXT("RING OUT");
	const bool bDepictMatch = Mode == EZoogiEpisodeMode::Off ? true : (Plan.Epilogue.IsEmpty() ? !Plan.bMeteor : (bDepictOut == bTargetOut));
	bool bExpect = true;
	FString Expect;
	if (TestVariant == TEXT("A")) { Expect = TEXT("meteor=1 ringout=0"); bExpect = (Mode == EZoogiEpisodeMode::Off || Plan.bMeteor) && !bTargetOut; }
	else if (TestVariant == TEXT("B") || TestVariant == TEXT("F")) { Expect = TEXT("meteor=1 ringout=1"); bExpect = (Mode == EZoogiEpisodeMode::Off || Plan.bMeteor) && bTargetOut; }
	else if (TestVariant == TEXT("E")) { Expect = TEXT("meteor=0 power_hit=0 tier<4"); bExpect = !Plan.bMeteor && !Truth.bPowerHit && R.Tier < 4; }
	else if (TestVariant == TEXT("N")) { Expect = TEXT("NEAR_MISS tier>=1"); bExpect = R.Tags.Contains(EZoogiEpisodeTag::NearMiss) && R.Tier >= 1; }
	else if (TestVariant == TEXT("R")) { Expect = TEXT("AIRBORNE tier>=3 air_shot camera_restored"); bExpect = R.Tags.Contains(EZoogiEpisodeTag::Airborne) && R.Tier >= 3 && (Mode == EZoogiEpisodeMode::Off || (Plan.bAirShot && Plan.bCameraRestored)); }
	else if (TestVariant == TEXT("K")) { Expect = TEXT("BANK tier>=2"); bExpect = (R.Tags.Contains(EZoogiEpisodeTag::Bank) || R.Tags.Contains(EZoogiEpisodeTag::DoubleBank)) && R.Tier >= 2; }
	else if (TestVariant == TEXT("S")) { Expect = TEXT("meteor=0 power_hit=1 panels"); bExpect = !Plan.bMeteor && Truth.bPowerHit && (Mode == EZoogiEpisodeMode::Off || Plan.Panels > 0); }
	FString Final, Deltas, TitanShots;
	for (const FZoogiEpisodeShot& Sh : Plan.Shots)
		if (Sh.Family == FName(TEXT("titan_meteor"))) TitanShots += FString::Printf(TEXT("%s%s"), TitanShots.IsEmpty() ? TEXT("") : TEXT(","), *Sh.ShotId.ToString().RightChop(13));
	for (int32 i = 0; i < Truth.FinalBallPos.Num(); ++i)
	{
		Final += FString::Printf(TEXT("%sP%d(%.1f,%.1f%s)"), Final.IsEmpty() ? TEXT("") : TEXT(" "), i + 1, Truth.FinalBallPos[i].X, Truth.FinalBallPos[i].Y, Truth.FinalBallOut[i] ? TEXT(",out") : TEXT(""));
		Deltas += FString::Printf(TEXT("%sP%d:%+d/%+d"), Deltas.IsEmpty() ? TEXT("") : TEXT(","), i + 1, Truth.ScoreDelta.IsValidIndex(i) ? Truth.ScoreDelta[i] : 0, Truth.RingMarkDelta.IsValidIndex(i) ? Truth.RingMarkDelta[i] : 0);
	}
	FString TagS;
	for (EZoogiEpisodeTag Tg : R.Tags) TagS += (TagS.IsEmpty() ? TEXT("") : TEXT(",")) + FString(ZoogiEpisode::TagName(Tg));
	UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST tags=%s panels=%d air_shot=%d camera_restored=%d glow=%d"), TagS.IsEmpty() ? TEXT("-") : *TagS, Plan.Panels, Plan.bAirShot ? 1 : 0, Plan.bCameraRestored ? 1 : 0, Plan.bGlow ? 1 : 0);
	UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST variant=%s mode=%s action=%d power_hit=%d crit=%d archetype=%s tier=%d score=%.0f meteor=%d meteor_s=%.2f target=P%d target_ringout=%d game_bOut=%d score/ringmark=%s depicted=\"%s\" depicted_matches_physics=%d result_unchanged=%d drift_cm=%.4f dilation_restored=%d titan_shots=%s shots=%d expect=[%s] %s final=%s"),
		*TestVariant, ZoogiEpisode::ModeName(Mode), Truth.ActionId, Truth.bPowerHit ? 1 : 0, Truth.bCrit ? 1 : 0, ZoogiEpisode::ArchetypeName(R.Archetype), R.Tier, R.Score,
		Plan.bMeteor ? 1 : 0, Plan.MeteorSeconds, TestTarget + 1, bTargetOut ? 1 : 0, bGameOut ? 1 : 0, *Deltas, *Plan.Epilogue, bDepictMatch ? 1 : 0, Plan.ResultUnchanged() ? 1 : 0,
		Plan.FreezeDriftCm, Plan.bDilationRestored ? 1 : 0, TitanShots.IsEmpty() ? TEXT("-") : *TitanShots, Plan.Shots.Num(), *Expect, (bExpect && bDepictMatch && Plan.ResultUnchanged()) ? TEXT("PASS") : TEXT("FAIL"), *Final);
	TestQuitAt = RealClock + 2.0;
	TestStep = 5;
}

bool UZoogiEpisodeSubsystem::TestSimple() const
{
	return TestVariant == TEXT("N") || TestVariant == TEXT("R") || TestVariant == TEXT("K");
}

// N: shooter rolls past a rival with a ~15 cm gap (NEAR MISS). R: full-power run up the snow-chute ramp (AIRBORNE).
// K: shooter banks off a pillar base into the target (BANK). Only start positions + aim are set; the rest is physics.
void UZoogiEpisodeSubsystem::TestSetupSimple()
{
	AZoogiGameMode* G = GM();
	AZoogiBall* S = G->Balls[0];
	AZoogiBall* T = G->Balls[TestTarget];
	const float R = AZoogiArena::ActiveKnockoffR;
	const float Rb = ZoogiConst::ZoogiRadius;
	const float Z0 = S->BallLocation().Z, Z1 = T->BallLocation().Z;
	auto Clear = [&](const TArray<FVector2D>& Spots, const TArray<TPair<FVector2D, FVector2D>>& Lanes, const AActor* IgnorePiece)
	{
		for (const FVector2D& P : Spots) if (G->Arena->IsOffCourt(P) || !G->Arena->IsSpotClear(P, 60.f)) return false;
		for (AZoogiOrb* O : G->Orbs)
		{
			if (!O || O->bOut) continue;
			const FVector2D OP(O->OrbLocation());
			for (const auto& L : Lanes) if (SegDist2D(OP, L.Key, L.Value) < 150.f) return false;
			for (const FVector2D& P : Spots) if (FVector2D::Distance(OP, P) < 150.f) return false;
		}
		for (const AZoogiRock* Rk : G->Arena->GetRocks())
		{
			if (!Rk || Rk->GetState() == EZoogiRockState::Shattered) continue;
			const FVector2D RP(Rk->GetActorLocation());
			for (const auto& L : Lanes) if (SegDist2D(RP, L.Key, L.Value) < Rk->GetRadius() + 80.f) return false;
		}
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
		{
			if (!Pc || Pc == IgnorePiece || Pc->GetPieceState() == EZoogiPieceState::Broken) continue;
			FVector C, Ext;
			Pc->GetActorBounds(true, C, Ext);
			const float PR = FVector2D(Ext.X, Ext.Y).Size();
			if (PR < 1.f || PR > 450.f) continue;
			for (const auto& L : Lanes) if (SegDist2D(FVector2D(C), L.Key, L.Value) < PR + 60.f) return false;
		}
		return true;
	};
	FVector2D SP(0, 0), TP(0, 0), Aim(0, 0);
	TArray<FVector2D> OthersP;
	bool bFound = false;
	if (TestVariant == TEXT("N"))
	{
		// N keeps the spawn layout (the shooter's lane to the centre is clear) and parks the target beside that lane,
		// 15 cm clear of the shooter's path; aim = the centre
		const FVector2D S0(S->BallLocation());
		const FVector2D U = (-S0).GetSafeNormal(), V(-U.Y, U.X);
		TP = S0 + U * 380.f + V * (2.f * Rb + 15.f);
		Aim = S0 + U * S0.Size();
		T->PlaceAt(FVector(TP, Z1), 0.f);
		S->bCPU = false;
		TestAimPoint = FVector(Aim, Z0);
		TestStep = 1;
		UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST setup variant=N mode=%s shooter=P1 (%.0f,%.0f) target=P%d (%.0f,%.0f) aim=(%.0f,%.0f)"), ZoogiEpisode::ModeName(Mode), S0.X, S0.Y, TestTarget + 1, TP.X, TP.Y, Aim.X, Aim.Y);
		return;
	}
	if (TestVariant == TEXT("R"))
	{
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
		{
			if (!Pc || Pc->GetKind() != FName(TEXT("ramp"))) continue;
			FVector C, Ext;
			Pc->GetActorBounds(true, C, Ext);
			const FVector2D F = FVector2D(Pc->GetActorForwardVector()).GetSafeNormal();
			const float Half = FMath::Abs(FVector2D::DotProduct(FVector2D(Ext.X, Ext.Y), F)) + 20.f;
			const FVector2D RC(Pc->GetActorLocation());
			SP = RC - F * (Half + 380.f);
			Aim = RC + F * 2600.f;                      // far: full power straight up the chute
			TP = SP - FVector2D(-F.Y, F.X) * 420.f;     // target parked beside the run-up
			OthersP = { SP + FVector2D(-F.Y, F.X) * 460.f, SP - F * 420.f };
			bFound = true;
			break;
		}
	}
	else
	{
		for (int32 k = 0; k < 24 && !bFound; ++k)
		{
			const float Th = FMath::DegreesToRadians(15.f * k);
			const FVector2D U(FMath::Cos(Th), FMath::Sin(Th)), V(-U.Y, U.X);
			TArray<TPair<FVector2D, FVector2D>> Lanes;
			const AActor* Ign = nullptr;
			if (TestVariant == TEXT("N"))
			{
				TP = U * (0.15f * R) + V * (0.1f * R);
				SP = TP - U * 600.f - V * (2.f * Rb + 15.f);
				Aim = SP + U * 900.f;                    // rolls ~900 cm: passes the target's flank at ~260 cm/s
				Lanes.Add({ SP, SP + U * 1000.f });
			}
			else
			{
				// K: a pillar base face; mirror bank (ball centre meets the face at Q, reflects to the target)
				TArray<const AZoogiArenaPiece*> Pillars;
				for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces()) if (Pc && Pc->GetKind().ToString().StartsWith(TEXT("pillar"))) Pillars.Add(Pc);
				if (Pillars.Num() == 0) break;
				const AZoogiArenaPiece* Pc = Pillars[(k / 8) % Pillars.Num()];
				const float Face = 61.f + Rb - 4.f;
				const FVector2D C(Pc->GetActorLocation());
				const FVector2D N = FVector2D(Pc->GetActorForwardVector()).GetSafeNormal().GetRotated(90.f * (k % 4));
				const float Side = (k / 4) % 2 ? 1.f : -1.f;
				const FVector2D Q = C + N * Face;
				const FVector2D In = (-N).GetRotated(Side * 35.f);       // incoming direction (towards the face)
				const FVector2D Out = In - 2.f * FVector2D::DotProduct(In, N) * N;
				SP = Q - In * 520.f;
				TP = Q + Out * 250.f;
				Aim = SP + In * 2600.f;                  // full power: the bank costs speed
				Lanes.Add({ SP, Q });
				Lanes.Add({ Q + Out * 60.f, TP });
				Ign = Pc;
			}
			const FVector2D Far = (TP - SP).GetSafeNormal();
			OthersP = { SP - FVector2D(-Far.Y, Far.X) * 520.f, TP + FVector2D(-Far.Y, Far.X) * 520.f };
			TArray<FVector2D> Spots = { SP, TP };
			Spots.Append(OthersP);
			if (Clear(Spots, Lanes, Ign)) bFound = true;
		}
	}
	if (!bFound) UE_LOG(LogZoogi, Warning, TEXT("TITAN_TEST %s: no clear layout, using the last one"), *TestVariant);
	S->PlaceAt(FVector(SP, Z0), FMath::RadiansToDegrees(FMath::Atan2(Aim.Y - SP.Y, Aim.X - SP.X)));
	T->PlaceAt(FVector(TP, Z1), 0.f);
	int32 Oi = 0;
	for (int32 i = 1; i < G->Balls.Num(); ++i)
		if (i != TestTarget && G->Balls[i] && OthersP.IsValidIndex(Oi)) G->Balls[i]->PlaceAt(FVector(OthersP[Oi++], G->Balls[i]->BallLocation().Z), 0.f);
	S->bCPU = false;
	TestAimPoint = FVector(Aim, Z0);
	TestStep = 1;
	UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST setup variant=%s mode=%s clear=%d shooter=P1 (%.0f,%.0f) target=P%d (%.0f,%.0f) aim=(%.0f,%.0f)"),
		*TestVariant, ZoogiEpisode::ModeName(Mode), bFound ? 1 : 0, SP.X, SP.Y, TestTarget + 1, TP.X, TP.Y, Aim.X, Aim.Y);
}

// Searches the game's own aim preview (the same one the HUD draws) from the shooter's real position:
//  K: a line whose preview banks off a surface and then contacts a Zoogi (smallest cut)
//  N: a line with no contact whose path passes a Zoogi with an 8-28 cm gap (closest to 15)
// The result is only an input (a free-aim point); everything after the launch is the normal simulation.
void UZoogiEpisodeSubsystem::TestPickSimpleAim()
{
	AZoogiGameMode* G = GM();
	if (!G || TestVariant == TEXT("R")) return;
	AZoogiBall* S = G->Balls[0];
	const FVector S0 = S->BallLocation();
	const float Rb = ZoogiConst::ZoogiRadius;
	float Best = 1e9f;
	FVector BestP = TestAimPoint;
	const float Dists[] = { 700.f, 1000.f, 1400.f, 1900.f };
	const bool bWasAim = G->bAimActive;
	G->bAimActive = true;                                  // the preview is only computed while aiming
	for (float Dd : Dists)
		for (float A = 0.f; A < 360.f; A += 0.25f)
		{
			const FVector P = S0 + FVector(FMath::Cos(FMath::DegreesToRadians(A)), FMath::Sin(FMath::DegreesToRadians(A)), 0.f) * Dd;
			if (G->Arena && G->Arena->IsOffCourt(FVector2D(P))) continue;
			G->SetFreeAim(P);
			G->UpdatePreview();
			const FZoogiAimPreview& Pv = G->Preview;
			if (!Pv.bValid) continue;
			if (TestVariant == TEXT("K"))
			{
				if (!Pv.bBank || !Pv.bContact || !Cast<AZoogiBall>(Pv.ContactActor.Get())) continue;
				const float Sc = Pv.CutDeg + Dd * 0.001f;
				if (Sc < Best) { Best = Sc; BestP = P; }
			}
			else
			{
				if (Pv.bContact || Pv.bBank || Pv.Path.Num() < 2) continue;
				bool bOff = false;
				for (bool b : Pv.PathOff) bOff |= b;
				if (bOff) continue;
				for (int32 i = 1; i < G->Balls.Num(); ++i)
				{
					AZoogiBall* B = G->Balls[i];
					if (!B || B->bOut) continue;
					float Dm = 1e9f;
					for (int32 k = 1; k < Pv.Path.Num(); ++k) Dm = FMath::Min(Dm, SegDist2D(FVector2D(B->BallLocation()), FVector2D(Pv.Path[k - 1]), FVector2D(Pv.Path[k])));
					const float Gap = Dm - 2.f * Rb;
					if (Gap < 8.f || Gap > 28.f) continue;
					const float Sc = FMath::Abs(Gap - 15.f) + Dd * 0.002f;
					if (Sc < Best) { Best = Sc; BestP = P; TestTarget = i; }
				}
			}
		}
	G->bAimActive = bWasAim;
	TestAimPoint = BestP;
	UE_LOG(LogZoogi, Log, TEXT("TITAN_TEST pick variant=%s found=%d aim=(%.0f,%.0f) score=%.2f target=P%d"), *TestVariant, Best < 1e8f ? 1 : 0, BestP.X, BestP.Y, Best < 1e8f ? Best : -1.f, TestTarget + 1);
}
