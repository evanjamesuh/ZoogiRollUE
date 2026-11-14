// BATTLE ROYALE rules + CPU brain (see ZoogiRoyale.h). Free-for-all, real time: every Zoogi drives at once, ring-outs cost a
// life, the last Zoogi with lives left wins.
#include "ZoogiRoyale.h"
#include "ZoogiVoyager.h" // b237
#include "ZoogiShooter.h" // b234
#include "ZoogiRoyaleMix.h"
#include "ZoogiGameMode.h"
#include "ZoogiStageShift.h"
#include "ZoogiFront.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiAncientLayout.h"
#include "ZoogiFX.h"
#include "ZoogiCrackFX.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiArtifacts.h"
#include "ZoogiPlayerController.h"
#include "ZoogiScoringUI.h"
#include "ZoogiMorph.h"
#include "ZoogiPhysics.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"

// ------------------------------------------------------------------ tuning / bookkeeping

const FZoogiRoyaleTune& FZoogiRoyaleTune::Get()
{
	static const FZoogiRoyaleTune T = []
	{
		FZoogiRoyaleTune X;
		if (GConfig)
		{
			const TCHAR* S = TEXT("ZoogiRoll.Royale");
			GConfig->GetInt(S, TEXT("Lives"), X.Lives, GGameIni);
			auto F = [S](const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };
			F(TEXT("Accel"), X.Accel); F(TEXT("MaxSpeed"), X.MaxSpeed); F(TEXT("BrakeMul"), X.BrakeMul); F(TEXT("Grip"), X.Grip);
			F(TEXT("AirControl"), X.AirControl); F(TEXT("KnockFrac"), X.KnockFrac); F(TEXT("EdgeMargin"), X.EdgeMargin); F(TEXT("KnockMin"), X.KnockMin);
			F(TEXT("KnockMax"), X.KnockMax); F(TEXT("HitStun"), X.HitStun); F(TEXT("OverSpeedControl"), X.OverSpeedControl); F(TEXT("KnockLift"), X.KnockLift); F(TEXT("CreditSeconds"), X.CreditSeconds);
			F(TEXT("AwakenFillSeconds"), X.AwakenFillSeconds); F(TEXT("ChargePerHit"), X.ChargePerHit); F(TEXT("ChargePerKO"), X.ChargePerKO);
			F(TEXT("ChargePerOrb"), X.ChargePerOrb); F(TEXT("AwakenSeconds"), X.AwakenSeconds); F(TEXT("AwakenSpeedMul"), X.AwakenSpeedMul);
			F(TEXT("AwakenAccelMul"), X.AwakenAccelMul); F(TEXT("AwakenMassMul"), X.AwakenMassMul); F(TEXT("AwakenKnockMul"), X.AwakenKnockMul);
			F(TEXT("AwakenTakenMul"), X.AwakenTakenMul); F(TEXT("AwakenModelScale"), X.AwakenModelScale); F(TEXT("BurstRadius"), X.BurstRadius);
			F(TEXT("BurstSpeed"), X.BurstSpeed); F(TEXT("SurgeStart"), X.SurgeStart); F(TEXT("SurgeFull"), X.SurgeFull); F(TEXT("SurgeMax"), X.SurgeMax);
			F(TEXT("RespawnDelay"), X.RespawnDelay); F(TEXT("ShieldSeconds"), X.ShieldSeconds); F(TEXT("CountdownSeconds"), X.CountdownSeconds);
			F(TEXT("CpuSkill"), X.CpuSkill); F(TEXT("LookSensitivity"), X.LookSensitivity); F(TEXT("KeyLookRate"), X.KeyLookRate);
			F(TEXT("CamDist"), X.CamDist); F(TEXT("CamPitch"), X.CamPitch); F(TEXT("CamFov"), X.CamFov); F(TEXT("CamHeight"), X.CamHeight);
			F(TEXT("PortraitDistMul"), X.PortraitDistMul);
			F(TEXT("MorphCooldown"), X.MorphCooldown); F(TEXT("RobotSpeedMul"), X.RobotSpeedMul); F(TEXT("RobotAccelMul"), X.RobotAccelMul);
			F(TEXT("RobotMassMul"), X.RobotMassMul); F(TEXT("RobotTakenMul"), X.RobotTakenMul); F(TEXT("MorphDriveMul"), X.MorphDriveMul);
			F(TEXT("FireInterval"), X.FireInterval); F(TEXT("BoltSpeed"), X.BoltSpeed); F(TEXT("BoltRange"), X.BoltRange); F(TEXT("BoltRadius"), X.BoltRadius);
			F(TEXT("BoltKnock"), X.BoltKnock); F(TEXT("BoltStun"), X.BoltStun); F(TEXT("AimAssistDeg"), X.AimAssistDeg); F(TEXT("AutoFireRange"), X.AutoFireRange);
			F(TEXT("CpuAimError"), X.CpuAimError); F(TEXT("JumpSpeed"), X.JumpSpeed); F(TEXT("JumpCooldown"), X.JumpCooldown);
			F(TEXT("RobotCamDist"), X.RobotCamDist); F(TEXT("RobotCamPitch"), X.RobotCamPitch); F(TEXT("RobotCamHeight"), X.RobotCamHeight);
			F(TEXT("ChaseBack"), X.ChaseBack); F(TEXT("ChaseUp"), X.ChaseUp); F(TEXT("ChasePitch"), X.ChasePitch); F(TEXT("ChaseFov"), X.ChaseFov);
			F(TEXT("RobotChaseBack"), X.RobotChaseBack); F(TEXT("RobotChaseUp"), X.RobotChaseUp);
			F(TEXT("TracerSpeed"), X.TracerSpeed); F(TEXT("TracerLength"), X.TracerLength);
			GConfig->GetInt(S, TEXT("MaxHP"), X.MaxHP, GGameIni); GConfig->GetInt(S, TEXT("BoltDamage"), X.BoltDamage, GGameIni);
			F(TEXT("CritChance"), X.CritChance); F(TEXT("CritMul"), X.CritMul); F(TEXT("RamDamagePerSpeed"), X.RamDamagePerSpeed);
		}
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiRoyaleLives="), X.Lives);
		X.Lives = FMath::Clamp(X.Lives, 1, 9);
		X.CpuSkill = FMath::Clamp(X.CpuSkill, 0.f, 1.f);
		X.SurgeFull = FMath::Max(X.SurgeFull, X.SurgeStart + 1.f);
		X.FireInterval = FMath::Max(0.05f, X.FireInterval);
		X.BoltSpeed = FMath::Max(300.f, X.BoltSpeed);
		X.BoltSpeed = FMath::Max(X.BoltSpeed, X.TracerSpeed); // Oct 27: fast tracer rounds (the old ini BoltSpeed stays a floor)
		X.MaxHP = FMath::Max(1, X.MaxHP);
		// Oct 27 (Evan, build 9): no delay on TRANSFORM / JUMP. Caps win over the old ini values (1.0 s / 0.9 s); the short
		// cooldown only stops one press counting twice.
		X.MorphCooldown = FMath::Min(X.MorphCooldown, ZoogiRoyaleFeel::MorphCooldown);
		X.JumpCooldown = FMath::Min(X.JumpCooldown, ZoogiRoyaleFeel::JumpCooldown);
		X.MorphDriveMul = 1.f; // full drive while the shell opens / closes
		{ // Oct 27: -ZoogiChaseBack= / -ZoogiChaseUp= / -ZoogiChaseFov= for camera tuning runs
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiChaseBack="), X.ChaseBack);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiChaseUp="), X.ChaseUp);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiChaseFov="), X.ChaseFov);
		}
		UE_LOG(LogZoogi, Log, TEXT("ROYALE tune: lives %d accel %.0f max %.0f knock %.2f/%.0f..%.0f surge x%.2f by %.0fs cpu %.2f | robot speed x%.2f mass x%.2f fire %.2fs bolt %.0f knock %.0f assist %.0f deg jump %.0f"),
			X.Lives, X.Accel, X.MaxSpeed, X.KnockFrac, X.KnockMin, X.KnockMax, X.SurgeMax, X.SurgeFull, X.CpuSkill,
			X.RobotSpeedMul, X.RobotMassMul, X.FireInterval, X.BoltSpeed, X.BoltKnock, X.AimAssistDeg, X.JumpSpeed);
		return X;
	}();
	return T;
}

void FZoogiRoyale::Reset(int32 NumPlayers, int32 Lives)
{
	Step = EZoogiRoyaleStep::Off;
	StepTime = 0.f;
	FightTime = 0.f;
	Surge = 1.f;
	SurgeShown = 100;
	Winner = -1;
	HumanIndex = -1;
	HumanMove = FVector2D::ZeroVector;
	P.Reset();
	P.SetNum(FMath::Max(0, NumPlayers));
	for (FZoogiRoyalePlayer& X : P) { X = FZoogiRoyalePlayer(); X.Lives = Lives; X.HP = FZoogiRoyaleTune::Get().MaxHP; }
	Dmg.Reset();
	KillBanner = FZoogiRoyaleBanner();
	Kicks.Reset();
	PairHitAt.Reset();
	for (FZoogiRoyaleBolt& B : Bolts) if (B.Actor.IsValid()) B.Actor->Destroy();
	Bolts.Reset();
	bHumanAim = false;
	HumanHitAt = -100.f;
	FrameShots = 0;
	FrameClock = 0.f;
}

int32 FZoogiRoyale::NumIn() const
{
	int32 N = 0;
	for (const FZoogiRoyalePlayer& X : P) N += X.IsIn() ? 1 : 0;
	return N;
}

ZoogiRoyaleUI::FPad ZoogiRoyaleUI::Layout(const FVector2D& View)
{
	FPad L;
	const float W = FMath::Max(View.X, 64.f), H = FMath::Max(View.Y, 64.f);
	L.U = FMath::Min(W, H) / 1080.f;
	const float U = L.U;
	// Oct 27 (Evan): positions / sizes measured off the Marble Clash reference shots (2000 x 923: one ref px = U * 1080 / 923)
	const float K = U * (1080.f / 923.f);
	L.StickR = 178.f * K;
	L.KnobR = 80.f * K;
	L.StickHome = FVector2D(262.f * K, H - 266.f * K);
	L.MorphR = 74.f * K;
	L.JumpR = 74.f * K;
	if (H > W) // portrait: stacked in the lower-right corner
	{
		L.MorphC = FVector2D(W - 70.f * U - L.MorphR, H - 110.f * U - L.MorphR);
		L.JumpC = FVector2D(W - 70.f * U - L.JumpR, L.MorphC.Y - L.MorphR - 70.f * U - L.JumpR);
	}
	else
	{
		L.MorphC = FVector2D(W - 422.f * K, H - 153.f * K); // TRANSFORM low, inboard
		L.JumpC = FVector2D(W - 150.f * K, H - 468.f * K);  // JUMP higher, at the edge
	}
	L.Menu = FBox2D(FVector2D(W - 150.f * K, 14.f * K), FVector2D(W - 8.f * K, 146.f * K)); // Nov 1 b52: the bigger P5 settings cog (generous)
	return L;
}

// ------------------------------------------------------------------ mode / match start

void AZoogiGameMode::RoyaleSetMode(bool bOn)
{
	if (bOn && bComboMode) ComboSetMode(false);
	bRoyaleMode = bOn;
	bRoyaleTraining = false; // RoyaleStartTraining turns it back on
	bRoyaleTrainPanel = false;
	if (bOn)
	{
		ZoogiUtil::SetComboColors(false);
		MenuNumPlayers = 4;
		// one pair of thumbs: P1 human, the other Zoogis CPU (the PLAY screen can still switch slots to CPU / OFF)
		for (int32 i = 0; i < MenuPlayers.Num(); ++i) MenuPlayers[i].bCPU = i > 0;
	}
	UE_LOG(LogZoogi, Log, TEXT("Mode: %s (%s)"), bOn ? TEXT("BATTLE ROYALE") : (bComboMode ? TEXT("2v2 COMBO") : TEXT("CLASSIC")), *ZoogiUtil::ArenaName(MenuArena));
}

void AZoogiGameMode::RoyaleStartMatch()
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	ZoogiUtil::SetComboColors(false);
	ClearMatch();
	ZoogiScoringUI::ResetMatch();
	if (Arena->GetTheme() != MenuArena) Arena->Build(MenuArena);
	Arena->ResetRocks();
	Arena->ResetPieces(); // no stage-forming intro in the brawl: the stage is up from the first frame
	SpawnOrbs();          // the arena's orbs stay: knocking one off feeds the Awaken charge
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	const int32 N = bRoyaleTraining ? FMath::Clamp(1 + RoyaleTrainBots, 1, 4) : FMath::Clamp(MenuNumPlayers, 2, 4);
	Royale.Reset(N, T.Lives);
	// b46: CPU Zoogis bring one life fewer than the human (min 1)
	for (int32 i = 0; i < Royale.P.Num(); ++i) if (i < MenuPlayers.Num() && MenuPlayers[i].bCPU) Royale.P[i].Lives = FMath::Max(1, T.Lives - 1);
	bRoyaleTrainPanel = false;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < N; ++i)
	{
		const FZoogiPlayerSetup& S = MenuPlayers[i];
		const bool bCPU = bRoyaleTraining ? i > 0 : (S.bCPU || Royale.HumanIndex >= 0); // one human drives; extra HUMAN slots play as CPU
		if (!bCPU) Royale.HumanIndex = i;
		const FVector L = FindFreeSpot(Arena->GetSpawnPoint(i));
		const float Yaw = Arena->GetSpawnYaw(i);
		AZoogiBall* B = GetWorld()->SpawnActor<AZoogiBall>(AZoogiBall::StaticClass(), FTransform(FRotator(0, Yaw, 0), L), SP);
		if (!B) { UE_LOG(LogZoogi, Warning, TEXT("ROYALE: could not spawn P%d"), i + 1); break; }
		B->InitZoogi(Roster[S.ZoogiIndex % Roster.Num()], i, bCPU, BallPM, Arena->GetRollingDecel());
		B->SpawnLocation = L;
		B->SpawnYaw = Yaw;
		B->AimYaw = Yaw;
		B->SetActiveTurn(true); // everyone is "live": rings, trails, sparkles
		B->EnableMorph();       // every Zoogi is the transforming model here (ZoogiMorph.h)
		Balls.Add(B);
	}
	Royale.P.SetNum(Balls.Num());
	for (int32 i = 0; i < Balls.Num(); ++i) { Royale.P[i].FacingYaw = Balls[i]->SpawnYaw; Royale.P[i].StrafeSign = (i % 2) ? -1.f : 1.f; Royale.P[i].FormThink = 1.5f + 0.4f * i; }
	RoyaleFloorZ = Balls.Num() > 0 ? Balls[0]->BallLocation().Z - ZoogiConst::ZoogiRadius : 0.f;
	ActivePlayer = Royale.HumanIndex >= 0 ? Royale.HumanIndex : 0; // camera / occlusion fade follow this one
	RoundNumber = 1;
	WinnerIndex = -1;
	ShotsFired = 0;
	TurnCounter = 0;
	Stage = EZoogiStage::Battle;
	BattleStep = EZoogiBattleStep::Rolling;
	Phase = EZoogiPhase::Rolling;
	PhaseTime = 0.f;
	Banner.Reset();
	Royale.Step = EZoogiRoyaleStep::Countdown;
	Royale.StepTime = 0.f;
	BigText = TEXT("BATTLE ROYALE");
	BigColor = FLinearColor(1.f, 0.78f, 0.2f);
	BigAge = 0.f;
	NotifyMatchEvent(EZoogiMatchEvent::MatchStart, 0); // arena music + announcer
	ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.6f);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnRoyaleStarted();
	UE_LOG(LogZoogi, Log, TEXT("ROYALE start: %d Zoogis, %d lives each, human %s, arena %s"), Balls.Num(), T.Lives,
		Royale.HumanIndex >= 0 ? *FString::Printf(TEXT("P%d"), Royale.HumanIndex + 1) : TEXT("none (spectate)"), *ZoogiUtil::ArenaName(MenuArena));
	TakeShotDelayed(TEXT("royale_countdown"), 1.2f);
	RoyaleHeroStart(); // Oct 31: the human as the new crystal-mech model (bots keep the transform model)
	ZoogiShooter::OnStart(this); // b234: SHOOTER modes (Voyager soldiers, lives / teams / waves)
	ZoogiStageShift::OnMatchStart(this); // b46
	if (bRoyaleTraining)
	{
		// training: no countdown, straight in
		RoyaleBeginFight();
		BigText = FString::Printf(TEXT("TRAINING\n%s CONTROLS"), bRoyaleMarble ? TEXT("MARBLE") : TEXT("BRAWL"));
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiTrainPanelTest"))) bRoyaleTrainPanel = true; // capture: the training menu open
		BigColor = FLinearColor(0.45f, 0.85f, 1.f);
		BigAge = 0.f;
	}
}

// ------------------------------------------------------------------ TRAINING sandbox (Oct 27)

void AZoogiGameMode::RoyaleStartTraining()
{
	RoyaleSetMode(true);
	bRoyaleTraining = true;
	RoyaleTrainBots = FMath::Clamp(ZoogiControls::TrainBots(), 0, 3); // b96c: Settings > TRAINING BOTS
	MenuPlayers[0].bCPU = false;
	UE_LOG(LogZoogi, Log, TEXT("TRAINING sandbox: solo, %s controls, arena %s"), bRoyaleMarble ? TEXT("MARBLE") : TEXT("BRAWL"), *ZoogiUtil::ArenaName(MenuArena));
	StartMatch();
}

void AZoogiGameMode::RoyaleTrainAction(const FName& Action, int32 Index)
{
	if (!bRoyaleMode || !bRoyaleTraining) return;
	if (Action == TEXT("TrainMenu")) { bRoyaleTrainPanel = !bRoyaleTrainPanel; return; }
	if (Action == TEXT("TrainControls"))
	{
		bRoyaleMarble = !bRoyaleMarble;
		bRoyaleTrainPanel = false;
		BigText = bRoyaleMarble ? FString(TEXT("MARBLE CONTROLS\nPULL BACK + RELEASE")) : FString(TEXT("BRAWL CONTROLS\nSTICK + JUMP + TRANSFORM"));
		BigColor = FLinearColor(0.45f, 0.85f, 1.f);
		BigAge = 0.f;
		ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.5f);
		UE_LOG(LogZoogi, Log, TEXT("TRAINING controls -> %s"), bRoyaleMarble ? TEXT("MARBLE") : TEXT("BRAWL"));
		return;
	}
	if (Action == TEXT("TrainBot"))
	{
		const int32 Want = FMath::Clamp(RoyaleTrainBots + (Index < 0 ? -1 : 1), 0, 3);
		if (Want == RoyaleTrainBots) return;
		RoyaleTrainBots = Want;
		UE_LOG(LogZoogi, Log, TEXT("TRAINING bots -> %d"), RoyaleTrainBots);
		StartMatch(); // restarts the sandbox with the new line-up
		return;
	}
	if (Action == TEXT("TrainReset")) { StartMatch(); return; }
	if (Action == TEXT("TrainExit"))
	{
		bRoyaleTrainPanel = false;
		MenuAction(TEXT("Menu"), 0);
		return;
	}
}

bool AZoogiGameMode::RoyaleMarbleReady() const
{
	const int32 H = Royale.HumanIndex;
	if (Royale.Step != EZoogiRoyaleStep::Fight || !Balls.IsValidIndex(H) || !Royale.P.IsValidIndex(H)) return false;
	const AZoogiBall* B = Balls[H];
	const FZoogiRoyalePlayer& R = Royale.P[H];
	if (!B || B->bOut || !R.IsIn() || R.RespawnIn >= 0.f || R.bRobot || R.MorphT < ZoogiRoyaleFeel::MorphSeconds) return false;
	return B->GetBallVelocity().Size2D() < 420.f; // marble rule: launch from (nearly) at rest
}

bool AZoogiGameMode::RoyaleMarbleLaunch(const FVector2D& WorldDir, float PullFrac)
{
	if (!RoyaleMarbleReady() || WorldDir.IsNearlyZero()) return false;
	AZoogiBall* B = Balls[Royale.HumanIndex];
	const float Frac = FMath::Clamp(PullFrac, 0.f, 1.f);
	const FVector2D V = WorldDir.GetSafeNormal() * ZoogiConst::LaunchSpeedForPull(Frac);
	B->Launch(V); // the same launch the classic slingshot uses: velocity right away, no hold
	B->AimYaw = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	const FVector L = B->BallLocation();
	AZoogiFX::Spawn(GetWorld(), FVector(L.X, L.Y, RoyaleFloorZ + 3.f), FLinearColor(0.9f, 0.95f, 1.f), 130.f, 0.3f, true, 7.f);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.5f, 0.75f + 0.35f * Frac);
	UE_LOG(LogZoogi, Log, TEXT("TRAINING marble launch %.0f%% -> %.0f cm/s"), Frac * 100.f, V.Size());
	return true;
}

void AZoogiGameMode::RoyaleBeginFight()
{
	Royale.Step = EZoogiRoyaleStep::Fight;
	Royale.StepTime = 0.f;
	Royale.FightTime = 0.f;
	BigText = TEXT("BRAWL!");
	BigColor = FLinearColor(1.f, 0.42f, 0.2f);
	BigAge = 0.f;
	ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.55f);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE fight on"));
	TakeShotDelayed(TEXT("royale_fight"), 2.5f);
	ZoogiShooter::OnFight(this); // b234: everyone on foot
}

AZoogiBall* AZoogiGameMode::RoyaleFocusBall() const
{
	auto InPlay = [this](int32 i) { return Balls.IsValidIndex(i) && Royale.P.IsValidIndex(i) && Balls[i] && Royale.P[i].IsIn(); };
	if (Royale.Step == EZoogiRoyaleStep::Over && Balls.IsValidIndex(Royale.Winner)) return Balls[Royale.Winner];
	if (InPlay(Royale.HumanIndex)) return Balls[Royale.HumanIndex];
	// spectating: the Zoogi with the most lives (then KOs) still in
	int32 Best = -1;
	for (int32 i = 0; i < Balls.Num(); ++i)
	{
		if (!InPlay(i) || Balls[i]->bOut) continue;
		if (Best < 0 || Royale.P[i].Lives > Royale.P[Best].Lives || (Royale.P[i].Lives == Royale.P[Best].Lives && Royale.P[i].KOs > Royale.P[Best].KOs)) Best = i;
	}
	if (Best >= 0) return Balls[Best];
	return Balls.IsValidIndex(Royale.HumanIndex) ? Balls[Royale.HumanIndex] : (Balls.Num() > 0 ? Balls[0] : nullptr);
}

// ------------------------------------------------------------------ tick

void AZoogiGameMode::RoyaleTick(float Dt)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	Royale.StepTime += Dt;
	{
		const float Now = GetWorld()->GetTimeSeconds();
		Royale.Dmg.RemoveAll([Now](const FZoogiRoyaleDmg& D) { return Now - D.Born > 1.4f; });
		if (Royale.Dmg.Num() > 60) Royale.Dmg.RemoveAt(0, Royale.Dmg.Num() - 60);
	}
	if (const AZoogiBall* F = RoyaleFocusBall()) ActivePlayer = F->PlayerIndex; // occlusion fade / feel hooks follow the focus Zoogi
	// knockbacks recorded in hit callbacks land now (outside the physics callback)
	for (const FZoogiRoyaleKick& K : Royale.Kicks)
	{
		AZoogiBall* V = Balls.IsValidIndex(K.Victim) ? Balls[K.Victim].Get() : nullptr;
		if (!V || V->bOut || !V->Sphere || !V->Sphere->IsSimulatingPhysics()) continue;
		V->Sphere->WakeRigidBody();
		V->Sphere->SetPhysicsLinearVelocity(V->Sphere->GetPhysicsLinearVelocity() + K.DeltaV);
	}
	Royale.Kicks.Reset();

	switch (Royale.Step)
	{
	case EZoogiRoyaleStep::Countdown:
		for (AZoogiBall* B : Balls) if (B) B->DriveInput = FVector2D::ZeroVector;
		if (Royale.StepTime >= T.CountdownSeconds) RoyaleBeginFight();
		break;
	case EZoogiRoyaleStep::Fight:
	{
		Royale.FightTime += Dt;
		Royale.Surge = bRoyaleTraining ? 1.f : 1.f + (T.SurgeMax - 1.f) * FMath::Clamp((Royale.FightTime - T.SurgeStart) / (T.SurgeFull - T.SurgeStart), 0.f, 1.f);
		const int32 Shown = FMath::FloorToInt(Royale.Surge * 4.f + 0.001f) * 25; // announce every +0.25
		if (Shown > Royale.SurgeShown && Royale.Surge > 1.01f)
		{
			Royale.SurgeShown = Shown;
			AddFeed(FString::Printf(TEXT("SURGE  knockback x%.2f"), Royale.Surge), FLinearColor(1.f, 0.55f, 0.15f));
			BigText = FString::Printf(TEXT("SURGE x%.2f"), Royale.Surge);
			BigColor = FLinearColor(1.f, 0.55f, 0.15f);
			BigAge = 0.f;
			ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.5f, 0.8f);
		}
		for (int32 i = 0; i < Balls.Num() && i < Royale.P.Num(); ++i)
		{
			FZoogiRoyalePlayer& R = Royale.P[i];
			AZoogiBall* B = Balls[i];
			if (!B) continue;
			B->DriveInput = FVector2D::ZeroVector;
			if (!R.IsIn()) continue;
			if (R.RespawnIn >= 0.f)
			{
				R.RespawnIn -= Dt;
				if (R.RespawnIn < 0.f) RoyaleRespawn(i);
				continue;
			}
			if (B->bOut) continue;
			if (R.IsAwake())
			{
				R.AwakenLeft -= Dt;
				R.AuraClock += Dt;
				if (R.AuraClock > 0.45f) // element glow pulsing under the transformed Zoogi
				{
					R.AuraClock = 0.f;
					const FVector BL = B->BallLocation();
					AZoogiFX::Spawn(GetWorld(), FVector(BL.X, BL.Y, BL.Z - ZoogiConst::ZoogiRadius + 4.f), ZoogiArtifacts::Color(ZoogiArtifacts::Affinity(B->Def.Id)), 120.f, 0.5f, true, 8.f, B);
				}
				if (R.AwakenLeft <= 0.f) RoyaleEndAwaken(i);
			}
			R.MorphCD = FMath::Max(0.f, R.MorphCD - Dt);
			R.MorphT += Dt;
			R.FireCD = FMath::Max(0.f, R.FireCD - Dt);
			R.JumpCD = FMath::Max(0.f, R.JumpCD - Dt);
			R.DodgeCD = FMath::Max(0.f, R.DodgeCD - Dt);
			if (R.ShieldLeft > 0.f)
			{
				R.ShieldLeft -= Dt;
				const bool bShow = R.ShieldLeft <= 0.f || FMath::Fmod(R.ShieldLeft, 0.24f) > 0.08f; // respawn blink
				if (B->Visual) B->Visual->SetVisibility(bShow);
				if (R.ShieldLeft <= 0.f) { R.ShieldLeft = 0.f; B->SetPhasing(false); }
			}
			static const bool bBotHuman = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleBotHuman")); // capture runs: the CPU brain drives P1 (camera follows its aim)
			if (i == Royale.HumanIndex && !bBotHuman)
			{
				R.Move = bRoyaleMarble ? FVector2D::ZeroVector : Royale.HumanMove; // MARBLE controls: it only rolls from launches
				RoyaleAimHuman(i);
				// edge assist (phones): close to the ring line, most of the stick's push toward the edge is held back - a
				// rival's hit still carries you out (hit-stun drops the drive), rolling off by yourself is much harder
				const FVector HL = B->BallLocation(), HV = B->GetBallVelocity();
				const FVector2D H2(HL.X, HL.Y), HV2(HV.X, HV.Y);
				if (!H2.IsNearlyZero() && (RoyaleDanger(H2 + HV2 * 0.35f) || RoyaleDanger(H2 + R.Move.GetSafeNormal() * 180.f)))
				{
					const FVector2D Out = H2.GetSafeNormal();
					const float Push = FVector2D::DotProduct(R.Move, Out);
					if (Push > 0.f) R.Move -= Out * Push * 0.8f;
				}
			}
			else RoyaleTickCPU(i, Dt);
			if (R.StunLeft > 0.f) { R.StunLeft -= Dt; R.Move = FVector2D::ZeroVector; } // knocked back: momentum carries it
			const bool bMorphing = R.MorphT < ZoogiRoyaleFeel::MorphSeconds;
			B->DriveInput = R.Move.GetClampedToMaxSize(1.f) * (bMorphing ? T.MorphDriveMul : 1.f);
			B->DriveAccel = T.Accel * (R.IsAwake() ? T.AwakenAccelMul : 1.f) * (R.bRobot ? T.RobotAccelMul : 1.f);
			B->DriveMaxSpeed = T.MaxSpeed * (R.IsAwake() ? T.AwakenSpeedMul : 1.f) * (R.bRobot ? T.RobotSpeedMul : 1.f);
			if (R.bRobot) B->AimYaw = R.FacingYaw;
			else if (!B->DriveInput.IsNearlyZero(0.15f)) B->AimYaw = FMath::RadiansToDegrees(FMath::Atan2(B->DriveInput.Y, B->DriveInput.X));
			if (B->Morph) B->Morph->SetFacingYaw(R.bRobot ? R.FacingYaw : B->AimYaw);
			// robot: the cannons fire on their own while the crosshair (CPU: its aim) is on a rival
			if (R.bRobot && !bMorphing && R.Lock >= 0 && R.FireCD <= 0.f && R.StunLeft <= 0.f) RoyaleFire(i);
		}
		{
			// capture runs (-ZoogiMarbleAutoTest): the MARBLE launch path fires by itself every ~3 s toward the middle
			static const bool bMarbleAuto = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMarbleAutoTest"));
			static float MarbleClock = 0.f;
			MarbleClock += Dt;
			if (bMarbleAuto && bRoyaleMarble && MarbleClock > 3.f && RoyaleMarbleReady() && Balls.IsValidIndex(Royale.HumanIndex))
			{
				MarbleClock = 0.f;
				const FVector L = Balls[Royale.HumanIndex]->BallLocation();
				RoyaleMarbleLaunch(FVector2D(-L.X, -L.Y).GetSafeNormal().GetRotated(FMath::FRandRange(-50.f, 50.f)), FMath::FRandRange(0.3f, 0.7f));
			}
		}
		RoyaleTickBolts(Dt);
		RoyaleCheckOuts();
		break;
	}
	case EZoogiRoyaleStep::Over:
		RoyaleTickBolts(Dt);
		for (int32 i = 0; i < Balls.Num() && i < Royale.P.Num(); ++i)
		{
			if (Balls[i]) Balls[i]->DriveInput = FVector2D::ZeroVector;
			if (Royale.P[i].IsAwake()) { Royale.P[i].AwakenLeft -= Dt; if (Royale.P[i].AwakenLeft <= 0.f) RoyaleEndAwaken(i); }
		}
		break;
	default:
		break;
	}
	RoyaleHeroTick(Dt); // Oct 31: hero model + relics
	ZoogiShooter::Tick(this, Dt); // b234: the Voyager soldiers
	ZoogiStageShift::RoyaleTick(this, Dt); // b46: Ancient eruption phase
	RoyaleFrameDump(Dt);
}

// ------------------------------------------------------------------ ring-outs, respawns, the end

static FVector RoyaleEdgePoint(const FVector& L, const AZoogiArena* Arena, float FloorZ)
{
	FVector2D P(L.X, L.Y);
	if (P.Size() < 1.f) P = FVector2D(1.f, 0.f);
	P = P.GetSafeNormal() * ((Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) - 60.f);
	return FVector(P.X, P.Y, FloorZ + 120.f);
}

void AZoogiGameMode::RoyaleCheckOuts()
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	// Oct 31 b41 (Evan): on Ancient the Royale is an action shooter / melee where you COLLECT orbs instead of knocking them off.
	// Touch an orb (ball or mech) -> +1 orb, Awaken charge, a little HP. Collected / fallen orbs drop back in at an island spot after 8 s.
	static const bool bOrbKnock = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleOrbKnock"));
	const bool bAncCollect = Arena && Arena->GetTheme() == EZoogiArena::Ancient;
	if (Arena && (bAncCollect || ZoogiShooter::On(this)) && !bOrbKnock) // b237: the SHOOTER modes collect crystals on every arena
	{
		static TMap<TWeakObjectPtr<AZoogiOrb>, float> OrbBack;
		const ZoogiAncient::FLayout& Lay = ZoogiAncient::Get();
		for (AZoogiOrb* O : Orbs)
		{
			if (!O) continue;
			if (O->bOut)
			{
				if (!bAncCollect) continue; // b237: only Ancient has drop-back spots
				float* Back = OrbBack.Find(O);
				if (!Back) { OrbBack.Add(O, Clock + 8.f); continue; }
				if (Clock < *Back || Lay.OrbSpots.Num() == 0) continue;
				FVector Best = Lay.OrbSpots[0]; float BestD = -1.f;
				for (int32 t = 0; t < 6; ++t)
				{
					const FVector S = Lay.OrbSpots[FMath::RandRange(0, Lay.OrbSpots.Num() - 1)];
					float Dm = 1.0e9f;
					for (const AZoogiBall* Bz : Balls) if (Bz && !Bz->bOut) Dm = FMath::Min(Dm, (float)FVector::Dist2D(Bz->BallLocation(), S));
					for (const AZoogiOrb* O2 : Orbs) if (O2 && O2 != O && !O2->bOut) Dm = FMath::Min(Dm, 2.f * (float)FVector::Dist2D(O2->OrbLocation(), S));
					if (Dm > BestD) { BestD = Dm; Best = S; }
				}
				O->Respawn(Best + FVector(0, 0, 120.f));
				OrbBack.Remove(O);
				AZoogiFX::Spawn(GetWorld(), Best, FLinearColor(0.5f, 0.85f, 1.f), 120.f, 0.5f, true, 6.f);
				continue;
			}
			const FVector L = O->OrbLocation();
			if (AZoogiArena::IsRingOut(L) || AZoogiArena::IsPortalGoal(L)) { O->SetOut(); continue; } // fell off: no score, it comes back
			int32 Who = -1; float WD = 175.f;
			for (int32 i = 0; i < Balls.Num() && i < Royale.P.Num(); ++i)
			{
				const AZoogiBall* Bz = Balls[i];
				if (!Bz || Bz->bOut || !Royale.P[i].IsIn() || Royale.P[i].RespawnIn >= 0.f) continue;
				const float D = (float)FVector::Dist(Bz->BallLocation(), L) - (Royale.P[i].bRobot ? 40.f : 0.f);
				if (D < WD) { WD = D; Who = i; }
			}
			if (Who < 0) continue;
			O->SetOut();
			FZoogiRoyalePlayer& Rc = Royale.P[Who];
			++Rc.Orbs;
			if (!Rc.IsAwake()) Rc.Charge = FMath::Min(1.f, Rc.Charge + T.ChargePerOrb * (O->bStar ? 2.f : 1.f));
			Rc.HP = FMath::Min(T.MaxHP, Rc.HP + (O->bStar ? 160 : 80));
			AZoogiFX::Niagara(GetWorld(), O->bStar ? TEXT("NS_Burst_Gold") : TEXT("NS_Burst_Blue"), L);
			if (Who == Royale.HumanIndex)
			{
				AddPopup(FString::Printf(TEXT("+ORB  %d"), Rc.Orbs), L + FVector(0, 0, 80.f), O->bStar ? FLinearColor(1.f, 0.8f, 0.2f) : FLinearColor(0.5f, 0.9f, 1.f));
				ZoogiUtil::PlaySfx(this, TEXT("Score"), 0.5f, O->bStar ? 1.25f : 1.f);
			}
			UE_LOG(LogZoogi, Log, TEXT("ROYALE orb collected by P%d (%d orbs)"), Who + 1, Rc.Orbs);
		}
	}
	else
	for (AZoogiOrb* O : Orbs)
	{
		if (!O || O->bOut) continue;
		const FVector L = O->OrbLocation();
		if (!AZoogiArena::IsRingOut(L) && !AZoogiArena::IsPortalGoal(L)) continue; // Oct 31: Ancient portals count as a knock-off
		const int32 By = O->LastHitBy;
		const FVector At = RoyaleEdgePoint(L, Arena, RoyaleFloorZ);
		O->SetOut();
		AZoogiFX::Niagara(GetWorld(), O->bStar ? TEXT("NS_Burst_Gold") : TEXT("NS_Burst_Blue"), At);
		if (Royale.P.IsValidIndex(By) && Royale.P[By].IsIn() && !Royale.P[By].IsAwake())
		{
			FZoogiRoyalePlayer& R = Royale.P[By];
			R.Charge = FMath::Min(1.f, R.Charge + T.ChargePerOrb * (O->bStar ? 2.f : 1.f));
			if (By == Royale.HumanIndex) AddPopup(O->bStar ? TEXT("+AWAKEN x2") : TEXT("+AWAKEN"), At + FVector(0, 0, 60.f), FLinearColor(1.f, 0.75f, 0.2f));
			ZoogiUtil::PlaySfx(this, TEXT("Score"), 0.5f, O->bStar ? 1.25f : 1.f);
		}
	}
	for (int32 i = 0; i < Balls.Num() && i < Royale.P.Num(); ++i)
	{
		AZoogiBall* B = Balls[i];
		const FZoogiRoyalePlayer& R = Royale.P[i];
		if (!B || B->bOut || !R.IsIn() || R.RespawnIn >= 0.f) continue;
		if (R.bDestroyPending)
		{
			Royale.P[i].bDestroyPending = false;
			if (!Royale.P.IsValidIndex(R.LastHitBy) || Clock - R.LastHitAt > 1.f) Royale.P[i].LastHitBy = -1;
			RoyaleLoseLife(B, true);
			if (Royale.Step != EZoogiRoyaleStep::Fight) return;
			continue;
		}
		// out = fell off, or pushed past the ring (centre within EdgeMargin of the court edge): works on walled arenas too
		const FVector BL = B->BallLocation();
		const FVector2D P2(BL.X, BL.Y);
		// pushed past the ring only counts right after a rival's hit: rolling into a wall yourself just bounces
		const bool bKnocked = Royale.P.IsValidIndex(R.LastHitBy) && R.LastHitBy != i && Clock - R.LastHitAt <= 1.5f;
		// Oct 31: not on Ancient - its links run across the radial direction and the recovery islets must get their chance: falling decides
		const bool bPastRing = bKnocked && Arena && Arena->GetTheme() != EZoogiArena::Ancient && !P2.IsNearlyZero() && Arena->IsOffCourt(P2 + P2.GetSafeNormal() * T.EdgeMargin);
		if (AZoogiArena::IsRingOut(BL) || bPastRing) RoyaleRingOut(B);
		if (Royale.Step != EZoogiRoyaleStep::Fight) return; // the match just ended
	}
}

void AZoogiGameMode::RoyaleRingOut(AZoogiBall* B)
{
	RoyaleLoseLife(B, false);
}

void AZoogiGameMode::RoyaleDamage(int32 Victim, int32 By, int32 Amount, bool bCrit, const FVector& At)
{
	if (Royale.Step != EZoogiRoyaleStep::Fight || !Royale.P.IsValidIndex(Victim) || !Balls.IsValidIndex(Victim) || Amount <= 0) return;
	FZoogiRoyalePlayer& RV = Royale.P[Victim];
	AZoogiBall* V = Balls[Victim];
	if (!V || V->bOut || !RV.IsIn() || RV.RespawnIn >= 0.f || RV.ShieldLeft > 0.f || RV.HP <= 0) return;
	// Oct 31 b46 (Evan: "bots on royale too hard to kill"): the human's hits on CPU Zoogis count double
	static float HumanVsBot = 2.f; static bool bHvB = false; if (!bHvB) { bHvB = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRoyaleHumanDmg="), HumanVsBot); }
	if (By == Royale.HumanIndex && Victim != Royale.HumanIndex) Amount = FMath::RoundToInt(Amount * HumanVsBot);
	const float Now = GetWorld()->GetTimeSeconds();
	RV.HP = FMath::Max(0, RV.HP - Amount);
	RV.HurtAt = Now;
	if (Royale.P.IsValidIndex(By) && By != Victim) { RV.LastHitBy = By; RV.LastHitAt = Clock; }
	FZoogiRoyaleDmg D;
	D.World = V->BallLocation() + FVector(0, 0, RV.bRobot ? 150.f : 80.f);
	D.Amount = Amount;
	D.Kind = Victim == Royale.HumanIndex ? 2 : (By == Royale.HumanIndex ? (bCrit ? 1 : 0) : 3);
	D.Born = Now;
	D.Jitter = FVector2D(FMath::FRandRange(-70.f, 70.f), FMath::FRandRange(-40.f, 20.f));
	Royale.Dmg.Add(D);
	if (RV.HP > 0) return;
	// destroyed: blown up on the court on the next tick (RoyaleCheckOuts) - a life lost, credited to the attacker
	RV.bDestroyPending = true;
}

void AZoogiGameMode::RoyaleLoseLife(AZoogiBall* B, bool bDestroyed)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	const int32 i = B->PlayerIndex;
	if (!Royale.P.IsValidIndex(i)) return;
	FZoogiRoyalePlayer& R = Royale.P[i];
	const FVector At = bDestroyed ? B->BallLocation() : RoyaleEdgePoint(B->BallLocation(), Arena, RoyaleFloorZ);
	const int32 By = R.LastHitBy;
	const bool bCredit = Royale.P.IsValidIndex(By) && By != i && Balls.IsValidIndex(By) && (Clock - R.LastHitAt) <= T.CreditSeconds;
	if (R.IsAwake()) RoyaleEndAwaken(i);
	if (!bRoyaleTraining && !ZoogiRoyaleMix::ActionRound() && !ZoogiShooter::KeepLives(this)) R.Lives = FMath::Max(0, R.Lives - 1); // training: endless; b149: a royale-mix action round too (falls cost points)
	R.Falls++;
	R.LastHitBy = -1;
	if (!bRoyaleTraining && i != Royale.HumanIndex) RoyaleRelicDrop(i, B->BallLocation()); // Oct 31: bots leave a relic behind
	const FLinearColor Tint = bCredit ? ZoogiUtil::PlayerColor(By) : FLinearColor(1.f, 0.35f, 0.3f);
	AZoogiFX::Spawn(GetWorld(), At, bDestroyed ? FLinearColor(1.f, 0.6f, 0.2f) : FLinearColor(1.f, 0.3f, 0.25f), bDestroyed ? 300.f : 220.f, 0.7f, false, 7.f);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), At, FRotator::ZeroRotator, 1.4f);
	if (bDestroyed) { AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Gold"), At, FRotator::ZeroRotator, 1.2f); ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.6f); }
	AZoogiFX::ShockwaveByMag(GetWorld(), At, 30000.f, &Tint, true, true);
	B->SetOut();
	B->DriveInput = FVector2D::ZeroVector;
	if (B->Visual) B->Visual->SetVisibility(true);
	if (!bDestroyed) ZoogiUtil::PlaySfx(this, TEXT("Fall"), 0.7f);
	const FString VName = B->Def.Name.ToUpper();
	if (bCredit)
	{
		AZoogiBall* A = Balls[By];
		FZoogiRoyalePlayer& RA = Royale.P[By];
		RA.KOs++;
		if (!RA.IsAwake()) RA.Charge = FMath::Min(1.f, RA.Charge + T.ChargePerKO);
		AddFeed(FString::Printf(TEXT("%s  %s  %s"), *A->Def.Name, bDestroyed ? TEXT("DESTROYED") : TEXT("RING-OUT"), *B->Def.Name), ZoogiUtil::PlayerColor(By));
		if (!bDestroyed) AddPopup(TEXT("RING OUT!"), At + FVector(0, 0, 90.f), ZoogiUtil::PlayerColor(By));
		// Marble Clash kill banner for the human (purple "YOU DESTROY" / "DESTROYED BY" + the other Zoogi's name)
		if (By == Royale.HumanIndex) Royale.KillBanner = { bDestroyed ? FString(TEXT("YOU DESTROY")) : FString(TEXT("YOU RING OUT")), VName, (float)GetWorld()->GetTimeSeconds() };
		else if (i == Royale.HumanIndex) Royale.KillBanner = { bDestroyed ? FString(TEXT("DESTROYED BY")) : FString(TEXT("KNOCKED OFF BY")), A->Def.Name.ToUpper(), (float)GetWorld()->GetTimeSeconds() };
		AZoogiFX::Spawn(GetWorld(), At - FVector(0, 0, 20.f), ZoogiUtil::PlayerColor(By), 420.f, 0.8f, true, 12.f);
	}
	else
	{
		AddFeed(FString::Printf(TEXT("%s  %s"), *B->Def.Name, bDestroyed ? TEXT("was destroyed") : TEXT("rolled off the edge")), FLinearColor(1.f, 0.4f, 0.35f));
		AddPopup(bDestroyed ? TEXT("DESTROYED") : TEXT("FELL OFF"), At + FVector(0, 0, 90.f), FLinearColor(1.f, 0.35f, 0.3f));
	}
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController()))
		if (i == Royale.HumanIndex || By == Royale.HumanIndex) PC->OnImpact(1.f);
	NotifyMatchEvent(EZoogiMatchEvent::RingOut, i, (float)i);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE %s P%d %s (lives left %d) %s at %.1f s"), bDestroyed ? TEXT("destroyed") : TEXT("ring-out"), i + 1, *B->Def.Name, R.Lives,
		bCredit ? *FString::Printf(TEXT("by P%d %s"), By + 1, *Balls[By]->Def.Name) : TEXT("self"), Royale.FightTime);
	TakeShotDelayed(FString::Printf(TEXT("royale_ringout_%d"), R.Falls + i * 10), 0.15f);
	ZoogiShooter::OnLoseLife(this, i, bCredit ? By : -1); // b234: TEAM DEATHMATCH score
	if (R.Lives > 0 || bRoyaleTraining)
	{
		R.RespawnIn = T.RespawnDelay;
		return;
	}
	R.Place = Royale.NumIn(); // counts this Zoogi too: first one out of 4 places 4th
	AddFeed(FString::Printf(TEXT("%s  ELIMINATED  (#%d)"), *B->Def.Name, R.Place), FLinearColor(1.f, 0.3f, 0.3f));
	if (i == Royale.HumanIndex)
	{
		BigText = FString::Printf(TEXT("ELIMINATED\n#%d"), R.Place);
		BigColor = FLinearColor(1.f, 0.3f, 0.3f);
		BigAge = 0.f;
	}
	NotifyMatchEvent(EZoogiMatchEvent::KnockedOut, i, (float)i);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE eliminated P%d %s place %d, %d left"), i + 1, *B->Def.Name, R.Place, Royale.NumIn());
	if (ZoogiShooter::OnEliminated(this, i)) return; // b234: HOLD THE LINE waves
	if (Royale.NumIn() <= 1) RoyaleFinish();
}

FVector AZoogiGameMode::RoyaleSpawnSpot(int32 Idx) const
{
	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	FVector Best = Balls.IsValidIndex(Idx) && Balls[Idx] ? Balls[Idx]->SpawnLocation : FVector(0, 0, ZoogiConst::ZoogiRadius + 2.f);
	float BestScore = -1.f;
	for (int32 Ring = 0; Ring < 2; ++Ring)
		for (int32 k = 0; k < 12; ++k)
		{
			const float A = (k + 0.5f * Ring) * UE_TWO_PI / 12.f;
			const float Rad = (Arena && Arena->GetTheme() == EZoogiArena::Ancient) ? (Ring == 0 ? 1500.f : 1680.f) : KR * (Ring == 0 ? 0.5f : 0.3f); // Oct 31: Ancient - the ring around the volcano
			const FVector2D C(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad);
			if (RoyaleDanger(C) || (Arena && !Arena->IsSpotClear(C, ZoogiConst::ZoogiRadius + 10.f))) continue;
			float Near = 1e6f;
			for (int32 j = 0; j < Balls.Num(); ++j)
			{
				if (j == Idx || !Balls[j] || Balls[j]->bOut) continue;
				Near = FMath::Min(Near, FVector2D::Distance(C, FVector2D(Balls[j]->BallLocation())));
			}
			if (Near > BestScore) { BestScore = Near; Best = FVector(C.X, C.Y, RoyaleFloorZ + ZoogiConst::ZoogiRadius + 2.f); }
		}
	return FindFreeSpot(Best);
}

void AZoogiGameMode::RoyaleRespawn(int32 Idx)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	AZoogiBall* B = Balls.IsValidIndex(Idx) ? Balls[Idx].Get() : nullptr;
	if (!B || !Royale.P.IsValidIndex(Idx)) return;
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	const FVector Spot = RoyaleSpawnSpot(Idx);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Spot.Y, -Spot.X)); // face the centre
	B->PlaceAt(Spot + FVector(0, 0, 220.f), Yaw); // drops in
	B->SetActiveTurn(true);
	B->SetPhasing(true);
	R.RespawnIn = -1.f;
	R.HP = T.MaxHP;
	R.bDestroyPending = false;
	R.ShieldLeft = T.ShieldSeconds;
	R.LastHitBy = -1;
	R.SmoothMove = FVector2D::ZeroVector;
	// back as a ball
	R.bRobot = false;
	R.MorphCD = 0.f;
	R.MorphT = 10.f;
	R.Lock = -1;
	R.FacingYaw = Yaw;
	B->SetMassScale(1.f);
	if (B->Morph) { B->Morph->SetFacingYaw(Yaw); B->Morph->SetRobot(false, true); }
	AZoogiFX::KFX(GetWorld(), TEXT("Effect5"), Spot - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), 0.45f, 2.5f);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.45f);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE respawn P%d at (%.0f, %.0f), %d lives"), Idx + 1, Spot.X, Spot.Y, R.Lives);
	ZoogiShooter::OnRespawn(this, Idx); // b234: back on foot at once (+ the wave's HP)
}

void AZoogiGameMode::RoyaleFinish()
{
	if (ZoogiRoyaleMix::OnRoyaleFinished(this)) return; // b149: a royale-mix action round ends on its clock
	if (Royale.Step == EZoogiRoyaleStep::Over) return;
	Royale.Step = EZoogiRoyaleStep::Over;
	Royale.StepTime = 0.f;
	int32 W = -1;
	for (int32 i = 0; i < Royale.P.Num(); ++i) if (Royale.P[i].IsIn()) { W = i; Royale.P[i].Place = 1; }
	Royale.Winner = W;
	WinnerIndex = W;
	Phase = EZoogiPhase::GameOver;
	PhaseTime = 0.f;
	for (int32 i = 0; i < Balls.Num(); ++i) if (Balls[i]) { Balls[i]->DriveInput = FVector2D::ZeroVector; if (Balls[i]->Visual) Balls[i]->Visual->SetVisibility(true); }
	if (Balls.IsValidIndex(W) && Balls[W])
	{
		Banner = FString::Printf(TEXT("%s WINS!"), *Balls[W]->Def.Name.ToUpper());
		BigText = W == Royale.HumanIndex ? FString(TEXT("VICTORY ROYALE!")) : Banner;
		BigColor = ZoogiUtil::PlayerColor(W);
		Balls[W]->PulseCore(1.f);
		Balls[W]->SetPhasing(false);
	}
	else
	{
		Banner = TEXT("DRAW!");
		BigText = Banner;
		BigColor = FLinearColor::White;
	}
	BigAge = 0.f;
	BannerTime = 0.f;
	ZoogiUtil::PlaySfx(this, TEXT("Win"), 0.8f);
	NotifyMatchEvent(EZoogiMatchEvent::MatchOver, W);
	PlayVictoryCrowd();
	FString Places;
	for (int32 i = 0; i < Royale.P.Num(); ++i)
		if (Balls.IsValidIndex(i) && Balls[i]) Places += FString::Printf(TEXT("P%d %s #%d (%d KO) "), i + 1, *Balls[i]->Def.Name, Royale.P[i].Place, Royale.P[i].KOs);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE over after %.1f s: %s | %s"), Royale.FightTime, *Banner, *Places);
	TakeShotDelayed(TEXT("royale_win"), 1.2f);
}

// ------------------------------------------------------------------ Awaken (transform)

bool AZoogiGameMode::RoyaleAwaken(int32 Idx, const TCHAR* Who)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	if (!bRoyaleMode || Royale.Step != EZoogiRoyaleStep::Fight || !Balls.IsValidIndex(Idx) || !Royale.P.IsValidIndex(Idx)) return false;
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* B = Balls[Idx];
	if (!B || B->bOut || !R.IsIn() || R.RespawnIn >= 0.f || R.IsAwake() || R.Charge < 0.999f) return false;
	const EZoogiArtifact A = ZoogiArtifacts::Affinity(B->Def.Id);
	const FLinearColor Col = ZoogiArtifacts::Color(A);
	R.Charge = 0.f;
	R.AwakenLeft = T.AwakenSeconds;
	if (B->Artifact) B->Artifact->SetInfused(A); // the element look (core glow, aura, trail) for the transformation
	B->SetMassScale(T.AwakenMassMul);
	B->SetModelScale(T.AwakenModelScale);
	B->ShellExpand(1.f);
	B->PulseCore(1.f);
	const FVector L = B->BallLocation();
	AZoogiCrackFX::OnAwaken(GetWorld(), FVector(L.X, L.Y, RoyaleFloorZ), A, false); // element cracks at the trigger (AwakenFX's turn-bound watchers are not used here)
	AZoogiFX::Spawn(GetWorld(), L, Col, 300.f, 0.8f, false, 14.f, B);
	AZoogiFX::Spawn(GetWorld(), FVector(L.X, L.Y, RoyaleFloorZ + 4.f), Col, T.BurstRadius, 0.55f, true, 10.f);
	AZoogiFX::ShockwaveByMag(GetWorld(), L, 26000.f, &Col, false, false);
	ZoogiUtil::PlaySfx(this, *FString::Printf(TEXT("Sting%s"), *ZoogiArtifacts::Key(A)), 0.8f);
	ZoogiUtil::PlaySfx(this, TEXT("AwakenCharge"), 0.6f);
	// transformation burst: rivals close by are blown back
	int32 Hit = 0;
	for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
	{
		AZoogiBall* O = Balls[j];
		FZoogiRoyalePlayer& RO = Royale.P[j];
		if (j == Idx || !O || O->bOut || !RO.IsIn() || RO.ShieldLeft > 0.f) continue;
		FVector D = O->BallLocation() - L; D.Z = 0.f;
		const float Dist = D.Size();
		if (Dist > T.BurstRadius || Dist < 1.f) continue;
		const float K = (1.f - 0.5f * Dist / T.BurstRadius) * Royale.Surge * (RO.IsAwake() ? T.AwakenTakenMul : 1.f);
		FZoogiRoyaleKick Kick;
		Kick.Victim = j;
		Kick.DeltaV = D / Dist * T.BurstSpeed * K + FVector(0, 0, 220.f * K);
		Royale.Kicks.Add(Kick);
		RO.LastHitBy = Idx;
		RO.LastHitAt = Clock;
		RO.StunLeft = FMath::Max(RO.StunLeft, T.HitStun);
		++Hit;
	}
	const FString El = ZoogiArtifacts::Name(A).ToUpper();
	AddPopup(FString::Printf(TEXT("AWAKEN  %s"), *El), L + FVector(0, 0, 190.f), Col);
	AddFeed(FString::Printf(TEXT("%s  AWAKENS  (%s)"), *B->Def.Name, *ZoogiArtifacts::Name(A)), Col);
	if (Idx == Royale.HumanIndex)
	{
		BigText = FString::Printf(TEXT("AWAKEN!\n%s"), *El);
		BigColor = Col;
		BigAge = 0.f;
		if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.6f);
	}
	UE_LOG(LogZoogi, Log, TEXT("ROYALE awaken P%d %s (%s, %s): %d rival(s) in the burst"), Idx + 1, *B->Def.Name, *ZoogiArtifacts::Name(A), Who, Hit);
	TakeShotDelayed(FString::Printf(TEXT("royale_awaken_P%d"), Idx + 1), 0.35f);
	return true;
}

void AZoogiGameMode::RoyaleEndAwaken(int32 Idx)
{
	if (!Royale.P.IsValidIndex(Idx)) return;
	Royale.P[Idx].AwakenLeft = 0.f;
	AZoogiBall* B = Balls.IsValidIndex(Idx) ? Balls[Idx].Get() : nullptr;
	if (!B) return;
	if (B->Artifact) B->Artifact->ClearInfused();
	B->SetMassScale(1.f);
	B->SetModelScale(1.f);
}

// ------------------------------------------------------------------ contacts: knockback + credit

void AZoogiGameMode::RoyaleOnBallHit(AZoogiBall* Self, AActor* Other, const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!Self || !IsRoyaleMatch() || Royale.Step != EZoogiRoyaleStep::Fight) return;
	if (AZoogiOrb* Orb = Cast<AZoogiOrb>(Other)) { Orb->LastHitBy = Self->PlayerIndex; return; }
	AZoogiBall* O = Cast<AZoogiBall>(Other);
	if (!O || O == Self) return;
	const int32 a = Self->PlayerIndex, b = O->PlayerIndex;
	if (!Royale.P.IsValidIndex(a) || !Royale.P.IsValidIndex(b) || !Balls.IsValidIndex(a) || !Balls.IsValidIndex(b)) return;
	const uint32 Key = (uint32)FMath::Min(a, b) * 64u + (uint32)FMath::Max(a, b);
	const float Now = Clock;
	if (const float* Last = Royale.PairHitAt.Find(Key)) if (Now - *Last < 0.25f) return; // one boost per contact (both spheres report it)
	Royale.PairHitAt.Add(Key, Now);
	FVector N = O->BallLocation() - Self->BallLocation();
	N.Z = 0.f;
	if (!N.Normalize()) return; // Self -> O
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	const FVector VA = Self->PrevVelocity, VB = O->PrevVelocity;
	const float CA = VA.X * N.X + VA.Y * N.Y;    // Self closing on O
	const float CB = -(VB.X * N.X + VB.Y * N.Y); // O closing on Self
	auto Push = [&](int32 Att, int32 Vic, const FVector& Dir, float Closing)
	{
		if (Closing < T.KnockMin) return;
		FZoogiRoyalePlayer& RA = Royale.P[Att];
		FZoogiRoyalePlayer& RV = Royale.P[Vic];
		if (RV.ShieldLeft > 0.f || RA.ShieldLeft > 0.f) return;
		float K = FMath::Min(Closing * T.KnockFrac, T.KnockMax) * Royale.Surge;
		if (RA.IsAwake()) K *= T.AwakenKnockMul;
		if (RV.IsAwake()) K *= T.AwakenTakenMul;
		if (RV.bRobot) K *= T.RobotTakenMul;
		FZoogiRoyaleKick Kick;
		Kick.Victim = Vic;
		Kick.DeltaV = Dir * K + FVector(0, 0, T.KnockLift * FMath::Clamp(K / FMath::Max(1.f, T.KnockMax), 0.f, 1.5f));
		Royale.Kicks.Add(Kick);
		RV.LastHitBy = Att;
		RV.LastHitAt = Now;
		RV.StunLeft = FMath::Max(RV.StunLeft, T.HitStun * FMath::Clamp(K / FMath::Max(1.f, T.KnockMax), 0.2f, 1.f));
		if (!RA.IsAwake()) RA.Charge = FMath::Min(1.f, RA.Charge + T.ChargePerHit * FMath::Clamp(Closing / 900.f, 0.3f, 1.5f));
		const FVector Mid = (Self->BallLocation() + O->BallLocation()) * 0.5f;
		// Oct 27: rams hurt too (HP), a fast ball-form ram is a crit
		{
			const bool bRamCrit = !RA.bRobot && Closing > 800.f;
			RoyaleDamage(Vic, Att, FMath::RoundToInt(Closing * T.RamDamagePerSpeed * (RA.IsAwake() ? T.AwakenKnockMul : 1.f) * (bRamCrit ? 1.15f : 1.f)), bRamCrit, Mid);
			if (Royale.P[Vic].HP <= 0) return; // destroyed by the ram
		}
		if (RA.IsAwake())
		{
			const FLinearColor Col = ZoogiArtifacts::Color(ZoogiArtifacts::Affinity(Balls[Att]->Def.Id));
			AZoogiFX::Spawn(GetWorld(), Mid, Col, 240.f, 0.4f, false, 12.f);
			ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.5f);
			AddPopup(TEXT("SMASH!"), Mid + FVector(0, 0, 130.f), Col);
		}
		else if (K > 520.f) AddPopup(TEXT("BONK!"), Mid + FVector(0, 0, 130.f), ZoogiUtil::PlayerColor(Att));
		if (Att == Royale.HumanIndex || Vic == Royale.HumanIndex)
			if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(FMath::Clamp(K / 1400.f, 0.15f, 1.f));
	};
	Push(a, b, N, CA);
	Push(b, a, -N, CB);
}

// ------------------------------------------------------------------ CPU brain

bool AZoogiGameMode::RoyaleDanger(const FVector2D& P) const
{
	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	if (!Arena) return P.Size() > KR - 120.f;
	if (Arena->GetTheme() == EZoogiArena::Ancient) return !ZoogiAncient::OnFloor(P, 70.f); // Oct 31: islands / bridges at three heights
	const FVector2D Out = P.IsNearlyZero() ? FVector2D::ZeroVector : P.GetSafeNormal() * (FZoogiRoyaleTune::Get().EdgeMargin + 70.f);
	if (Arena->IsOffCourt(P) || Arena->IsOffCourt(P + Out)) return true;
	// holes / pits / gaps the court test does not know: no static floor under the point
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRoyaleFloor), false);
	for (const AZoogiBall* B : Balls) if (B) Q.AddIgnoredActor(B);
	const FVector S(P.X, P.Y, RoyaleFloorZ + 220.f);
	return !GetWorld()->LineTraceTestByObjectType(S, S - FVector(0, 0, 520.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
}

void AZoogiGameMode::RoyaleTickCPU(int32 Idx, float Dt)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* Me = Balls[Idx];
	const float Skill = T.CpuSkill;
	const FVector L3 = Me->BallLocation(), V3 = Me->GetBallVelocity();
	const FVector2D L(L3.X, L3.Y), V(V3.X, V3.Y);
	const float KR = Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	auto Usable = [this](int32 j)
	{
		return Balls.IsValidIndex(j) && Royale.P.IsValidIndex(j) && Balls[j] && !Balls[j]->bOut && Royale.P[j].IsIn() && Royale.P[j].RespawnIn < 0.f;
	};
	FVector2D Want = FVector2D::ZeroVector;
	FVector2D Home = L.IsNearlyZero() ? FVector2D::ZeroVector : -L.GetSafeNormal();
	const bool bAnc = Arena && Arena->GetTheme() == EZoogiArena::Ancient; // Oct 31: Royale field - route along the bridges
	const int32 MyArea = bAnc ? ZoogiAncient::RouteArea(ZoogiAncient::AreaOf(L), L) : 0;
	if (bAnc) { const FVector2D H = ZoogiAncient::PathStep(L, 0); if (!H.IsNearlyZero()) Home = H; }
	if (bAnc && MyArea == 0) { const float Rr = L.Size(); Home = Rr < 1350.f ? L.GetSafeNormal() : (Rr > 1650.f ? -L.GetSafeNormal() : FVector2D::ZeroVector); } // the volcano fills the hub centre
	const FVector2D Pred = L + V * (0.3f + 0.3f * Skill);
	const bool bDanger = RoyaleDanger(Pred) || (!V.IsNearlyZero(50.f) && RoyaleDanger(L + V.GetSafeNormal() * 150.f));
	if (bDanger)
	{
		// edge: brake and head for the middle
		Want = (Home * 1.2f - V.GetSafeNormal()).GetSafeNormal();
		if (Want.IsNearlyZero()) Want = Home;
	}
	else
	{
		R.Rethink -= Dt;
		if (R.Rethink <= 0.f || !Usable(R.Target) || R.Target == Idx)
		{
			int32 Best = -1;
			float BestScore = 1e9f;
			for (int32 j = 0; j < Balls.Num(); ++j)
			{
				if (j == Idx || !Usable(j) || Royale.P[j].ShieldLeft > 0.f || ZoogiShooter::Allies(this, Idx, j)) continue; // b234: no targeting teammates
				const FVector2D TL(Balls[j]->BallLocation());
				float S = FVector2D::Distance(L, TL) + 0.6f * FMath::Max(0.f, KR - TL.Size()); // near and near the edge
				if (Royale.P[j].IsAwake() && !R.IsAwake()) S += 900.f;                      // leave a transformed rival alone
				if (j == R.Target) S -= 220.f;                                               // stick with the plan
				if (bAnc && ZoogiAncient::RouteArea(ZoogiAncient::AreaOf(TL), TL) != MyArea) S += 1400.f; // Ancient: rivals on this island first
				S += FMath::FRandRange(0.f, 260.f) * (1.2f - Skill);
				if (S < BestScore) { BestScore = S; Best = j; }
			}
			R.Target = Best;
			R.Rethink = FMath::FRandRange(1.0f, 2.4f);
			R.Wander = FMath::FRandRange(-14.f, 14.f) * (1.2f - Skill);
		}
		if (Usable(R.Target))
		{
			const AZoogiBall* Tg = Balls[R.Target];
			const FVector T3 = Tg->BallLocation(), TV3 = Tg->GetBallVelocity();
			const FVector2D TL(T3.X, T3.Y), TV(TV3.X, TV3.Y);
			const float Dist = FVector2D::Distance(L, TL);
			const FVector2D Aim = TL + TV * (FMath::Clamp(Dist / 1100.f, 0.f, 0.6f) * Skill);
			// push the target outward: come at it from the centre side
			const FVector2D PushDir = TL.Size() > 250.f ? TL.GetSafeNormal() : (TL - L).GetSafeNormal();
			const FVector2D ToAim = (Aim - L).GetSafeNormal();
			const bool bLined = FVector2D::DotProduct(ToAim, PushDir) > 0.35f || Dist < 280.f;
			FVector2D Goal = bLined ? Aim : Aim - PushDir * 300.f;
			if (RoyaleDanger(Goal)) Goal = bLined ? Aim : TL * 0.6f;
			Want = (Goal - L).GetSafeNormal();
			if (bAnc) // Ancient: a rival on another island -> drive the bridges there instead of straight across the void
			{
				const int32 TgArea = ZoogiAncient::RouteArea(ZoogiAncient::AreaOf(TL), TL);
				if (TgArea >= 0 && TgArea != MyArea) { const FVector2D St = ZoogiAncient::PathStep(L, TgArea); if (!St.IsNearlyZero()) Want = St; }
			}
			if (Royale.P[R.Target].IsAwake() && !R.IsAwake() && Dist < 650.f) Want = ((L - TL).GetSafeNormal() + Home).GetSafeNormal(); // flee
			// form: robot (cannons) at range, ball (ramming) up close
			R.FormThink -= Dt;
			if (R.FormThink <= 0.f)
			{
				R.FormThink = FMath::FRandRange(1.2f, 2.8f);
				bool bWantRobot = R.bRobot;
				if (Dist > 800.f && FMath::FRand() < 0.45f + 0.4f * Skill) bWantRobot = true;
				else if (Dist < 450.f) bWantRobot = false;
				else if (FMath::FRand() < 0.15f) bWantRobot = !R.bRobot;
				if (ZoogiShooter::On(this)) bWantRobot = true; // b234: shooters stay on foot
				if (bWantRobot != R.bRobot) RoyaleTransform(Idx, TEXT("cpu"));
				if (FMath::FRand() < 0.35f) R.StrafeSign = -R.StrafeSign;
			}
			if (R.bRobot && !(Royale.P[R.Target].IsAwake() && !R.IsAwake() && Dist < 650.f))
			{
				// keep shooting range: circle the target, close in when far, back off when close
				const FVector2D To = (TL - L).GetSafeNormal();
				const FVector2D Perp(-To.Y * R.StrafeSign, To.X * R.StrafeSign);
				Want = (Perp * 0.75f + To * FMath::Clamp((Dist - 950.f) / 450.f, -1.f, 1.f)).GetSafeNormal();
				if (RoyaleDanger(L + Perp * 260.f)) R.StrafeSign = -R.StrafeSign;
			}
		}
		else Want = Home * (L.Size() > 300.f ? 0.5f : 0.f);
		Want = Want.GetRotated(R.Wander);
		// never drive straight off: steer back in when the next stretch is bad
		if (!Want.IsNearlyZero() && RoyaleDanger(L + Want * 240.f)) Want = (Want * 0.3f + Home).GetSafeNormal();
	}
	R.SmoothMove = FMath::Vector2DInterpTo(R.SmoothMove, Want, Dt, FMath::Lerp(4.f, 10.f, Skill));
	R.Move = R.SmoothMove.GetClampedToMaxSize(1.f);

	// aim: lead the target; locked (auto-fire) when it is in range, in sight and the body has turned to it
	R.Lock = -1;
	if (Usable(R.Target))
	{
		const AZoogiBall* Tg = Balls[R.Target];
		const FVector T3 = Tg->BallLocation();
		const float Dist = FVector::Dist2D(L3, T3);
		const FVector Lead = T3 + Tg->GetBallVelocity() * FMath::Clamp(Dist / T.BoltSpeed, 0.f, 0.8f) * Skill;
		R.AimPoint = Lead;
		const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(Lead.Y - L3.Y, Lead.X - L3.X));
		R.FacingYaw = FMath::FixedTurn(R.FacingYaw, WantYaw, FMath::Lerp(240.f, 520.f, Skill) * Dt); // turns, does not snap
		const bool bFacing = FMath::Abs(FRotator::NormalizeAxis(WantYaw - R.FacingYaw)) < 12.f;
		if (R.bRobot && bFacing && Dist < T.AutoFireRange && Royale.P[R.Target].ShieldLeft <= 0.f && RoyaleSight(L3, T3)) R.Lock = R.Target;
	}
	else if (!R.Move.IsNearlyZero(0.1f)) R.FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(R.Move.Y, R.Move.X));

	// dodge: hop over a bolt that is about to hit
	if (R.DodgeCD <= 0.f)
	{
		for (const FZoogiRoyaleBolt& Bo : Royale.Bolts)
		{
			if (Bo.Owner == Idx) continue;
			const FVector D = L3 - Bo.Pos;
			if (D.Size() > 520.f || FVector::DotProduct(Bo.Vel.GetSafeNormal(), D.GetSafeNormal()) < 0.93f) continue;
			R.DodgeCD = 1.2f;
			if (FMath::FRand() < 0.25f + 0.45f * Skill) RoyaleJump(Idx, TEXT("cpu dodge"));
			break;
		}
	}
}

// ------------------------------------------------------------------ transform / jump / aim / cannons (Oct 26)

bool AZoogiGameMode::RoyaleSight(const FVector& From, const FVector& To) const
{
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRoyaleSight), false);
	for (const AZoogiBall* B : Balls) if (B) Q.AddIgnoredActor(B);
	for (const AZoogiOrb* O : Orbs) if (O) Q.AddIgnoredActor(O);
	return !GetWorld()->LineTraceTestByObjectType(From + FVector(0, 0, 40.f), To + FVector(0, 0, 20.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q);
}

bool AZoogiGameMode::RoyaleTransform(int32 Idx, const TCHAR* Who)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	if (!bRoyaleMode || Royale.Step != EZoogiRoyaleStep::Fight || !Balls.IsValidIndex(Idx) || !Royale.P.IsValidIndex(Idx)) return false;
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* B = Balls[Idx];
	if (!B || B->bOut || !R.IsIn() || R.RespawnIn >= 0.f || R.MorphCD > 0.f) return false;
	if (ZoogiShooter::On(this) && !ZoogiShooter::AllowTransform(this, Idx, R.bRobot)) return false; // b236: crystals buy the ball dash
	static const bool bRobotOk = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleRobot"));
	if (!ZoogiShooter::On(this) && !R.bRobot && !bRobotOk) return false; // b243 (Evan): two-legged forms only in the shooter modes
	R.bRobot = !R.bRobot;
	R.MorphCD = T.MorphCooldown;
	R.MorphT = 0.f;
	R.FireCD = FMath::Max(R.FireCD, ZoogiRoyaleFeel::MorphSeconds); // the cannons deploy first (fast ToBot)
	R.Lock = -1;
	B->SetMassScale(R.bRobot ? T.RobotMassMul : 1.f);
	if (B->Morph) { B->Morph->SetFacingYaw(R.FacingYaw); B->Morph->SetRobot(R.bRobot); }
	const FVector L = B->BallLocation();
	const FLinearColor Col = ZoogiUtil::PlayerColor(Idx);
	AZoogiFX::Spawn(GetWorld(), L, Col, 150.f, 0.35f, false, 10.f, B);
	AZoogiFX::Spawn(GetWorld(), FVector(L.X, L.Y, RoyaleFloorZ + 4.f), Col, 170.f, 0.4f, true, 8.f);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.55f, R.bRobot ? 1.2f : 0.9f);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE transform P%d -> %s (%s)"), Idx + 1, R.bRobot ? TEXT("ROBOT") : TEXT("BALL"), Who);
	static int32 Shots = 0;
	if (R.bRobot && Shots < 4) TakeShotDelayed(FString::Printf(TEXT("royale_robot_%d"), ++Shots), 1.0f);
	return true;
}

bool AZoogiGameMode::RoyaleJump(int32 Idx, const TCHAR* Who)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	if (!bRoyaleMode || Royale.Step != EZoogiRoyaleStep::Fight || !Balls.IsValidIndex(Idx) || !Royale.P.IsValidIndex(Idx)) return false;
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* B = Balls[Idx];
	if (!B || B->bOut || !R.IsIn() || R.RespawnIn >= 0.f || R.JumpCD > 0.f || !B->Sphere || !B->Sphere->IsSimulatingPhysics()) return false;
	if (!ZoogiPhysics::IsGrounded(B->Sphere, ZoogiConst::ZoogiRadius)) return false;
	R.JumpCD = T.JumpCooldown;
	FVector V = B->Sphere->GetPhysicsLinearVelocity();
	V.Z = FMath::Max(V.Z, 0.f) + T.JumpSpeed;
	B->Sphere->WakeRigidBody();
	B->Sphere->SetPhysicsLinearVelocity(V);
	const FVector L = B->BallLocation();
	AZoogiFX::Spawn(GetWorld(), FVector(L.X, L.Y, RoyaleFloorZ + 3.f), R.bRobot ? FLinearColor(0.4f, 0.8f, 1.f) : FLinearColor(0.9f, 0.95f, 1.f), 110.f, 0.3f, true, 6.f);
	ZoogiUtil::PlaySfx(this, TEXT("Arm"), 0.35f, 1.5f);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE jump P%d (%s)"), Idx + 1, Who);
	return true;
}

void AZoogiGameMode::RoyaleAimHuman(int32 Idx)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	const AZoogiBall* B = Balls[Idx];
	R.Lock = -1;
	if (!B) return;
	const FVector L = B->BallLocation();
	const FVector Dir = Royale.HumanCamDir.GetSafeNormal();
	const float CamYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const bool bShooterAim = ZoogiShooter::On(this); // b237: the crosshair is the aim
	if (!Royale.bHumanAim && !bShooterAim) { R.FacingYaw = CamYaw; R.AimPoint = L + FRotator(0.f, CamYaw, 0.f).Vector() * 1500.f; return; }
	// crosshair lock: rivals inside a small yaw cone around the crosshair (the arena is flat: pitch never blocks a lock)
	int32 Best = -1;
	float BestScore = 1e9f;
	for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num() && !bShooterAim; ++j)
	{
		const AZoogiBall* O = Balls[j];
		const FZoogiRoyalePlayer& RO = Royale.P[j];
		if (j == Idx || !O || O->bOut || !RO.IsIn() || RO.RespawnIn >= 0.f || RO.ShieldLeft > 0.f) continue;
		const FVector TL = O->BallLocation();
		const float D = FVector::Dist2D(L, TL);
		if (D > T.AutoFireRange || D < 1.f) continue;
		const float Off = FMath::Abs(FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(TL.Y - L.Y, TL.X - L.X)) - CamYaw));
		const float Cone = T.AimAssistDeg + FMath::RadiansToDegrees(FMath::Atan2(ZoogiConst::ZoogiRadius * 1.2f, D));
		if (Off > Cone || !RoyaleSight(L, TL)) continue;
		const float Score = Off + D * 0.002f;
		if (Score < BestScore) { BestScore = Score; Best = j; }
	}
	if (Best >= 0)
	{
		const AZoogiBall* O = Balls[Best];
		const float D = FVector::Dist(L, O->BallLocation());
		R.AimPoint = O->BallLocation() + O->GetBallVelocity() * FMath::Clamp(D / T.BoltSpeed, 0.f, 0.8f);
		R.Lock = Best;
	}
	else
	{
		// nothing locked: the cannons point at whatever is under the crosshair
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRoyaleAim), false);
		for (const AZoogiBall* X : Balls) if (X) Q.AddIgnoredActor(X);
		FHitResult Hit;
		const FVector S = Royale.HumanCamLoc, E = S + Dir * 6000.f;
		R.AimPoint = GetWorld()->LineTraceSingleByObjectType(Hit, S, E, FCollisionObjectQueryParams(ECC_WorldStatic), Q) ? FVector(Hit.ImpactPoint) : E;
	}
	const FVector To = R.AimPoint - L;
	R.FacingYaw = (!bShooterAim && FVector2D(To.X, To.Y).Size() > 150.f) ? FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)) : CamYaw; // b237: a shooter faces where the camera looks
}

bool ZoogiRoyaleGatlingShot(); // ZoogiRoyaleHero.cpp (b45)
float GZoogiShotKick = 0.f; double GZoogiShotAt = -10.0; // b264: shooter recoil kick + aim-zoom clock (camera in ZoogiRoyaleInput.cpp)

void AZoogiGameMode::RoyaleFire(int32 Idx)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	FZoogiRoyalePlayer& R = Royale.P[Idx];
	AZoogiBall* B = Balls[Idx];
	if (!B) return;
	R.FireCD = T.FireInterval;
	const bool bLeft = R.bFireLeft;
	R.bFireLeft = !R.bFireLeft;
	FVector M = B->Morph ? B->Morph->Fire(bLeft) : B->BallLocation() + FVector(0, 0, 60.f);
	FVector Dir = (R.AimPoint - M).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = FRotator(0.f, R.FacingYaw, 0.f).Vector();
	float Power = 1.f;
	if (RoyaleHeroFire(Idx, M, Dir, Power)) return; // Oct 31: hero hand muzzle / relic melee + blast
	if (Idx == Royale.HumanIndex) { const FVector D2 = (R.AimPoint - M).GetSafeNormal(); if (!D2.IsNearlyZero()) Dir = D2; }
	if (Idx != Royale.HumanIndex) // CPU spread
	{
		const float E = T.CpuAimError * (1.f - T.CpuSkill * 0.8f);
		Dir = FRotator(FMath::FRandRange(-E, E) * 0.4f, FMath::FRandRange(-E, E), 0.f).RotateVector(Dir);
	}
	const FLinearColor Col = ZoogiUtil::PlayerColor(Idx);
	FZoogiRoyaleBolt Bo;
	Bo.Pos = M;
	const bool bGatling = Idx == Royale.HumanIndex && ZoogiRoyaleGatlingShot();
	if (bGatling) Dir = FRotator(FMath::FRandRange(-0.7f, 0.7f), FMath::FRandRange(-1.1f, 1.1f), 0.f).RotateVector(Dir); // b45 Gatling: a light spread
	Bo.Vel = Dir * T.BoltSpeed * (bGatling ? 1.7f : 1.f);
	Bo.Owner = Idx;
	Bo.Power = Power;
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AZoogiRoyaleBolt* A = GetWorld()->SpawnActor<AZoogiRoyaleBolt>(AZoogiRoyaleBolt::StaticClass(), FTransform(Dir.Rotation(), M), SP))
	{
		// Oct 27 Marble Clash tracer: a thin white-hot streak, faintly tinted with the shooter's colour
		A->Init(FMath::Lerp(Col, FLinearColor(0.85f, 0.95f, 1.f), 0.65f));
		A->SetStreak(bGatling ? 90.f : 30.f, M, Dir);
		Bo.Actor = A;
	}
	Royale.Bolts.Add(Bo);
	AZoogiFX::Spawn(GetWorld(), M, FLinearColor(1.f, 0.82f, 0.3f), 22.f, 0.07f, false, 14.f); // yellow muzzle flash
	if (Idx == Royale.HumanIndex && ZoogiShooter::On(this)) { GZoogiShotKick = FMath::Min(GZoogiShotKick + 0.9f, 3.f); GZoogiShotAt = GetWorld()->GetTimeSeconds(); } // b264 recoil
	ZoogiUtil::PlaySfx(this, TEXT("Zap"), Idx == Royale.HumanIndex ? 0.32f : 0.16f, 1.35f + FMath::FRandRange(-0.06f, 0.06f));
	static int32 Logged = 0;
	if (Logged < 12) { ++Logged; UE_LOG(LogZoogi, Log, TEXT("ROYALE fire P%d at P%d (%s cannon)"), Idx + 1, R.Lock + 1, bLeft ? TEXT("left") : TEXT("right")); }
}

void AZoogiGameMode::RoyaleTickBolts(float Dt)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRoyaleBolt), false);
	for (const AZoogiBall* X : Balls) if (X) Q.AddIgnoredActor(X);
	for (int32 k = Royale.Bolts.Num() - 1; k >= 0; --k)
	{
		FZoogiRoyaleBolt& Bo = Royale.Bolts[k];
		const FVector From = Bo.Pos, Step = Bo.Vel * Dt, To = From + Step;
		const float Len2 = FMath::Max(Step.SizeSquared(), 1.f);
		// Zoogis on the way (swept sphere vs sphere; a robot's hit capsule reaches up to its chest)
		int32 HitBall = -1;
		float HitT = 2.f;
		for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
		{
			const AZoogiBall* O = Balls[j];
			if (j == Bo.Owner || !O || O->bOut || !Royale.P[j].IsIn() || Royale.P[j].RespawnIn >= 0.f) continue;
			const FVector C = O->BallLocation();
			const float t = FMath::Clamp(FVector::DotProduct(C - From, Step) / Len2, 0.f, 1.f);
			FVector D = From + Step * t - C;
			if (Royale.P[j].bRobot && D.Z > 0.f) D.Z = FMath::Max(0.f, D.Z - 60.f);
			if (D.Size() <= ZoogiConst::ZoogiRadius + T.BoltRadius && t < HitT) { HitT = t; HitBall = j; }
		}
		FHitResult WH;
		const bool bWall = GetWorld()->LineTraceSingleByObjectType(WH, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Q);
		const float WallT = bWall ? FVector::Dist(From, WH.ImpactPoint) / FMath::Sqrt(Len2) : 2.f;
		if (HitBall >= 0 && HitT <= WallT)
		{
			const FZoogiRoyaleBolt Copy = Bo;
			if (Copy.Actor.IsValid()) Copy.Actor->Destroy();
			Royale.Bolts.RemoveAtSwap(k);
			RoyaleBoltHit(HitBall, Copy, From + Step * HitT);
			continue;
		}
		if (bWall || Bo.Travel + Step.Size() > T.BoltRange || To.Z < RoyaleFloorZ - 400.f)
		{
			if (bWall) AZoogiFX::Spawn(GetWorld(), WH.ImpactPoint, FLinearColor(1.f, 0.8f, 0.35f), 40.f, 0.15f, false, 10.f); // spark on walls
			if (Bo.Actor.IsValid()) Bo.Actor->Destroy();
			Royale.Bolts.RemoveAtSwap(k);
			continue;
		}
		Bo.Pos = To;
		Bo.Travel += Step.Size();
		if (AZoogiRoyaleBolt* BA = Cast<AZoogiRoyaleBolt>(Bo.Actor.Get())) BA->SetStreak(FMath::Min(T.TracerLength * 100.f, Bo.Travel + 30.f), Bo.Pos, Bo.Vel.GetSafeNormal());
	}
}

void AZoogiGameMode::RoyaleBoltHit(int32 Victim, const FZoogiRoyaleBolt& Bo, const FVector& At)
{
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	if (!Royale.P.IsValidIndex(Victim)) return;
	FZoogiRoyalePlayer& RV = Royale.P[Victim];
	const FLinearColor Col = ZoogiUtil::PlayerColor(Bo.Owner);
	if (ZoogiShooter::Allies(this, Victim, Bo.Owner)) return; // b234: no friendly fire
	if (RV.ShieldLeft > 0.f)
	{
		AZoogiFX::Spawn(GetWorld(), At, FLinearColor(0.8f, 0.9f, 1.f), 60.f, 0.2f, false, 8.f); // the respawn shield eats it
		return;
	}
	FVector Dir = Bo.Vel;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	float K = T.BoltKnock * Royale.Surge * Bo.Power;
	if (RV.bRobot) K *= T.RobotTakenMul;
	if (RV.IsAwake()) K *= T.AwakenTakenMul;
	FZoogiRoyaleKick Kick;
	Kick.Victim = Victim;
	Kick.DeltaV = Dir * K + FVector(0, 0, 60.f);
	static const bool bShooterKnock = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShooterKnock"));
	if (!ZoogiShooter::On(this) || bShooterKnock) Royale.Kicks.Add(Kick); // b238 (Evan: "I get moved by enemy shots"): in the SHOOTER modes a hit hurts, it does not shove you
	if (Royale.P.IsValidIndex(Bo.Owner) && Bo.Owner != Victim)
	{
		RV.LastHitBy = Bo.Owner;
		RV.LastHitAt = Clock;
	}
	if (!ZoogiShooter::On(this) || bShooterKnock) RV.StunLeft = FMath::Max(RV.StunLeft, T.BoltStun); // b238: no stun either - you keep control under fire
	// Oct 27 Marble Clash impact: yellow-orange sparks + a small white pop, a damage number over the victim
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 0.78f, 0.25f), 70.f, 0.18f, false, 14.f);
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 1.f, 1.f), 14.f, 0.08f, false, 16.f);
	if (ZoogiShooter::On(this)) ZoogiVoyager::Fx(GetWorld(), TEXT("ImpactMetal"), At, 1.f, 1.2f); // b237: Voyager hit sparks
	(void)Col;
	ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.3f, 0.85f);
	if (Bo.Owner == Royale.HumanIndex) Royale.HumanHitAt = GetWorld()->GetTimeSeconds();
	if (Victim == Royale.HumanIndex)
		if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.2f);
	const bool bCrit = FMath::FRand() < T.CritChance;
	RoyaleDamage(Victim, Bo.Owner, FMath::RoundToInt(T.BoltDamage * Bo.Power * (bCrit ? T.CritMul : 1.f) * FMath::FRandRange(0.97f, 1.03f)), bCrit, At);
}


// ------------------------------------------------------------------ capture (-ZoogiRoyaleFrames -ZoogiShots=<dir>)

void AZoogiGameMode::RoyaleFrameDump(float Dt)
{
	static const bool bFrames = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleFrames"));
	if (!bFrames || ShotDir.IsEmpty() || Royale.FrameShots >= 2400) return;
	Royale.FrameClock += Dt;
	static const float FrameHz = [] { float H = 15.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRoyaleFrameHz="), H); return FMath::Clamp(H, 0.2f, 30.f); }(); // Oct 31: sparse captures
	if (Royale.FrameClock < 1.f / FrameHz) return;
	Royale.FrameClock = 0.f;
	const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("frame_%05d.png"), Royale.FrameShots++));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
}
