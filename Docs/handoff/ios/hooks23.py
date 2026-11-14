import sys, os; sys.path.insert(0,'/tmp'); from ed import ed
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-combined/Source/ZoogiRoll/')
# ---------------- GameMode.h
ed(R+'Public/ZoogiGameMode.h',[
('class ZOOGIROLL_API AZoogiGameMode : public AGameModeBase\n{\n\tGENERATED_BODY()\n',
 'class ZOOGIROLL_API AZoogiGameMode : public AGameModeBase\n{\n\tGENERATED_BODY()\n\tfriend class UZoogiFunSys; // Oct 29 build 23: action / fun / timing pack (ZoogiActionFun.h)\n'),
('\tconstexpr float DrawCPU = 0.9f;', '\tconstexpr float DrawCPU = 0.35f;        // build 23 #2: snappy CPU turns (was 0.9)'),
('\tconstexpr float AwakenCPU = 0.9f;', '\tconstexpr float AwakenCPU = 0.45f;  // build 23 #2 (was 0.9)'),
('\tconstexpr float CPULockAt = 0.35f;', '\tconstexpr float CPULockAt = 0.15f;  // build 23 #2 (was 0.35)'),
('\tconstexpr float CPUMain = 1.2f;', '\tconstexpr float CPUMain = 0.55f;    // build 23 #2 (was 1.2)'),
('\tconstexpr float CPUBattleHold = 0.3f;', '\tconstexpr float CPUBattleHold = 0.15f; // build 23 #2 (was 0.3)'),
('\tconstexpr float SettleTime = 0.8f;', '\tconstexpr float SettleTime = 0.45f;      // build 23 #1 (was 0.8)'),
('\tconstexpr float ResolveMin = 0.7f;', '\tconstexpr float ResolveMin = 0.45f;      // build 23 #1 (was 0.7)'),
('\tconstexpr float EndBeat = 0.35f;', '\tconstexpr float EndBeat = 0.2f;          // build 23 #1 (was 0.35)'),
('\tbool bCPUPlanned = false;', '\tbool bCPUPlanned = false;\n\tbool bCPUPlanDeferred = false;   // build 23 #1: bodies still drifting at turn start - plan when they settle\n\tbool bReplayDoneThisEnd = false; // build 23 #10: round-end replay already offered for this EndTurn'),
])
# ---------------- GameMode.cpp
ed(R+'Private/ZoogiGameMode.cpp',[
('#include "ZoogiAnimeSubsystem.h"\n', '#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiActionFun.h"\n'),
('\tif (Ball->bCPU) PlanCPUShot();\n\tClearLock();\n\tSetStage(EZoogiStage::Draw);\n',
 '''	bReplayDoneThisEnd = false;
	bCPUPlanDeferred = false;
	if (Ball->bCPU)
	{
		// Oct 29 (build 23 #1): after an early turn end bodies can still be drifting - the CPU plans once they settle (1.2 s at most)
		if (AllAtRest() || FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoEarlySettle"))) PlanCPUShot(); else bCPUPlanDeferred = true;
	}
	ClearLock();
	SetStage(EZoogiStage::Draw);
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) Fun->OnTurnBegin(this, PlayerIdx);
'''),
('\tif (bComboMode) { ComboEndTurn(); return; }\n\tif (AZoogiBall* Shooter = GetActiveBall())\n',
 '''	if (bComboMode) { ComboEndTurn(); return; }
	// Oct 29 (build 23 #10): at a round end the best moment of the round replays first - EndTurn waits while it plays
	if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this))
	{
		if (Fun->IsReplaying()) return;
		if (!bReplayDoneThisEnd && !(bLaunchFlow && LaunchFlow.bOpening))
		{
			bReplayDoneThisEnd = true;
			const bool bRoundEnd = (ActivePlayer + 1) % FMath::Max(1, Balls.Num()) == RoundStartPlayer || OrbsLeft() == 0;
			if (bRoundEnd && Fun->StartReplay(this)) return;
		}
	}
	if (AZoogiBall* Shooter = GetActiveBall())
'''),
('\tif (Phase == EZoogiPhase::DropIntro) TickDropIntro(Dt);\n',
 '\tif (Phase == EZoogiPhase::DropIntro) TickDropIntro(Dt);\n\tif (UZoogiFunSys* FunR = UZoogiFunSys::Get(this)) if (FunR->IsReplaying()) return; // build 23 #10: the highlight replay owns the bodies\n'),
('\t\tAZoogiBall* Ball = GetActiveBall();\n\t\tif (Ball && Ball->bCPU && bCPUPlanned && !bArtifactMode) CPUPowerTick();\n',
 '''		AZoogiBall* Ball = GetActiveBall();
		if (bCPUPlanDeferred && Ball && Ball->bCPU && (AllAtRest() || PhaseTime > 1.2f))
		{
			bCPUPlanDeferred = false;
			PlanCPUShot();
			if (Stage == EZoogiStage::Battle) StepTime = 0.f;
			UE_LOG(LogZoogi, Log, TEXT("CPU plan after the drift settled (%.2f s into the turn)"), PhaseTime);
		}
		if (Ball && Ball->bCPU && bCPUPlanned && !bArtifactMode) CPUPowerTick();
'''),
('''		{
			UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
			const bool bBusy = Anime && Anime->IsBusy();
			if (PhaseTime > 0.25f && AllAtRest() && !bBusy)''',
'''		// Oct 29 night (Evan, build 23 #1): call the turn as soon as nothing is fast and nothing is heading over an edge -
		// slow bodies finish rolling while the next player aims (each body's stopping point from its speed and the floor's
		// rolling decel must stay on the court). -ZoogiNoEarlySettle = old wait-for-rest.
		{
			static const bool bEarlyOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoEarlySettle"));
			static float EarlyFor = 0.f;
			if (PhaseTime <= Dt * 1.5f) EarlyFor = 0.f;
			UZoogiAnimeSubsystem* AnE = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
			const bool bBusyE = PullLeft > 0.f || RicochetTimer >= 0.f || Clones.Num() > 0 || (AnE && AnE->IsBusy()) || ClimaxLeft > 0.f;
			if (bEarlyOn && !bComboMode && Phase == EZoogiPhase::Rolling && PhaseTime > 1.4f && !bBusyE && RoamState == EZoogiRoamState::None && !AllAtRest())
			{
				const float Dec = FMath::Max(Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel, 40.f);
				bool bSafe = true;
				float MaxSp = 0.f;
				auto Check = [&](const FVector& L, const FVector& V, float R)
				{
					const FVector2D V2(V.X, V.Y);
					const float Sp = V2.Size();
					MaxSp = FMath::Max(MaxSp, Sp);
					if (Sp > 130.f || FMath::Abs(V.Z) > 120.f) { bSafe = false; return; }
					if (Sp < 5.f) return;
					const FVector2D E = FVector2D(L.X, L.Y) + V2 / Sp * (Sp * Sp / (2.f * Dec) + R);
					if ((Arena && Arena->IsOffCourt(E)) || AZoogiArena::IsRingOut(FVector(E.X, E.Y, L.Z))) bSafe = false;
				};
				for (AZoogiBall* Bb : Balls) if (Bb && !Bb->bOut && !Bb->bParked && !IsOnLaunchPad(Bb) && bSafe) Check(Bb->BallLocation(), Bb->GetBallVelocity(), ZoogiConst::ZoogiRadius);
				for (AZoogiOrb* Oo : Orbs) if (Oo && !Oo->bOut && Oo->Sphere && bSafe) Check(Oo->OrbLocation(), Oo->Sphere->GetPhysicsLinearVelocity(), ZoogiConst::OrbRadius);
				EarlyFor = bSafe ? EarlyFor + Dt : 0.f;
				if (EarlyFor >= 0.3f)
				{
					UE_LOG(LogZoogi, Log, TEXT("SETTLE early turn end at %.1f s (fastest body %.0f cm/s, every stopping point on the court)"), PhaseTime, MaxSp);
					EarlyFor = 0.f;
					CheckKnockoffs();
					SetStage(EZoogiStage::Resolve);
				}
			}
			else EarlyFor = 0.f;
		}
		if (Phase == EZoogiPhase::Rolling)
		{
			UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
			const bool bBusy = Anime && Anime->IsBusy();
			if (PhaseTime > 0.25f && AllAtRest() && !bBusy)'''),
('\t\t\t\tint32 Pts = O->bStar ? 2 * ZoogiConst::ScoreOrb : ZoogiConst::ScoreOrb;\n',
 '\t\t\t\tint32 Pts = O->bStar ? 2 * ZoogiConst::ScoreOrb : ZoogiConst::ScoreOrb;\n\t\t\t\tif (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) Pts *= Fun->OrbPointMul(this, Scorer); // build 23: final rounds x2 / DOUBLE boost\n'),
('''				AZoogiBall* A = Balls[B->LastHitPlayer];
				{
					FZoogiLedgerEntry E;
					E.Player = A->PlayerIndex; E.Points = ZoogiConst::ScorePlayer; E.bKO = true; E.Victim = B->PlayerIndex;''',
'''				AZoogiBall* A = Balls[B->LastHitPlayer];
				int32 KOPts = ZoogiConst::ScorePlayer;
				if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) KOPts *= Fun->KOPointMul(this, A->PlayerIndex); // build 23: DOUBLE boost
				{
					FZoogiLedgerEntry E;
					E.Player = A->PlayerIndex; E.Points = KOPts; E.bKO = true; E.Victim = B->PlayerIndex;'''),
('AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), ZoogiConst::ScorePlayer, A->PlayerIndex + 1)',
 'AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), KOPts, A->PlayerIndex + 1)'),
])
# ---------------- Targeting.cpp
ed(R+'Private/ZoogiTargeting.cpp',[
('#include "ZoogiAnimeSubsystem.h"\n', '#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiActionFun.h"\n'),
('\tif (bCinematicCam) return BattleSwing + (B && B->bCPU ? CPUBattleHold : BattleHold) + (IsLongLock() ? TargetBeat : 0.f);\n',
 '''	static const bool bOldPace = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPace"));
	if (!bOldPace && B && B->bCPU && !bComboMode && !bRoyaleMode) return bCinematicCam ? 0.5f + CPUBattleHold : 0.3f; // build 23 #2: snappy CPU turns
	if (bCinematicCam) return BattleSwing + (B && B->bCPU ? CPUBattleHold : BattleHold) + (IsLongLock() ? TargetBeat : 0.f);
'''),
('\tif (!bOldHold && B && !B->bCPU && !bCinematicCam && !bComboMode && !bRoyaleMode) return 0.f;\n',
 '\tif (!bOldHold && B && !B->bCPU && !bCinematicCam && !bComboMode && !bRoyaleMode) return 0.f;\n\tif (!bOldHold && B && !B->bCPU && bCinematicCam && !bComboMode && !bRoyaleMode) return 0.2f; // build 23 #3: CINE launches 0.2 s after release (the drag push-in already framed the shot)\n'),
('\t\tif (AZoogiBall* Settled = GetActiveBall()) ZoogiScoringUI::OnShotSettled(this, Settled->BallLocation());\n',
 '''		if (AZoogiBall* Settled = GetActiveBall()) ZoogiScoringUI::OnShotSettled(this, Settled->BallLocation());
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) // build 23 #8 on-fire streaks / #10 replay score
		{
			int32 FP = 0, FK = 0;
			for (const FZoogiLedgerEntry& LE : Ledger) if (LE.Player == ActivePlayer && LE.Points > 0) { FP += LE.Points; if (LE.bKO) ++FK; }
			Fun->OnTurnResolved(this, ActivePlayer, FP, FK);
		}
'''),
])
# ---------------- Anime
ed(R+'Public/ZoogiAnimeSubsystem.h',[
('\tTMap<FString, int32> FullPlays;', '\tTMap<FString, int32> FullPlays;\n\tTMap<FName, int32> EventFullPlays; // build 23 #4: one full cut-in per event type per match'),
])
ed(R+'Private/ZoogiAnimeSubsystem.cpp',[
('''	bShort = Mode == EZoogiAnimeMode::Short || E.bCPUOnly || Plays >= FMath::Max(1, Row.FullPlaysPerMatch);
	if (!bShort) ++Plays;
	Length = FMath::Max(0.2f, bShort ? Row.ShortLength : Row.Length);''',
'''	// Oct 29 night (Evan, build 23 #4): the full cut-in plays only the first time each event type happens in a match
	// (first KO, first combo ...); repeats are a 0.4 s flash. -ZoogiCutInAlways = old per-row rule.
	static const bool bCutInAlways = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCutInAlways"));
	int32& EvPlays = EventFullPlays.FindOrAdd(E.Event);
	bShort = Mode == EZoogiAnimeMode::Short || E.bCPUOnly || Plays >= FMath::Max(1, Row.FullPlaysPerMatch) || (!bCutInAlways && EvPlays >= 1);
	if (!bShort) { ++Plays; ++EvPlays; }
	Length = FMath::Max(0.2f, bShort ? (bCutInAlways ? Row.ShortLength : FMath::Min(Row.ShortLength, 0.4f)) : Row.Length);'''),
('\tFullPlays.Reset();\n', '\tFullPlays.Reset();\n\tEventFullPlays.Reset();\n'),
])
# ---------------- Ball
ed(R+'Private/ZoogiBall.cpp',[
('#include "ZoogiBall.h"\n', '#include "ZoogiBall.h"\n#include "ZoogiActionFun.h"\n'),
('\tif (G && PlayerIndex == Shooter && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor))) G->OnShooterContact(this, OtherActor);\n',
 '\tif (G && PlayerIndex == Shooter && (Cast<AZoogiBall>(OtherActor) || Cast<AZoogiOrb>(OtherActor))) G->OnShooterContact(this, OtherActor);\n\tif (G) if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) Fun->OnBallContact(this, OtherActor); // build 23: timing tap / parry / boosts\n'),
])
# ---------------- PC
ed(R+'Private/ZoogiPlayerController.cpp',[
('#include "ZoogiFrozenFun.h"\n', '#include "ZoogiFrozenFun.h"\n#include "ZoogiActionFun.h"\n'),
('\tif (bPtrPress)\n\t{\n\t\tstatic double LastTapTime',
 '''	bool bFunTap = false; // Oct 29 build 23: impact timing tap / parry / replay skip / CPU fast-forward
	if (bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick())))
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap();
	if (bPtrPress && !bFunTap)
	{
		static double LastTapTime'''),
])
# ---------------- ShowTicker drives FunSys
ed(R+'Private/ZoogiFrozenFun.cpp',[
('#include "ZoogiFrozenFun.h"\n', '#include "ZoogiFrozenFun.h"\n#include "ZoogiActionFun.h"\n'),
('\tif (UWorld* W = GetWorld()) if (UZoogiShowSys* S = W->GetSubsystem<UZoogiShowSys>()) S->ShowTick(Dt);\n',
 '\tif (UWorld* W = GetWorld()) if (UZoogiShowSys* S = W->GetSubsystem<UZoogiShowSys>()) S->ShowTick(Dt);\n\tif (UWorld* W = GetWorld()) if (UZoogiFunSys* F = W->GetSubsystem<UZoogiFunSys>()) F->FunTick(Dt); // build 23 action / fun pack\n'),
])
