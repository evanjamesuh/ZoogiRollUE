#include "ZoogiGameMode.h"
#include "ZoogiSpawnElevator.h"
#include "ZoogiIceWall.h"
#include "ZoogiEpisodeDirector.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiKFXPreview.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiRock.h"
#include "ZoogiClone.h"
#include "ZoogiFX.h"
#include "ZoogiPhysics.h"
#include "ZoogiPlayerController.h"
#include "ZoogiHUD.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiCombo.h"
#include "ZoogiStageForming.h"
#include "ZoogiArenaPiece.h"
#include "EngineUtils.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCardHandWidget.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ZoogiCPUBrain.h"
#include "ZoogiPortals.h"
#include "ZoogiObstacle.h"

AZoogiGameMode::AZoogiGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AZoogiPlayerController::StaticClass();
	HUDClass = AZoogiHUD::StaticClass();

	MenuPlayers.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		MenuPlayers[i].ZoogiIndex = i % FMath::Max(1, ZoogiUtil::Roster().Num());
		MenuPlayers[i].bCPU = i > 0; // P1 human, others CPU by default
	}
	MenuPlayers[1].ZoogiIndex = 1;
}

void AZoogiGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Chaos materials. Restitution combines by average, so Zoogi-Zoogi ~0.75 and Zoogi-orb ~0.68 like the web profiles.
	BallPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.75f, false);
	OrbPM = ZoogiPhysics::MakeMaterial(this, 0.25f, 0.6f, false);

	const TCHAR* Cmd = FCommandLine::Get();
	bAuto = FParse::Param(Cmd, TEXT("ZoogiAuto"));
	bPowerTest = FParse::Param(Cmd, TEXT("ZoogiPowerTest"));
	bMenuShot = FParse::Param(Cmd, TEXT("ZoogiMenuShot"));
	bArtifactMode = !FParse::Param(Cmd, TEXT("ZoogiClassicPowers"));
	bCinematicCam = FParse::Param(Cmd, TEXT("ZoogiCinematic"));
	bRockTest = FParse::Param(Cmd, TEXT("ZoogiRockTest"));
	bClimaxTest = FParse::Param(Cmd, TEXT("ZoogiClimaxTest"));
	bNoForming = FParse::Param(Cmd, TEXT("ZoogiNoForm"));
	bFormShots = FParse::Param(Cmd, TEXT("ZoogiFormShots"));
	bDamageShots = FParse::Param(Cmd, TEXT("ZoogiDamageShots"));
	FParse::Value(Cmd, TEXT("ZoogiFormSlow="), FormTimeScale);
	if (bFormShots && FormTimeScale <= 1.f) FormTimeScale = 4.f;
	{
		int32 Vol = 100;
		if (FParse::Value(Cmd, TEXT("ZoogiSfxVol="), Vol)) ZoogiUtil::SetSfxVolume(Vol / 100.f);
	}
	FParse::Value(Cmd, TEXT("ZoogiShots="), ShotDir);
	if (FParse::Param(Cmd, TEXT("ZoogiKFXPreview"))) GetWorld()->SpawnActor<AZoogiKFXPreview>(); // review captures of the KriptoFX ports
	FParse::Value(Cmd, TEXT("ZoogiQuit="), QuitAfter);
	{
		// Gameplay random stream (host only: the GameMode exists on the server).
		const TCHAR* Src = TEXT("fresh");
		if (FParse::Value(Cmd, TEXT("ZoogiMatchSeed="), GameSeed)) Src = TEXT("ZoogiMatchSeed");
		else if (FParse::Value(Cmd, TEXT("ZoogiEpisodeSeed="), GameSeed)) Src = TEXT("ZoogiEpisodeSeed");
		else GameSeed = (int32)(FPlatformTime::Cycles() ^ (uint32)FMath::Rand());
		GameRng.Initialize(GameSeed);
		UE_LOG(LogZoogi, Log, TEXT("GAME rng seed=%d source=%s"), GameSeed, Src);
	}
	int32 NumPlayers = 0;
	if (FParse::Value(Cmd, TEXT("ZoogiPlayers="), NumPlayers)) MenuNumPlayers = FMath::Clamp(NumPlayers, 2, 4);
	int32 ArenaIdx = 0;
	if (FParse::Value(Cmd, TEXT("ZoogiArena="), ArenaIdx)) MenuArena = (EZoogiArena)FMath::Clamp(ArenaIdx, 0, (int32)EZoogiArena::MAX - 1);
	int32 Rounds = 0;
	if (FParse::Value(Cmd, TEXT("ZoogiRounds="), Rounds)) MenuRounds = FMath::Clamp(Rounds, 1, 30);
	if (!ShotDir.IsEmpty()) IFileManager::Get().MakeDirectory(*ShotDir, true);
	// -ZoogiPicks=4,5,6,0 : roster index per player slot (automated tests of specific Zoogis).
	FString Picks;
	if (FParse::Value(Cmd, TEXT("ZoogiPicks="), Picks, false))
	{
		TArray<FString> Parts;
		Picks.ParseIntoArray(Parts, TEXT(","));
		for (int32 i = 0; i < Parts.Num() && i < MenuPlayers.Num(); ++i) MenuPlayers[i].ZoogiIndex = FMath::Clamp(FCString::Atoi(*Parts[i]), 0, ZoogiUtil::Roster().Num() - 1);
	}

	Arena = GetWorld()->SpawnActor<AZoogiArena>(AZoogiArena::StaticClass(), FTransform::Identity);
	Arena->Build(MenuArena);
	SpawnOrbs();
	Phase = EZoogiPhase::Menu;
	PlayMusic(TEXT("MenuMusic"));
	UZoogiCardHandWidget::Spawn(this); // cardui: opt-in -ZoogiCardHandUI drag-to-assign hand (no-op without the flag; shows only with cards on)

	// 2v2 Competitive Combo: -ZoogiCombo (mode), -ZoogiComboDemo=1|2|3 (scripted verification runs, implies -ZoogiCombo).
	FParse::Value(Cmd, TEXT("ZoogiComboDemo="), ComboDemo);
	if (FParse::Param(Cmd, TEXT("ZoogiCombo")) || ComboDemo > 0) ComboSetMode(true);
	if (bAuto)
	{
		for (FZoogiPlayerSetup& P : MenuPlayers) P.bCPU = true;
		if (FParse::Param(Cmd, TEXT("ZoogiHumanTest")) || FParse::Param(Cmd, TEXT("ZoogiUiTest")) || FParse::Param(Cmd, TEXT("ZoogiUIPolish"))) MenuPlayers[0].bCPU = false; // ui-polish capture: hold Awaken without UITEST scripts
		if (ComboDemo > 0) for (FZoogiPlayerSetup& P : MenuPlayers) P.bCPU = false; // the demo director plays every Zoogi through the human controls
		if (!bMenuShot)
		{
			StartMatch();
		}
	}
}

AZoogiBall* AZoogiGameMode::GetActiveBall() const
{
	return Balls.IsValidIndex(ActivePlayer) ? Balls[ActivePlayer].Get() : nullptr;
}

int32 AZoogiGameMode::OrbsLeft() const
{
	int32 N = 0;
	for (const AZoogiOrb* O : Orbs) if (O && !O->bOut) ++N;
	return N;
}

bool AZoogiGameMode::IsHumanAiming() const
{
	const AZoogiBall* B = GetActiveBall();
	return Phase == EZoogiPhase::Aiming && B && !B->bCPU;
}

// ------------------------------------------------------------------ menu

void AZoogiGameMode::MenuAction(FName Action, int32 Index)
{
	// v3 P6: one physical input can arrive twice in the same frame (touch + its emulated mouse click, a key repeat plus a
	// button). Gameplay decisions (cards, follow-up, episode skip) are accepted once per (action, index, frame).
	{
		const FString A = Action.ToString();
		if (A.StartsWith(TEXT("Card")) || A.StartsWith(TEXT("Follow")) || A.StartsWith(TEXT("Episode")))
		{
			if (Action == LastGameplayAction && Index == LastGameplayIndex && GFrameCounter == LastGameplayFrame)
			{
				++DuplicateInputsIgnored;
				UE_LOG(LogZoogi, Log, TEXT("INPUT duplicate %s %d in frame %llu ignored"), *A, Index, (uint64)GFrameCounter);
				return;
			}
			LastGameplayAction = Action; LastGameplayIndex = Index; LastGameplayFrame = GFrameCounter;
		}
	}
	const int32 RosterNum = ZoogiUtil::Roster().Num();
	if (Action == "Players") MenuNumPlayers = bComboMode ? 4 : FMath::Clamp(Index, 2, 4);
	else if (Action == "Mode") ComboSetMode(Index == 1);
	else if (Action == "ComboSelect") ComboSelect(Index);
	else if (Action == "ComboForce") { if (IsAimStep()) ComboSetForce((EZoogiForce)FMath::Clamp(Index, 0, 2)); }
	else if (Action == "FollowUp") ComboAcceptFollowUp();
	else if (Action == "FollowSkip") ComboDeclineFollowUp();
	else if (Action == "ZoogiNext" && MenuPlayers.IsValidIndex(Index)) MenuPlayers[Index].ZoogiIndex = (MenuPlayers[Index].ZoogiIndex + 1) % RosterNum;
	else if (Action == "ZoogiPrev" && MenuPlayers.IsValidIndex(Index)) MenuPlayers[Index].ZoogiIndex = (MenuPlayers[Index].ZoogiIndex + RosterNum - 1) % RosterNum;
	else if (Action == "ToggleCPU" && MenuPlayers.IsValidIndex(Index)) MenuPlayers[Index].bCPU = !MenuPlayers[Index].bCPU;
	else if (Action == "Arena")
	{
		MenuArena = (EZoogiArena)FMath::Clamp(Index, 0, (int32)EZoogiArena::MAX - 1);
		ClearMatch();
		Arena->Build(MenuArena);
		SpawnOrbs();
	}
	else if (Action == "Rounds") MenuRounds = FMath::Clamp(Index, 1, 30);
	else if (Action == "SfxVol") ZoogiUtil::SetSfxVolume(FMath::Clamp(Index, 0, 100) / 100.f);
	else if (Action == "Start") StartMatch();
	else if (Action == "Power") { if (!bArtifactMode) TryPower(); }
	// Milestone "Awaken": Awaken actions and clickable aiming (Canvas HUD buttons -> here).
	else if (Action == "AwakenPick") AwakenPick = FMath::Max(0, Index);
	else if (Action == "Infuse") AwakenInfuse(Index >= 0 ? Index : AwakenPick);
	else if (Action == "Guard") AwakenGuard(Index >= 0 ? Index : AwakenPick);
	else if (Action == "Save") AwakenSave();
	else if (Action == "CardToggle") CardsToggle(Index);
	else if (Action == "CardClear") CardsClear();
	else if (Action == "CardCommit") CardsCommit();
	else if (Action == "CardAwaken") CardsToggleAwaken();
	else if (Action == "EpisodeSkip") { if (UZoogiEpisodeDirector* D = UZoogiEpisodeDirector::Get(this)) D->SkipCurrent(); }
	else if (Action == "AimNudge") NudgeAim(Index);
	else if (Action == "AimPower") AimPowerStep = FMath::Clamp(Index, 0, 3);
	else if (Action == "AimConfirm") CommitLock();
	else if (Action == "AimCancel") ClearLock();
	else if (Action == "AimNext") CycleLock(false);
	else if (Action == "AnimeSkip") { if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) Anime->Skip(); }
	else if (Action == "FormSkip") SkipStageForming();
	else if (Action == "CamStyle") bCinematicCam = !bCinematicCam;
	else if (Action == "Menu") ReturnToMenu();
	else if (Action == "Rematch") StartMatch();
	else if (Action == "Quit") UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	// UI sounds (team brief: short, soft, rounded, never harsh when repeated): confirm / cancel / click.
	if (Action == "Start" || Action == "Rematch" || Action == "Infuse" || Action == "Guard" || Action == "Save" || Action == "CardCommit" || Action == "AimConfirm" || Action == "FollowUp")
		ZoogiUtil::PlaySfx(this, TEXT("UiConfirm"), 0.4f);
	else if (Action == "AimCancel" || Action == "Menu" || Action == "Quit")
		ZoogiUtil::PlaySfx(this, TEXT("UiCancel"), 0.4f);
	else if (Action != "Power" && Action != "AimNudge" && Action != "AnimeSkip" && Action != "FormSkip")
		ZoogiUtil::PlaySfx(this, TEXT("UiClick"), 0.55f);
	MenuRevision++;
}

void AZoogiGameMode::ClearMatch()
{
	EndPowers();
	TargetLock.Reset();
	bSoftLock = false;
	for (AZoogiBall* B : Balls) if (B) B->Destroy();
	for (AZoogiOrb* O : Orbs) if (O) O->Destroy();
	Balls.Reset();
	Orbs.Reset();
	Popups.Reset();
	BigAge = 99.f;
	Flyers.Reset();
	Feed.Reset();
	bAimActive = false;
	AimPull = FVector::ZeroVector;
	Ledger.Reset();
	PullLeft = 0.f;
	bFreeAim = false;
	AimCutDeg = 0.f;
	bClimaxUsed = false;
	bClimaxHitPending = false;
	ClimaxLeft = 0.f;
	ClimaxFlashAt = -100.f;
	if (ComboArena) { ComboArena->Destroy(); ComboArena = nullptr; }
	Callouts.Reset();
	PendingKicks.Reset();
	bFollowUpOffered = false;
	if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) Anime->ResetMatch();
	AZoogiIceWall::DestroyAll(GetWorld(), TEXT("match reset")); // Oct 14: Crystal Collapse walls last the match only
}

bool AZoogiGameMode::OrbSpot(FVector& L) const
{
	if (!Arena) return true;
	// Nudge orbs off solid court pieces (Night Circuit pads and posts).
	const FVector2D P(L.X, L.Y);
	const FVector2D Dir = P.IsNearlyZero() ? FVector2D(1.f, 0.f) : P.GetSafeNormal();
	for (float Off : { 0.f, 70.f, -70.f, 140.f, -140.f, 210.f, -210.f })
	{
		const FVector2D Q = P + Dir * Off;
		if (Arena->IsSpotClear(Q, ZoogiConst::OrbRadius) && !Arena->IsOffCourt(Q + Dir * 100.f)) { L.X = Q.X; L.Y = Q.Y; return true; }
	}
	return false;
}

void AZoogiGameMode::SpawnOrbs()
{
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	// createOrbs: 15 orbs on a ring of radius 5, plus a small centre triangle for a fuller court.
	const int32 NRing = (Arena && Arena->OrbCountOverride > 0) ? Arena->OrbCountOverride : ZoogiConst::OrbCount; // r19: Meadow 6
	for (int32 i = 0; i < NRing; ++i)
	{
		// Per-map ring (Meadow r9: wider ring clear of the full-size spiral); other maps keep the default ring.
		const float RingR = (Arena && Arena->OrbRingRadiusOverride > 0.f) ? Arena->OrbRingRadiusOverride : ZoogiConst::OrbRingRadius;
		const float A = (float)i / NRing * 2.f * PI + (Arena ? FMath::DegreesToRadians(Arena->OrbRingYawOffset) : 0.f);
		FVector L(FMath::Cos(A) * RingR, FMath::Sin(A) * RingR, ZoogiConst::OrbRadius + 1.f);
		if (!OrbSpot(L)) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (O)
		{
			O->InitOrb(OrbPM, Decel);
			if (i % 5 == 0) O->MakeStar(); // three gold star orbs: double points + a power charge
			Orbs.Add(O);
		}
	}
	for (int32 i = 0; i < ((Arena && Arena->OrbCountOverride > 0) ? 0 : 3); ++i)
	{
		const float A = i * 2.f * PI / 3.f + PI / 6.f;
		FVector L(FMath::Cos(A) * 90.f, FMath::Sin(A) * 90.f, ZoogiConst::OrbRadius + 1.f);
		if (Arena && Arena->IsNeonRect()) L = FVector(-200.f + i * 200.f, 0.f, L.Z); // between the centre channels
		if (!OrbSpot(L)) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (O)
		{
			O->InitOrb(OrbPM, Decel);
			Orbs.Add(O);
		}
	}
	// Arena-provided orb spots off the main disc (Frozen: one on each bleacher islet's apron, reached over the bridges).
	if (Arena)
		for (const FVector& L : Arena->ExtraOrbSpots)
			if (AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L)))
			{
				O->InitOrb(OrbPM, Decel);
				Orbs.Add(O);
			}
	for (AZoogiOrb* O : Orbs) ZoogiUtil::ApplyOrbStyle(O); // Oct 8: [ZoogiRoll.Characters] OrbStyle
}

void AZoogiGameMode::StartMatch()
{
	if (bComboMode) { ComboStartMatch(); return; }
	ZoogiUtil::SetComboColors(false);
	ClearMatch();
	if (Arena->GetTheme() != MenuArena) Arena->Build(MenuArena);
	Arena->ResetRocks(); // destructible rocks come back every match
	SpawnOrbs();
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	for (int32 i = 0; i < MenuNumPlayers; ++i)
	{
		const FZoogiPlayerSetup& S = MenuPlayers[i];
		const FVector L = Arena->GetSpawnPoint(i);
		const float Yaw = Arena->GetSpawnYaw(i);
		AZoogiBall* B = GetWorld()->SpawnActor<AZoogiBall>(AZoogiBall::StaticClass(), FTransform(FRotator(0, Yaw, 0), L));
		if (!B) continue;
		B->InitZoogi(Roster[S.ZoogiIndex % Roster.Num()], i, S.bCPU, BallPM, Arena->GetRollingDecel());
		B->SpawnLocation = L;
		B->SpawnYaw = Yaw;
		B->AimYaw = Yaw;
		if (bPowerTest) B->Charges = 3;
		Balls.Add(B);
	}
	RoundNumber = 1;
	WinnerIndex = -1;
	ShotsFired = 0;
	TurnCounter = 0;
	UE_LOG(LogZoogi, Log, TEXT("Match started: %d players on %s"), Balls.Num(), *ZoogiUtil::ArenaName(MenuArena));
	NotifyMatchEvent(EZoogiMatchEvent::MatchStart, 0); // arena music (falls back to MatchMusic) + announcer
	if (!BeginStageForming(false)) { BeginTurn(0); AZoogiSpawnElevator::PlayAll(this); } // spawn elevators: presentation only
}

bool AZoogiGameMode::BeginStageForming(bool bCombo)
{
	if (!Arena || !Arena->Forming || bNoForming)
	{
		if (Arena) Arena->ResetPieces();
		return false;
	}
	if (Arena->Forming->Num() == 0) { Arena->ResetPieces(); return false; }
	bFormingCombo = bCombo;
	FormPass = 0;
	FormCamMode = 0;
	Arena->BeginForming(FormTimeScale);
	bElevatorsPlayed = false;
	Phase = EZoogiPhase::Forming;
	PhaseTime = 0.f;
	FormShotIdx = 0;
	ZoogiUtil::PlaySfx(this, TEXT("RockCrack"), 0.5f, 0.6f);
	UE_LOG(LogZoogi, Log, TEXT("STAGE FORMING: %d pieces, %.1f s (x%.1f slow)%s"), Arena->Forming->Num(), Arena->Forming->GetLength() * FormTimeScale, FormTimeScale, bCombo ? TEXT(" [2v2 COMBO]") : TEXT(""));
	return true;
}

void AZoogiGameMode::SkipStageForming()
{
	if (Phase != EZoogiPhase::Forming || !Arena || !Arena->Forming) return;
	if (PhaseTime < 0.25f) return;
	Arena->Forming->Skip();
	UE_LOG(LogZoogi, Log, TEXT("STAGE FORMING skipped at %.2f s"), PhaseTime);
}

void AZoogiGameMode::ReturnToMenu()
{
	ClearMatch();
	SpawnOrbs();
	Phase = EZoogiPhase::Menu;
	PhaseTime = 0.f;
	NotifyMatchEvent(EZoogiMatchEvent::ReturnToMenu);
	PlayMusic(TEXT("MenuMusic"));
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnMenu();
}

// ------------------------------------------------------------------ turns

void AZoogiGameMode::BeginTurn(int32 PlayerIdx)
{
	ActivePlayer = PlayerIdx;
	Phase = EZoogiPhase::Aiming;
	PhaseTime = 0.f;
	SettleTimer = 0.f;
	bAimActive = false;
	AimPull = FVector::ZeroVector;
	bCPUPlanned = false;
	EndPowers();
	++TurnCounter;
	Ledger.Reset();
	TurnGuardBlocks = 0;
	ResolveNotes.Reset();
	bFreeAim = false;
	AimCutDeg = 0.f;
	AimPowerStep = 0;
	HumanDragFrac = -1.f;
	bInfusedContact = false;
	bLastHitCrit = false;
	ContactImpactSpeed = 0.f;
	PullLeft = 0.f;
	Preview = FZoogiAimPreview();

	for (AZoogiBall* B : Balls) { B->LastHitPlayer = -1; B->SetActiveTurn(false); }
	for (AZoogiOrb* O : Orbs) if (O) O->LastHitBy = -1;

	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	if (Ball->bOut) RespawnBall(Ball);
	Ball->SetActiveTurn(true);

	// Default heading: toward the arena centre (camera spec A5).
	const FVector L = Ball->BallLocation();
	if (FVector2D(L.X, L.Y).Size() > 200.f)
	{
		Ball->AimYaw = FRotator(FVector(-L.X, -L.Y, 0.f).Rotation()).Yaw;
	}
	if (Ball->bCPU) PlanCPUShot();
	ClearLock();
	SetStage(EZoogiStage::Draw);

	Banner = FString::Printf(TEXT("P%d  %s%s"), PlayerIdx + 1, *Ball->Def.Name, Ball->bCPU ? TEXT("  (CPU)") : TEXT(""));
	if (bComboMode)
	{
		ComboResetShot();
		Banner = FString::Printf(TEXT("%s  P%d  %s%s"), *TeamName(ComboTeam), PlayerIdx + 1, *Ball->Def.Name, Ball->bCPU ? TEXT("  (CPU)") : TEXT(""));
	}
	ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.45f);
	BannerTime = 0.f;
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnTurnStarted(Ball);
}

void AZoogiGameMode::EndTurn()
{
	if (bComboMode) { ComboEndTurn(); return; }
	if (AZoogiBall* Shooter = GetActiveBall())
	{
		int32 Moved = 0;
		for (AZoogiOrb* O : Orbs) if (O && !O->bOut && O->GetVelocity().Size() >= 0.f) ++Moved;
		UE_LOG(LogZoogi, Log, TEXT("Turn end: P%d travelled %.0f cm (shot %.0f cm/s), %s, orbs left %d, %.1fs"), ActivePlayer + 1,
			FVector::Dist2D(Shooter->bOut ? Shooter->OutLocation : Shooter->BallLocation(), ShotStart), ShotSpeed, Shooter->bOut ? TEXT("OUT") : TEXT("on court"), OrbsLeft(), PhaseTime); // Oct 15: OUT = distance to where it was ruled out (was a -1 placeholder)
	}
	EndPowers();
	for (AZoogiBall* B : Balls) if (B->bOut) RespawnBall(B);
	if (OrbsLeft() == 0)
	{
		FinishGame();
		return;
	}
	int32 Next = ActivePlayer;
	for (int32 Guard = 0; Guard <= Balls.Num(); ++Guard)
	{
		Next = (Next + 1) % FMath::Max(1, Balls.Num());
		if (Next == 0)
		{
			++RoundNumber;
			if (RoundNumber > MenuRounds)
			{
				FinishGame();
				return;
			}
		}
		AZoogiBall* NB = Balls[Next];
		if (NB && NB->StunTurns > 0 && Guard < Balls.Num() - 1)
		{
			NB->StunTurns--;
			UE_LOG(LogZoogi, Log, TEXT("P%d is stunned and skips this turn"), Next + 1);
			if (Balls.IsValidIndex(Next)) AddFeed(FString::Printf(TEXT("P%d %s  is stunned and skips a turn"), Next + 1, *Balls[Next]->Def.Name), FLinearColor(0.8f, 0.65f, 1.f));
			AddPopup(FString::Printf(TEXT("P%d STUNNED - SKIPS TURN"), Next + 1), NB->BallLocation() + FVector(0, 0, 120.f), FLinearColor(0.7f, 0.4f, 1.f));
			continue;
		}
		break;
	}
	BeginTurn(Next);
}

void AZoogiGameMode::FinishGame()
{
	Phase = EZoogiPhase::GameOver;
	PhaseTime = 0.f;
	bAimActive = false;
	WinnerIndex = 0;
	bool bTie = false;
	for (int32 i = 1; i < Balls.Num(); ++i)
	{
		if (Balls[i]->Score > Balls[WinnerIndex]->Score) { WinnerIndex = i; bTie = false; }
		else if (Balls[i]->Score == Balls[WinnerIndex]->Score) bTie = true;
	}
	for (AZoogiBall* B : Balls) B->SetActiveTurn(false);
	if (bTie)
	{
		Banner = TEXT("IT'S A DRAW!");
		WinnerIndex = -1;
	}
	else if (Balls.IsValidIndex(WinnerIndex))
	{
		Banner = FString::Printf(TEXT("P%d %s WINS!"), WinnerIndex + 1, *Balls[WinnerIndex]->Def.Name);
		Balls[WinnerIndex]->SetActiveTurn(true);
	}
	BannerTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Game over: %s"), *Banner);
	{
		FString Final;
		for (AZoogiBall* B : Balls) if (B) Final += FString::Printf(TEXT("%s=%d "), *B->Def.Name, B->Score);
		UE_LOG(LogZoogi, Log, TEXT("FinalScores: %s"), *Final);
	}
	ZoogiUtil::PlaySfx(this, TEXT("Win"), 0.8f);
	NotifyMatchEvent(EZoogiMatchEvent::MatchOver, WinnerIndex);
	PlayVictoryCrowd();
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnGameOver(GetActiveBall());
}

void AZoogiGameMode::PlayVictoryCrowd()
{
	// Wave 1 right after the win sting, wave 2 (denser, the main cheer) ~3 s later. Both sit under the music (the brief:
	// the crowd always sits underneath) and build by density, not loudness. A draw gets only the first, polite wave.
	ZoogiUtil::PlaySfx(this, TEXT("CrowdWave1"), 0.45f);
	if (WinnerIndex < 0) return;
	FTimerHandle H;
	TWeakObjectPtr<AZoogiGameMode> Self(this);
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([Self]()
	{
		if (Self.IsValid() && Self->Phase == EZoogiPhase::GameOver) ZoogiUtil::PlaySfx(Self.Get(), TEXT("CrowdWave2"), 0.5f);
	}), 3.0f, false);
	UE_LOG(LogZoogi, Log, TEXT("Victory crowd: wave 1 now, wave 2 in 3.0 s"));
}

bool AZoogiGameMode::IsClimaxMoment(const AZoogiBall* Shooter, int32 KOs) const
{
	if (bClimaxUsed || !Shooter || KOs <= 0 || !Balls.IsValidIndex(ActivePlayer)) return false;
	if (bClimaxTest) return true;
	if (bComboMode) return bComboMatchOver && ComboWinner == Shooter->TeamIndex; // the match-winning knock-out
	if (RoundNumber < MenuRounds) return false;
	// Shooter must be the sole leader after this turn's ledger.
	for (const AZoogiBall* B : Balls) if (B && B != Shooter && B->Score >= Shooter->Score) return false;
	// Final turn of the match: no orbs left, or nobody after the shooter still gets a turn this (last) round.
	int32 TurnsLeft = 0;
	for (int32 j = ActivePlayer + 1; j < Balls.Num(); ++j) if (Balls[j] && Balls[j]->StunTurns <= 0) ++TurnsLeft;
	if (OrbsLeft() == 0 || TurnsLeft == 0) return true; // the match-winning KO
	// Next-to-last turn of the match: only if this KO took the sole lead (the shooter was not leading alone before it).
	if (TurnsLeft > 1) return false;
	const int32 Before = Shooter->Score - (ResolveDeltas.IsValidIndex(ActivePlayer) ? ResolveDeltas[ActivePlayer] : 0);
	for (int32 i = 0; i < Balls.Num(); ++i)
	{
		const AZoogiBall* B = Balls[i];
		if (!B || B == Shooter) continue;
		const int32 OtherBefore = B->Score - (ResolveDeltas.IsValidIndex(i) ? ResolveDeltas[i] : 0);
		if (OtherBefore >= Before) return true; // someone was level or ahead before this KO
	}
	return false;
}

void AZoogiGameMode::BeginClimax(AZoogiBall* Shooter, EZoogiArtifact A, int32 KOs, int32 ShooterScores)
{
	bClimaxUsed = true;
	ClimaxShooter = Shooter;
	ClimaxArtifact = A;
	ClimaxKOs = KOs;
	ClimaxScores = ShooterScores;
	ClimaxLeft = 2.55f; // ClimaxRumble length: 2.25 s rumble + 0.3 s silence
	NotifyMatchEvent(EZoogiMatchEvent::ClimaxBegin); // the one full music drop-out of the match
	ZoogiUtil::PlaySfx(this, TEXT("ClimaxRumble"), 1.f);
	UE_LOG(LogZoogi, Log, TEXT("CLIMAX: P%d %s match-deciding KO x%d: music out, rumble, hit in %.2f s"), Shooter ? Shooter->PlayerIndex + 1 : 0, Shooter ? *Shooter->Def.Name : TEXT("?"), KOs, ClimaxLeft);
	TakeShotDelayed(TEXT("climax_rumble"), 1.2f);
}

void AZoogiGameMode::LandClimax()
{
	AZoogiBall* Shooter = ClimaxShooter.Get();
	ClimaxFlashAt = Clock;  // impact flash on this frame...
	bClimaxHitPending = true; // ...and PlayEventSfx plays the big hit on this same frame
	if (Shooter)
	{
		if (ClimaxScores >= 2) EmitAnime(TEXT("Combo"), Shooter, ClimaxArtifact, Shooter, Shooter->BallLocation(), ClimaxScores, false);
		else EmitAnime(TEXT("KO"), Shooter, ClimaxArtifact, Shooter, Shooter->BallLocation(), ClimaxKOs, false);
	}
	if (bClimaxHitPending) { bClimaxHitPending = false; ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 1.f, 0.9f); }
	// Music swells back in big.
	NotifyMatchEvent(EZoogiMatchEvent::ClimaxLand);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnImpact(0.6f);
	UE_LOG(LogZoogi, Log, TEXT("CLIMAX hit: flash + boom, music swells back"));
	TakeShotDelayed(TEXT("climax_hit"), 0.03f);
}


FVector AZoogiGameMode::FindFreeSpot(const FVector& Desired) const
{
	auto IsFree = [this](const FVector& P)
	{
		for (const AZoogiBall* B : Balls)
		{
			if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), P) < ZoogiConst::ZoogiRadius * 2.f + 20.f) return false;
		}
		for (const AZoogiOrb* O : Orbs)
		{
			if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 20.f) return false;
		}
		return true;
	};
	if (IsFree(Desired)) return Desired;
	const float R = FVector2D(Desired.X, Desired.Y).Size();
	const float A0 = FMath::Atan2(Desired.Y, Desired.X);
	for (int32 Ring = 0; Ring < 6; ++Ring)
	{
		const float RR = FMath::Max(150.f, R - Ring * 130.f);
		for (int32 k = 1; k < 12; ++k)
		{
			const float A = A0 + ((k % 2) ? 1.f : -1.f) * ((k + 1) / 2) * FMath::DegreesToRadians(9.f);
			const FVector P(FMath::Cos(A) * RR, FMath::Sin(A) * RR, Desired.Z);
			if (IsFree(P)) return P;
		}
	}
	return Desired + FVector(0, 0, 120.f);
}

void AZoogiGameMode::RespawnBall(AZoogiBall* Ball)
{
	const FVector P = FindFreeSpot(Ball->SpawnLocation);
	Ball->PlaceAt(P, Ball->SpawnYaw);
	AZoogiFX::KFX(GetWorld(), TEXT("Effect5"), P - FVector(0, 0, ZoogiConst::ZoogiRadius - 3.f), 0.45f, 2.5f); // KriptoFX portal on respawn
	Ball->LastHitPlayer = -1;
}

void AZoogiGameMode::AddFeed(const FString& Text, const FLinearColor& Color)
{
	FZoogiFeedItem F;
	F.Text = Text;
	F.Color = Color;
	Feed.Insert(F, 0);
	if (Feed.Num() > 5) Feed.SetNum(5);
	static int32 FeedShots = 0;
	if (FeedShots < 3) TakeShotDelayed(FString::Printf(TEXT("feed_%d"), ++FeedShots), 1.2f);
}

void AZoogiGameMode::AddPopup(const FString& Text, const FVector& World, const FLinearColor& Color)
{
	FZoogiPopup P;
	P.Text = Text;
	P.World = World;
	P.Color = Color;
	Popups.Add(P);
}

bool AZoogiGameMode::AllAtRest() const
{
	if (PullLeft > 0.f) return false;
	for (const AZoogiBall* B : Balls) if (B && !B->IsAtRest()) return false;
	for (const AZoogiOrb* O : Orbs) if (O && !O->IsAtRest()) return false;
	for (const AZoogiClone* C : Clones) if (IsValid(C)) return false;
	if (RicochetTimer >= 0.f) return false;
	return true;
}

static FVector EdgePoint(const FVector& L, const AZoogiArena* Arena)
{
	FVector2D P(L.X, L.Y);
	if (Arena && Arena->IsNeonRect())
	{
		// Rectangular court: clamp onto the rails' inner box.
		return FVector(FMath::Clamp(P.X, -1140.f, 1140.f), FMath::Clamp(P.Y, -740.f, 740.f), 120.f);
	}
	if (P.Size() < 1.f) P = FVector2D(1, 0);
	P = P.GetSafeNormal() * ((Arena ? Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) - 60.f);
	return FVector(P.X, P.Y, 120.f);
}

void AZoogiGameMode::CheckKnockoffs()
{
	if (bComboMode) { ComboCheckKnockoffs(); return; }
	for (AZoogiOrb* O : Orbs)
	{
		if (!O || O->bOut) continue;
		const FVector L = O->OrbLocation();
		if (AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			const int32 Scorer = Balls.IsValidIndex(O->LastHitBy) ? O->LastHitBy : ActivePlayer;
			O->SetOut();
			AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena), O->bStar ? FLinearColor(1.f, 0.75f, 0.2f) : FLinearColor(0.5f, 0.85f, 1.f), 160.f, 0.6f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), O->bStar ? TEXT("NS_Burst_Gold") : TEXT("NS_Burst_Blue"), EdgePoint(L, Arena));
			if (Balls.IsValidIndex(Scorer))
			{
				const int32 Pts = O->bStar ? 2 * ZoogiConst::ScoreOrb : ZoogiConst::ScoreOrb;
				// Recorded now, committed in Resolve (authoritative results).
				FZoogiLedgerEntry E;
				E.Player = Scorer; E.Points = Pts; E.bStar = O->bStar;
				Ledger.Add(E);
				UE_LOG(LogZoogi, Log, TEXT("%s off the edge -> P%d +%d (pending Resolve)"), O->bStar ? TEXT("Star orb") : TEXT("Orb"), Scorer + 1, Pts);
				ZoogiUtil::PlaySfx(this, TEXT("Score"), O->bStar ? 1.f : 0.85f, O->bStar ? 1.25f : 1.f);
				AddFeed(O->bStar ? FString::Printf(TEXT("P%d %s  knocked off a GOLD STAR  +%d  (%s)"), Scorer + 1, *Balls[Scorer]->Def.Name, Pts, bArtifactMode ? TEXT("+1 artifact draw") : TEXT("+1 power"))
					: FString::Printf(TEXT("P%d %s  knocked off an orb  +%d"), Scorer + 1, *Balls[Scorer]->Def.Name, Pts), O->bStar ? FLinearColor(1.f, 0.8f, 0.3f) : ZoogiUtil::PlayerColor(Scorer));
				{
					FZoogiFlyer Fl;
					Fl.World = EdgePoint(L, Arena);
					Fl.Player = Scorer;
					Fl.Points = Pts;
					Fl.Color = O->bStar ? FLinearColor(1.f, 0.8f, 0.25f) : FLinearColor(0.45f, 0.8f, 1.f);
					Flyers.Add(Fl);
				}
				AddPopup(O->bStar ? FString::Printf(TEXT("+%d STAR  %s  P%d"), Pts, bArtifactMode ? TEXT("+1 DRAW") : TEXT("+1 POWER"), Scorer + 1) : FString::Printf(TEXT("+%d ORB  P%d"), Pts, Scorer + 1),
					EdgePoint(L, Arena), O->bStar ? FLinearColor(1.f, 0.8f, 0.3f) : ZoogiUtil::PlayerColor(Scorer));
			}
		}
	}
	for (AZoogiBall* B : Balls)
	{
		if (!B || B->bOut) continue;
		const FVector L = B->BallLocation();
		if (AZoogiArena::IsRingOut(L)) // Frozen Ring 2: also out past r2300
		{
			AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena), FLinearColor(1.f, 0.3f, 0.25f), 220.f, 0.7f, false, 7.f);
			AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), EdgePoint(L, Arena), FRotator::ZeroRotator, 1.4f);
			B->SetOut();
			ZoogiUtil::PlaySfx(this, TEXT("Fall"), 0.7f);
			{
				FZoogiLedgerEntry E;
				E.Player = B->PlayerIndex; E.Points = -ZoogiConst::PenaltyFall; E.bFall = true; E.Victim = B->PlayerIndex;
				Ledger.Add(E);
			}
			UE_LOG(LogZoogi, Log, TEXT("P%d fell off (last hit by %s)"), B->PlayerIndex + 1, B->LastHitPlayer >= 0 ? *FString::Printf(TEXT("P%d"), B->LastHitPlayer + 1) : TEXT("nobody"));
			AddPopup(FString::Printf(TEXT("-%d FELL OFF  P%d"), ZoogiConst::PenaltyFall, B->PlayerIndex + 1), EdgePoint(L, Arena), FLinearColor(1.f, 0.3f, 0.25f));
			if (!Balls.IsValidIndex(B->LastHitPlayer) || B->LastHitPlayer == B->PlayerIndex)
				AddFeed(FString::Printf(TEXT("P%d %s  rolled off the edge  -%d"), B->PlayerIndex + 1, *B->Def.Name, ZoogiConst::PenaltyFall), FLinearColor(1.f, 0.4f, 0.35f));
			if (Balls.IsValidIndex(B->LastHitPlayer) && B->LastHitPlayer != B->PlayerIndex)
			{
				AZoogiBall* A = Balls[B->LastHitPlayer];
				{
					FZoogiLedgerEntry E;
					E.Player = A->PlayerIndex; E.Points = ZoogiConst::ScorePlayer; E.bKO = true; E.Victim = B->PlayerIndex;
					Ledger.Add(E);
				}
				AddFeed(FString::Printf(TEXT("P%d %s  KNOCKED OUT  P%d %s   +%d / -%d"), A->PlayerIndex + 1, *A->Def.Name, B->PlayerIndex + 1, *B->Def.Name, ZoogiConst::ScorePlayer, ZoogiConst::PenaltyFall), ZoogiUtil::PlayerColor(A->PlayerIndex));
				AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), ZoogiConst::ScorePlayer, A->PlayerIndex + 1), EdgePoint(L, Arena) + FVector(0, 0, 90.f), ZoogiUtil::PlayerColor(A->PlayerIndex));
				BigText = FString::Printf(TEXT("KNOCKOUT!\n%s  >  %s"), *A->Def.Name.ToUpper(), *B->Def.Name.ToUpper());
				BigColor = ZoogiUtil::PlayerColor(A->PlayerIndex);
				BigAge = 0.f;
				AZoogiFX::Spawn(GetWorld(), EdgePoint(L, Arena) - FVector(0, 0, 20.f), BigColor, 420.f, 0.8f, true, 12.f);
				if (AZoogiPlayerController* PCtl = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PCtl->OnImpact(1.f);
				TakeShotDelayed(TEXT("knockout"), 0.25f);
			}
		}
	}
}

// ------------------------------------------------------------------ aiming

FVector2D AZoogiGameMode::PullToVelocity(const FVector& Pull, float& OutFraction)
{
	const FVector2D P(Pull.X, Pull.Y);
	const float Raw = P.Size() * ZoogiConst::DragScale;
	OutFraction = FMath::Min(Raw, ZoogiConst::FullPull) / ZoogiConst::FullPull;
	if (Raw < 100.f) // launchPlanarVelocity ignores drags of 1 unit or less
	{
		OutFraction = 0.f;
		return FVector2D::ZeroVector;
	}
	const float Speed = ZoogiConst::LaunchSpeedForPull(OutFraction);
	return P.GetSafeNormal() * Speed;
}

float AZoogiGameMode::AimFraction() const
{
	float F = 0.f;
	PullToVelocity(AimPull, F);
	return bAimActive ? F : 0.f;
}

void AZoogiGameMode::SetAim(bool bActive, const FVector& Pull)
{
	bAimActive = bActive;
	AimPull = FVector(Pull.X, Pull.Y, 0.f);
}

void AZoogiGameMode::RotateAim(float DeltaYaw)
{
	if (AZoogiBall* B = GetActiveBall()) B->AimYaw = FRotator::NormalizeAxis(B->AimYaw + DeltaYaw);
}

bool AZoogiGameMode::ReleaseAim()
{
	if (Phase != EZoogiPhase::Aiming)
	{
		bAimActive = false;
		return false;
	}
	float Frac = 0.f;
	FVector2D V = PullToVelocity(GetEffectivePull(), Frac);
	bAimActive = false;
	if (V.IsNearlyZero()) return false;
	AZoogiBall* Ball = GetActiveBall();
	if (Ball && Ball->BoundShots > 0)
	{
		// Bandage Bind: this shot comes out weaker.
		Ball->BoundShots--;
		V *= 0.65f;
		AddPopup(TEXT("BOUND - WEAK SHOT"), Ball->BallLocation() + FVector(0, 0, 120.f), FLinearColor(0.9f, 0.78f, 0.55f));
	}
	if (Ball) ApplyStatusOnShot(Ball, V);
	if (bComboMode && Ball) ComboModifyShot(Ball, V);
	UZoogiCardSubsystem* CardFx = CardsOn() ? UZoogiCardSubsystem::Get(this) : nullptr; // v3 Phase 3: card shot modifiers (cards on only)
	if (CardFx && Ball) CardFx->FxModifyShot(this, Ball, V);
	if (CardFx && Ball && Ball->bCPU) CPUSafetyClamp(Ball, V); // Oct 14 playtest fix: no CPU self ring-out (cards mode)
	FireShot(V);
	if (CardFx && Ball) CardFx->FxAfterShot(this, Ball, V);
	SetBattleStep(EZoogiBattleStep::Rolling);
	return true;
}

// Final CPU guard, after every card / power modifier: if the shot's free roll (no Zoogi / piece contact on its line)
// would carry the Zoogi past the ring edge, cap the speed so it stops 180 cm inside. Contacts = live Zoogis
// and breakable pieces on the line before the edge (a contact shot is the CPU's intent; the physics decides the rest).
void AZoogiGameMode::CPUSafetyClamp(AZoogiBall* Ball, FVector2D& V)
{
	if (!Ball || !Arena || V.SizeSquared() < 1.f) return;
	const FVector2D P(Ball->BallLocation());
	const FVector2D D = V.GetSafeNormal();
	const float Speed = V.Size(), Decel = Arena->GetRollingDecel();
	const float Roll = Speed * Speed / (2.f * Decel);
	const float Edge = Arena->DistanceToEdge(P, D);
	const float Margin = 180.f;
	if (Roll <= Edge - Margin) return;
	const float Reach = FMath::Min(Roll, Edge);
	auto Hits = [&](const FVector2D& C, float R) { const FVector2D Rel = C - P; const float T = FVector2D::DotProduct(Rel, D); return T > 0.f && T < Reach && FMath::Abs(FVector2D::CrossProduct(D, Rel)) < R; };
	FString Hit;
	for (AZoogiBall* O : Balls) if (O && O != Ball && !O->bOut && !O->bKnockedOut && Hits(FVector2D(O->BallLocation()), ZoogiConst::ZoogiRadius * 2.f)) { Hit = O->Def.Name; break; }
	// orbs are light: the Zoogi rolls on through them (Volcanic smoke: 3 self-falls after "orb contact"), so they do not count as a stop
	if (Hit.IsEmpty()) for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It)
	{
		if (!UZoogiCardSubsystem::IsCollapseEligible(*It)) continue;
		FVector Org, Ext; It->GetActorBounds(true, Org, Ext);
		const float R = FMath::Min(FMath::Max(Ext.X, Ext.Y), 400.f) + ZoogiConst::ZoogiRadius;
		if (Hits(FVector2D(Org), R)) { Hit = It->GetName(); break; }
	}
	if (!Hit.IsEmpty()) { UE_LOG(LogZoogi, Log, TEXT("CPU P%d safety: roll %.0f > edge %.0f but contact predicted (%s), shot kept"), Ball->PlayerIndex + 1, Roll, Edge, *Hit); return; }
	const float Safe = FMath::Sqrt(2.f * Decel * FMath::Max(0.f, Edge - Margin));
	UE_LOG(LogZoogi, Log, TEXT("CPU P%d safety clamp: %.0f -> %.0f cm/s (free roll %.0f cm, edge %.0f cm, no contact on the line)"), Ball->PlayerIndex + 1, Speed, Safe, Roll, Edge);
	V = D * Safe;
}

void AZoogiGameMode::FireShot(const FVector2D& Velocity)
{
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	Ball->Launch(Velocity);
	// Oct 4 SFX: anime dash whoosh (pitch / level follow the launch speed), the old launch thump underneath.
	ZoogiUtil::PlaySfx(this, TEXT("Dash"), FMath::Clamp(Velocity.Size() / 2200.f, 0.35f, 1.f), FMath::Lerp(0.9f, 1.15f, FMath::Clamp(Velocity.Size() / 3600.f, 0.f, 1.f)));
	ZoogiUtil::PlaySfx(this, TEXT("Launch"), FMath::Clamp(Velocity.Size() / 2500.f, 0.3f, 1.f) * 0.45f, FMath::Lerp(0.85f, 1.2f, FMath::Clamp(Velocity.Size() / 3600.f, 0.f, 1.f)));
	OnArmedShot(Ball);
	Phase = EZoogiPhase::Rolling;
	bShotImpacted = false;
	PhaseTime = 0.f;
	SettleTimer = 0.f;
	ShotsFired++;
	ShotClock = 0.f;
	ShotStart = Ball->BallLocation();
	ShotSpeed = Velocity.Size();
	UE_LOG(LogZoogi, Log, TEXT("P%d %s shot at %.0f cm/s from (%.0f, %.0f) dir %.0f deg, expected roll %.0f cm"), ActivePlayer + 1, *Ball->Def.Name, Velocity.Size(),
		ShotStart.X, ShotStart.Y, FMath::RadiansToDegrees(FMath::Atan2(Velocity.Y, Velocity.X)), Velocity.SizeSquared() / (2.f * Arena->GetRollingDecel()));
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnShotFired(Ball);
	if (UZoogiEpisodeSubsystem* Ep = GetWorld()->GetSubsystem<UZoogiEpisodeSubsystem>()) Ep->OnShotFired(Ball); // Episode Engine: truth capture (read-only)
}

static float InvertLaunch(float Speed)
{
	float Lo = 0.f, Hi = 1.f;
	for (int32 i = 0; i < 30; ++i)
	{
		const float Mid = 0.5f * (Lo + Hi);
		if (ZoogiConst::LaunchSpeedForPull(Mid) < Speed) Lo = Mid; else Hi = Mid;
	}
	return 0.5f * (Lo + Hi);
}

void AZoogiGameMode::ReplanCPUShot()
{
	AZoogiBall* Me = GetActiveBall();
	if (!Me || !Me->bCPU || Phase != EZoogiPhase::Aiming || Stage == EZoogiStage::Battle || bComboMode) return;
	ClearLock();
	PlanCPUShot();
}

void AZoogiGameMode::PlanCPUShot()
{
	AZoogiBall* Me = GetActiveBall();
	if (!Me) return;
	const FVector2D B(Me->BallLocation());
	const float Decel = Arena->GetRollingDecel();

	struct FCand { FVector2D P; float Value; float TargetMass; float Radius; AActor* Actor; };
	TArray<FCand> Cands;
	for (AZoogiOrb* O : Orbs) if (O && !O->bOut) Cands.Add({ FVector2D(O->OrbLocation()), 50.f, ZoogiConst::OrbMassRatio, ZoogiConst::OrbRadius, O });
	// Decide on the power first: rival-hunting powers make rivals more attractive targets.
	// More charges banked = more willing to spend one.
	bCPUUsePower = !bArtifactMode && Me->Charges > 0 && (bPowerTest || GameRng.FRand() < (Me->Charges >= 2 ? 0.8f : 0.5f));
	const EZoogiPower MyPower = Me->Def.PowerKind;
	const bool bHuntRivals = bCPUUsePower && (MyPower == EZoogiPower::ShadowStun || MyPower == EZoogiPower::StaticShock || MyPower == EZoogiPower::BandageBind || MyPower == EZoogiPower::RuneSeal);
	for (AZoogiBall* Other : Balls) if (Other && Other != Me && !Other->bOut && !IsAlly(Me, Other)) Cands.Add({ FVector2D(Other->BallLocation()), bHuntRivals ? 100.f : 40.f, Other->Def.MassRatio(), ZoogiConst::ZoogiRadius, Other });
	// Oct 15 fix ("travelled -1 cm, OUT" on Frozen Ring): targets beyond the ring-out edge (the Frozen islet orbs on the
	// bleacher aprons, a rival standing on an islet) gave a negative DistanceToEdge -> the cheapest plan and a NaN power
	// -> a full-power shot straight over the bridge, and the shooter flew off the islet (contacts=0). The edge model
	// can't plan those, so the CPU skips them (humans can still play them). Only changes plans on arenas that have
	// off-court targets (Frozen islets); the GameRng draw sequence is unchanged.
	{
		int32 NSkip = 0;
		for (int32 i = Cands.Num() - 1; i >= 0; --i) if (Arena && Arena->IsOffCourt(Cands[i].P)) { Cands.RemoveAt(i); ++NSkip; }
		if (NSkip) UE_LOG(LogZoogi, Log, TEXT("CPU P%d skips %d off-court target(s) (beyond the ring-out edge %.0f cm: islet orbs / rivals)"), ActivePlayer + 1, NSkip, Arena->GetKnockoffRadius());
	}

	auto Blocked = [&](const FVector2D& From, const FVector2D& To, const FVector2D& Ignore)
	{
		const FVector2D D = To - From;
		const float Len = D.Size();
		if (Len < 1.f) return false;
		const FVector2D N = D / Len;
		auto Test = [&](const FVector2D& Q, float Rad)
		{
			if (Q.Equals(Ignore, 1.f)) return false;
			const float T = FVector2D::DotProduct(Q - From, N);
			if (T < 0.f || T > Len - 10.f) return false;
			return FVector2D::Distance(From + N * T, Q) < ZoogiConst::ZoogiRadius + Rad - 5.f;
		};
		for (AZoogiOrb* O : Orbs) if (O && !O->bOut && Test(FVector2D(O->OrbLocation()), ZoogiConst::OrbRadius)) return true;
		for (AZoogiBall* Ob : Balls) if (Ob && Ob != Me && !Ob->bOut && Test(FVector2D(Ob->BallLocation()), ZoogiConst::ZoogiRadius)) return true;
		return false;
	};

	// Oct 15 (Evan: more action, no stalls) - the edge model now sees the real arena collision:
	// Walled(): a sphere sweep against the arena's static geometry (rim walls, arches, lips, stands) along a line;
	// a wall before the ring-out edge means a straight push won't carry the target out there (Frozen Ring's rim) and
	// that the shooter's own follow-through on that line is stopped (no self ring-out). Arenas without walls on the
	// line are unaffected. No GameRng draws here (decision-making only).
	UWorld* PW = GetWorld();
	int32 NSweeps = 0, NWalled = 0;
	auto Walled = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Len, float Rad) -> bool
	{
		if (!PW || Len <= 1.f) return false;
		++NSweeps;
		FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUExit), false); QP.AddIgnoredActor(Me);
		// a real wall blocks the line at rolling height AND 50 cm higher (low curbs / lips / ramps a fast roller rides over
		// don't count - Arabian Nights: shooters "stopped" by curbs in the model rolled off in the game)
		auto Hit = [&](float Up)
		{
			TArray<FHitResult> Hits;
			const FVector S(From.X, From.Y, Z + Up), E = S + FVector(Dir.X, Dir.Y, 0.f) * Len;
			PW->SweepMultiByObjectType(Hits, S, E, FQuat::Identity, OQ, FCollisionShape::MakeSphere(Rad), QP);
			for (const FHitResult& H : Hits)
			{
				if (H.bStartPenetrating) continue;
				const AActor* A = H.GetActor();
				if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) continue;
				const UPrimitiveComponent* Cm = H.GetComponent();
				if (!Cm || Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Block) continue;
				if (FMath::Abs(H.ImpactNormal.Z) > 0.6f) continue; // floor / ramps / bridge decks
				return true;
			}
			return false;
		};
		if (Hit(6.f) && Hit(50.f)) { ++NWalled; return true; }
		return false;
	};
	// ExitRun(): follows the target's departure line over the real floor (down traces every 50 cm, the floor height
	// followed up / down bridge decks and aprons) until the floor ends (= the drop where it rings out) or a wall stops it.
	// Returns the run to the drop (>= the geometric edge distance), or -1 when a wall comes first (no exit on this line).
	int32 NRuns = 0, NRunWalled = 0, NLaunch = 0;
	auto ExitRun = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Edge, float Rad, bool& bLaunch) -> float
	{
		bLaunch = false;
		if (!PW) return Edge;
		++NRuns;
		// cheap first: a wall at court level before the edge (one sweep) = no exit on this line
		if (Walled(From, Z, Dir, FMath::Max(Edge, 0.f) + 150.f, Rad)) { ++NRunWalled; return -1.f; }
		FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUExitRun), false); QP.AddIgnoredActor(Me);
		auto Solid = [](const FHitResult& H)
		{
			const AActor* A = H.GetActor();
			if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) return false;
			const UPrimitiveComponent* Cm = H.GetComponent();
			return Cm && Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block;
		};
		const float FloorOff = Z - Rad / 0.8f; // centre height above the floor
		const float S0 = FMath::Max(50.f, Edge - 100.f);
		FVector Prev(From.X + Dir.X * (S0 - 100.f), From.Y + Dir.Y * (S0 - 100.f), Z);
		const float MaxRun = FMath::Max(Edge, 0.f) + 2500.f;
		for (float S = S0; S <= MaxRun; S += 100.f)
		{
			const FVector2D Q2 = From + Dir * S;
			TArray<FHitResult> Dn;
			PW->LineTraceMultiByObjectType(Dn, FVector(Q2.X, Q2.Y, Prev.Z + 120.f), FVector(Q2.X, Q2.Y, Prev.Z - 260.f), OQ, QP);
			float FloorZ = TNumericLimits<float>::Lowest();
			for (const FHitResult& H : Dn) if (Solid(H) && H.ImpactNormal.Z > 0.5f) { FloorZ = H.ImpactPoint.Z; break; }
			if (FloorZ == TNumericLimits<float>::Lowest()) return S; // the floor ends: it drops out here
			const FVector Cur(Q2.X, Q2.Y, FloorZ + (FloorOff > 0.f ? FloorOff : Rad) + 0.f);
			TArray<FHitResult> Hs;
			PW->SweepMultiByObjectType(Hs, Prev + FVector(0, 0, 6.f), Cur + FVector(0, 0, 6.f), FQuat::Identity, OQ, FCollisionShape::MakeSphere(Rad), QP);
			for (const FHitResult& H : Hs) if (!H.bStartPenetrating && Solid(H) && FMath::Abs(H.ImpactNormal.Z) <= 0.6f)
			{
				// a wall far out on floor that runs on past the edge (Frozen: bridge deck -> islet stands): a roller that
				// gets there fast flies off the deck / over it (measured: ~3000 cm/s Zoogis left Frozen this way) -> a
				// launch exit that needs FarExitSpeed at the edge; a wall at the edge itself = no exit
				if (S > FMath::Max(Edge, 0.f) + 300.f) { bLaunch = true; ++NLaunch; return FMath::Max(Edge, 0.f); }
				++NRunWalled; return -1.f;
			}
			Prev = Cur;
		}
		return FMath::Max(Edge, 0.f); // no drop found within reach (catch floors etc.): keep the geometric edge
	};
	// ShooterRun(): how far the shooter can roll along a line before it rings out - the first hole / drop in the floor
	// (Meadow's crater, gaps), or the geometric edge; 1e6 when a real wall stops it first. Down traces every 75 cm.
	auto RealWallDist = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Len, float Rad) -> float
	{
		if (!PW || Len <= 1.f) return Len;
		FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUWallDist), false); QP.AddIgnoredActor(Me);
		float D[2] = { Len, Len };
		for (int32 h = 0; h < 2; ++h)
		{
			TArray<FHitResult> Hits;
			const FVector S(From.X, From.Y, Z + (h ? 50.f : 6.f)), E = S + FVector(Dir.X, Dir.Y, 0.f) * Len;
			PW->SweepMultiByObjectType(Hits, S, E, FQuat::Identity, OQ, FCollisionShape::MakeSphere(Rad), QP);
			for (const FHitResult& H : Hits)
			{
				if (H.bStartPenetrating) continue;
				const AActor* A = H.GetActor();
				if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) continue;
				const UPrimitiveComponent* Cm = H.GetComponent();
				if (!Cm || Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Block || FMath::Abs(H.ImpactNormal.Z) > 0.6f) continue;
				// Oct 15: only a face-on wall stops the shooter; a glancing one (round toppings, curb ends, angled walls) deflects
				// it along the wall - often toward the edge (Arabian) - so it doesn't count as a stop
				if (FVector2D::DotProduct(FVector2D(H.ImpactNormal.X, H.ImpactNormal.Y).GetSafeNormal(), Dir) > -0.7f) continue;
				D[h] = H.Distance; break;
			}
		}
		return FMath::Max(D[0], D[1]);
	};
	auto DropDist = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Len) -> float
	{
		if (!PW) return 1e6f;
		FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUDrop), false); QP.AddIgnoredActor(Me);
		for (float S = 75.f; S <= Len; S += 75.f)
		{
			const FVector2D Q = From + Dir * S;
			TArray<FHitResult> Dn;
			PW->LineTraceMultiByObjectType(Dn, FVector(Q.X, Q.Y, Z + 100.f), FVector(Q.X, Q.Y, Z - 220.f), OQ, QP);
			bool bFloor = false;
			for (const FHitResult& H : Dn)
			{
				const AActor* A = H.GetActor();
				if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) continue;
				const UPrimitiveComponent* Cm = H.GetComponent();
				if (Cm && Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block && H.ImpactNormal.Z > 0.5f) { bFloor = true; break; }
			}
			if (!bFloor) return S;
		}
		return 1e6f;
	};
	int32 NHoles = 0;
	auto ShooterRun = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Edge, float& WallAt) -> float
	{
		const float E = FMath::Max(Edge, 0.f);
		const float W = RealWallDist(From, Z, Dir, E + 300.f, ZoogiConst::ZoogiRadius * 0.8f);
		const float Lim = FMath::Min(W, E);
		const float Dr = DropDist(From, Z, Dir, Lim);
		WallAt = W;
		if (Dr < Lim) { ++NHoles; return Dr; } // a hole / drop in the floor before any wall or the edge
		return W < E + 300.f ? 1e6f : E;      // a real wall stops it, else the edge
	};
	// Oct 15: boost portals (Arabian / Frozen arches) add BoostAdd to whatever rolls through them: a follow-through that would
	// stop safely can cross one and come out fast enough to leave the court. PortalRun(): the safe run along a line once the
	// first portal crossing is taken into account (exact for one portal: it may be reached only slow enough that the
	// boosted speed still stops within the run, else the shooter has to stop short of it).
	const UZoogiPortalSubsystem* PortalSys = (PW && UZoogiPortalSubsystem::Enabled()) ? PW->GetSubsystem<UZoogiPortalSubsystem>() : nullptr;
	int32 NPortalClamp = 0;
	auto PortalRun = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Run) -> float
	{
		if (!PortalSys || Run <= 0.f || Run >= 1e5f) return Run;
		float PBest = Run;
		for (const UZoogiPortalSubsystem::FPortal& Pt : PortalSys->Portals)
		{
			if (!Pt.Arch.IsValid()) continue;
			const FVector L0 = Pt.X.InverseTransformPositionNoScale(FVector(From.X, From.Y, Z));
			const FVector LD = Pt.X.InverseTransformVectorNoScale(FVector(Dir.X, Dir.Y, 0.f));
			if (FMath::Abs(LD.X) < 1e-3f) continue;
			const float S = -L0.X / LD.X;
			if (S <= 0.f || S >= PBest) continue;
			if (FMath::Abs(L0.Y + S * LD.Y) > Pt.HalfW + ZoogiConst::ZoogiRadius) continue;
			PBest = S;
		}
		if (PBest >= Run) return Run;
		++NPortalClamp;
		const float VOutMax = FMath::Sqrt(2.f * Decel * FMath::Max(0.f, Run - PBest)) - PortalSys->BoostAdd;
		return VOutMax <= 0.f ? FMath::Max(0.f, PBest - 60.f) : PBest + VOutMax * VOutMax / (2.f * Decel);
	};
	// Oct 15: speed-adding obstacles (bumpers with BounceGain > 1, boost / launch / heading-snap / min-speed / multiplier
	// pads, low-friction zones - Arabian's toppings) send a shooter far past any decel estimate (5-6 m rolls on 14 m/s
	// shots in the cards-off runs). A follow-through / miss line that reaches one is only safe up to it.
	struct FHaz { FVector2D C; float R; };
	TArray<FHaz> Hazards;
	for (const TObjectPtr<AZoogiObstacle>& Ob : Arena->GetObstacles())
	{
		if (!Ob || Ob->IsHidden()) continue;
		bool bHaz = Ob->BounceGain > 1.001f;
		for (const FZoogiObsTrigger& T : Ob->Triggers)
			if (T.Kind == 1 || T.Kind == 2 || T.Kind == 5 || T.Kind == 6 || T.Kind == 7 || (T.Kind == 4 && ((T.P[0] > 0.f && T.P[0] < Decel * 0.9f) || (T.P[0] <= 0.f && T.P[1] > 0.f && T.P[1] < 0.9f)))) bHaz = true;
		if (!bHaz) continue;
		FVector O, E; Ob->GetActorBounds(false, O, E);
		Hazards.Add({ FVector2D(O), (float)FMath::Max(E.X, E.Y) });
	}
	int32 NHazClamp = 0;
	auto HazardRun = [&](const FVector2D& From, const FVector2D& Dir, float Run) -> float
	{
		if (Hazards.Num() == 0 || Run <= 0.f) return Run;
		float Best = Run;
		for (const FHaz& H : Hazards)
		{
			const FVector2D Rel = H.C - From;
			const float Along = FVector2D::DotProduct(Rel, Dir);
			const float R = H.R + ZoogiConst::ZoogiRadius;
			const float Perp2 = Rel.SizeSquared() - Along * Along;
			if (Perp2 > R * R || Along + R < 0.f) continue;
			const float S = Along - FMath::Sqrt(FMath::Max(0.f, R * R - Perp2));
			if (S < Best) Best = FMath::Max(S, 0.f);
		}
		if (Best >= Run) return Run;
		++NHazClamp;
		return FMath::Max(0.f, Best - 60.f);
	};
	const float MeZ = Me->BallLocation().Z;
	static const float WallFrac = []
	{
		float F = 0.5f; // Oct 15: 0.7 -> 0.5 (walled lines now plan the orb's real hop speed; this is only a floor)
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("WalledLinePower"), F, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUWallPower="), F);
		return FMath::Clamp(F, 0.3f, 1.f);
	}();
	static const float FarExitV = []
	{
		float V = 2400.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("LaunchExitSpeed"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPULaunchExit="), V);
		return FMath::Clamp(V, 300.f, 3600.f);
	}();
	// Oct 15: a shooter that reaches a low rim wall / seat wall / curb too fast hops it (Meadow "AIRBORNE" self ring-outs):
	// walls only stop a shooter arriving slower than this ([ZoogiRoll.CPU] WallHopSpeed, -ZoogiCPUWallHop=)
	static const float WallHopV = []
	{
		float V = 500.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("WallHopSpeed"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUWallHop="), V);
		return FMath::Clamp(V, 0.f, 3000.f);
	}();
	int32 NHopClamp = 0, NRimBank = 0;
	// Oct 15: an orb reaching a face-on wall at this speed hops / breaks it ([ZoogiRoll.CPU] OrbWallHopSpeed, -ZoogiCPUOrbHop=);
	// bank restitution: measured from the wall's physical material (combined with the orb's ~0.7), BankRestitution overrides
	static const float OrbHopV = []
	{
		float V = 1200.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("OrbWallHopSpeed"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUOrbHop="), V);
		return FMath::Clamp(V, 0.f, 3600.f);
	}();
	constexpr float BankRest = 0.6f;
	static const float BankRestOverride = []
	{
		float R = -1.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("BankRestitution"), R, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUBankRest="), R);
		return R;
	}();
	static const float ThinKeep = []
	{
		float K = 0.4f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("FollowThroughKeep"), K, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUFollowKeep="), K);
		return FMath::Clamp(K, 0.f, 1.f);
	}();
	constexpr float ExitV = 300.f;    // the target should still be moving this fast at the edge (clears lips / gutters)
	constexpr float FollowE = 0.55f;  // follow-through restitution (conservative: the shooter keeps more speed than e=0.7)
	const float ErrMaxDeg = FMath::Max(0.f, (100.f - Me->Def.Control) / 12.f);
	// Oct 15 LineExit(): what it really takes to score along a departure line (Frozen evidence: orbs hit at 15-22 m/s
	// ricochet off the 115 cm ice walls - restitution 0.9 - and never leave; they score where the floor ends first):
	//  Kind 0 - the floor ends (drop) before any face-on wall: just get it there (ExitV), whatever stands beyond the drop;
	//  Kind 1 - no wall, floor continues (bridge decks / aprons): ExitRun's push / launch model;
	//  Kind 2 - a low wall (clear at 130 cm): hop it at OrbWallHopSpeed;
	//  Kind 3 - a tall wall (blocks at 130 cm too): no score along this line, only ping-pong.
	struct FLineExit { float Need; int32 Kind; float Dist; };
	// still this fast at the drop: a dribble stalls on lips / snow drifts (Frozen: 30% drop-line shots scored 0) - [ZoogiRoll.CPU] DropExitSpeed
	static const float DropExitV = []
	{
		float V = 1000.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("DropExitSpeed"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUDropExit="), V);
		return FMath::Clamp(V, 0.f, 3600.f);
	}();
	auto WallAt = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Len, float Rad, float H) -> float
	{
		if (!PW || Len <= 1.f) return Len;
		FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUWallAt), false); QP.AddIgnoredActor(Me);
		TArray<FHitResult> Hits;
		const FVector S(From.X, From.Y, Z + H), E = S + FVector(Dir.X, Dir.Y, 0.f) * Len;
		PW->SweepMultiByObjectType(Hits, S, E, FQuat::Identity, OQ, FCollisionShape::MakeSphere(Rad), QP);
		for (const FHitResult& Ht : Hits)
		{
			if (Ht.bStartPenetrating) continue;
			const AActor* A = Ht.GetActor();
			if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) continue;
			const UPrimitiveComponent* Cm = Ht.GetComponent();
			if (!Cm || Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Block || FMath::Abs(Ht.ImpactNormal.Z) > 0.6f) continue;
			return Ht.Distance;
		}
		return Len;
	};
	int32 NLineDrop = 0, NLineTall = 0;
	// Oct 15: a tall wall (still blocking 130 cm up - Frozen's 115 cm ice ricochet walls) takes a much faster shooter to hop
	// than a low seat wall / curb: [ZoogiRoll.CPU] TallWallHopSpeed (1000 cm/s), -ZoogiCPUTallHop=
	static const float TallHopV = []
	{
		float V = 1000.f;
		if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.CPU"), TEXT("TallWallHopSpeed"), V, GGameIni);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiCPUTallHop="), V);
		return FMath::Clamp(V, 0.f, 3600.f);
	}();
	auto HopRun = [&](const FVector2D& From, const FVector2D& Dir, float WallD) -> float
	{
		const bool bTall = WallAt(From, MeZ, Dir, WallD + 60.f, ZoogiConst::ZoogiRadius * 0.8f, 130.f) < WallD + 60.f;
		const float V = bTall ? FMath::Max(TallHopV, WallHopV) : WallHopV;
		return WallD + V * V / (2.f * Decel);
	};
	auto LineExit = [&](const FVector2D& P, float Z, const FVector2D& N, float Rad, float Edge) -> FLineExit
	{
		const float Reach = FMath::Max(Edge, 0.f) + 600.f;
		const float Wd = RealWallDist(P, Z, N, Reach, Rad * 0.8f);
		const float Dr = DropDist(P, Z, N, FMath::Min(Wd, Reach));
		if (Dr < FMath::Min(Wd, Reach)) { ++NLineDrop; return { FMath::Sqrt(DropExitV * DropExitV + 2.f * Decel * (Dr + 150.f)), 0, Dr }; }
		if (Wd >= Reach)
		{
			bool bL = false;
			const float Rn = ExitRun(P, Z, N, Edge, Rad * 0.8f, bL);
			const float Run = FMath::Max(Edge, Rn);
			return { FMath::Sqrt(FMath::Square(bL ? FarExitV : ExitV) + 2.f * Decel * (Run + 250.f)), 1, Run };
		}
		if (WallAt(P, Z, N, Wd + 60.f, Rad * 0.8f, 130.f) < Wd + 60.f) { ++NLineTall; return { FMath::Sqrt(OrbHopV * OrbHopV + 2.f * Decel * Wd), 3, Wd }; }
		return { FMath::Sqrt(OrbHopV * OrbHopV + 2.f * Decel * Wd), 2, Wd };
	};

	CPUPlannedCut = 0.f;
	const EZoogiCPUTier CPUTier = ZoogiCPU::ActiveTier();
	struct FPlan { float Cost; FVector2D Dir; float Frac; AActor* Actor; bool bStraight; float Dist; float Radius; float Risk = 0.f; float Exposure = 0.f; bool bBank = false; float MaxSafeFrac = 1.f; float DbgCoast = -1.f; float DbgSafe = -1.f; float DbgMiss = -1.f; float DbgNeed = -1.f; bool bRimBank = false; int32 DbgKind = -1; float DbgExit = -1.f; }; // Risk/Exposure: tier lookahead inputs only (Oct 14); MaxSafeFrac: Oct 15 edge clamp
	TArray<FPlan> Plans;
	FVector2D BestDir(-B.GetSafeNormal());
	float BestFrac = 0.45f;
	float PickSafeFrac = 1.f;
	const float M1 = Me->Def.MassRatio();
	const float VMax = ZoogiConst::LaunchSpeedForPull(1.f);
	for (const FCand& C : Cands)
	{
		const FVector2D D = C.P - B;
		const float Dist = D.Size();
		if (Dist < 1.f) continue;
		const FVector2D Dir = D / Dist;
		const float Edge = Arena->DistanceToEdge(C.P, Dir);
		const float CZ = C.Actor ? C.Actor->GetActorLocation().Z : MeZ;
		bool bLaunchExit = false;
		const float Run = ExitRun(C.P, CZ, Dir, Edge, C.Radius * 0.8f, bLaunchExit);
		bool bExitWalled = Run < 0.f;
		// Oct 15: the floor may end before the wall (stands / scenery beyond a drop): that line scores like an open one
		const FLineExit LX = LineExit(C.P, CZ, Dir, C.Radius, Edge);
		if (LX.Kind == 0) { bExitWalled = false; bLaunchExit = false; }
		const float LineExitV = bLaunchExit ? FarExitV : ExitV;
		const float PushRun = LX.Kind == 0 ? FMath::Max(0.f, LX.Dist - 170.f) : (bExitWalled ? Edge : FMath::Max(Edge, Run)); // floor that continues past the edge (bridge decks / aprons) must be crossed too
		float Cost = Dist * 0.5f + (LX.Kind == 0 ? LX.Dist : Edge) - C.Value * 6.f;
		if (bExitWalled) Cost += LX.Kind == 3 ? 1100.f : 400.f; // a wall before the edge: tall = ping-pong only, low = hop it
		if (Blocked(B, C.P, C.P)) Cost += 1500.f;
		// Night Circuit pads / posts: in the way of the shot, or between the target and its edge.
		if (Arena->IsPathBlocked(B, C.P - Dir * (ZoogiConst::ZoogiRadius + C.Radius), ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
		else if (Arena->IsPathBlocked(C.P + Dir * (C.Radius + 10.f), C.P + Dir * FMath::Min(Edge, 2500.f), C.Radius - 5.f)) Cost += 700.f;
		// Power needed to push the target ~2.5 m past the edge (1D collision, restitution ~0.7), still moving ExitV there.
		float Need = FMath::Sqrt(LineExitV * LineExitV + 2.f * Decel * (PushRun + 250.f));
		if (bExitWalled && LX.Kind >= 2) Need = LX.Need; // Oct 15: a low wall - reach it at the hop speed (tall: a firm ping-pong hit)
		float Transfer = (1.f + 0.7f) * M1 / (M1 + C.TargetMass);
		if (bCPUUsePower && MyPower == EZoogiPower::BandageBind && Cast<AZoogiBall>(C.Actor)) Transfer *= 1.25f; // bind yank
		const float VIn = Need / FMath::Max(Transfer, 0.3f);
		const float Gap = FMath::Max(0.f, Dist - ZoogiConst::ZoogiRadius - C.Radius);
		const float V0 = FMath::Sqrt(VIn * VIn + 2.f * Decel * Gap);
		if (V0 > VMax) Cost += 300.f + (V0 - VMax) * 0.5f; // can't knock it off from here: less attractive
		float Frac = FMath::Clamp(InvertLaunch(FMath::Min(V0, VMax)), 0.3f, 0.97f);
		if (bExitWalled) Frac = FMath::Max(Frac, WallFrac); // walled line: a confident ping-pong hit ([ZoogiRoll.CPU] WalledLinePower), not a dribble into the wall
		// Self knock-off risk: the shooter keeps rolling after the hit (heavier shooter / lighter target = more).
		const float VLaunch = ZoogiConst::LaunchSpeedForPull(Frac);
		const float VHit = FMath::Sqrt(FMath::Max(0.f, VLaunch * VLaunch - 2.f * Decel * Gap));
		// Oct 15: an imperfect (thin / cut-variety / aim-error) contact leaves the shooter with more than the head-on share:
		// predict the stop with at least ThinKeep of the hit speed (Frozen 7.7 m follow-throughs after "safe" 97% hits)
		const float KAfter = FMath::Max(ThinKeep, (M1 - FollowE * C.TargetMass) / (M1 + C.TargetMass));
		const float VAfter = VHit * KAfter;
		const float Coast = VAfter * VAfter / (2.f * Decel);
		const FVector2D Contact = C.P - Dir * (ZoogiConst::ZoogiRadius + C.Radius);
		const float MyEdge = Arena->DistanceToEdge(Contact, Dir);
		float MyWall = 1e6f;
		const float MyRun = ShooterRun(Contact, MeZ, Dir, MyEdge, MyWall);
		const bool bMyWalled = MyRun >= 1e5f;
		// walled: the shooter may reach the wall at up to WallHopV, no faster (else it hops the wall)
		const float SafeRun = HazardRun(Contact, Dir, PortalRun(Contact, MeZ, Dir, bMyWalled ? HopRun(Contact, Dir, MyWall) - 60.f : MyRun - 120.f));
		float MaxSafeFrac = 1.f;
		if (KAfter > 0.01f)
		{
			// Oct 15 edge clamp: the hardest shot whose follow-through stops 1.2 m inside the edge
			const float VHitMax = FMath::Sqrt(2.f * Decel * FMath::Max(0.f, SafeRun)) / KAfter;
			MaxSafeFrac = InvertLaunch(FMath::Min(FMath::Sqrt(VHitMax * VHitMax + 2.f * Decel * Gap), VMax * 1.01f));
		}
		if (Coast > SafeRun)
		{
			// still a knock-out if the target only needs to just clear the edge (no ExitV, 60 cm past)? then play that, softer
			const float VMinOut = FMath::Sqrt(FMath::Square(FMath::Sqrt((bLaunchExit ? FarExitV * FarExitV * 0.8f : 0.f) + 2.f * Decel * (FMath::Max(0.f, PushRun) + 60.f)) / FMath::Max(Transfer, 0.3f)) + 2.f * Decel * Gap);
			if (!bExitWalled && MaxSafeFrac >= 0.3f && ZoogiConst::LaunchSpeedForPull(MaxSafeFrac) >= VMinOut) Frac = MaxSafeFrac;
			else if (bExitWalled && bMyWalled && MaxSafeFrac >= 0.5f) { Frac = MaxSafeFrac; ++NHopClamp; } // walled ping-pong: still a firm hit, just no wall hop
			else Cost += 900.f; // would follow the target off the edge (-75 and a wasted turn)
		}
		// a thin hit / miss (aim error) rolls the whole line: off the edge from here?
		float DbgMissRun = -1.f;
		// Oct 15: also when cut variety may thin the contact (artifact mode, 6-20 deg cuts): clamp only, no extra cost
		const bool bAimMiss = Dist * FMath::Sin(FMath::DegreesToRadians(ErrMaxDeg)) > 0.5f * (ZoogiConst::ZoogiRadius + C.Radius);
		if (bAimMiss || (bArtifactMode && Dist > 250.f))
		{
			const float FullRoll = FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac)) / (2.f * Decel);
			float BWall = 1e6f;
			const float BRun0 = ShooterRun(B, MeZ, Dir, Arena->DistanceToEdge(B, Dir), BWall);
			const float BRun = BRun0 < 1e5f ? BRun0 : HopRun(B, Dir, BWall) + 70.f; // walled: hop distance
			const float EdgeB = HazardRun(B, Dir, PortalRun(B, MeZ, Dir, BRun));
			DbgMissRun = EdgeB;
			if (FullRoll > EdgeB - 80.f)
			{
				// Oct 15: a miss would roll the shooter off. Keep the full roll on court when that still knocks the target
				// out; otherwise make this a less attractive plan (other targets usually exist - not timid, just safer)
				const float MissSafe = InvertLaunch(FMath::Sqrt(2.f * Decel * FMath::Max(0.f, EdgeB - 150.f)));
				const float VMinOut2 = FMath::Sqrt(FMath::Square(FMath::Sqrt(2.f * Decel * (FMath::Max(0.f, PushRun) + 60.f)) / FMath::Max(Transfer, 0.3f)) + 2.f * Decel * Gap);
				if (!bExitWalled && MissSafe >= 0.3f && ZoogiConst::LaunchSpeedForPull(MissSafe) >= VMinOut2) { Frac = FMath::Min(Frac, MissSafe); MaxSafeFrac = FMath::Min(MaxSafeFrac, MissSafe); }
				else if (bExitWalled && MissSafe >= 0.5f) { Frac = FMath::Min(Frac, MissSafe); MaxSafeFrac = FMath::Min(MaxSafeFrac, MissSafe); ++NHopClamp; } // walled: firm, but no full-roll hop
				else if (bAimMiss) Cost += 650.f;
			}
		}
		const float CoastF = FMath::Square(VHit * KAfter) / (2.f * Decel);
		FPlan Sp{ Cost, Dir, Frac, C.Actor, true, Dist, C.Radius, bMyWalled ? 0.f : FMath::Clamp(FMath::Min(Coast, CoastF) / FMath::Max(MyEdge, 1.f), 0.f, 1.f), bMyWalled ? 0.f : 1.f - FMath::Clamp(MyEdge / 1500.f, 0.f, 1.f) };
		Sp.MaxSafeFrac = MaxSafeFrac; Sp.DbgKind = LX.Kind; Sp.DbgExit = LX.Dist; Sp.DbgNeed = Need; Sp.DbgCoast = FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac) * KAfter) / (2.f * Decel); Sp.DbgSafe = SafeRun; Sp.DbgMiss = DbgMissRun;
		Plans.Add(Sp);
	}
	// Angled (cut) shots: aim the ghost-ball contact so the target departs along a chosen line.
	// Oct 15 rim / bridge-aware planner: every departure line is scored by what it really takes to score along it -
	//  - open line / bridge gap: push it past the edge (bridge decks: the whole deck, LaunchExitSpeed at the end);
	//  - face-on wall: the orb has to reach it at [ZoogiRoll.CPU] OrbWallHopSpeed to hop / break it (Frozen rim ice, arches);
	//  - one bank off a wall (simple reflection, restitution measured from the wall's physical material) into such a line.
	// Each candidate line is then checked with the full shooter-safety model, and the line that scores while the shooter's
	// follow-through stays on court wins. Tiers stay decision-only: Easy plans no banks, Normal banks at +350, Hard/Expert +150.
	if (bArtifactMode)
	{
		int32 LastKind = -1; float LastDist = -1.f;
		auto ExitNeed = [&](const FVector2D& P, float Z, const FVector2D& N, float Rad, float& Extra) -> float
		{
			const FLineExit X = LineExit(P, Z, N, Rad, Arena->DistanceToEdge(P, N));
			LastKind = X.Kind; LastDist = X.Dist;
			Extra = X.Kind == 0 ? X.Dist : X.Kind == 1 ? X.Dist + 150.f : X.Kind == 2 ? X.Dist + 400.f : 2500.f;
			return X.Need;
		};
		// shoot so the target leaves along N needing NeedT; full shooter-safety model; false = not a usable cut
		auto EvalContact = [&](const FCand& C, const FVector2D& N, float NeedT, float Extra, float Dist, FPlan& Out) -> bool
		{
			const FVector2D Ghost = C.P - N * (ZoogiConst::ZoogiRadius + C.Radius);
			const FVector2D ToGhost = Ghost - B;
			const float GDist = ToGhost.Size();
			if (GDist < 1.f) return false;
			const FVector2D Dir = ToGhost / GDist;
			const float CosCut = FVector2D::DotProduct(Dir, N);
			if (CosCut < FMath::Cos(FMath::DegreesToRadians(55.f)) || CosCut > FMath::Cos(FMath::DegreesToRadians(4.f))) return false; // not a real cut, or too thin
			float Cost = GDist * 0.5f + Extra - C.Value * 6.f + 20.f + NeedT * 0.35f; // small bias: straight shots are more reliable (Oct 4: 60 -> 20 for variety)
			if (Blocked(B, Ghost, C.P)) Cost += 1500.f;
			if (Arena->IsPathBlocked(B, Ghost, ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
			const float Transfer = (1.f + 0.7f) * M1 / (M1 + C.TargetMass) * CosCut;
			const float VIn = NeedT / FMath::Max(Transfer, 0.3f);
			const float V0 = FMath::Sqrt(VIn * VIn + 2.f * Decel * GDist);
			if (V0 > VMax) Cost += 300.f + (V0 - VMax) * 0.5f;
			float Frac = FMath::Clamp(InvertLaunch(FMath::Min(V0, VMax)), 0.3f, 0.97f);
			const float FracNeed = Frac;
			const float VHit = FMath::Sqrt(FMath::Max(0.f, FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac)) - 2.f * Decel * GDist));
			// mass-aware shooter deflection + the same stop model as straight shots
			const FVector2D VAv = Dir - N * ((1.f + FollowE) * C.TargetMass / (M1 + C.TargetMass) * CosCut);
			const FVector2D Tang = VAv.GetSafeNormal();
			const float KCut = FMath::Max(ThinKeep, VAv.Size());
			const float VAfter = VHit * KCut;
			const float TEdge = Tang.IsNearlyZero() ? 1e6f : Arena->DistanceToEdge(Ghost, Tang);
			float CutMaxSafe = 1.f, CSafe = -1.f, CMiss = -1.f;
			if (!Tang.IsNearlyZero())
			{
				float TWall = 1e6f;
				const float TRun = ShooterRun(Ghost, MeZ, Tang, TEdge, TWall);
				CSafe = HazardRun(Ghost, Tang, PortalRun(Ghost, MeZ, Tang, TRun >= 1e5f ? HopRun(Ghost, Tang, TWall) - 60.f : TRun - 120.f));
				const float VHitMax = FMath::Sqrt(2.f * Decel * FMath::Max(0.f, CSafe)) / FMath::Max(KCut, 0.01f);
				CutMaxSafe = InvertLaunch(FMath::Min(FMath::Sqrt(VHitMax * VHitMax + 2.f * Decel * GDist), VMax * 1.01f));
				if (VAfter * VAfter / (2.f * Decel) > CSafe)
				{
					if (CutMaxSafe >= 0.3f && CutMaxSafe >= 0.8f * Frac) Frac = CutMaxSafe; // a slightly softer cut still does it
					else Cost += 900.f;
				}
			}
			{
				// a miss / thin contact rolls on along the aim line
				const float FullRoll = FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac)) / (2.f * Decel);
				float BWall = 1e6f;
				const float BRun0 = ShooterRun(B, MeZ, Dir, Arena->DistanceToEdge(B, Dir), BWall);
				CMiss = HazardRun(B, Dir, PortalRun(B, MeZ, Dir, BRun0 < 1e5f ? BRun0 : HopRun(B, Dir, BWall) + 70.f));
				if (FullRoll > CMiss - 80.f)
				{
					const float MissSafe = InvertLaunch(FMath::Sqrt(2.f * Decel * FMath::Max(0.f, CMiss - 150.f)));
					if (MissSafe >= 0.3f && MissSafe >= 0.8f * Frac) { Frac = FMath::Min(Frac, MissSafe); CutMaxSafe = FMath::Min(CutMaxSafe, MissSafe); }
					else if (Dist * FMath::Sin(FMath::DegreesToRadians(ErrMaxDeg)) > 0.5f * (ZoogiConst::ZoogiRadius + C.Radius)) Cost += 650.f;
				}
			}
			if (Frac < FracNeed * 0.9f) Cost += (FracNeed - Frac) * 1500.f; // clamped below what it takes: likely no score
			Out = FPlan{ Cost, Dir, Frac, C.Actor, false, GDist, C.Radius, Tang.IsNearlyZero() ? 0.f : FMath::Clamp((VAfter * VAfter / (2.f * Decel)) / FMath::Max(TEdge, 1.f), 0.f, 1.f), 1.f - FMath::Clamp(Arena->DistanceToEdge(Ghost, Dir) / 1500.f, 0.f, 1.f) };
			Out.MaxSafeFrac = CutMaxSafe; Out.DbgCoast = VAfter * VAfter / (2.f * Decel); Out.DbgSafe = CSafe; Out.DbgMiss = CMiss; Out.DbgNeed = NeedT;
			return true;
		};
		// a face-on-or-glancing wall hit along a line: distance (ball centre travel), 2D normal, restitution of the wall
		auto WallHit = [&](const FVector2D& From, float Z, const FVector2D& Dir, float Len, float Rad, FVector2D& Nrm, float& Rest) -> float
		{
			if (!PW) return -1.f;
			FCollisionObjectQueryParams OQ; OQ.AddObjectTypesToQuery(ECC_WorldStatic); OQ.AddObjectTypesToQuery(ECC_WorldDynamic);
			FCollisionQueryParams QP(SCENE_QUERY_STAT(ZoogiCPUBank), false); QP.AddIgnoredActor(Me); QP.bReturnPhysicalMaterial = true;
			TArray<FHitResult> Hits;
			const FVector S(From.X, From.Y, Z + 20.f), E = S + FVector(Dir.X, Dir.Y, 0.f) * Len;
			PW->SweepMultiByObjectType(Hits, S, E, FQuat::Identity, OQ, FCollisionShape::MakeSphere(Rad), QP);
			for (const FHitResult& H : Hits)
			{
				if (H.bStartPenetrating) continue;
				const AActor* A = H.GetActor();
				if (Cast<AZoogiBall>(A) || Cast<AZoogiOrb>(A) || Cast<AZoogiClone>(A)) continue;
				const UPrimitiveComponent* Cm = H.GetComponent();
				if (!Cm || Cm->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Block || FMath::Abs(H.ImpactNormal.Z) > 0.6f) continue;
				Nrm = FVector2D(H.ImpactNormal.X, H.ImpactNormal.Y).GetSafeNormal();
				Rest = H.PhysMaterial.IsValid() ? FMath::Clamp(0.5f * (H.PhysMaterial->Restitution + 0.7f), 0.2f, 0.95f) : BankRest; // combined with the orb's ~0.7
				if (BankRestOverride > 0.f) Rest = BankRestOverride;
				return H.Distance;
			}
			return -1.f;
		};
		const bool bBanks = CPUTier != EZoogiCPUTier::Easy;
		const float BankCost = CPUTier == EZoogiCPUTier::Normal ? 350.f : 150.f;
		for (const FCand& C : Cands)
		{
			const FVector2D D = C.P - B;
			const float Dist = D.Size();
			if (Dist < 150.f) continue;
			const FVector2D Straight = D / Dist;
			const float CZ = C.Actor ? C.Actor->GetActorLocation().Z : MeZ;
			// 1) direct departure lines, +-56 deg in 4 deg steps; rank by need, evaluate the best few with the safety model
			struct FOpt { FVector2D N; float Need; float Extra; float Rank; bool bBank; int32 Kind; float XDist; };
			TArray<FOpt> Opts;
			for (int32 k = -14; k <= 14; ++k)
			{
				if (k == 0) continue; // the straight plan covers it
				const FVector2D N = Straight.GetRotated(k * 4.f);
				float Ex = 0.f;
				const float Nd = ExitNeed(C.P, CZ, N, C.Radius, Ex);
				Opts.Add({ N, Nd, Ex, Nd * (1.f / FMath::Max(0.3f, FMath::Cos(FMath::DegreesToRadians(k * 4.f)))) + Ex, false, LastKind, LastDist });
			}
			// 2) one bank off a wall into a scoring line (orbs only; tier-gated)
			if (bBanks && Cast<AZoogiOrb>(C.Actor) && PW)
			{
				for (int32 k = 0; k < 60; ++k)
				{
					const FVector2D N = FVector2D(1.f, 0.f).GetRotated(k * 6.f);
					if (FVector2D::DotProduct(N, Straight) < 0.5f) continue; // the cue ball must be able to send it this way (<60 deg cut)
					FVector2D Nw; float Rest = BankRest;
					const float Wd = WallHit(C.P, CZ, N, 2200.f, C.Radius * 0.8f, Nw, Rest);
					if (Wd < 60.f) continue;
					const float CosI = -FVector2D::DotProduct(N, Nw);
					if (CosI < 0.15f) continue; // grazing, not a bank
					const FVector2D N2 = (N + Nw * 2.f * CosI).GetSafeNormal();
					const FVector2D Hp = C.P + N * Wd;
					float Ex2 = 0.f;
					const float Need2 = ExitNeed(Hp + N2 * 5.f, CZ, N2, C.Radius, Ex2);
					if (Ex2 >= 2500.f) continue; // a bank into a tall wall is just ping-pong
					const float KB = FMath::Sqrt(Rest * Rest * CosI * CosI + (1.f - CosI * CosI));
					const float V1 = Need2 / FMath::Max(KB, 0.2f);
					const float NeedOrb = FMath::Sqrt(V1 * V1 + 2.f * Decel * Wd);
					Opts.Add({ N, NeedOrb, Ex2 + BankCost + Wd * 0.2f, NeedOrb + Ex2 + BankCost + Wd * 0.2f, true, LastKind, LastDist });
				}
			}
			Opts.Sort([](const FOpt& X, const FOpt& Y) { return X.Rank < Y.Rank; });
			FPlan Best; bool bHave = false;
			int32 NEval = 0;
			for (const FOpt& O : Opts)
			{
				if (NEval >= 6) break;
				FPlan Pl;
				if (!EvalContact(C, O.N, O.Need, O.Extra, Dist, Pl)) continue;
				++NEval;
				Pl.bRimBank = O.bBank; Pl.DbgKind = O.Kind; Pl.DbgExit = O.XDist;
				if (!bHave || Pl.Cost < Best.Cost) { Best = Pl; bHave = true; }
			}
			if (bHave) { if (Best.bRimBank) ++NRimBank; Plans.Add(Best); }
		}
	}
	// Oct 14: Crystal Collapse ice walls are bank surfaces too (cards on only: no walls = no extra plans, no RNG draws).
	// Mirror the target across the wall's bounce line (wall face + Zoogi radius), aim at the mirror; the bounce point must
	// land on a live segment, both legs clear, and the hit soft enough not to shatter the segment instead of banking.
	{
		TArray<AZoogiIceWall::FSeg2D> Ice;
		AZoogiIceWall::LiveSegments(GetWorld(), Ice);
		int32 NBank = 0;
		const float Shatter = AZoogiIceWall::Tune().ShatterSpeed;
		for (const AZoogiIceWall::FSeg2D& W : Ice)
		{
			const FVector2D Nw = W.N;
			const FVector2D Ax = W.B - W.A;
			const float SegL = Ax.Size();
			if (SegL < 1.f) continue;
			const FVector2D AxN = Ax / SegL;
			for (const FCand& C : Cands)
			{
				const float SB = FVector2D::DotProduct(B - W.A, Nw), SC = FVector2D::DotProduct(C.P - W.A, Nw);
				if (SB * SC <= 0.f || FMath::Abs(SB) < W.Half + ZoogiConst::ZoogiRadius + 20.f) continue; // same side, room to roll
				const FVector2D Pl = W.A + Nw * (SB > 0.f ? 1.f : -1.f) * (W.Half + ZoogiConst::ZoogiRadius);
				const FVector2D Mir = C.P - Nw * 2.f * FVector2D::DotProduct(C.P - Pl, Nw);
				const FVector2D L = Mir - B;
				const float Den = FVector2D::DotProduct(L, Nw);
				if (FMath::Abs(Den) < 1.f) continue;
				const float T = -FVector2D::DotProduct(B - Pl, Nw) / Den;
				if (T <= 0.f || T >= 1.f) continue;
				const FVector2D X = B + L * T; // ball centre at the bounce
				const float U = FVector2D::DotProduct(X - W.A, AxN);
				if (U < 12.f || U > SegL - 12.f) continue;
				const float Leg1 = FVector2D::Distance(B, X), Leg2 = FVector2D::Distance(X, C.P);
				if (Leg1 < 60.f || Leg2 < 120.f) continue;
				const FVector2D Dir = (X - B) / Leg1, Dir2 = (C.P - X) / Leg2;
				const float Edge = Arena->DistanceToEdge(C.P, Dir2);
				float Cost = (Leg1 + Leg2) * 0.5f + Edge - C.Value * 6.f + 250.f; // banks are harder than a straight line
				if (Blocked(B, X, C.P) || Arena->IsPathBlocked(B, X, ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
				if (Blocked(X, C.P, C.P) || Arena->IsPathBlocked(X + Dir2 * 5.f, C.P - Dir2 * (ZoogiConst::ZoogiRadius + C.Radius), ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
				const float Need = FMath::Sqrt(2.f * Decel * (Edge + 250.f));
				const float VIn = Need / FMath::Max((1.f + 0.7f) * M1 / (M1 + C.TargetMass), 0.3f);
				const float VOut = FMath::Sqrt(VIn * VIn + 2.f * Decel * FMath::Max(0.f, Leg2 - ZoogiConst::ZoogiRadius - C.Radius));
				const float V0 = FMath::Sqrt(FMath::Square(VOut / 0.75f) + 2.f * Decel * Leg1); // ~75% kept off the ice
				if (V0 > VMax) Cost += 300.f + (V0 - VMax) * 0.5f;
				const float Frac = FMath::Clamp(InvertLaunch(FMath::Min(V0, VMax)), 0.3f, 0.97f);
				const float VWall = FMath::Sqrt(FMath::Max(0.f, FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac)) - 2.f * Decel * Leg1)) * FMath::Abs(FVector2D::DotProduct(Dir, Nw));
				if (VWall > Shatter * 0.9f) Cost += 800.f; // it would smash through, not bank
				FPlan Bp{ Cost, Dir, Frac, C.Actor, false, Leg1 + Leg2, C.Radius, 0.25f, 1.f - FMath::Clamp(Arena->DistanceToEdge(X, Dir2) / 1500.f, 0.f, 1.f) };
				Bp.bBank = true;
				Plans.Add(Bp);
				++NBank;
			}
		}
		if (Ice.Num()) UE_LOG(LogZoogi, Log, TEXT("CPU icewall: %d live segment(s) as obstacles, %d bank plan(s) for P%d"), Ice.Num(), NBank, ActivePlayer + 1);
	}
	// Pick one of the three cheapest shots so CPU players vary their targets.
	Plans.Sort([](const FPlan& A, const FPlan& Bp) { return A.Cost < Bp.Cost; });
	if (Plans.Num() > 0)
	{
		// Vary among the three cheapest, but never pick a clearly worse (blocked / risky) shot for variety's sake.
		// Oct 14 CPU tiers (ZoogiCPUBrain, decision-making only): Normal = this legacy pick, unchanged (same GameRng draws).
		int32 PickIdx = 0;
		if (CPUTier == EZoogiCPUTier::Normal)
		{
			int32 NPick = 1;
			while (NPick < FMath::Min(3, Plans.Num()) && Plans[NPick].Cost < Plans[0].Cost + 350.f) ++NPick;
			PickIdx = GameRng.RandRange(0, NPick - 1);
		}
		else
		{
			TArray<FZoogiCPUShotOption> Opts;
			for (const FPlan& Pl : Plans) Opts.Add({ Pl.Cost, Pl.Risk, Pl.Exposure });
			PickIdx = ZoogiCPU::PickShot(Opts, ZoogiCPU::TierRow(CPUTier), GameRng);
			UE_LOG(LogZoogi, Log, TEXT("CPU tier=%s P%d picked plan %d/%d cost=%.0f risk=%.2f"), ZoogiProgress::TierName(CPUTier), ActivePlayer + 1, PickIdx, Plans.Num(), Plans[PickIdx].Cost, Plans[PickIdx].Risk);
		}
		const FPlan& Pick = Plans[PickIdx];
		if (Pick.bRimBank) UE_LOG(LogZoogi, Log, TEXT("CPU P%d plays a RIM BANK at %s (orb needs %.0f cm/s, cost %.0f)"), ActivePlayer + 1, *GetNameSafe(Pick.Actor), Pick.DbgNeed, Pick.Cost);
		if (Pick.bBank) UE_LOG(LogZoogi, Log, TEXT("CPU icewall: P%d plays a BANK off the ice wall at %s (cost %.0f)"), ActivePlayer + 1, *GetNameSafe(Pick.Actor), Pick.Cost);
		BestDir = Pick.Dir;
		BestFrac = Pick.Frac;
		CPULock = Pick.Actor;
		PickSafeFrac = Pick.MaxSafeFrac;
		if (Pick.DbgSafe >= 0.f) UE_LOG(LogZoogi, Log, TEXT("CPU P%d safety: follow-through ~%.0f cm vs safe run %.0f cm, miss run %.0f cm, max safe %.0f%%; exit kind %d at %.0f cm, target needs %.0f cm/s (%s)"), ActivePlayer + 1, Pick.DbgCoast, FMath::Min(Pick.DbgSafe, 99999.f), Pick.DbgMiss, Pick.MaxSafeFrac * 100.f, Pick.DbgKind, Pick.DbgExit, Pick.DbgNeed, Pick.bStraight ? TEXT("straight") : Pick.bRimBank ? TEXT("rim bank") : TEXT("cut"));
		// Rival-only powers: don't waste a charge if the best shot ended up being an orb.
		if (bHuntRivals && !bPowerTest && !Cast<AZoogiBall>(Pick.Actor)) bCPUUsePower = false;
		// Cut variety (Oct 4): about half of the straight shots are played slightly off-centre (6-20 deg contact cut),
		// so CPUs aren't always head-on (and don't crit every powered hit). Aim offset a: Dist * sin(a) = (R1 + R2) * sin(cut).
		if (bArtifactMode && Pick.bStraight && Pick.Dist > 250.f && GameRng.FRand() < 0.5f)
		{
			const float CutDeg = GameRng.FRandRange(6.f, 20.f) * (GameRng.RandRange(0, 1) == 1 ? 1.f : -1.f);
			const float SinA = FMath::Clamp((ZoogiConst::ZoogiRadius + Pick.Radius) * FMath::Sin(FMath::DegreesToRadians(CutDeg)) / Pick.Dist, -0.5f, 0.5f);
			BestDir = BestDir.GetRotated(FMath::RadiansToDegrees(FMath::Asin(SinA)));
			BestFrac = FMath::Min(0.97f, BestFrac / FMath::Sqrt(FMath::Max(0.5f, FMath::Cos(FMath::DegreesToRadians(CutDeg)))));
			CPUPlannedCut = CutDeg;
		}
	}
	// Bolt phases through Zoogis while powered, so the shot won't stop on a rival: keep it short of the edge.
	if (bCPUUsePower && MyPower == EZoogiPower::StaticShock)
	{
		const float Run = Arena->DistanceToEdge(B, BestDir);
		const float Safe = FMath::Sqrt(2.f * Decel * FMath::Max(0.f, Run - 180.f));
		BestFrac = FMath::Min(BestFrac, FMath::Max(0.2f, InvertLaunch(Safe)));
	}
	// Imperfect aim: less control = more error (ZoogiStats.control).
	const float ErrDeg = GameRng.FRandRange(-1.f, 1.f) * (100.f - Me->Def.Control) / 12.f * (CPUTier == EZoogiCPUTier::Normal ? 1.f : ZoogiCPU::AimNoise(ZoogiCPU::TierRow(CPUTier)));
	// Oct 14: Precision (x0.7) / Heavy Roll (x1.2) scale the aim error in card mode (after the GameRng draw: cards-off parity unchanged)
	if (CardsOn()) if (UZoogiCardSubsystem* CardAim = UZoogiCardSubsystem::Get(this)) BestDir = BestDir.GetRotated(ErrDeg * (CardAim->AimErrorMul(ActivePlayer) - 1.f));
	BestDir = BestDir.GetRotated(ErrDeg);
	BestFrac = FMath::Clamp(BestFrac * GameRng.FRandRange(0.96f, 1.04f), 0.25f, 1.f);
	// Oct 15 edge clamp (after the GameRng draws): the cut-variety boost / power jitter never push the follow-through off
	if (BestFrac > PickSafeFrac && PickSafeFrac >= 0.3f)
	{
		UE_LOG(LogZoogi, Log, TEXT("CPU P%d edge clamp: %.0f%% -> %.0f%% (follow-through stays on court)"), ActivePlayer + 1, BestFrac * 100.f, PickSafeFrac * 100.f);
		BestFrac = PickSafeFrac;
	}
	if (NWalled || NRunWalled || NHoles) UE_LOG(LogZoogi, Log, TEXT("CPU P%d wall check: exit lines walled %d/%d (launch exits %d), follow-through lines walled %d/%d, floor holes on shooter lines %d, wall-hop clamps %d, portal lines %d, hazard lines %d/%d, rim-bank plans %d, line exits drop/tall %d/%d; pick %.0f%%"), ActivePlayer + 1, NRunWalled, NRuns, NLaunch, NWalled, NSweeps, NHoles, NHopClamp, NPortalClamp, NHazClamp, Hazards.Num(), NRimBank, NLineDrop, NLineTall, BestFrac * 100.f);
	// Oct 8: Frozen aerial layer: with the battle map props up, a CPU sometimes plays a prop route (rail / halo / bumper) instead.
	// No-op (and no GameRng draw) without the map layout, so every existing mode and test plans exactly as before.
	if (UZoogiBattleStateMachine* BSM = GetWorld()->GetSubsystem<UZoogiBattleStateMachine>())
		if (BSM->PlanCPUProp(ActivePlayer, BestDir, BestFrac, CPULock)) { bCPUUsePower = false; CPUPlannedCut = 0.f; } // a clean prop route, no power
	// -ZoogiRockTest: hard straight shot at the nearest standing rock (screenshots of cracking / shattering).
	if (bRockTest && Arena)
	{
		if (AZoogiRock* R = Arena->NearestStandingRock(B))
		{
			BestDir = (FVector2D(R->GetActorLocation()) - B).GetSafeNormal();
			BestFrac = R->GetState() == EZoogiRockState::Intact ? 0.6f : 0.97f; // medium shot cracks, the hard one shatters
			UE_LOG(LogZoogi, Log, TEXT("RockTest: P%d aims at %s at %.0f%%"), ActivePlayer + 1, *R->GetName(), BestFrac * 100.f);
		}
	}
	// Oct 14 playtest fix (cards mode only): Crystal Collapse armed -> aim at the nearest breakable piece in range,
	// and plan the speed the shot will really have after Storm / Heavy Roll (FxModifyShot multiplies it later).
	if (CardsOn()) if (UZoogiCardSubsystem* CardPlan = UZoogiCardSubsystem::Get(this))
	{
		if (AZoogiArenaPiece* Pc = CardPlan->CPUCollapseTarget(Me))
		{
			const FVector2D To = FVector2D(Pc->GetActorLocation()) - B;
			BestDir = To.GetSafeNormal(); CPULock = Pc;
			BestFrac = FMath::Clamp(InvertLaunch(FMath::Sqrt(2.f * Decel * (To.Size() + 350.f))), 0.35f, 0.9f);
			UE_LOG(LogZoogi, Log, TEXT("CPU P%d aims Crystal Collapse at %s (%.0f cm)"), ActivePlayer + 1, *Pc->GetName(), To.Size());
		}
		else if (const int32 Intent = CardPlan->CPUCardIntent(ActivePlayer))
		{
			// fantasy cards want a rival on the line (Storm chain, Frost beam, Precision homing, Steal tether, Heavy boulder,
			// Ember fireballs, Double clone) or a stop on a rival (Titan meteor lands where the shot stops)
			AZoogiBall* Best = nullptr; float BD = 2600.f;
			for (AZoogiBall* O : Balls)
			{
				if (!O || O == Me || O->bOut || O->bKnockedOut || (bComboMode && O->TeamIndex == Me->TeamIndex)) continue;
				const float D = FVector::Dist2D(O->BallLocation(), Me->BallLocation());
				if (D < BD) { BD = D; Best = O; }
			}
			if (Best)
			{
				BestDir = (FVector2D(Best->BallLocation()) - B).GetSafeNormal(); CPULock = Best;
				// a miss rolls on past the rival: keep that free roll 400 cm inside the edge (terrain hops on Volcanic)
				const float Run = FMath::Min(BD + (Intent == 2 ? -60.f : 300.f), Arena->DistanceToEdge(B, BestDir) - 400.f);
				BestFrac = FMath::Clamp(InvertLaunch(FMath::Sqrt(2.f * Decel * FMath::Max(0.f, Run))), 0.3f, 0.95f);
				UE_LOG(LogZoogi, Log, TEXT("CPU P%d card plan: %s P%d (%.0f cm)"), ActivePlayer + 1, Intent == 2 ? TEXT("meteor stop on") : TEXT("rival line to"), Best->PlayerIndex + 1, BD);
			}
		}
		const float Mul = CardPlan->ShotMul(ActivePlayer);
		if (Mul > 1.001f)
		{
			const float Was = BestFrac;
			BestFrac = FMath::Clamp(InvertLaunch(ZoogiConst::LaunchSpeedForPull(BestFrac) / Mul), 0.2f, 1.f);
			UE_LOG(LogZoogi, Log, TEXT("CPU P%d card shot x%.2f: plan %.0f%% -> %.0f%%"), ActivePlayer + 1, Mul, Was * 100.f, BestFrac * 100.f);
		}
	}
	if (bComboMode) ComboPlanCPUForce(BestFrac);
	const float PullLen = BestFrac * ZoogiConst::FullPull / ZoogiConst::DragScale;
	CPUTargetPull = FVector(BestDir.X, BestDir.Y, 0.f) * PullLen;
	Me->AimYaw = FVector(BestDir.X, BestDir.Y, 0.f).Rotation().Yaw;
	UE_LOG(LogZoogi, Log, TEXT("CPU P%d plans %s at %.0f%% (power %s, cut %+.0f deg)"), ActivePlayer + 1, CPULock.IsValid() ? *TargetName(CPULock.Get()) : TEXT("-"), BestFrac * 100.f, bCPUUsePower ? TEXT("yes") : TEXT("no"), CPUPlannedCut);
	bCPUPlanned = true;
}

// ------------------------------------------------------------------ tick

void AZoogiGameMode::TakeShot(const FString& Name)
{
	if (ShotDir.IsEmpty() || ShotsTaken.Contains(Name)) return;
	if (ComboDemo > 0 && !Name.StartsWith(TEXT("cd"))) return; // combo demo runs keep only their own captures
	ShotsTaken.Add(Name);
	const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("%02d_%s.png"), ShotSeq++, *Name));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogZoogi, Log, TEXT("Screenshot requested: %s"), *Path);
}

void AZoogiGameMode::PlayMusic(const TCHAR* Name)
{
	if (CurrentMusic == Name) return;
	CurrentMusic = Name;
	if (MusicComp) { MusicComp->FadeOut(1.0f, 0.f); MusicComp = nullptr; }
	USoundBase* S = ZoogiUtil::LoadSound(Name);
	UE_LOG(LogZoogi, Log, TEXT("Music: %s (%s)"), Name, S ? TEXT("loaded") : TEXT("missing"));
	if (S)
	{
		MusicComp = UGameplayStatics::CreateSound2D(this, S, 0.35f, 1.f, 0.f, nullptr, false, false);
		if (MusicComp) MusicComp->FadeIn(1.5f, 0.35f);
	}
}

void AZoogiGameMode::TakeShotDelayed(const FString& Name, float Delay)
{
	if (ShotDir.IsEmpty() || ShotsTaken.Contains(Name)) return;
	DelayedShots.Add(Name, Delay);
}

void AZoogiGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (auto It = DelayedShots.CreateIterator(); It; ++It)
	{
		It.Value() -= DeltaSeconds;
		if (It.Value() <= 0.f) { TakeShot(It.Key()); It.RemoveCurrent(); }
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	Clock += DeltaSeconds;
	if (ClimaxLeft > 0.f)
	{
		ClimaxLeft -= DeltaSeconds;
		if (ClimaxLeft <= 0.f) { ClimaxLeft = 0.f; LandClimax(); }
	}
	PhaseTime += Dt;
	BannerTime += Dt;
	for (FZoogiPopup& P : Popups) P.Age += Dt;
	BigAge += Dt;
	for (FZoogiFlyer& F : Flyers) F.Age += Dt;
	Flyers.RemoveAll([](const FZoogiFlyer& F) { return F.Age > 1.2f; });
	for (FZoogiFeedItem& F : Feed) F.Age += Dt;
	Feed.RemoveAll([](const FZoogiFeedItem& F) { return F.Age > 6.f; });
	Popups.RemoveAll([](const FZoogiPopup& P) { return P.Age > 2.6f; });

	if (bAuto && bMenuShot && Phase == EZoogiPhase::Menu)
	{
		if (Clock > 4.f) TakeShot(TEXT("menu"));
		if (Clock > 5.5f) StartMatch();
	}
	if (QuitAfter > 0.f && Clock > QuitAfter)
	{
		QuitAfter = 0.f;
		UE_LOG(LogZoogi, Log, TEXT("ZoogiQuit reached (%.0f s), quitting"), Clock);
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		HardQuitAt = FPlatformTime::Seconds() + 5.0;
	}
	// Automation safety net: if the polite quit didn't take, force the exit.
	if (HardQuitAt > 0.0 && FPlatformTime::Seconds() > HardQuitAt)
	{
		HardQuitAt = 0.0;
		UE_LOG(LogZoogi, Warning, TEXT("Quit did not complete, forcing exit"));
		FPlatformMisc::RequestExit(false);
	}

	if (bComboMode && (Phase == EZoogiPhase::Aiming || Phase == EZoogiPhase::Rolling || Phase == EZoogiPhase::Resolving)) ComboTick(Dt);
	if (bDamageShots) TickDamageShots(Dt);
	switch (Phase)
	{
	case EZoogiPhase::Forming:
		if (bFormShots) TickFormShots();
		// spawn elevators rise over the last ~1.25 s of the stage intro (wide forming camera), landing as the first turn starts
		if (!bElevatorsPlayed && FormPass == 0 && Arena && Arena->Forming && PhaseTime >= Arena->Forming->GetLength() * FormTimeScale - 1.25f) { bElevatorsPlayed = true; AZoogiSpawnElevator::PlayAll(this); }
		if (!Arena || !Arena->IsForming())
		{
			UE_LOG(LogZoogi, Log, TEXT("STAGE FORMED after %.2f s: colliders live, first turn"), PhaseTime);
			if (bFormShots && FormPass == 0)
			{
				TakeShot(TEXT("form_99_settled"));
				// Second pass: replay the forming with the camera on the hub pillars rising through the floor.
				FormPass = 1;
				FormCamMode = 1;
				FormShotIdx = 0;
				Arena->BeginForming(FormTimeScale);
				break;
			}
			if (bFormShots) TakeShot(TEXT("pillar_99_settled"));
			FormCamMode = 0;
			if (bFormingCombo) ComboBeginTeamTurn(0); else BeginTurn(0);
			if (!bElevatorsPlayed) { bElevatorsPlayed = true; AZoogiSpawnElevator::PlayAll(this); } // (forming skipped early)
		}
		break;
	case EZoogiPhase::Aiming:
	{
		AZoogiBall* Ball = GetActiveBall();
		if (Ball && Ball->bCPU && bCPUPlanned && !bArtifactMode) CPUPowerTick();
		TickStages(Dt);
		TickArtifacts(Dt);
		break;
	}
	case EZoogiPhase::Resolving:
	{
		TickArtifacts(Dt);
		CheckKnockoffs(); // something still falling after a timeout: commit it too
		if (Ledger.Num() > 0) { int32 S = 0, K = 0; CommitLedger(S, K); }
		TickStages(Dt);
		break;
	}
	case EZoogiPhase::Rolling:
	{
		StageTime += Dt;
		StepTime += Dt;
		TickPowers(Dt);
		TickArtifacts(Dt);
		if (!bArtifactMode) CPUPowerTick();
		CheckKnockoffs();
		if (ShotClock >= 0.f)
		{
			ShotClock += Dt;
			if (ShotsFired <= 4 && StepTime > 2.8f) TakeShot(FString::Printf(TEXT("s%d_5_rolling"), ShotsFired));
			if (ShotsFired <= 6 && ShotClock > 0.9f) TakeShot(FString::Printf(TEXT("follow_shot%d"), ShotsFired));
			if (ShotsFired <= 3 && ShotClock > 3.2f) TakeShot(FString::Printf(TEXT("follow_late_shot%d"), ShotsFired));
		}
		{
			UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
			const bool bBusy = Anime && Anime->IsBusy();
			if (PhaseTime > 0.25f && AllAtRest() && !bBusy)
			{
				SettleTimer += Dt;
				if (SettleTimer >= ZoogiStageTune::SettleTime) SetStage(EZoogiStage::Resolve);
			}
			else
			{
				SettleTimer = 0.f;
			}
			if (Phase == EZoogiPhase::Rolling && PhaseTime > ZoogiStageTune::ResolveTimeout && !bBusy)
			{
				// Timeout: stop everything where it is, then Resolve (final positions are what you see).
				UE_LOG(LogZoogi, Log, TEXT("Resolve timeout after %.1f s: stopping all bodies"), PhaseTime);
				for (AZoogiBall* B : Balls) if (B && !B->bOut) B->SetPlanarVelocity(FVector::ZeroVector);
				for (AZoogiOrb* O : Orbs) if (O && !O->bOut) { O->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector); O->Sphere->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector); }
				EndPowers();
				PullLeft = 0.f;
				CheckKnockoffs();
				SetStage(EZoogiStage::Resolve);
			}
		}
		break;
	}
	case EZoogiPhase::GameOver:
		if (PhaseTime > 1.5f) TakeShot(TEXT("gameover"));
		if (bAuto && PhaseTime > 4.f && HardQuitAt <= 0.0)
		{
			UE_LOG(LogZoogi, Log, TEXT("Auto match over, quitting"));
			UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
			HardQuitAt = FPlatformTime::Seconds() + 5.0;
		}
		break;
	default:
		break;
	}
}

void AZoogiGameMode::OnRockEvent(AZoogiRock* Rock, bool bShattered, AZoogiBall* By, const FVector& PopupAt)
{
	AddPopup(bShattered ? TEXT("SHATTERED!") : TEXT("CRACK!"), PopupAt, bShattered ? FLinearColor(1.f, 0.8f, 0.5f) : FLinearColor(0.85f, 0.7f, 0.5f));
	if (By) AddFeed(FString::Printf(TEXT("P%d %s  %s a rock"), By->PlayerIndex + 1, *By->Def.Name, bShattered ? TEXT("shattered") : TEXT("cracked")), FLinearColor(0.75f, 0.6f, 0.42f));
	if (bShattered) { TakeShotDelayed(TEXT("rock_shatter"), 0.12f); TakeShotDelayed(TEXT("rock_shatter_after"), 1.1f); TakeShotDelayed(TEXT("rock_rubble"), 3.4f); }
	else { TakeShotDelayed(TEXT("rock_crack"), 0.1f); TakeShotDelayed(TEXT("rock_crack_after"), 0.9f); }
}

void AZoogiGameMode::DuckMusic(float Seconds)
{
	if (NotifyMatchEvent(EZoogiMatchEvent::Duck, -1, Seconds) || !MusicComp) return;
	MusicComp->AdjustVolume(0.05f, 0.6f); // ~-4.4 dB duck under big hits (team brief: duck 3-6 dB, never mute; full silence is saved for one climax)
	FTimerHandle H;
	TWeakObjectPtr<UAudioComponent> M = MusicComp;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([M]() { if (M.IsValid()) M->AdjustVolume(0.6f, 1.f); }), Seconds, false);
}
