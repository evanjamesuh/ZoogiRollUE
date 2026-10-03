#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiClone.h"
#include "ZoogiFX.h"
#include "ZoogiPhysics.h"
#include "ZoogiPlayerController.h"
#include "ZoogiHUD.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Components/AudioComponent.h"
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
		MenuPlayers[i].ZoogiIndex = i % 7;
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

	if (bAuto)
	{
		for (FZoogiPlayerSetup& P : MenuPlayers) P.bCPU = true;
		if (FParse::Param(Cmd, TEXT("ZoogiHumanTest"))) MenuPlayers[0].bCPU = false;
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
	if (Action == "Players") MenuNumPlayers = FMath::Clamp(Index, 2, 4);
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
	else if (Action == "Start") StartMatch();
	else if (Action == "Power") TryPower();
	else if (Action == "Menu") ReturnToMenu();
	else if (Action == "Rematch") StartMatch();
	else if (Action == "Quit") UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	if (Action != "Start" && Action != "Quit" && Action != "Power") ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.8f);
	MenuRevision++;
}

void AZoogiGameMode::ClearMatch()
{
	EndPowers();
	for (AZoogiBall* B : Balls) if (B) B->Destroy();
	for (AZoogiOrb* O : Orbs) if (O) O->Destroy();
	Balls.Reset();
	Orbs.Reset();
	Popups.Reset();
	bAimActive = false;
	AimPull = FVector::ZeroVector;
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
	ClearMatch();
	if (Arena->GetTheme() != MenuArena) Arena->Build(MenuArena);
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
	UE_LOG(LogZoogi, Log, TEXT("Match started: %d players on %s"), Balls.Num(), *ZoogiUtil::ArenaName(MenuArena));
	PlayMusic(TEXT("MatchMusic"));
	BeginTurn(0);
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

	Banner = FString::Printf(TEXT("P%d  %s%s"), PlayerIdx + 1, *Ball->Def.Name, Ball->bCPU ? TEXT("  (CPU)") : TEXT(""));
	ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.45f);
	BannerTime = 0.f;
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnTurnStarted(Ball);
}

void AZoogiGameMode::EndTurn()
{
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
	ZoogiUtil::PlaySfx(this, TEXT("Win"), 0.8f);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnGameOver(GetActiveBall());
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
				Balls[Scorer]->Score += Pts;
				Balls[Scorer]->OrbsScored++;
				if (O->bStar) Balls[Scorer]->Charges = FMath::Min(3, Balls[Scorer]->Charges + 1);
				UE_LOG(LogZoogi, Log, TEXT("%s off the edge -> P%d +%d (score %d)"), O->bStar ? TEXT("Star orb") : TEXT("Orb"), Scorer + 1, Pts, Balls[Scorer]->Score);
				ZoogiUtil::PlaySfx(this, TEXT("Score"), O->bStar ? 0.9f : 0.6f, O->bStar ? 1.25f : 1.f);
				AddPopup(O->bStar ? FString::Printf(TEXT("+%d STAR  +1 POWER  P%d"), Pts, Scorer + 1) : FString::Printf(TEXT("+%d ORB  P%d"), Pts, Scorer + 1),
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
			B->Score = FMath::Max(0, B->Score - ZoogiConst::PenaltyFall);
			UE_LOG(LogZoogi, Log, TEXT("P%d fell off (last hit by %s)"), B->PlayerIndex + 1, B->LastHitPlayer >= 0 ? *FString::Printf(TEXT("P%d"), B->LastHitPlayer + 1) : TEXT("nobody"));
			AddPopup(FString::Printf(TEXT("-%d FELL OFF  P%d"), ZoogiConst::PenaltyFall, B->PlayerIndex + 1), EdgePoint(L, Arena), FLinearColor(1.f, 0.3f, 0.25f));
			if (Balls.IsValidIndex(B->LastHitPlayer) && B->LastHitPlayer != B->PlayerIndex)
			{
				AZoogiBall* A = Balls[B->LastHitPlayer];
				A->Score += ZoogiConst::ScorePlayer;
				A->KnockOuts++;
				AddPopup(FString::Printf(TEXT("+%d KNOCKOUT  P%d"), ZoogiConst::ScorePlayer, A->PlayerIndex + 1), EdgePoint(L, Arena) + FVector(0, 0, 90.f), ZoogiUtil::PlayerColor(A->PlayerIndex));
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
	FireShot(V);
	return true;
}

void AZoogiGameMode::FireShot(const FVector2D& Velocity)
{
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	Ball->Launch(Velocity);
	ZoogiUtil::PlaySfx(this, TEXT("Launch"), FMath::Clamp(Velocity.Size() / 2500.f, 0.3f, 1.f), FMath::Lerp(0.85f, 1.2f, FMath::Clamp(Velocity.Size() / 3600.f, 0.f, 1.f)));
	OnArmedShot(Ball);
	Phase = EZoogiPhase::Rolling;
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

	struct FCand { FVector2D P; float Value; float TargetMass; float Radius; };
	TArray<FCand> Cands;
	for (AZoogiOrb* O : Orbs) if (O && !O->bOut) Cands.Add({ FVector2D(O->OrbLocation()), 50.f, ZoogiConst::OrbMassRatio, ZoogiConst::OrbRadius });
	// Decide on the power first: rival-hunting powers make rivals more attractive targets.
	bCPUUsePower = Me->Charges > 0 && (bPowerTest || FMath::FRand() < 0.55f);
	const EZoogiPower MyPower = Me->Def.PowerKind;
	const bool bHuntRivals = bCPUUsePower && (MyPower == EZoogiPower::ShadowStun || MyPower == EZoogiPower::StaticShock || MyPower == EZoogiPower::BandageBind);
	for (AZoogiBall* Other : Balls) if (Other && Other != Me && !Other->bOut) Cands.Add({ FVector2D(Other->BallLocation()), bHuntRivals ? 140.f : 40.f, Other->Def.MassRatio(), ZoogiConst::ZoogiRadius });

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

	struct FPlan { float Cost; FVector2D Dir; float Frac; };
	TArray<FPlan> Plans;
	FVector2D BestDir(-B.GetSafeNormal());
	float BestFrac = 0.45f;
	for (const FCand& C : Cands)
	{
		float BestCost = TNumericLimits<float>::Max();
		const FVector2D D = C.P - B;
		const float Dist = D.Size();
		if (Dist < 1.f) continue;
		const FVector2D Dir = D / Dist;
		const float Edge = Arena->DistanceToEdge(C.P, Dir);
		float Cost = Dist * 0.5f + Edge - C.Value * 6.f;
		if (Blocked(B, C.P, C.P)) Cost += 1500.f;
		if (Cost < BestCost)
		{
			BestCost = Cost;
			BestDir = Dir;
			const float Need = FMath::Sqrt(2.f * Decel * (Edge + 250.f));
			const float M1 = Me->Def.MassRatio();
			const float Transfer = (1.f + 0.7f) * M1 / (M1 + C.TargetMass);
			const float VIn = Need / FMath::Max(Transfer, 0.3f);
			const float Gap = FMath::Max(0.f, Dist - ZoogiConst::ZoogiRadius - C.Radius);
			const float V0 = FMath::Sqrt(VIn * VIn + 2.f * Decel * Gap);
			BestFrac = FMath::Clamp(InvertLaunch(V0), 0.3f, 0.97f);
		}
		Plans.Add({ BestCost, BestDir, BestFrac });
	}
	// Pick one of the three cheapest shots so CPU players vary their targets.
	Plans.Sort([](const FPlan& A, const FPlan& Bp) { return A.Cost < Bp.Cost; });
	if (Plans.Num() > 0)
	{
		const FPlan& Pick = Plans[FMath::RandRange(0, FMath::Min(2, Plans.Num() - 1))];
		BestDir = Pick.Dir;
		BestFrac = Pick.Frac;
	}
	// Imperfect aim: less control = more error (ZoogiStats.control).
	const float ErrDeg = FMath::FRandRange(-1.f, 1.f) * (100.f - Me->Def.Control) / 12.f;
	BestDir = BestDir.GetRotated(ErrDeg);
	BestFrac = FMath::Clamp(BestFrac * FMath::FRandRange(0.96f, 1.04f), 0.25f, 1.f);
	const float PullLen = BestFrac * ZoogiConst::FullPull / ZoogiConst::DragScale;
	CPUTargetPull = FVector(BestDir.X, BestDir.Y, 0.f) * PullLen;
	Me->AimYaw = FVector(BestDir.X, BestDir.Y, 0.f).Rotation().Yaw;
	bCPUPlanned = true;
}

// ------------------------------------------------------------------ tick

void AZoogiGameMode::TakeShot(const FString& Name)
{
	if (ShotDir.IsEmpty() || ShotsTaken.Contains(Name)) return;
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
	PhaseTime += Dt;
	BannerTime += Dt;
	for (FZoogiPopup& P : Popups) P.Age += Dt;
	Popups.RemoveAll([](const FZoogiPopup& P) { return P.Age > 2.6f; });

	if (bAuto && bMenuShot && Phase == EZoogiPhase::Menu)
	{
		if (Clock > 4.f) TakeShot(TEXT("menu"));
		if (Clock > 5.5f) StartMatch();
	}
	if (QuitAfter > 0.f && Clock > QuitAfter)
	{
		QuitAfter = 0.f;
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}

	switch (Phase)
	{
	case EZoogiPhase::Aiming:
	{
		AZoogiBall* Ball = GetActiveBall();
		if (Ball && !Ball->bCPU && bAimActive && AimFraction() > 0.6f) TakeShot(FString::Printf(TEXT("human_drag_round%d"), RoundNumber));
		if (Ball && Ball->bCPU && bCPUPlanned)
		{
			CPUPowerTick();
			if (PhaseTime > 1.55f && ShotsFired < 3) TakeShot(FString::Printf(TEXT("aimpose_shot%d"), ShotsFired + 1));
			// CPU: let the camera swoop in, draw the slingshot back over ~1.1 s, release.
			const float DrawTime = 1.1f;
			const float T = (PhaseTime - CPUStartDelay) / DrawTime;
			if (T > 0.f)
			{
				const float E = FMath::InterpEaseOut(0.f, 1.f, FMath::Min(T, 1.f), 2.f);
				SetAim(true, CPUTargetPull * E);
				if (T > 0.85f && ShotsFired < 6) TakeShot(FString::Printf(TEXT("aim_shot%d"), ShotsFired + 1));
				if (T >= 1.15f) ReleaseAim();
			}
		}
		break;
	}
	case EZoogiPhase::Rolling:
	{
		TickPowers(Dt);
		CPUPowerTick();
		CheckKnockoffs();
		if (ShotClock >= 0.f)
		{
			ShotClock += Dt;
			if (ShotsFired <= 6 && ShotClock > 0.9f) TakeShot(FString::Printf(TEXT("follow_shot%d"), ShotsFired));
			if (ShotsFired <= 3 && ShotClock > 3.2f) TakeShot(FString::Printf(TEXT("follow_late_shot%d"), ShotsFired));
		}
		if (PhaseTime > 0.25f && AllAtRest())
		{
			SettleTimer += Dt;
			if (SettleTimer >= ZoogiConst::SettleDelay) EndTurn();
		}
		else
		{
			SettleTimer = 0.f;
		}
		if (PhaseTime > 25.f) EndTurn(); // safety net
		break;
	}
	case EZoogiPhase::GameOver:
		if (PhaseTime > 1.5f) TakeShot(TEXT("gameover"));
		if (bAuto && !ShotDir.IsEmpty() && PhaseTime > 4.f)
		{
			UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		}
		break;
	default:
		break;
	}
}
