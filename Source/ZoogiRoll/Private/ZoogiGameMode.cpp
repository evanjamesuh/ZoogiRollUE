#include "ZoogiGameMode.h"
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
#include "Engine/World.h"
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
	FParse::Value(Cmd, TEXT("ZoogiQuit="), QuitAfter);
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

	// 2v2 Competitive Combo: -ZoogiCombo (mode), -ZoogiComboDemo=1|2|3 (scripted verification runs, implies -ZoogiCombo).
	FParse::Value(Cmd, TEXT("ZoogiComboDemo="), ComboDemo);
	if (FParse::Param(Cmd, TEXT("ZoogiCombo")) || ComboDemo > 0) ComboSetMode(true);
	if (bAuto)
	{
		for (FZoogiPlayerSetup& P : MenuPlayers) P.bCPU = true;
		if (FParse::Param(Cmd, TEXT("ZoogiHumanTest")) || FParse::Param(Cmd, TEXT("ZoogiUiTest"))) MenuPlayers[0].bCPU = false;
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
	if (Action == "Start" || Action == "Rematch" || Action == "Infuse" || Action == "Guard" || Action == "Save" || Action == "AimConfirm" || Action == "FollowUp")
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
	for (int32 i = 0; i < ZoogiConst::OrbCount; ++i)
	{
		const float A = (float)i / ZoogiConst::OrbCount * 2.f * PI;
		FVector L(FMath::Cos(A) * ZoogiConst::OrbRingRadius, FMath::Sin(A) * ZoogiConst::OrbRingRadius, ZoogiConst::OrbRadius + 1.f);
		if (!OrbSpot(L)) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (O)
		{
			O->InitOrb(OrbPM, Decel);
			if (i % 5 == 0) O->MakeStar(); // three gold star orbs: double points + a power charge
			Orbs.Add(O);
		}
	}
	for (int32 i = 0; i < 3; ++i)
	{
		const float A = i * 2.f * PI / 3.f + PI / 6.f;
		FVector L(FMath::Cos(A) * 90.f, FMath::Sin(A) * 90.f, ZoogiConst::OrbRadius + 1.f);
		if (Arena && Arena->IsNeon()) L = FVector(-200.f + i * 200.f, 0.f, L.Z); // between the centre channels
		if (!OrbSpot(L)) continue;
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (O)
		{
			O->InitOrb(OrbPM, Decel);
			Orbs.Add(O);
		}
	}
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
	PlayMusic(TEXT("MatchMusic"));
	if (!BeginStageForming(false)) BeginTurn(0);
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
			Shooter->bOut ? -1.f : FVector::Dist2D(Shooter->BallLocation(), ShotStart), ShotSpeed, Shooter->bOut ? TEXT("OUT") : TEXT("on court"), OrbsLeft(), PhaseTime);
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
	if (MusicComp) MusicComp->AdjustVolume(0.25f, 0.f); // the one full music drop-out of the match
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
	if (MusicComp) MusicComp->AdjustVolume(1.6f, 1.f);
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
	if (Arena && Arena->IsNeon())
	{
		// Rectangular court: clamp onto the rails' inner box.
		return FVector(FMath::Clamp(P.X, -1140.f, 1140.f), FMath::Clamp(P.Y, -740.f, 740.f), 120.f);
	}
	if (P.Size() < 1.f) P = FVector2D(1, 0);
	P = P.GetSafeNormal() * (ZoogiConst::KnockoffRadius - 60.f);
	return FVector(P.X, P.Y, 120.f);
}

void AZoogiGameMode::CheckKnockoffs()
{
	if (bComboMode) { ComboCheckKnockoffs(); return; }
	for (AZoogiOrb* O : Orbs)
	{
		if (!O || O->bOut) continue;
		const FVector L = O->OrbLocation();
		if (L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f)
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
		if (L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f)
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
	FireShot(V);
	SetBattleStep(EZoogiBattleStep::Rolling);
	return true;
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
	bCPUUsePower = !bArtifactMode && Me->Charges > 0 && (bPowerTest || FMath::FRand() < (Me->Charges >= 2 ? 0.8f : 0.5f));
	const EZoogiPower MyPower = Me->Def.PowerKind;
	const bool bHuntRivals = bCPUUsePower && (MyPower == EZoogiPower::ShadowStun || MyPower == EZoogiPower::StaticShock || MyPower == EZoogiPower::BandageBind || MyPower == EZoogiPower::RuneSeal);
	for (AZoogiBall* Other : Balls) if (Other && Other != Me && !Other->bOut && !IsAlly(Me, Other)) Cands.Add({ FVector2D(Other->BallLocation()), bHuntRivals ? 100.f : 40.f, Other->Def.MassRatio(), ZoogiConst::ZoogiRadius, Other });

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

	CPUPlannedCut = 0.f;
	struct FPlan { float Cost; FVector2D Dir; float Frac; AActor* Actor; bool bStraight; float Dist; float Radius; };
	TArray<FPlan> Plans;
	FVector2D BestDir(-B.GetSafeNormal());
	float BestFrac = 0.45f;
	const float M1 = Me->Def.MassRatio();
	const float VMax = ZoogiConst::LaunchSpeedForPull(1.f);
	for (const FCand& C : Cands)
	{
		const FVector2D D = C.P - B;
		const float Dist = D.Size();
		if (Dist < 1.f) continue;
		const FVector2D Dir = D / Dist;
		const float Edge = Arena->DistanceToEdge(C.P, Dir);
		float Cost = Dist * 0.5f + Edge - C.Value * 6.f;
		if (Blocked(B, C.P, C.P)) Cost += 1500.f;
		// Night Circuit pads / posts: in the way of the shot, or between the target and its edge.
		if (Arena->IsPathBlocked(B, C.P - Dir * (ZoogiConst::ZoogiRadius + C.Radius), ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
		else if (Arena->IsPathBlocked(C.P + Dir * (C.Radius + 10.f), C.P + Dir * FMath::Min(Edge, 2500.f), C.Radius - 5.f)) Cost += 700.f;
		// Power needed to push the target ~2.5 m past the edge (1D collision, restitution ~0.7).
		const float Need = FMath::Sqrt(2.f * Decel * (Edge + 250.f));
		float Transfer = (1.f + 0.7f) * M1 / (M1 + C.TargetMass);
		if (bCPUUsePower && MyPower == EZoogiPower::BandageBind && Cast<AZoogiBall>(C.Actor)) Transfer *= 1.25f; // bind yank
		const float VIn = Need / FMath::Max(Transfer, 0.3f);
		const float Gap = FMath::Max(0.f, Dist - ZoogiConst::ZoogiRadius - C.Radius);
		const float V0 = FMath::Sqrt(VIn * VIn + 2.f * Decel * Gap);
		if (V0 > VMax) Cost += 300.f + (V0 - VMax) * 0.5f; // can't knock it off from here: less attractive
		const float Frac = FMath::Clamp(InvertLaunch(FMath::Min(V0, VMax)), 0.3f, 0.97f);
		// Self knock-off risk: the shooter keeps rolling after the hit (heavier shooter / lighter target = more).
		const float VLaunch = ZoogiConst::LaunchSpeedForPull(Frac);
		const float VHit = FMath::Sqrt(FMath::Max(0.f, VLaunch * VLaunch - 2.f * Decel * Gap));
		const float VAfter = FMath::Max(0.f, VHit * (M1 - 0.7f * C.TargetMass) / (M1 + C.TargetMass));
		const float Coast = VAfter * VAfter / (2.f * Decel);
		const FVector2D Contact = C.P - Dir * (ZoogiConst::ZoogiRadius + C.Radius);
		const float MyEdge = Arena->DistanceToEdge(Contact, Dir);
		if (Coast > MyEdge - 80.f) Cost += 900.f; // would follow the target off the edge (-75 and a wasted turn)
		Plans.Add({ Cost, Dir, Frac, C.Actor, true, Dist, C.Radius });
	}
	// Angled (cut) shots: aim the ghost-ball contact so the target departs toward its nearest edge.
	// The target leaves along the contact normal; a cut of c degrees transfers ~cos(c) of the head-on push.
	if (bArtifactMode)
	{
		for (const FCand& C : Cands)
		{
			const FVector2D D = C.P - B;
			const float Dist = D.Size();
			if (Dist < 150.f) continue;
			const FVector2D Straight = D / Dist;
			float BestEdge = TNumericLimits<float>::Max();
			FVector2D BestN = Straight;
			for (int32 k = -5; k <= 5; ++k)
			{
				const FVector2D N = Straight.GetRotated(k * 9.f);
				const float E = Arena->DistanceToEdge(C.P, N);
				if (E < BestEdge) { BestEdge = E; BestN = N; }
			}
			const FVector2D Ghost = C.P - BestN * (ZoogiConst::ZoogiRadius + C.Radius);
			const FVector2D ToGhost = Ghost - B;
			const float GDist = ToGhost.Size();
			if (GDist < 1.f) continue;
			const FVector2D Dir = ToGhost / GDist;
			const float CosCut = FVector2D::DotProduct(Dir, BestN);
			if (CosCut < FMath::Cos(FMath::DegreesToRadians(50.f)) || CosCut > FMath::Cos(FMath::DegreesToRadians(4.f))) continue; // not a real cut, or too thin
			float Cost = GDist * 0.5f + BestEdge - C.Value * 6.f + 20.f; // small bias: straight shots are more reliable (Oct 4: 60 -> 20 for variety)
			if (Blocked(B, Ghost, C.P)) Cost += 1500.f;
			if (Arena->IsPathBlocked(B, Ghost, ZoogiConst::ZoogiRadius - 5.f)) Cost += 1500.f;
			else if (Arena->IsPathBlocked(C.P + BestN * (C.Radius + 10.f), C.P + BestN * FMath::Min(BestEdge, 2500.f), C.Radius - 5.f)) Cost += 700.f;
			const float Need = FMath::Sqrt(2.f * Decel * (BestEdge + 250.f));
			const float Transfer = (1.f + 0.7f) * M1 / (M1 + C.TargetMass) * CosCut;
			const float VIn = Need / FMath::Max(Transfer, 0.3f);
			const float V0 = FMath::Sqrt(VIn * VIn + 2.f * Decel * GDist);
			if (V0 > VMax) Cost += 300.f + (V0 - VMax) * 0.5f;
			const float Frac = FMath::Clamp(InvertLaunch(FMath::Min(V0, VMax)), 0.3f, 0.97f);
			// The shooter deflects off sideways (90 deg rule): check it doesn't follow its own line off the edge.
			const FVector2D Tang = (Dir - CosCut * BestN).GetSafeNormal();
			const float VHit = FMath::Sqrt(FMath::Max(0.f, FMath::Square(ZoogiConst::LaunchSpeedForPull(Frac)) - 2.f * Decel * GDist));
			const float VAfter = VHit * FMath::Sqrt(FMath::Max(0.f, 1.f - CosCut * CosCut));
			if (!Tang.IsNearlyZero() && VAfter * VAfter / (2.f * Decel) > Arena->DistanceToEdge(Ghost, Tang) - 80.f) Cost += 900.f;
			Plans.Add({ Cost, Dir, Frac, C.Actor, false, GDist, C.Radius });
		}
	}
	// Pick one of the three cheapest shots so CPU players vary their targets.
	Plans.Sort([](const FPlan& A, const FPlan& Bp) { return A.Cost < Bp.Cost; });
	if (Plans.Num() > 0)
	{
		// Vary among the three cheapest, but never pick a clearly worse (blocked / risky) shot for variety's sake.
		int32 NPick = 1;
		while (NPick < FMath::Min(3, Plans.Num()) && Plans[NPick].Cost < Plans[0].Cost + 350.f) ++NPick;
		const FPlan& Pick = Plans[FMath::RandRange(0, NPick - 1)];
		BestDir = Pick.Dir;
		BestFrac = Pick.Frac;
		CPULock = Pick.Actor;
		// Rival-only powers: don't waste a charge if the best shot ended up being an orb.
		if (bHuntRivals && !bPowerTest && !Cast<AZoogiBall>(Pick.Actor)) bCPUUsePower = false;
		// Cut variety (Oct 4): about half of the straight shots are played slightly off-centre (6-20 deg contact cut),
		// so CPUs aren't always head-on (and don't crit every powered hit). Aim offset a: Dist * sin(a) = (R1 + R2) * sin(cut).
		if (bArtifactMode && Pick.bStraight && Pick.Dist > 250.f && FMath::FRand() < 0.5f)
		{
			const float CutDeg = FMath::FRandRange(6.f, 20.f) * (FMath::RandBool() ? 1.f : -1.f);
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
	const float ErrDeg = FMath::FRandRange(-1.f, 1.f) * (100.f - Me->Def.Control) / 12.f;
	BestDir = BestDir.GetRotated(ErrDeg);
	BestFrac = FMath::Clamp(BestFrac * FMath::FRandRange(0.96f, 1.04f), 0.25f, 1.f);
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
	if (!MusicComp) return;
	MusicComp->AdjustVolume(0.05f, 0.6f); // ~-4.4 dB duck under big hits (team brief: duck 3-6 dB, never mute; full silence is saved for one climax)
	FTimerHandle H;
	TWeakObjectPtr<UAudioComponent> M = MusicComp;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([M]() { if (M.IsValid()) M->AdjustVolume(0.6f, 1.f); }), Seconds, false);
}
