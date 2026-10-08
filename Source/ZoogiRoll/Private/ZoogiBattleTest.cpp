// -ZoogiBattleTest=<case>: battle engine acceptance director (T5). Like the Episode test director it only sets up the
// start state and gives P1's inputs through the calls the HUD uses (placement, Awaken choice, lock / free aim, commit,
// combo select, air reaction choice); everything after the launch is the real simulation. Other players are CPUs.
// Each case logs one BATTLE_TEST case=<c> result=PASS|FAIL line (+ final positions for the Off comparisons).
#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeEnergyComponent.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiMiniGame.h"
#include "ZoogiEpisodeLedger.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiRock.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	enum class ELay : uint8 { None, Lane, KO, Miss, Near, Ramp, Bank, MidAir };
	enum class EAw : uint8 { Save, Infuse, Guard };
	struct FCase
	{
		const TCHAR* Name;
		ELay Lay;
		EAw Aw;
		const TCHAR* Combo;   // nullptr = none
		bool bLock;           // lock-on shot (else free aim at the layout's aim point)
		float Energy;         // P1 start energy (-1 = untouched)
		int32 Actions;        // actions to observe before checking (CPU cases)
		bool bScriptP1;
	};
	const FCase GCases[] = {
		{ TEXT("roll"),                  ELay::Lane,   EAw::Save,   nullptr,               true,  -1.f, 1, true },
		{ TEXT("miss_no_consolation"),   ELay::Miss,   EAw::Infuse, nullptr,               false, -1.f, 1, true },
		{ TEXT("timeout"),               ELay::Lane,   EAw::Save,   nullptr,               true,  -1.f, 1, true },
		{ TEXT("shatter_basic"),         ELay::Near,   EAw::Save,   TEXT("signature_strike"), false, 50.f, 1, true },
		{ TEXT("energy_gain"),           ELay::KO,     EAw::Save,   nullptr,               true,  0.f,  1, true },
		{ TEXT("combo_spend"),           ELay::Lane,   EAw::Save,   TEXT("signature_strike"), true,  50.f, 1, true },
		{ TEXT("energy_full_no_awaken"), ELay::Ramp,   EAw::Save,   nullptr,               false, 100.f, 1, true },
		{ TEXT("awaken_gate"),           ELay::Ramp,   EAw::Guard,  nullptr,               false, -1.f, 1, true },
		{ TEXT("air_entitled"),          ELay::Ramp,   EAw::Guard,  nullptr,               false, -1.f, 1, true },
		{ TEXT("air_not_entitled"),      ELay::Ramp,   EAw::Save,   nullptr,               false, -1.f, 1, true },
		{ TEXT("air_fail"),              ELay::Ramp,   EAw::Guard,  nullptr,               false, -1.f, 1, true },
		{ TEXT("halo_success"),          ELay::Bank,   EAw::Save,   TEXT("halo_bank"),     false, 50.f, 1, true },
		{ TEXT("halo_fail"),             ELay::Lane,   EAw::Save,   TEXT("halo_bank"),     true,  50.f, 1, true },
		{ TEXT("orbline_success"),       ELay::Lane,   EAw::Save,   TEXT("orb_line"),      true,  50.f, 1, true },
		{ TEXT("orbline_fail"),          ELay::Lane,   EAw::Save,   TEXT("orb_line"),      true,  50.f, 1, true },
		{ TEXT("halo_pass"),             ELay::Lane,   EAw::Save,   TEXT("halo_bank"),     true,  50.f, 1, true },  // T3 props: through a Resonance Halo
		{ TEXT("halo_rim"),              ELay::Lane,   EAw::Save,   TEXT("halo_bank"),     true,  50.f, 1, true },  // T3 props: off the halo rim
		{ TEXT("sky_bumper"),            ELay::Lane,   EAw::Save,   TEXT("sky_bumper"),    true,  50.f, 1, true },  // T3 props: rebound x2 from the contact normal
		{ TEXT("rail_release"),          ELay::Miss,   EAw::Save,   TEXT("rail_release"),  false, 50.f, 1, true },  // T3 props: center window
		{ TEXT("rail_timeout"),          ELay::Miss,   EAw::Save,   TEXT("rail_release"),  false, 50.f, 1, true },  // T3 props: no release -> shatter
		{ TEXT("ko"),                    ELay::KO,     EAw::Save,   nullptr,               true,  -1.f, 1, true },
		{ TEXT("segment_retained"),      ELay::Lane,   EAw::Save,   TEXT("signature_strike"), true,  60.f, 2, true },
		{ TEXT("unlocks"),               ELay::KO,     EAw::Save,   TEXT("signature_strike"), true,  50.f, 1, true },
		{ TEXT("queued_effects"),        ELay::Miss,   EAw::Guard,  TEXT("signature_strike"), false, 50.f, 1, true },
		{ TEXT("midair_collision"),      ELay::Miss,   EAw::Save,   nullptr,               false, -1.f, 1, true },  // EARLY rail release over a parked Zoogi
		{ TEXT("replay_order"),          ELay::None,   EAw::Save,   nullptr,               false, -1.f, 6, false },
		{ TEXT("one_v_one"),             ELay::None,   EAw::Save,   nullptr,               false, -1.f, 6, false },
		{ TEXT("four_player_2v2"),       ELay::None,   EAw::Save,   nullptr,               false, -1.f, 8, false },
	};
	const FCase* FindCase(const FString& N)
	{
		for (const FCase& C : GCases) if (N.Equals(C.Name, ESearchCase::IgnoreCase)) return &C;
		return nullptr;
	}
	float SegDist2(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float L2 = AB.SizeSquared();
		const float T = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * T);
	}
}

void UZoogiBattleStateMachine::TestReport(bool bPass, const FString& Detail)
{
	if (bTestDone) return;
	bTestDone = true;
	const AZoogiGameMode* G = GM();
	FString Fin;
	if (G) for (const AZoogiBall* B : G->Balls) if (B) Fin += FString::Printf(TEXT("%sP%d(%.1f,%.1f%s s%d)"), Fin.IsEmpty() ? TEXT("") : TEXT(" "), B->PlayerIndex + 1, B->BallLocation().X, B->BallLocation().Y, B->bOut ? TEXT(",out") : TEXT(""), B->Score);
	FString Trail;
	for (int32 i = FMath::Max(0, StateTrail.Num() - 14); i < StateTrail.Num(); ++i) Trail += (Trail.IsEmpty() ? TEXT("") : TEXT(" | ")) + StateTrail[i];
	UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST trail %s"), *Trail);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST case=%s result=%s flow=%d episode=%s invalid=%d shatters=%d actions=%d ledger_events=%d ledger_hash=%08x detail=\"%s\" first_final=%s final=%s"),
		*TestCase, bPass ? TEXT("PASS") : TEXT("FAIL"), bFlow ? 1 : 0, Episode() ? ZoogiEpisode::ModeName(Episode()->GetMode()) : TEXT("-"), InvalidTransitions, ShatterCount, TestActions,
		Ledger ? Ledger->Events.Num() : 0, Ledger ? Ledger->Hash() : 0u, *Detail, TestFinal.IsEmpty() ? TEXT("-") : *TestFinal, *Fin);
	TestQuitAt = RealClock + 2.0;
}

bool UZoogiBattleStateMachine::TestLayout(int32 Shooter, int32 Target, int32 Variant)
{
	AZoogiGameMode* G = GM();
	AZoogiBall* S = G->Balls[Shooter];
	AZoogiBall* T = G->Balls[Target];
	const ELay Lay = (ELay)Variant;
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
			for (const auto& L : Lanes) if (SegDist2(OP, L.Key, L.Value) < 150.f) return false;
			for (const FVector2D& P : Spots) if (FVector2D::Distance(OP, P) < 150.f) return false;
		}
		for (const AZoogiRock* Rk : G->Arena->GetRocks())
		{
			if (!Rk || Rk->GetState() == EZoogiRockState::Shattered) continue;
			const FVector2D RP(Rk->GetActorLocation());
			for (const auto& L : Lanes) if (SegDist2(RP, L.Key, L.Value) < Rk->GetRadius() + 80.f) return false;
			for (const FVector2D& P : Spots) if (FVector2D::Distance(RP, P) < Rk->GetRadius() + 90.f) return false;
		}
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
		{
			if (!Pc || Pc == IgnorePiece || Pc->GetPieceState() == EZoogiPieceState::Broken) continue;
			FVector C, Ext;
			Pc->GetActorBounds(true, C, Ext);
			const float PR = FVector2D(Ext.X, Ext.Y).Size();
			if (PR < 1.f || PR > 450.f) continue;
			for (const auto& L : Lanes) if (SegDist2(FVector2D(C), L.Key, L.Value) < PR + 60.f) return false;
			for (const FVector2D& P : Spots) if (FVector2D::Distance(FVector2D(C), P) < PR + 80.f) return false;
		}
		return true;
	};
	FVector2D SP(0, 0), TP(0, 0), Aim(0, 0);
	TArray<FVector2D> OthersP;
	bool bFound = false;
	if (Lay == ELay::Ramp || Lay == ELay::MidAir)
	{
		for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
		{
			if (!Pc || Pc->GetKind() != FName(TEXT("ramp"))) continue;
			FVector C, Ext;
			Pc->GetActorBounds(true, C, Ext);
			const FVector2D F = FVector2D(Pc->GetActorForwardVector()).GetSafeNormal();
			const FVector2D Side(-F.Y, F.X);
			const float Half = FMath::Abs(FVector2D::DotProduct(FVector2D(Ext.X, Ext.Y), F)) + 20.f;
			const FVector2D RC(Pc->GetActorLocation());
			SP = RC - F * (Half + 380.f);
			Aim = RC + F * 2600.f;
			TP = Lay == ELay::MidAir ? RC + F * (Half + 260.f) : SP - Side * 420.f; // MidAir: parked just past the lip (the flight path)
			OthersP = { SP + Side * 460.f, SP - F * 420.f };
			bFound = true;
			break;
		}
	}
	else if (Lay == ELay::Near)
	{
		const FVector2D S0(S->BallLocation());
		const FVector2D U = (-S0).GetSafeNormal(), V(-U.Y, U.X);
		SP = S0;
		TP = S0 + U * 380.f + V * (2.f * Rb + 15.f);
		Aim = S0 + U * S0.Size();
		bFound = true;
	}
	else
	{
		for (int32 kk = 0; kk < 48 * 3 && !bFound; ++kk)
		{
			const int32 k = kk % 48;
			const float Sc = kk < 48 ? 1.f : (kk < 96 ? 0.75f : 1.18f); // radius bands: mid, inner, outer
			const float Th = FMath::DegreesToRadians(7.5f * k + (kk >= 48 ? 3.75f : 0.f));
			const FVector2D U(FMath::Cos(Th), FMath::Sin(Th)), V(-U.Y, U.X);
			TArray<TPair<FVector2D, FVector2D>> Lanes;
			const AActor* Ign = nullptr;
			if (Lay == ELay::Lane) { TP = U * (0.40f * R * Sc); SP = U * (0.40f * R * Sc + 500.f); Aim = TP; Lanes.Add({ SP, TP - U * 150.f }); }
			else if (Lay == ELay::KO) { TP = V * (0.62f * R) + U * (0.38f * R * Sc); SP = TP - U * 650.f; Aim = TP; Lanes.Add({ SP, TP + U * 0.25f * R }); }
			else if (Lay == ELay::Miss) { TP = U * (0.40f * R * Sc); SP = U * (0.40f * R * Sc + 500.f); Aim = SP + (TP - SP).GetSafeNormal().GetRotated(40.f) * 520.f; Lanes.Add({ SP, Aim }); }
			else // Bank: a pillar base face (mirror bank like the Episode K test)
			{
				TArray<const AZoogiArenaPiece*> Pillars;
				for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces()) if (Pc && Pc->GetKind().ToString().StartsWith(TEXT("pillar"))) Pillars.Add(Pc);
				if (Pillars.Num() == 0) break;
				const AZoogiArenaPiece* Pc = Pillars[(k / 8) % Pillars.Num()];
				const float Face = 61.f + Rb - 4.f;
				const FVector2D C(Pc->GetActorLocation());
				const FVector2D N = FVector2D(Pc->GetActorForwardVector()).GetSafeNormal().GetRotated(90.f * (k % 4));
				const float Side = (k / 4) % 2 ? 1.f : -1.f;
				const FVector2D Q = C + N * Face;
				const FVector2D In = (-N).GetRotated(Side * 35.f);
				const FVector2D Out = In - 2.f * FVector2D::DotProduct(In, N) * N;
				SP = Q - In * 520.f;
				TP = Q + Out * 250.f;
				Aim = SP + In * 2600.f;
				Lanes.Add({ SP, Q });
				Lanes.Add({ Q + Out * 60.f, TP });
				Ign = Pc;
			}
			const FVector2D Far = (TP - SP).GetSafeNormal();
			OthersP = { SP - FVector2D(-Far.Y, Far.X) * 560.f, TP + FVector2D(-Far.Y, Far.X) * 560.f, SP - Far * 500.f };
			if (!Clear({ SP, TP }, Lanes, Ign)) continue;
			// the other Zoogies: any clear spots well away from the lane
			const FVector2D Mid = (SP + TP) * 0.5f, Nn(-Far.Y, Far.X);
			const FVector2D Cand[] = { Mid + Nn * 560.f, Mid - Nn * 560.f, SP - Far * 520.f, TP + Far * 520.f, Mid + Nn * 820.f, Mid - Nn * 820.f };
			OthersP.Reset();
			for (const FVector2D& Cd : Cand) if (OthersP.Num() < 3 && Clear({ Cd }, {}, nullptr) && SegDist2(Cd, SP, TP) > 300.f) OthersP.Add(Cd);
			bFound = OthersP.Num() >= G->Balls.Num() - 2;
		}
	}
	if (!bFound) UE_LOG(LogZoogi, Warning, TEXT("BATTLE_TEST %s: no clear layout, using the last one"), *TestCase);
	S->PlaceAt(FVector(SP, Z0), FMath::RadiansToDegrees(FMath::Atan2(Aim.Y - SP.Y, Aim.X - SP.X)));
	T->PlaceAt(FVector(TP, Z1), 0.f);
	int32 Oi = 0;
	for (int32 i = 0; i < G->Balls.Num(); ++i)
		if (i != Shooter && i != Target && G->Balls[i] && OthersP.IsValidIndex(Oi)) G->Balls[i]->PlaceAt(FVector(OthersP[Oi++], G->Balls[i]->BallLocation().Z), 0.f);
	TestAim = FVector(Aim, Z0);
	UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST setup case=%s layout=%d clear=%d shooter=P%d (%.0f,%.0f) target=P%d (%.0f,%.0f) aim=(%.0f,%.0f)"), *TestCase, Variant, bFound ? 1 : 0,
		Shooter + 1, SP.X, SP.Y, Target + 1, TP.X, TP.Y, Aim.X, Aim.Y);
	return bFound;
}

void UZoogiBattleStateMachine::TestScriptTurn()
{
	AZoogiGameMode* G = GM();
	const FCase* C = FindCase(TestCase);
	AZoogiBall* S = G->Balls[0];
	TestShooter = 0;
	TestTarget = G->bComboMode ? 2 : 1;
	const bool bSecond = TestScriptedTurns > 0;
	ELay Lay = C->Lay;
	FName Combo = C->Combo ? FName(C->Combo) : NAME_None;
	bool bLock = C->bLock;
	if (TestCase == TEXT("segment_retained") && bSecond) { Combo = TEXT("halo_bank"); bLock = true; } // 2nd P1 turn: a direct shot cannot bank -> shatter
	TestLayout(0, TestTarget, (int32)Lay);
	S->bCPU = false;
	if (S->Artifact)
	{
		if (C->Aw == EAw::Infuse) S->Artifact->Drawn = EZoogiArtifact::Storm;
		if (C->Aw == EAw::Guard) S->Artifact->Drawn = EZoogiArtifact::Titan;
	}
	if (C->Energy >= 0.f && !bSecond) if (UZoogiEpisodeEnergyComponent* E = Energy(0)) E->SetValue(C->Energy, TEXT("test"));
	// orb placement (Orb Line): on the lane (success) or off it (fail; committed explicitly)
	TestOrb = -1;
	if (TestCase.StartsWith(TEXT("orbline")) && G->Orbs.Num() > 0)
	{
		const FVector2D SP(S->BallLocation()), TP(G->Balls[TestTarget]->BallLocation());
		const FVector2D U = (TP - SP).GetSafeNormal(), V(-U.Y, U.X);
		const FVector2D OP = TestCase == TEXT("orbline_success") ? SP + (TP - SP) * 0.45f : SP + (TP - SP) * 0.45f + V * 260.f;
		for (int32 i = 0; i < G->Orbs.Num(); ++i) if (G->Orbs[i] && !G->Orbs[i]->bOut) { TestOrb = i; break; }
		if (TestOrb >= 0) G->Orbs[TestOrb]->Respawn(FVector(OP, ZoogiConst::OrbRadius + 1.f));
		UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST orb O%d placed at (%.0f,%.0f)"), TestOrb + 1, OP.X, OP.Y);
	}
	// T3 aerial props (debug geometry) on the scripted lane
	if (!bSecond && (TestCase.StartsWith(TEXT("halo_pass")) || TestCase.StartsWith(TEXT("halo_rim")) || TestCase == TEXT("sky_bumper") || TestCase.StartsWith(TEXT("rail_")) || TestCase == TEXT("midair_collision")))
	{
		ClearProps();
		const FVector2D SP(S->BallLocation());
		const FVector2D Dir = ((TestCase.StartsWith(TEXT("rail_")) || TestCase == TEXT("midair_collision")) ? FVector2D(TestAim) - SP : FVector2D(G->Balls[TestTarget]->BallLocation()) - SP).GetSafeNormal();
		const FVector2D Side(-Dir.Y, Dir.X);
		const float Floor = S->BallLocation().Z - ZoogiConst::ZoogiRadius;
		const FVector Fwd(Dir, 0.f);
		if (TestCase == TEXT("halo_pass")) SpawnProp(EZoogiPropKind::ResonanceHalo, FVector(SP + Dir * 250.f, Floor), Fwd);
		else if (TestCase == TEXT("halo_rim")) SpawnProp(EZoogiPropKind::ResonanceHalo, FVector(SP + Dir * 250.f + Side * 90.f, Floor), Fwd);
		else if (TestCase == TEXT("sky_bumper")) SpawnProp(EZoogiPropKind::SkyBumper, FVector(SP + Dir * 250.f + Side * 60.f, S->BallLocation().Z), Fwd);
		else SpawnProp(EZoogiPropKind::LaunchRail, FVector(SP + Dir * 170.f, Floor), Fwd);
		if (TestCase == TEXT("midair_collision"))
		{
			// the target parks on the EARLY (high) launch arc: P1 meets it in the air (Zone 2)
			AZoogiBall* T = G->Balls[TestTarget];
			T->PlaceAt(FVector(SP + Dir * 400.f, T->BallLocation().Z), 0.f);
			UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST midair target P%d parked at (%.0f,%.0f)"), TestTarget + 1, T->BallLocation().X, T->BallLocation().Y);
		}
		UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST props case=%s count=%d"), *TestCase, Props.Num());
	}
	TestNotes.Add(FString::Printf(TEXT("turn%d:%s"), TestScriptedTurns + 1, Combo == NAME_None ? TEXT("shot") : *Combo.ToString()));
	TestStep = 1;
	++TestScriptedTurns;
	TestDeadline = RealClock + 75.0;
	// the combo is committed during the Battle aim step (step 2)
	TestNotes.Add(Combo.ToString());
	TestAimLock = bLock;
	TestCombo = Combo;
}

void UZoogiBattleStateMachine::TickTest(float RealDt)
{
	AZoogiGameMode* G = GM();
	UWorld* W = GetWorld();
	if (!G || !W) return;
	if (TestQuitAt > 0.0 && RealClock > TestQuitAt)
	{
		TestQuitAt = 0.0;
		UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST done, quitting"));
		UKismetSystemLibrary::QuitGame(W, W->GetFirstPlayerController(), EQuitPreference::Quit, false);
		return;
	}
	const FCase* C = FindCase(TestCase);
	if (!C) { if (!bTestDone) TestReport(false, FString::Printf(TEXT("unknown case %s"), *TestCase)); return; }
	if (bTestDone) return;
	if (TestDeadline > 0.0 && RealClock > TestDeadline) { TestReport(false, FString::Printf(TEXT("deadline at step %d (state %s, stage %s)"), TestStep, ZoogiBattle::StateName(State), AZoogiGameMode::StageName(G->Stage))); return; }
	if (W->IsPaused() && TestStep != 4) return;
	if (TestStep == 0)
	{
		if (TestDeadline == 0.0) TestDeadline = RealClock + 240.0;
		if (G->Phase != EZoogiPhase::Aiming || G->Stage != EZoogiStage::Draw || G->Balls.Num() < 2 || !G->Arena) return;
		if (!C->bScriptP1) { TestStep = 5; TestDeadline = RealClock + 60.0 * C->Actions; return; }
		if (G->ActivePlayer != 0) return;
		TestScriptTurn();
		return;
	}
	switch (TestStep)
	{
	case 1: // Awaken
		if (G->Stage == EZoogiStage::Awaken && G->StageTime > 0.25f)
		{
			const EAw Aw = FindCase(TestCase)->Aw;
			const bool bOk = Aw == EAw::Infuse ? G->AwakenInfuse(0) : (Aw == EAw::Guard ? G->AwakenGuard(0) : G->AwakenSave());
			UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST awaken %s=%d"), Aw == EAw::Infuse ? TEXT("infuse") : (Aw == EAw::Guard ? TEXT("guard") : TEXT("save")), bOk ? 1 : 0);
			TestStep = 2;
		}
		else if (G->Stage == EZoogiStage::Battle) TestStep = 2;
		break;
	case 2: // aim + combo + commit
		if (G->IsAimStep() && G->StepTime > 0.3f)
		{
			AZoogiBall* T = G->Balls[TestTarget];
			if (TestAimLock) { G->LockOn(T, false); if (TestCase == TEXT("ko") || TestCase == TEXT("energy_gain") || TestCase == TEXT("unlocks")) G->AimPowerStep = 3; }
			else G->SetFreeAim(TestAim);
			if (TestCombo != NAME_None)
			{
				const bool bSel = SelectCombo(0, TestCombo, TEXT("test"));
				if (bSel && !TestAimLock) Pending.DeclaredTarget = TestTarget; // declared on the HUD (free aim has no lock)
				if (bSel && TestCase == TEXT("orbline_fail") && TestOrb >= 0) Pending.CommittedOrbs = { 100 + TestOrb };
				UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST combo %s selected=%d"), *TestCombo.ToString(), bSel ? 1 : 0);
			}
			const bool bOk = G->CommitLock();
			UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST aim lock=%d committed=%d"), TestAimLock ? 1 : 0, bOk ? 1 : 0);
			bTestInjectOffer = TestCase == TEXT("timeout") || TestCase == TEXT("queued_effects");
			TestStep = 3;
		}
		break;
	case 3: // launched
		if (G->Phase == EZoogiPhase::Rolling)
		{
			TestActionId = CurrentActionId;
			if (TestCase == TEXT("timeout")) G->Balls[1]->bCPU = false; // the partner is human for the offer
			TestStep = 4;
		}
		break;
	case 4: // in flight: Air Reaction / Rail Release input (human P1) per case
		if ((TestCase == TEXT("rail_release") || TestCase == TEXT("midair_collision")) && IsRailOpen())
		{
			const UZoogiMiniGameRailRelease* RM = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame);
			const float At = TestCase == TEXT("midair_collision") ? Tune.RailEarlyWindow * 0.5f : Tune.RailEarlyWindow + Tune.RailCenterWindow * 0.5f;
			if (RM && RM->Elapsed >= At) SubmitRailRelease(TEXT("test"));
		}
		if (IsAirReactionOpen() && (TestCase == TEXT("air_entitled") || TestCase == TEXT("awaken_gate")))
		{
			const UZoogiMiniGame* M = ActiveMiniGame;
			if (M && M->InputLeft < Tune.MiniGameInputTimeout - 0.4f) SubmitAirChoice(EZoogiAirChoice::Brace, TEXT("test"));
		}
		break;
	case 5: // CPU-only cases: count actions (TestOnFinalized), then check
	default:
		break;
	}
	if (TestStep == 6 && RealClock >= TestWaitUntil) TestCheck();
}

void UZoogiBattleStateMachine::TestOnFinalized(const FZoogiGameplayTruthRecord& Truth, const FZoogiEpisodeRecognitionRecord& Rec, const FZoogiCinematicPlan& Plan)
{
	const FCase* C = FindCase(TestCase);
	if (!C || bTestDone) return;
	if (C->bScriptP1 && (TestStep < 4 || Truth.Actor != 0)) return; // only the scripted P1 action(s)
	if (!C->bScriptP1 && TestStep != 5) return;
	++TestActions;
	if (TestFinal.IsEmpty())
	{
		for (int32 i = 0; i < Truth.FinalBallPos.Num(); ++i)
			TestFinal += FString::Printf(TEXT("%sP%d(%.1f,%.1f%s)"), TestFinal.IsEmpty() ? TEXT("") : TEXT(" "), i + 1, Truth.FinalBallPos[i].X, Truth.FinalBallPos[i].Y, Truth.FinalBallOut[i] ? TEXT(",out") : TEXT(""));
	}
	TestNotes.Add(FString::Printf(TEXT("a%d:P%d tier=%d tags=%d ring=%d"), Truth.ActionId, Truth.Actor + 1, Rec.Tier, Rec.Tags.Num(), Truth.RingOuts.Num()));
	if (TestActions < C->Actions)
	{
		if (C->bScriptP1) TestStep = 0; // set up P1's next turn when it comes
		return;
	}
	TestResolveAt = RealClock;
	TestStep = 6;
	TestWaitUntil = RealClock + (TestCase == TEXT("timeout") ? Tune.FollowUpOfferTimeout + 3.0 : 2.5); // let the next turn start
}

void UZoogiBattleStateMachine::TestOnMatchEnd()
{
	// a CPU-only case whose match ended before its action count: check what was played (the game quits right after)
	const FCase* C = FindCase(TestCase);
	if (bTestDone || bInTestCheck || !C) return;
	if (C->bScriptP1)
	{
		if (TestStep == 6) TestCheck(); // the scripted action is done: check now (the game quits right after)
		else TestReport(false, FString::Printf(TEXT("match ended at step %d before the scripted check (actions %d/%d)"), TestStep, TestActions, C->Actions));
		return;
	}
	if (TestStep != 5) return;
	UE_LOG(LogZoogi, Log, TEXT("BATTLE_TEST %s: match ended after %d/%d actions, checking now"), *TestCase, TestActions, C->Actions);
	TestCheck();
}

void UZoogiBattleStateMachine::TestCheck()
{
	AZoogiGameMode* G = GM();
	TArray<FString> Fails;
	FString D;
	TGuardValue<bool> InCheck(bInTestCheck, true);
	auto Need = [&](bool b, const FString& What) { if (!b) Fails.Add(What); };
	auto TrailHas = [&](const TCHAR* S) { for (const FString& T : StateTrail) if (T.StartsWith(S)) return true; return false; };
	auto Last = [&]() -> const FZoogiPendingSegment* { return History.Num() ? &History.Last() : nullptr; };
	const bool bNext = G->ActivePlayer != 0 && State == EZoogiBattleState::WaitForCommand;
	if (G->Balls.IsValidIndex(0) && G->Balls[0]) G->Balls[0]->bCPU = true;
	if (TestCase == TEXT("timeout") && G->Balls.IsValidIndex(1)) G->Balls[1]->bCPU = true;
	Need(InvalidTransitions == 0, TEXT("invalid transitions"));
	const FString& Case = TestCase;
	if (Case == TEXT("roll"))
	{
		for (const TCHAR* S : { TEXT("WAIT_FOR_COMMAND>ACTION_PREVIEW"), TEXT("ACTION_PREVIEW>PHYSICS_RESOLVE"), TEXT("PHYSICS_RESOLVE>RESULT"), TEXT("RESULT>EPISODE_EVENT"), TEXT("EPISODE_EVENT>NEXT_TURN"), TEXT("NEXT_TURN>WAIT_FOR_COMMAND") })
			Need(TrailHas(S), FString::Printf(TEXT("missing %s"), S));
		Need(bNext, TEXT("next player not waiting"));
	}
	else if (Case == TEXT("miss_no_consolation"))
	{
		Need(SuppressedMissCards >= 1, TEXT("miss card not suppressed"));
		Need(bNext, TEXT("turn did not advance"));
		D = FString::Printf(TEXT("suppressed_miss=%d"), SuppressedMissCards);
	}
	else if (Case == TEXT("timeout"))
	{
		Need(FollowUpTimeouts == 1, TEXT("no follow-up timeout"));
		Need(G->ActivePlayer >= 2, TEXT("team B did not get the turn"));
		D = FString::Printf(TEXT("timeouts=%d limit=%.1fs active=P%d"), FollowUpTimeouts, Tune.FollowUpOfferTimeout, G->ActivePlayer + 1);
	}
	else if (Case == TEXT("shatter_basic") || Case == TEXT("halo_fail") || Case == TEXT("orbline_fail") || Case == TEXT("queued_effects"))
	{
		const FZoogiPendingSegment* S = Last();
		Need(ShatterCount == 1 && S && S->Comic == EZoogiComicState::Shattered, TEXT("no shatter"));
		Need(TrailHas(TEXT("PHYSICS_RESOLVE>SHATTER_FAILURE")) && TrailHas(TEXT("SHATTER_FAILURE>NEXT_TURN")), TEXT("FAIL->SHATTER->NEXT path"));
		Need(bNext, TEXT("next player not waiting"));
		Need(Players[0].SegmentsCompleted == 0, TEXT("segment awarded"));
		if (Case == TEXT("queued_effects")) Need(!G->bFollowUpOffered, TEXT("follow-up offer left pending"));
		D = FString::Printf(TEXT("segment=%s reason=\"%s\" energy=%.0f spent=%.0f"), S ? *S->SegmentId : TEXT("-"), *LastShatterReason, GetEnergy(0), Players[0].EnergySpent);
	}
	else if (Case == TEXT("energy_gain"))
	{
		const float E1 = GetEnergy(0);
		float Others = 0.f;
		for (int32 i = 1; i < Players.Num(); ++i) Others += Players[i].EnergyGained;
		Need(E1 > 0.f && E1 <= Tune.EnergyCap, TEXT("P1 gained nothing / over cap"));
		Need(Others == 0.f, TEXT("energy leaked to non-actors"));
		// cap + table: a gain at cap-1 stops at the cap; a changed table value changes the gain
		UZoogiEpisodeEnergyComponent* EC = Energy(0);
		EC->SetValue(Tune.EnergyCap - 1.f, TEXT("test_cap"));
		AddEnergy(0, Tune.Gain(TEXT("Hit")), TEXT("Hit"), -1);
		Need(FMath::IsNearlyEqual(EC->Value, Tune.EnergyCap), TEXT("cap not respected"));
		const float Old = Tune.Gain(TEXT("Hit"));
		Tune.Gains.Add(TEXT("Hit"), Old + 3.f);
		EC->SetValue(0.f, TEXT("test_table"));
		AddEnergy(0, Tune.Gain(TEXT("Hit")), TEXT("Hit"), -1);
		Need(FMath::IsNearlyEqual(EC->Value, Old + 3.f), TEXT("table value not used"));
		Tune.Gains.Add(TEXT("Hit"), Old);
		D = FString::Printf(TEXT("p1_energy_after_shot=%.0f others=%.0f gains_src=%s"), E1, Others, *Tune.GainsSource);
	}
	else if (Case == TEXT("combo_spend") || Case == TEXT("halo_success") || Case == TEXT("orbline_success"))
	{
		const FZoogiPendingSegment* S = Last();
		Need(S && S->Comic == EZoogiComicState::Completed, TEXT("segment not completed"));
		Need(Players[0].EnergySpent > 0.f, TEXT("cost not paid"));
		Need(!IsAwakened(0), TEXT("awakening changed by energy"));
		bool bLedger = false;
		if (Ledger && S) for (const FZoogiEpisodeLedgerEvent& E : Ledger->Events) if (E.episode_segment_id == S->SegmentId && E.comic_state == TEXT("COMPLETED") && !E.clip_id.IsEmpty()) bLedger = true;
		Need(bLedger, TEXT("ledger has no COMPLETED event with clip"));
		D = FString::Printf(TEXT("segment=%s clip=%s detail=\"%s\" spent=%.0f energy=%.0f"), S ? *S->SegmentId : TEXT("-"), S ? *S->ClipId.ToString() : TEXT("-"), S ? *S->Detail : TEXT("-"), Players[0].EnergySpent, GetEnergy(0));
	}
	else if (Case == TEXT("halo_pass") || Case == TEXT("halo_rim"))
	{
		const FZoogiPendingSegment* S = Last();
		if (Case == TEXT("halo_pass"))
		{
			Need(HaloPasses >= 1 && HaloRims == 0, TEXT("no clean halo pass"));
			Need(S && S->HaloEvent == TEXT("pass"), TEXT("segment did not see the pass"));
			Need(S && S->Comic == EZoogiComicState::Completed, TEXT("halo pass bank not COMPLETED"));
		}
		else
		{
			Need(HaloRims >= 1 && HaloPasses == 0, TEXT("no rim-only hit"));
			Need(S && S->HaloEvent == TEXT("rim"), TEXT("segment did not see the rim"));
			Need(S && (S->Comic == EZoogiComicState::Completed || S->Comic == EZoogiComicState::Shattered), TEXT("segment unresolved"));
		}
		bool bZone = false;
		if (Ledger) for (const FZoogiEpisodeLedgerEvent& E : Ledger->Events) if (E.kind == TEXT("ACTION") && E.actor_player == 0 && E.zones.Contains(TEXT("aerial"))) bZone = true;
		Need(bZone, TEXT("ledger action lacks the aerial zone tag"));
		D = FString::Printf(TEXT("passes=%d rims=%d segment=%s comic=%s detail=\"%s\""), HaloPasses, HaloRims, S ? *S->SegmentId : TEXT("-"), S ? ZoogiBattle::ComicName(S->Comic) : TEXT("-"), S ? *S->Detail : TEXT("-"));
	}
	else if (Case == TEXT("sky_bumper"))
	{
		const AZoogiAerialProp* Bp = Props.Num() ? Props[0].Get() : nullptr;
		Need(Bp && Bp->Rebounds >= 1, TEXT("no bumper rebound"));
		if (Bp && Bp->Rebounds >= 1)
		{
			const FVector In = Bp->LastIn, Out = Bp->LastOut, N = Bp->LastNormal;
			const float Want = FMath::Min(In.Size() * Tune.SkyBumperRebound, Tune.SkyBumperMaxSpeed);
			FVector Refl = In - 2.f * FVector::DotProduct(In, N) * N;
			Refl.Z = 0.f;
			Need(FMath::IsNearlyEqual(Out.Size(), Want, Want * 0.01f), TEXT("rebound speed != min(in x mul, cap)"));
			Need(FVector::DotProduct(Out.GetSafeNormal(), Refl.GetSafeNormal()) > 0.999f, TEXT("rebound not the reflection about the contact normal"));
			Need(FVector::DotProduct(Out, N) > 0.f, TEXT("rebound into the bumper"));
			D = FString::Printf(TEXT("in=%.0f out=%.0f want=%.0f mul=%.2f n=(%.2f,%.2f) post_speed=%.0f "), In.Size(), Out.Size(), Want, Tune.SkyBumperRebound, N.X, N.Y, BumperPostSpeed);
			Need(BumperPostSpeed > 0.8f * Want, TEXT("the simulation did not carry the rebound"));
		}
		const FZoogiPendingSegment* S = Last();
		Need(S && (S->Comic == EZoogiComicState::Completed || S->Comic == EZoogiComicState::Shattered), TEXT("segment unresolved"));
		Need(S && S->BumperAt >= 0.f, TEXT("segment did not see the bumper"));
		D += FString::Printf(TEXT("segment=%s comic=%s detail=\"%s\""), S ? *S->SegmentId : TEXT("-"), S ? ZoogiBattle::ComicName(S->Comic) : TEXT("-"), S ? *S->Detail : TEXT("-"));
	}
	else if (Case == TEXT("rail_release"))
	{
		const FZoogiPendingSegment* S = Last();
		Need(RailEntries == 1 && RailReleases == 1, TEXT("rail not entered / released"));
		Need(LastRailWindow == EZoogiRailWindow::Center, TEXT("window != CENTER"));
		const FVector V = LastRailVelocity;
		const float Pitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(V.Z / FMath::Max(1.f, V.Size()), -1.f, 1.f)));
		const float WantSpeed = FMath::Max(LastRailEntrySpeed * Tune.RailSpeedMul, Tune.RailMinSpeed);
		Need(FMath::IsNearlyEqual(Pitch, Tune.RailCenterPitch, 0.5f), TEXT("center pitch"));
		Need(FMath::IsNearlyEqual(V.Size(), WantSpeed, 2.f), TEXT("release speed"));
		Need(S && S->Comic == EZoogiComicState::Completed && S->RailWindow == EZoogiRailWindow::Center, TEXT("segment not COMPLETED with CENTER"));
		Need(TrailHas(TEXT("PHYSICS_RESOLVE>MINIGAME")) && TrailHas(TEXT("MINIGAME>PHYSICS_RESOLVE")), TEXT("MINIGAME in PHYSICS_RESOLVE path"));
		// the three windows map to the three launch vectors (pure, deterministic)
		const FVector F(1, 0, 0), P(0, -500, 0); // centre on the +Y side -> late banks left (+yaw)
		const FVector E = UZoogiMiniGameRailRelease::LaunchVelocity(Tune, EZoogiRailWindow::Early, F, P, 1000.f);
		const FVector C = UZoogiMiniGameRailRelease::LaunchVelocity(Tune, EZoogiRailWindow::Center, F, P, 1000.f);
		const FVector L = UZoogiMiniGameRailRelease::LaunchVelocity(Tune, EZoogiRailWindow::Late, F, P, 1000.f);
		auto PitchOf = [](const FVector& X) { return FMath::RadiansToDegrees(FMath::Asin(X.Z / X.Size())); };
		const float LYaw = FMath::RadiansToDegrees(FMath::Atan2(L.Y, L.X));
		Need(FMath::IsNearlyEqual(PitchOf(E), Tune.RailHighPitch, 0.1f) && FMath::IsNearlyEqual(PitchOf(C), Tune.RailCenterPitch, 0.1f) && FMath::IsNearlyEqual(PitchOf(L), Tune.RailLatePitch, 0.1f), TEXT("window pitches"));
		Need(FMath::IsNearlyEqual(LYaw, Tune.RailLateYaw, 0.1f), TEXT("late bank yaw toward the centre"));
		Need(UZoogiMiniGameRailRelease::WindowAt(Tune, 0.01f) == EZoogiRailWindow::Early && UZoogiMiniGameRailRelease::WindowAt(Tune, Tune.RailEarlyWindow + 0.01f) == EZoogiRailWindow::Center
			&& UZoogiMiniGameRailRelease::WindowAt(Tune, Tune.RailEarlyWindow + Tune.RailCenterWindow + 0.01f) == EZoogiRailWindow::Late
			&& UZoogiMiniGameRailRelease::WindowAt(Tune, Tune.RailEarlyWindow + Tune.RailCenterWindow + Tune.RailLateWindow + 0.01f) == EZoogiRailWindow::None, TEXT("window table"));
		D = FString::Printf(TEXT("window=%s pitch=%.1f speed=%.0f entry=%.0f early_pitch=%.1f late_yaw=%.1f late_pitch=%.1f segment=%s"), ZoogiBattle::RailWindowName(LastRailWindow), Pitch, V.Size(), LastRailEntrySpeed,
			PitchOf(E), LYaw, PitchOf(L), S ? ZoogiBattle::ComicName(S->Comic) : TEXT("-"));
	}
	else if (Case == TEXT("rail_timeout"))
	{
		const FZoogiPendingSegment* S = Last();
		Need(RailEntries == 1 && RailTimeouts == 1 && RailReleases == 0, TEXT("no rail timeout"));
		Need(ShatterCount == 1 && S && S->Comic == EZoogiComicState::Shattered, TEXT("no shatter"));
		Need(TrailHas(TEXT("MINIGAME>SHATTER_FAILURE")) && TrailHas(TEXT("SHATTER_FAILURE>PHYSICS_RESOLVE")), TEXT("shatter -> ordinary resolution"));
		Need(bNext, TEXT("turn did not advance"));
		Need(Players[0].SegmentsCompleted == 0, TEXT("segment awarded"));
		D = FString::Printf(TEXT("reason=\"%s\" segment=%s"), *LastShatterReason, S ? *S->SegmentId : TEXT("-"));
	}
	else if (Case == TEXT("energy_full_no_awaken"))
	{
		Need(!IsAwakened(0), TEXT("full energy awakened P1"));
		Need(!AirOfferedTo.Contains(0), TEXT("reaction offered"));
		D = FString::Printf(TEXT("energy=%.0f air_events=%d offered=%d"), GetEnergy(0), AirOffersSeen, AirOffersAllowed);
	}
	else if (Case == TEXT("awaken_gate") || Case == TEXT("air_entitled"))
	{
		Need(AirOfferedTo.Contains(0), TEXT("awakened P1 not offered"));
		Need(Players[0].Reactions == 1, TEXT("reaction not recorded"));
		bool bLedger = false;
		if (Ledger) for (const FZoogiEpisodeLedgerEvent& E : Ledger->Events) if (E.kind == TEXT("REACTION") && E.actor_player == 0) bLedger = true;
		Need(bLedger, TEXT("no REACTION ledger event"));
		if (Case == TEXT("awaken_gate"))
		{
			FString Why3;
			const bool b3 = CanOfferAir(2, &Why3);
			Need(!b3, TEXT("P3 (not awakened) would be offered"));
			for (int32 P : AirOfferedTo) Need(IsAwakened(P) || P == 0, FString::Printf(TEXT("P%d offered without awakening"), P + 1));
			D = FString::Printf(TEXT("p1_offered=1 p3_gate=%d(%s)"), b3 ? 1 : 0, *Why3);
		}
		const FZoogiPendingSegment* S = Last();
		D += FString::Printf(TEXT(" reaction=%s result=%s detail=\"%s\""), S ? *S->SegmentId : TEXT("-"), S ? ZoogiBattle::ComicName(S->Comic) : TEXT("-"), S ? *S->Detail : TEXT("-"));
	}
	else if (Case == TEXT("air_not_entitled"))
	{
		Need(AirOffersSeen >= 1, TEXT("P1 never airborne"));
		Need(AirOffersAllowed == 0, TEXT("offered without awakening"));
		D = FString::Printf(TEXT("air_events=%d offered=%d"), AirOffersSeen, AirOffersAllowed);
	}
	else if (Case == TEXT("air_fail"))
	{
		Need(AirOfferedTo.Contains(0), TEXT("not offered"));
		Need(ShatterCount >= 1, TEXT("no shatter"));
		Need(TrailHas(TEXT("MINIGAME>SHATTER_FAILURE")) && TrailHas(TEXT("SHATTER_FAILURE>PHYSICS_RESOLVE")), TEXT("shatter -> ordinary resolution"));
		Need(bNext, TEXT("turn did not advance"));
		D = FString::Printf(TEXT("reason=\"%s\""), *LastShatterReason);
	}
	else if (Case == TEXT("ko"))
	{
		bool bLedgerKO = false;
		for (const FZoogiEpisodeLedgerEvent& E : Ledger->Events) if (E.kind == TEXT("ACTION") && E.actor_player == 0 && E.ring_out) bLedgerKO = true;
		const bool bGameOut = G->Balls[TestTarget]->bOut || G->Balls[TestTarget]->bKnockedOut || Players[TestTarget].TimesKOd > 0;
		Need(bLedgerKO, TEXT("no KO in ledger"));
		Need(bLedgerKO == bGameOut, TEXT("ledger KO != game"));
		Need(Players[0].KOs >= 1, TEXT("attacker KO count"));
		if (!bMatchEnded) OnMatchEnd(TEXT("test_end"));
		bool bDefeat = false;
		for (const FZoogiPlayerAssembly& A : Assembler->PlayerResults) if (A.player == TestTarget) for (const FZoogiEpisodeFragment& F : A.fragments) if (F.kind == TEXT("defeat")) bDefeat = true;
		Need(bDefeat, TEXT("no defeat fragment for the victim"));
		D = FString::Printf(TEXT("kos=%d victim_out=%d defeat_fragment=%d"), Players[0].KOs, bGameOut ? 1 : 0, bDefeat ? 1 : 0);
	}
	else if (Case == TEXT("segment_retained"))
	{
		const bool bFirst = History.Num() >= 2 && History[0].Comic == EZoogiComicState::Completed;
		const bool bSecond = History.Num() >= 2 && History.Last().Comic == EZoogiComicState::Shattered;
		Need(bFirst, TEXT("first segment not kept COMPLETED"));
		Need(bSecond, TEXT("second segment not SHATTERED"));
		if (!bMatchEnded) OnMatchEnd(TEXT("test_end"));
		bool bIn1 = false, bIn2 = false;
		for (const FZoogiPlaylistEntry& P : Assembler->Playlist)
		{
			if (History.Num() && P.clip_id == History[0].ClipId.ToString()) bIn1 = true;
			if (History.Num() >= 2 && P.clip_id == History.Last().ClipId.ToString()) bIn2 = true;
		}
		Need(bIn1 && !bIn2, TEXT("playlist retention"));
		D = FString::Printf(TEXT("history=%d first=%s second=%s in_playlist=%d/%d"), History.Num(), History.Num() ? ZoogiBattle::ComicName(History[0].Comic) : TEXT("-"),
			History.Num() ? ZoogiBattle::ComicName(History.Last().Comic) : TEXT("-"), bIn1 ? 1 : 0, bIn2 ? 1 : 0);
	}
	else if (Case == TEXT("unlocks"))
	{
		// strong winner P1 vs the KO'd loser; thresholds lowered for this run, then the defaults (ordinary win -> no full)
		const FZoogiBattleTuning Saved = Tune;
		Tune.FullEpisodeMinSegments = 1; Tune.FullEpisodePerf = 1.f; Tune.FullEpisodeMaxShatters = 0;
		LedgerSimple(TEXT("MATCH_END"), 0, TEXT("test_end"));
		Assembler->Assemble(this, Ledger, 0, -1, G->bComboMode, MatchId + TEXT("_a"), TEXT("test_strong"));
		FString U1, U2;
		for (const FZoogiPlayerAssembly& A : Assembler->PlayerResults) { if (A.player == 0) U1 = A.unlock; if (A.player == TestTarget) U2 = A.unlock; }
		Tune = Saved;
		Assembler->Assemble(this, Ledger, 0, -1, G->bComboMode, MatchId + TEXT("_b"), TEXT("test_defaults"));
		FString U1d;
		for (const FZoogiPlayerAssembly& A : Assembler->PlayerResults) if (A.player == 0) U1d = A.unlock;
		Need(U1 == TEXT("full"), TEXT("winner not full with met thresholds"));
		Need(U2 == TEXT("partial"), TEXT("loser not partial"));
		Need(U1d != TEXT("full"), TEXT("ordinary win unlocked the full episode"));
		D = FString::Printf(TEXT("winner=%s loser=%s winner_default_thresholds=%s"), *U1, *U2, *U1d);
	}
	else if (Case == TEXT("midair_collision"))
	{
		const UZoogiEpisodeSubsystem* Ep = Episode();
		bool bAirHit = false;
		if (Ep)
		{
			const FZoogiGameplayTruthRecord& T = Ep->GetTruth();
			for (const FZoogiContactEvent& E : T.Contacts)
			{
				if (!((E.Body == 0 && E.OtherBody == TestTarget) || (E.Body == TestTarget && E.OtherBody == 0))) continue;
				for (const FZoogiAirSegment& A : T.Air) if (A.Body == 0 && E.Time >= A.Start && E.Time <= A.End) bAirHit = true;
				D += FString::Printf(TEXT("contact t=%.2f z=%.0f; "), E.Time, E.Location.Z);
			}
		}
		Need(bAirHit, TEXT("no contact while P1 airborne"));
		Need(LastRailWindow == EZoogiRailWindow::Early, TEXT("no EARLY rail launch"));
		D += FString::Printf(TEXT("rail=%s v=(%.0f,%.0f,%.0f)"), ZoogiBattle::RailWindowName(LastRailWindow), LastRailVelocity.X, LastRailVelocity.Y, LastRailVelocity.Z);
	}
	else if (Case == TEXT("one_v_one") || Case == TEXT("replay_order") || Case == TEXT("four_player_2v2"))
	{
		if (!bMatchEnded) OnMatchEnd(TEXT("test_end"));
		int32 Acts = 0, PrevSeq = -1;
		bool bOrder = true, bActors = true;
		for (const FZoogiEpisodeLedgerEvent& E : Ledger->Events) if (E.kind == TEXT("ACTION")) ++Acts;
		for (const FZoogiPlaylistEntry& P : Assembler->Playlist)
		{
			if (P.seq < 0) continue;
			if (P.seq < PrevSeq) bOrder = false;
			PrevSeq = P.seq;
			if (P.kind != TEXT("finale") && Ledger->Events.IsValidIndex(P.seq) && Ledger->Events[P.seq].actor_id != P.actor_id) bActors = false;
		}
		Need(Acts >= TestActions, TEXT("ledger missing actions"));
		Need(bOrder, TEXT("playlist out of ledger order"));
		Need(bActors, TEXT("playlist actor != ledger actor"));
		FString Order;
		for (int32 i = 0; i < TestTurnOrder.Num(); ++i) Order += FString::Printf(TEXT("%sP%d%s"), Order.IsEmpty() ? TEXT("") : TEXT(","), TestTurnOrder[i] + 1, TestTurnFollowUp[i] ? TEXT("fu") : TEXT(""));
		if (Case == TEXT("one_v_one"))
			for (int32 i = 1; i < TestTurnOrder.Num(); ++i) Need(TestTurnOrder[i] != TestTurnOrder[i - 1], TEXT("1v1 did not alternate"));
		if (Case == TEXT("four_player_2v2"))
		{
			int32 PrevTeam = -1;
			for (int32 i = 0; i < TestTurnOrder.Num(); ++i)
			{
				if (TestTurnFollowUp[i]) { Need(AZoogiGameMode::TeamOfIndex(TestTurnOrder[i]) == PrevTeam, TEXT("follow-up outside its team turn")); continue; }
				const int32 Tm = AZoogiGameMode::TeamOfIndex(TestTurnOrder[i]);
				Need(Tm != PrevTeam, TEXT("team turns did not alternate"));
				PrevTeam = Tm;
			}
			for (const FZoogiBattlePlayerState& P : Players) Need(P.Contributions >= 1, FString::Printf(TEXT("P%d has no contribution"), P.Player + 1));
			for (const FZoogiPlayerAssembly& A : Assembler->PlayerResults)
				for (int32 Ix : A.edit)
				{
					const FZoogiPlaylistEntry& P = Assembler->Playlist[Ix];
					Need(P.kind == TEXT("bridge") || P.kind == TEXT("finale") || P.players.Contains(A.player), FString::Printf(TEXT("P%d edit has a foreign entry"), A.player + 1));
				}
		}
		D = FString::Printf(TEXT("order=%s ledger_actions=%d playlist=%d"), *Order, Acts, Assembler->Playlist.Num());
	}
	FString F = FString::Join(Fails, TEXT("; "));
	TestReport(Fails.Num() == 0, D + (F.IsEmpty() ? FString() : FString::Printf(TEXT(" FAILS: %s"), *F)));
}
