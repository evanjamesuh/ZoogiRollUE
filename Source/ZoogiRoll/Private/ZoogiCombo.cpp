// 2v2 Competitive Combo System (Evan's development package, Part 4), implemented on AZoogiGameMode.
// Team turns (DRAW -> AWAKEN -> BATTLE -> RESOLVE -> END), CONTROL / DRIVE / LAUNCH forces, Ally Bump,
// per-team Combo Gauge, one Partner Follow-Up per team turn, Guard Reaction / Guard Break, Ring Marks with the
// Recovery Gate, neutral Arena Orbs (ORB OUT = score + Resonance + Arena Control, then respawn), reactive terrain.
#include "ZoogiCombo.h"
#include "ZoogiArenaPiece.h"
#include "ZoogiFloorScars.h"
#include "ZoogiArena.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiGameMode.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiArena.h"
#include "ZoogiRock.h"
#include "ZoogiFX.h"
#include "ZoogiPlayerController.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiAnimeSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	FVector CPos(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}
	const FVector2D GComboSpawns[4] = { FVector2D(-1000.f, 50.f), FVector2D(-700.f, -400.f), FVector2D(1000.f, -50.f), FVector2D(700.f, 400.f) };
	// Frozen Ring (Oct 6): the starts sit in the clear lanes between the hub pillars / obelisks (45 / 180 / 225 / 0 deg).
	const FVector2D GComboSpawnsFrozen[4] = { FVector2D(-1000.f, 0.f), FVector2D(-640.f, -640.f), FVector2D(1000.f, 0.f), FVector2D(640.f, 640.f) };
	const FVector2D GComboOrbHomes[6] = { FVector2D(-350.f, -150.f), FVector2D(300.f, 150.f), FVector2D(0.f, 0.f), FVector2D(-450.f, -750.f), FVector2D(-200.f, 1050.f), FVector2D(500.f, -1050.f) };
}

// ------------------------------------------------------------------ helpers

FLinearColor AZoogiGameMode::TeamColor(int32 Team)
{
	return Team == 0 ? ZoogiUtil::Hex(TEXT("#38BDF8")) : ZoogiUtil::Hex(TEXT("#F43F5E"));
}

const TCHAR* AZoogiGameMode::ForceName(EZoogiForce F)
{
	switch (F)
	{
	case EZoogiForce::Control: return TEXT("CONTROL");
	case EZoogiForce::Drive: return TEXT("DRIVE");
	case EZoogiForce::Launch: return TEXT("LAUNCH");
	}
	return TEXT("");
}

FString AZoogiGameMode::TriggerNames(uint32 Mask)
{
	static const TCHAR* Names[ZoogiComboTrigger::Num] = { TEXT("ALLY BUMP"), TEXT("BANK SETUP"), TEXT("AIRBORNE SETUP"), TEXT("ORB RELAY"), TEXT("GUARD BREAK"), TEXT("POWER SETUP"), TEXT("TERRAIN SETUP") };
	TArray<FString> Out;
	for (int32 i = 0; i < ZoogiComboTrigger::Num; ++i) if (Mask & (1u << i)) Out.Add(Names[i]);
	return FString::Join(Out, TEXT(" + "));
}

bool AZoogiGameMode::IsAlly(const AActor* A, const AActor* B) const
{
	const AZoogiBall* BA = Cast<AZoogiBall>(A);
	const AZoogiBall* BB = Cast<AZoogiBall>(B);
	return bComboMode && BA && BB && BA != BB && BA->TeamIndex == BB->TeamIndex;
}

AZoogiBall* AZoogiGameMode::PartnerOf(const AZoogiBall* B) const
{
	if (!B || !bComboMode) return nullptr;
	for (AZoogiBall* O : Balls) if (O && O != B && O->TeamIndex == B->TeamIndex) return O;
	return nullptr;
}

int32 AZoogiGameMode::TeamAlive(int32 Team) const
{
	int32 N = 0;
	for (const AZoogiBall* B : Balls) if (B && B->TeamIndex == Team && !B->bKnockedOut) ++N;
	return N;
}

int32 AZoogiGameMode::TeamScore(int32 Team) const
{
	int32 S = 0;
	for (const AZoogiBall* B : Balls) if (B && B->TeamIndex == Team) S += B->Score;
	return S;
}

FString AZoogiGameMode::ComboShot(const FString& Name, float Delay)
{
	const FString N = FString::Printf(TEXT("cd%d_%s"), ComboDemo, *Name);
	if (Delay <= 0.f) TakeShot(N); else TakeShotDelayed(N, Delay);
	return N;
}

void AZoogiGameMode::ComboCallout(const FString& Text, const FLinearColor& Color)
{
	FZoogiCallout C;
	C.Text = Text;
	C.Color = Color;
	Callouts.Insert(C, 0);
	if (Callouts.Num() > 4) Callouts.SetNum(4);
}

void AZoogiGameMode::ComboAddEnergy(int32 Team, float Amount, const FString& Why)
{
	if (Team < 0 || Team > 1 || Amount == 0.f) return;
	const float Before = ComboEnergy[Team];
	ComboEnergy[Team] = FMath::Clamp(ComboEnergy[Team] + Amount, 0.f, ZoogiComboTune::GaugeMax);
	UE_LOG(LogZoogi, Log, TEXT("COMBO gauge %s %+.0f (%s): %.0f -> %.0f"), *TeamName(Team), Amount, *Why, Before, ComboEnergy[Team]);
}

void AZoogiGameMode::ComboTrigger(uint32 Flag, float Energy, const FVector& Where)
{
	if (ShotTriggers & Flag) return;
	ShotTriggers |= Flag;
	const FString L = TriggerNames(Flag);
	ComboAddEnergy(ComboTeam, Energy, L);
	const FLinearColor C(1.f, 0.85f, 0.3f);
	ComboCallout(FString::Printf(TEXT("%s!  +%.0f COMBO"), *L, Energy), C);
	AddPopup(L + TEXT("!"), Where + FVector(0, 0, 170.f), C);
	AddFeed(FString::Printf(TEXT("%s  %s  +%.0f combo"), *TeamName(ComboTeam), *L, Energy), TeamColor(ComboTeam));
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.8f, 1.8f);
	AZoogiFX::Spawn(GetWorld(), Where, C, 160.f, 0.45f, true, 9.f);
	UE_LOG(LogZoogi, Log, TEXT("COMBO trigger: %s (P%d %s, %s shot%s)"), *L, ActivePlayer + 1, Balls.IsValidIndex(ActivePlayer) ? *Balls[ActivePlayer]->Def.Name : TEXT("?"), ForceName(ComboShotForce), bInFollowUp ? TEXT(", follow-up") : TEXT(""));
	ComboShot(FString::Printf(TEXT("trigger_%s"), *L.Replace(TEXT(" "), TEXT("_")).ToLower()), 0.12f);
}

// ------------------------------------------------------------------ mode / match

void AZoogiGameMode::ComboSetMode(bool bOn)
{
	bComboMode = bOn;
	ZoogiUtil::SetComboColors(bOn);
	if (bOn)
	{
		MenuNumPlayers = 4;
		const EZoogiArena Want = ComboArenaFor(MenuArena, ComboDemo);
		if (MenuArena != Want)
		{
			MenuArena = Want;
			if (Arena)
			{
				ClearMatch();
				Arena->Build(MenuArena);
				SpawnOrbs();
			}
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("Mode: %s"), bOn ? (MenuArena == EZoogiArena::Frozen ? TEXT("2v2 COMBO (Frozen Ring)") : TEXT("2v2 COMBO (Meadow prototype arena)")) : TEXT("CLASSIC"));
}

void AZoogiGameMode::ComboSpawnOrbs()
{
	const float Decel = Arena ? Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
	ComboOrbHome.Reset();
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector L(GComboOrbHomes[i].X, GComboOrbHomes[i].Y, ZoogiConst::OrbRadius + 1.f);
		AZoogiOrb* O = GetWorld()->SpawnActor<AZoogiOrb>(AZoogiOrb::StaticClass(), FTransform(L));
		if (!O) continue;
		O->InitOrb(OrbPM, Decel);
		if (i == 2) O->MakeStar();
		Orbs.Add(O);
		ComboOrbHome.Add(L);
	}
}

void AZoogiGameMode::ComboStartMatch()
{
	ClearMatch();
	ZoogiUtil::SetComboColors(true);
	MenuNumPlayers = 4;
	MenuArena = ComboArenaFor(MenuArena, ComboDemo);
	if (Arena->GetTheme() != MenuArena) Arena->Build(MenuArena);
	Arena->ResetRocks();
	AZoogiComboArena::SetLayout(MenuArena);
	const bool bFrozen = MenuArena == EZoogiArena::Frozen;
	ComboArena = GetWorld()->SpawnActor<AZoogiComboArena>(AZoogiComboArena::StaticClass(), FTransform::Identity);
	if (ComboArena)
	{
		ComboArena->SetArena(Arena);
		TArray<FVector2D> Homes;
		for (const FVector2D& H : GComboOrbHomes) Homes.Add(H);
		ComboArena->SetOrbHomes(Homes);
		ComboArena->Build();
	}
	ComboSpawnOrbs();
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	for (int32 i = 0; i < 4; ++i)
	{
		const FZoogiPlayerSetup& S = MenuPlayers[i];
		const FVector2D Sp = bFrozen ? GComboSpawnsFrozen[i] : GComboSpawns[i];
		const FVector L(Sp.X, Sp.Y, ZoogiConst::ZoogiRadius + 2.f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-L.Y, -L.X));
		AZoogiBall* B = GetWorld()->SpawnActor<AZoogiBall>(AZoogiBall::StaticClass(), FTransform(FRotator(0, Yaw, 0), L));
		if (!B) continue;
		B->InitZoogi(Roster[S.ZoogiIndex % Roster.Num()], i, S.bCPU, BallPM, Arena->GetRollingDecel());
		B->TeamIndex = TeamOfIndex(i);
		// A team with a human in it is played by that human (they pick either Zoogi each team turn).
		if (B->bCPU && !MenuPlayers[i ^ 1].bCPU) { B->bCPU = false; UE_LOG(LogZoogi, Log, TEXT("COMBO: P%d is controlled by the human teammate P%d"), i + 1, (i ^ 1) + 1); }
		B->RingMarks = 2;
		B->bKnockedOut = false;
		B->SpawnLocation = L;
		B->SpawnYaw = Yaw;
		B->AimYaw = Yaw;
		Balls.Add(B);
	}
	for (int32 t = 0; t < 2; ++t) { ComboEnergy[t] = 0.f; ComboResonance[t] = 0; ArenaControl[t] = 0.f; ComboOrbOuts[t] = 0; ComboNextPick[t] = 0; }
	bComboMatchOver = false;
	ComboWinner = -1;
	ComboOverReason.Reset();
	Callouts.Reset();
	AirState.Reset();
	AirState.SetNum(Balls.Num());
	PendingKicks.Reset();
	RoundNumber = 1;
	WinnerIndex = -1;
	ShotsFired = 0;
	TurnCounter = 0;
	DemoTurn = 0; DemoStep = 0; DemoSeenTurn = -1;
	UE_LOG(LogZoogi, Log, TEXT("Match started: 2v2 COMBO on %s  TEAM A: %s + %s   TEAM B: %s + %s"), *ZoogiUtil::ArenaName(MenuArena),
		Balls.IsValidIndex(0) ? *Balls[0]->Def.Name : TEXT("?"), Balls.IsValidIndex(1) ? *Balls[1]->Def.Name : TEXT("?"),
		Balls.IsValidIndex(2) ? *Balls[2]->Def.Name : TEXT("?"), Balls.IsValidIndex(3) ? *Balls[3]->Def.Name : TEXT("?"));
	PlayMusic(TEXT("MatchMusic"));
	if (ComboDemo > 0) DemoStage();
	if (!BeginStageForming(true)) ComboBeginTeamTurn(0);
}

void AZoogiGameMode::ComboBeginTeamTurn(int32 Team)
{
	ComboTeam = Team;
	bFollowUpUsed = false;
	bFollowUpOffered = false;
	bInFollowUp = false;
	FollowRelayOrb.Reset();
	const int32 Pref = Team * 2 + ComboNextPick[Team], Other = Team * 2 + 1 - ComboNextPick[Team];
	auto Alive = [this](int32 i) { return Balls.IsValidIndex(i) && Balls[i] && !Balls[i]->bKnockedOut; };
	auto Ready = [this, &Alive](int32 i) { return Alive(i) && Balls[i]->StunTurns <= 0; };
	int32 Pick = Ready(Pref) ? Pref : (Ready(Other) ? Other : -1);
	// Stunned Zoogis sit this team turn out (the stun ticks down).
	for (int32 i : { Pref, Other })
	{
		if (Alive(i) && Balls[i]->StunTurns > 0)
		{
			Balls[i]->StunTurns--;
			AddFeed(FString::Printf(TEXT("P%d %s  is stunned and sits this team turn out"), i + 1, *Balls[i]->Def.Name), FLinearColor(0.8f, 0.65f, 1.f));
		}
	}
	if (Pick < 0) Pick = Alive(Pref) ? Pref : Other; // whole team stunned: the stun was spent above, play on
	if (!Alive(Pick))
	{
		ComboFinish(1 - Team, FString::Printf(TEXT("%s has no Zoogi left"), *TeamName(Team)));
		return;
	}
	ComboMainShooter = Pick;
	UE_LOG(LogZoogi, Log, TEXT("COMBO team turn: %s (round %d) -> P%d %s pre-selected"), *TeamName(Team), RoundNumber, Pick + 1, *Balls[Pick]->Def.Name);
	BeginTurn(Pick);
}

void AZoogiGameMode::ComboResetShot()
{
	ShotTriggers = 0;
	RelayOrb.Reset();
	BumpedAlly.Reset();
	bRelayTouched = false;
	bShotBanked = false;
	bShotTerrain = false;
	bShotContact = false;
	bFollowUpBroken = false;
	ShotTerrainWhat.Reset();
	if (!bCPUPlanned) ComboForce = EZoogiForce::Drive; // keep a CPU's planned force
	for (FBallAir& S : AirState) S.bTerrainLaunch = false;
}

void AZoogiGameMode::ComboBeginShot(int32 PlayerIdx)
{
	// A Partner Follow-Up: same team turn, straight to BATTLE (no new draw / awaken).
	ActivePlayer = PlayerIdx;
	Phase = EZoogiPhase::Aiming;
	PhaseTime = 0.f;
	SettleTimer = 0.f;
	bAimActive = false;
	AimPull = FVector::ZeroVector;
	bCPUPlanned = false;
	EndPowers();
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
	for (AZoogiBall* B : Balls) if (B) { B->LastHitPlayer = -1; B->SetActiveTurn(false); }
	for (AZoogiOrb* O : Orbs) if (O) O->LastHitBy = -1;
	AZoogiBall* Ball = GetActiveBall();
	if (!Ball) return;
	Ball->SetActiveTurn(true);
	const FVector L = Ball->BallLocation();
	if (FVector2D(L.X, L.Y).Size() > 200.f) Ball->AimYaw = FRotator(FVector(-L.X, -L.Y, 0.f).Rotation()).Yaw;
	ComboResetShot();
	if (Ball->bCPU) PlanCPUShot();
	ClearLock();
	SetStage(EZoogiStage::Battle);
	Banner = FString::Printf(TEXT("FOLLOW-UP  P%d  %s%s"), PlayerIdx + 1, *Ball->Def.Name, Ball->bCPU ? TEXT("  (CPU)") : TEXT(""));
	BannerTime = 0.f;
	ZoogiUtil::PlaySfx(this, TEXT("Turn"), 0.5f, 1.2f);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnTurnStarted(Ball);
}

bool AZoogiGameMode::CanComboSelect() const
{
	if (!bComboMode || bInFollowUp || Phase != EZoogiPhase::Aiming) return false;
	return (Stage == EZoogiStage::Awaken && !bAwakenDone) || IsAimStep();
}

bool AZoogiGameMode::ComboSelect(int32 PlayerIdx)
{
	AZoogiBall* Old = GetActiveBall();
	if (!CanComboSelect() || !Old) return false;
	if (PlayerIdx < 0) { AZoogiBall* P = PartnerOf(Old); PlayerIdx = P ? P->PlayerIndex : -1; }
	if (!Balls.IsValidIndex(PlayerIdx) || PlayerIdx == ActivePlayer) return false;
	AZoogiBall* New = Balls[PlayerIdx];
	if (!New || New->TeamIndex != ComboTeam || New->bKnockedOut || New->bOut || New->StunTurns > 0) return false;
	// The Awaken result travels with the selection (drawn card, or the infusion when switching in Battle).
	if (Old->Artifact && New->Artifact)
	{
		if (Old->Artifact->Drawn != EZoogiArtifact::None) { New->Artifact->Drawn = Old->Artifact->Drawn; Old->Artifact->Drawn = EZoogiArtifact::None; }
		if (Old->Artifact->Infused != EZoogiArtifact::None)
		{
			const EZoogiArtifact A = Old->Artifact->Infused;
			Old->Artifact->ClearInfused();
			New->Artifact->SetInfused(A);
			RefreshArtifactMass(Old, false);
		}
	}
	Old->SetActiveTurn(false);
	ActivePlayer = PlayerIdx;
	ComboMainShooter = PlayerIdx;
	New->SetActiveTurn(true);
	ClearLock();
	bFreeAim = false;
	AimCutDeg = 0.f;
	Preview = FZoogiAimPreview();
	bCPUPlanned = false;
	if (New->bCPU) PlanCPUShot();
	Banner = FString::Printf(TEXT("%s  P%d  %s"), *TeamName(ComboTeam), PlayerIdx + 1, *New->Def.Name);
	BannerTime = 0.6f;
	AZoogiFX::Spawn(GetWorld(), New->BallLocation(), ZoogiUtil::PlayerColor(PlayerIdx), 130.f, 0.4f, true, 8.f);
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.7f, 1.3f);
	UE_LOG(LogZoogi, Log, TEXT("COMBO select: %s switches to P%d %s"), *TeamName(ComboTeam), PlayerIdx + 1, *New->Def.Name);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnTurnStarted(New);
	return true;
}

void AZoogiGameMode::ComboSetForce(EZoogiForce F)
{
	if (!bComboMode) return;
	if (ComboForce != F) UE_LOG(LogZoogi, Log, TEXT("COMBO force: P%d -> %s"), ActivePlayer + 1, ForceName(F));
	ComboForce = F;
	ZoogiUtil::PlaySfx(this, TEXT("HitClick"), 0.5f, F == EZoogiForce::Control ? 1.6f : (F == EZoogiForce::Drive ? 1.3f : 1.0f));
}

float AZoogiGameMode::ComboForceFraction(float BaseFrac) const
{
	switch (ComboForce)
	{
	case EZoogiForce::Control: return FMath::Clamp(BaseFrac, 0.12f, ZoogiComboTune::ControlMaxFrac);
	case EZoogiForce::Drive: return ZoogiComboTune::DriveFrac;
	case EZoogiForce::Launch: return ZoogiComboTune::LaunchFrac;
	}
	return BaseFrac;
}

float AZoogiGameMode::ComboShotMul(const AZoogiBall* Ball) const
{
	if (!Ball) return 1.f;
	// Existing stats ride along (small, learnable): CTL steadies CONTROL, SPD drives DRIVE, PWR fuels LAUNCH.
	float M = 1.f;
	switch (ComboForce)
	{
	case EZoogiForce::Control: M = 1.f + (Ball->Def.Control - 60) * 0.001f; break;
	case EZoogiForce::Drive: M = 1.f + (Ball->Def.Speed - 60) * 0.0015f; break;
	case EZoogiForce::Launch: M = 1.f + (Ball->Def.Power - 60) * 0.0015f; break;
	}
	if (bInFollowUp) M *= ZoogiComboTune::FollowUpSpeedMul;
	return FMath::Clamp(M, 0.9f, 1.2f);
}

void AZoogiGameMode::ComboModifyShot(AZoogiBall* Ball, FVector2D& V)
{
	const float M = ComboShotMul(Ball);
	V *= M;
	ComboShotForce = ComboForce;
	if (!bInFollowUp) ComboMainShooter = ActivePlayer;
	if (ComboShotForce == EZoogiForce::Launch && Ball)
	{
		const float Inf = (Ball->Artifact && Ball->Artifact->Infused != EZoogiArtifact::None) ? ZoogiArtifacts::Get(Ball->Artifact->Infused).MassMul : 1.f;
		Ball->SetMassScale(Inf * ZoogiComboTune::LaunchMassMul);
	}
	UE_LOG(LogZoogi, Log, TEXT("COMBO shot: P%d %s %s %.0f cm/s (x%.3f stats%s)"), ActivePlayer + 1, Ball ? *Ball->Def.Name : TEXT("?"), ForceName(ComboShotForce), V.Size(), M, bInFollowUp ? TEXT(", follow-up") : TEXT(""));
	bDemoShotFired = true;
}

void AZoogiGameMode::ComboPlanCPUForce(float& Frac)
{
	if (Frac <= ZoogiComboTune::ControlMaxFrac + 0.03f) { ComboForce = EZoogiForce::Control; Frac = FMath::Min(Frac, ZoogiComboTune::ControlMaxFrac); }
	else if (Frac <= 0.78f) { ComboForce = EZoogiForce::Drive; Frac = ZoogiComboTune::DriveFrac; }
	else { ComboForce = EZoogiForce::Launch; Frac = ZoogiComboTune::LaunchFrac; }
	UE_LOG(LogZoogi, Log, TEXT("CPU P%d picks %s"), ActivePlayer + 1, ForceName(ComboForce));
}

// ------------------------------------------------------------------ follow-up / end / finish

bool AZoogiGameMode::ComboAcceptFollowUp()
{
	if (!bFollowUpOffered) return false;
	AZoogiBall* Shooter = GetActiveBall();
	AZoogiBall* P = PartnerOf(Shooter);
	if (!P || P->bKnockedOut || P->bOut) { bFollowUpOffered = false; return false; }
	bFollowUpOffered = false;
	bFollowUpUsed = true;
	ComboEnergy[ComboTeam] = FMath::Max(0.f, ComboEnergy[ComboTeam] - ZoogiComboTune::FollowUpCost);
	FollowRelayOrb = (OfferTriggers & ZoogiComboTrigger::OrbRelay) ? RelayOrb : nullptr;
	UE_LOG(LogZoogi, Log, TEXT("COMBO PARTNER FOLLOW-UP: P%d %s acts (setup %s, -%.0f gauge -> %.0f)"), P->PlayerIndex + 1, *P->Def.Name, *TriggerNames(OfferTriggers), ZoogiComboTune::FollowUpCost, ComboEnergy[ComboTeam]);
	ComboCallout(FString::Printf(TEXT("PARTNER FOLLOW-UP!  %s"), *P->Def.Name.ToUpper()), TeamColor(ComboTeam));
	BigText = FString::Printf(TEXT("PARTNER FOLLOW-UP!\n%s  ->  %s"), *TriggerNames(OfferTriggers), *P->Def.Name.ToUpper());
	BigColor = TeamColor(ComboTeam);
	BigAge = 0.f;
	AZoogiFX::Spawn(GetWorld(), P->BallLocation(), TeamColor(ComboTeam), 240.f, 0.6f, true, 12.f);
	ComboBeginShot(P->PlayerIndex);
	bInFollowUp = true;
	ComboShot(TEXT("followup_start"), 0.35f);
	return true;
}

bool AZoogiGameMode::ComboDeclineFollowUp()
{
	if (!bFollowUpOffered) return false;
	bFollowUpOffered = false;
	ComboCallout(TEXT("FOLLOW-UP SKIPPED"), FLinearColor(0.7f, 0.75f, 0.85f));
	UE_LOG(LogZoogi, Log, TEXT("COMBO follow-up declined by %s (gauge kept at %.0f)"), *TeamName(ComboTeam), ComboEnergy[ComboTeam]);
	return true;
}

void AZoogiGameMode::ComboEndTurn()
{
	EndPowers();
	// Guards last until the end of the opponent's team turn (follow-ups included).
	for (AZoogiBall* B : Balls)
	{
		if (!B || !B->Artifact) continue;
		if (B->Artifact->Guard != EZoogiArtifact::None && B->Artifact->GuardSetTurn < TurnCounter)
		{
			UE_LOG(LogZoogi, Log, TEXT("Guard expired on P%d (%s)"), B->PlayerIndex + 1, *ZoogiArtifacts::Name(B->Artifact->Guard));
			B->Artifact->ClearGuard();
		}
		RefreshArtifactMass(B, false);
		B->bEdgeSave = false;
	}
	if (Balls.IsValidIndex(ComboMainShooter)) ComboNextPick[ComboTeam] = 1 - (ComboMainShooter % 2);
	if (bComboMatchOver) { ComboFinish(ComboWinner, ComboOverReason); return; }
	const int32 Next = 1 - ComboTeam;
	if (Next == 0)
	{
		++RoundNumber;
		if (RoundNumber > MenuRounds)
		{
			int32 M[2] = { 0, 0 };
			for (const AZoogiBall* B : Balls) if (B && !B->bKnockedOut) M[B->TeamIndex] += B->RingMarks;
			const int32 W = M[0] != M[1] ? (M[0] > M[1] ? 0 : 1) : (TeamScore(0) != TeamScore(1) ? (TeamScore(0) > TeamScore(1) ? 0 : 1) : -1);
			ComboFinish(W, FString::Printf(TEXT("round limit: Ring Marks %d - %d, score %d - %d"), M[0], M[1], TeamScore(0), TeamScore(1)));
			return;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("COMBO end of team turn: %s done -> %s (gauge A %.0f, B %.0f)"), *TeamName(ComboTeam), *TeamName(Next), ComboEnergy[0], ComboEnergy[1]);
	ComboBeginTeamTurn(Next);
}

void AZoogiGameMode::ComboFinish(int32 WinTeam, const FString& Why)
{
	if (Phase == EZoogiPhase::GameOver) return;
	Phase = EZoogiPhase::GameOver;
	PhaseTime = 0.f;
	bAimActive = false;
	bFollowUpOffered = false;
	ComboWinner = WinTeam;
	ComboOverReason = Why;
	WinnerIndex = -1;
	for (AZoogiBall* B : Balls) if (B) B->SetActiveTurn(false);
	if (WinTeam >= 0)
	{
		for (AZoogiBall* B : Balls) if (B && B->TeamIndex == WinTeam && !B->bKnockedOut) { WinnerIndex = B->PlayerIndex; B->SetActiveTurn(true); break; }
		Banner = FString::Printf(TEXT("%s WINS!"), *TeamName(WinTeam));
	}
	else Banner = TEXT("IT'S A DRAW!");
	BannerTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Game over: MATCH OVER - %s (%s)"), *Banner, *Why);
	UE_LOG(LogZoogi, Log, TEXT("FinalScores: TEAM A=%d TEAM B=%d"), TeamScore(0), TeamScore(1));
	ZoogiUtil::PlaySfx(this, TEXT("Win"), 0.8f);
	PlayVictoryCrowd();
	ComboShot(TEXT("match_over"), 1.4f);
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PC->OnGameOver(Balls.IsValidIndex(WinnerIndex) ? Balls[WinnerIndex].Get() : nullptr);
}

// ------------------------------------------------------------------ collisions

void AZoogiGameMode::ComboOnBallHit(AZoogiBall* Self, AActor* Other, UPrimitiveComponent* OtherComp, const FVector& NormalImpulse, const FHitResult& Hit)
{
	if (!Self || Phase != EZoogiPhase::Rolling) return;
	AZoogiBall* Shooter = GetActiveBall();
	AZoogiBall* OB = Cast<AZoogiBall>(Other);
	AZoogiOrb* OO = Cast<AZoogiOrb>(Other);
	const int32 SI = Self->PlayerIndex;
	if (AirState.Num() != Balls.Num()) AirState.SetNum(Balls.Num());

	// Bumpers kick any Zoogi back out (applied after the solver step in ComboTick).
	if (OtherComp && OtherComp->ComponentHasTag(TEXT("ZBumper")) && AirState.IsValidIndex(SI) && Clock - AirState[SI].LastBumpAt > 0.25f && ComboArena)
	{
		AirState[SI].LastBumpAt = Clock;
		const FVector C = OtherComp->GetComponentLocation();
		FVector Dir = (Self->BallLocation() - C).GetSafeNormal2D();
		if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
		PendingKicks.Add({ Self, Dir, ComboArena->BumperKickSpeed() });
		AddPopup(TEXT("BUMPER!"), Self->BallLocation() + FVector(0, 0, 120.f), FLinearColor(1.f, 0.4f, 0.8f));
		if (Self == Shooter && !bShotContact) { bShotTerrain = true; ShotTerrainWhat = TEXT("BUMPER"); }
		UE_LOG(LogZoogi, Log, TEXT("Bumper kick: P%d +%.0f cm/s"), SI + 1, ComboArena->BumperKickSpeed());
	}
	// Heavy Zoogi collisions on top of the damageable section count as impacts too.
	if ((OB || OO) && ComboArena && NormalImpulse.Size() > 9000.f && ComboArena->IsInSection(FVector2D(Hit.ImpactPoint), 30.f))
	{
		if (ComboArena->FloorImpact(Hit.ImpactPoint, Self->GetBallVelocity().GetSafeNormal2D(), NormalImpulse.Size())) ComboOnFloorChanged(Self);
	}

	if (Self != Shooter)
	{
		// Orb Relay: the relayed orb reaches the shooter's partner.
		if (OO && RelayOrb.Get() == OO && PartnerOf(Shooter) == Self && !bRelayTouched)
		{
			bRelayTouched = true;
			UE_LOG(LogZoogi, Log, TEXT("Orb relay: the orb reached P%d"), SI + 1);
		}
		return;
	}

	if (!OB && !OO)
	{
		// Walls, rim rocks, platform side, raised floor edges: a bank when the face is mostly vertical (not the floor or a ramp top).
		if (OtherComp && !OtherComp->ComponentHasTag(TEXT("ZBumper")) && FMath::Abs(Hit.ImpactNormal.Z) < 0.6f && !bShotContact && !bShotBanked)
		{
			bShotBanked = true;
			BankLogAt = Clock + 0.12f;
			UE_LOG(LogZoogi, Log, TEXT("Bank: P%d off %s (%s) at (%.0f, %.0f), in %.0f cm/s heading %.1f deg, normal %.1f deg"), SI + 1, Other ? *Other->GetName() : TEXT("?"), OtherComp->ComponentTags.Num() ? *OtherComp->ComponentTags[0].ToString() : TEXT("rim"),
				Self->BallLocation().X, Self->BallLocation().Y, FVector2D(Self->PrevVelocity).Size(), FMath::RadiansToDegrees(FMath::Atan2(Self->PrevVelocity.Y, Self->PrevVelocity.X)), FMath::RadiansToDegrees(FMath::Atan2(Hit.ImpactNormal.Y, Hit.ImpactNormal.X)));
		}
		return;
	}

	const FVector Mid = (Self->BallLocation() + CPos(Other)) * 0.5f;
	if (OB && IsAlly(Self, OB))
	{
		// Friendly contact = tactical redirection, never damage: no artifact effect, no knock-out credit.
		const FVector N = (OB->BallLocation() - Self->BallLocation()).GetSafeNormal2D();
		const float Rel = FVector::DotProduct(Self->PrevVelocity - OB->PrevVelocity, N);
		if (Rel > ZoogiComboTune::AllyBumpMinSpeed && !(ShotTriggers & ZoogiComboTrigger::AllyBump))
		{
			BumpedAlly = OB;
			UE_LOG(LogZoogi, Log, TEXT("ALLY BUMP: P%d redirects P%d (%.0f cm/s along the contact normal)"), SI + 1, OB->PlayerIndex + 1, Rel);
			if (bShotBanked && !bShotContact) ComboTrigger(ZoogiComboTrigger::BankSetup, ZoogiComboTune::EBank, Mid);
			ComboTrigger(ZoogiComboTrigger::AllyBump, ZoogiComboTune::EAllyBump, Mid);
			AZoogiFX::Spawn(GetWorld(), Mid, TeamColor(ComboTeam), 200.f, 0.5f, true, 10.f);
			ComboShot(TEXT("ally_bump"), 0.25f);
		}
		bShotContact = true;
		return;
	}
	// Enemy Zoogi or Arena Orb.
	if (bShotBanked && !bShotContact) ComboTrigger(ZoogiComboTrigger::BankSetup, ZoogiComboTune::EBank, Mid);
	if (bShotTerrain && !bShotContact) ComboTrigger(ZoogiComboTrigger::TerrainSetup, ZoogiComboTune::ETerrain, Mid);
	const bool bMeAir = AirState.IsValidIndex(SI) && AirState[SI].bAir && AirState[SI].bTerrainLaunch;  // launched off ramp / lip / platform
	const bool bThemAir = OB && AirState.IsValidIndex(OB->PlayerIndex) && AirState[OB->PlayerIndex].bAir && AirState[OB->PlayerIndex].AirTime > 0.2f;
	if (bMeAir || bThemAir)
	{
		UE_LOG(LogZoogi, Log, TEXT("Airborne interception: P%d %s hits %s"), SI + 1, bMeAir ? TEXT("(airborne)") : TEXT(""), *TargetName(Other));
		ComboTrigger(ZoogiComboTrigger::AirborneSetup, ZoogiComboTune::EAirborne, Mid);
	}
	if (OO && !RelayOrb.IsValid() && !bInFollowUp) RelayOrb = OO;
	if (bInfusedContact || bPowerUsed) ComboTrigger(ZoogiComboTrigger::PowerSetup, ZoogiComboTune::EPower, Mid);
	bShotContact = true;
}

void AZoogiGameMode::ComboOnFloorChanged(AZoogiBall* By)
{
	if (!ComboArena) return;
	const EZoogiFloorState S = ComboArena->GetFloorState();
	const FVector C((AZoogiComboArena::SectionMin() + AZoogiComboArena::SectionMax()) * 0.5f, 40.f);
	const FString T = S == EZoogiFloorState::Damaged ? TEXT("FLOOR DAMAGED - SLOPE + RAISED EDGE") : TEXT("FLOOR CRACKED - RAISED LIP");
	ComboCallout(T, FLinearColor(1.f, 0.6f, 0.25f));
	AddPopup(S == EZoogiFloorState::Damaged ? TEXT("DAMAGED!") : TEXT("CRACKED!"), C + FVector(0, 0, 150.f), FLinearColor(1.f, 0.6f, 0.25f));
	AddFeed(FString::Printf(TEXT("%s  heavy impact: floor section %s"), By ? *By->Def.Name : TEXT("?"), *AZoogiComboArena::FloorStateName(S)), FLinearColor(1.f, 0.6f, 0.25f));
	if (By && By == GetActiveBall() && Phase == EZoogiPhase::Rolling) ComboTrigger(ZoogiComboTrigger::TerrainSetup, ZoogiComboTune::ETerrain, C);
	ComboShot(S == EZoogiFloorState::Damaged ? TEXT("floor_damaged") : TEXT("floor_cracked"), 0.7f);
}

// ------------------------------------------------------------------ tick: kicks, airborne, landings, edge saves

void AZoogiGameMode::ComboTick(float Dt)
{
	if (AirState.Num() != Balls.Num()) AirState.SetNum(Balls.Num());
	for (const FPendingKick& K : PendingKicks)
	{
		AZoogiBall* B = K.Ball.Get();
		if (!B || B->bOut) continue;
		const FVector V = B->GetBallVelocity();
		FVector P(V.X, V.Y, 0.f);
		const float Out = FVector::DotProduct(P, K.Dir);
		P += K.Dir * (K.Speed - FMath::Min(0.f, Out) * 0.f);
		if (P.Size() > 2600.f) P = P.GetSafeNormal() * 2600.f;
		B->SetPlanarVelocity(P);
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), FLinearColor(1.f, 0.4f, 0.8f), 120.f, 0.3f, false, 10.f);
		ZoogiUtil::PlaySfx(this, TEXT("Clack"), 0.8f, 0.7f);
	}
	PendingKicks.Reset();

	AZoogiBall* Shooter = GetActiveBall();
	if (BankLogAt > 0.f && Clock >= BankLogAt && Shooter)
	{
		BankLogAt = -1.f;
		const FVector V = Shooter->GetBallVelocity();
		UE_LOG(LogZoogi, Log, TEXT("Bank out: P%d at (%.0f, %.0f) %.0f cm/s heading %.1f deg"), Shooter->PlayerIndex + 1, Shooter->BallLocation().X, Shooter->BallLocation().Y, FVector2D(V).Size(), FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)));
	}
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiComboAir), false);
	for (AZoogiBall* B : Balls) if (B) Q.AddIgnoredActor(B);
	for (AZoogiOrb* O : Orbs) if (O) Q.AddIgnoredActor(O);
	for (int32 i = 0; i < Balls.Num(); ++i)
	{
		AZoogiBall* B = Balls[i];
		if (!B || B->bOut || !AirState.IsValidIndex(i)) continue;
		FBallAir& S = AirState[i];
		const FVector L = B->BallLocation();
		FHitResult H;
		float Height = 1000.f;
		FName Tag = NAME_None;
		if (GetWorld()->LineTraceSingleByObjectType(H, L, L - FVector(0, 0, 900.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			Height = L.Z - H.ImpactPoint.Z - ZoogiConst::ZoogiRadius;
			if (H.GetComponent() && H.GetComponent()->ComponentTags.Num()) Tag = H.GetComponent()->ComponentTags[0];
		}
		const bool bAirNow = Height > ZoogiComboTune::AirborneHeight;
		const FVector V = B->GetBallVelocity();
		if (bAirNow && !S.bAir)
		{
			S.AirTime = 0.f;
			const bool bFromTerrain = S.GroundTag == TEXT("ZRamp") || S.GroundTag == TEXT("ZFloor") || S.GroundTag == TEXT("ZPlatform");
			if (bFromTerrain && Height < 400.f && FVector2D(V.X, V.Y).Size() > 300.f)
			{
				S.bTerrainLaunch = true;
				const FString What = S.GroundTag == TEXT("ZRamp") ? TEXT("LAUNCH RAMP") : (S.GroundTag == TEXT("ZFloor") ? TEXT("CRACKED LIP") : TEXT("PLATFORM EDGE"));
				AddPopup(TEXT("AIRBORNE!"), L + FVector(0, 0, 130.f), FLinearColor(0.6f, 0.9f, 1.f));
				UE_LOG(LogZoogi, Log, TEXT("Airborne: P%d off the %s at %.0f cm/s (vz %.0f)"), i + 1, *What, FVector2D(V.X, V.Y).Size(), V.Z);
				if (B == Shooter && Phase == EZoogiPhase::Rolling && !bShotContact) { bShotTerrain = true; ShotTerrainWhat = What; }
				ComboShot(FString::Printf(TEXT("airborne_%s"), *What.Replace(TEXT(" "), TEXT("_")).ToLower()), 0.2f);
			}
		}
		if (!bAirNow && S.bAir)
		{
			UE_LOG(LogZoogi, Log, TEXT("Landing: P%d vz %.0f cm/s after %.2f s in the air%s"), i + 1, S.PrevVz, S.AirTime, ComboArena && ComboArena->IsInSection(FVector2D(L)) ? TEXT(" on the floor section") : TEXT(""));
			if (S.PrevVz < -200.f && ComboArena && ComboArena->IsInSection(FVector2D(L), 40.f))
			{
				if (ComboArena->FloorImpact(L, V.GetSafeNormal2D(), -S.PrevVz)) ComboOnFloorChanged(B);
			}
			// Frozen Ring reactive floor: a hard landing scars the ice where it lands (visual only), and the heavy landing
			// shakes the stage pieces next to it (power-reactive: Ember / Titan landings hit them harder).
			else if (S.PrevVz < -250.f && Arena && Arena->GetScars() && FVector2D(L).Size() < ZoogiConst::KnockoffRadius - 60.f)
			{
				const EZoogiArtifact E = (B->Artifact && B->Artifact->Infused != EZoogiArtifact::None) ? B->Artifact->Infused : EZoogiArtifact::None;
				Arena->GetScars()->Impact(FVector(L.X, L.Y, 0.f), -S.PrevVz / 350.f, E);
				AZoogiArenaPiece::DamageArea(GetWorld(), FVector(L.X, L.Y, 0.f), 180.f, FMath::Clamp((-S.PrevVz - 250.f) / 25.f, 4.f, 30.f), E, B);
			}
		}
		if (bAirNow) S.AirTime += Dt;
		S.bAir = bAirNow;
		S.PrevVz = V.Z;
		if (!bAirNow) S.GroundTag = Tag;
		// Guard Reaction: catch the next push over the rim this turn.
		if (B->bEdgeSave && Phase == EZoogiPhase::Rolling)
		{
			const FVector2D P(L.X, L.Y);
			const float R = P.Size();
			if (R > ZoogiConst::FloorRadius - 90.f && R > 1.f && FVector2D::DotProduct(FVector2D(V.X, V.Y), P / R) > 100.f)
			{
				B->bEdgeSave = false;
				B->SetPlanarVelocity(FVector(-P.X / R, -P.Y / R, 0.f) * 220.f);
				AddPopup(TEXT("GUARD: EDGE SAVE"), L + FVector(0, 0, 140.f), FLinearColor(0.5f, 1.f, 0.8f));
				ComboCallout(FString::Printf(TEXT("GUARD REACTION: %s HOLDS THE EDGE"), *B->Def.Name.ToUpper()), FLinearColor(0.5f, 1.f, 0.8f));
				AZoogiFX::Spawn(GetWorld(), L, FLinearColor(0.5f, 1.f, 0.8f), 160.f, 0.5f, true, 10.f);
				UE_LOG(LogZoogi, Log, TEXT("Guard Reaction edge save: P%d stopped at the rim"), i + 1);
				ComboShot(TEXT("guard_edge_save"), 0.15f);
			}
		}
	}
	for (FZoogiCallout& C : Callouts) C.Age += Dt;
	Callouts.RemoveAll([](const FZoogiCallout& C) { return C.Age > 4.5f; });
	if (ComboDemo > 0) TickComboDemo(Dt);
}

// ------------------------------------------------------------------ ring-outs and ORB OUT

void AZoogiGameMode::ComboCheckKnockoffs()
{
	auto Edge = [this](const FVector& L)
	{
		FVector2D P(L.X, L.Y);
		if (P.Size() < 1.f) P = FVector2D(1, 0);
		P = P.GetSafeNormal() * (ZoogiConst::KnockoffRadius - 60.f);
		return FVector(P.X, P.Y, 120.f);
	};
	for (int32 i = 0; i < Orbs.Num(); ++i)
	{
		AZoogiOrb* O = Orbs[i];
		if (!O || O->bOut) continue;
		const FVector L = O->OrbLocation();
		if (!(L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f)) continue;
		const int32 Scorer = Balls.IsValidIndex(O->LastHitBy) ? O->LastHitBy : ActivePlayer;
		const int32 Team = TeamOfIndex(Scorer);
		const bool bRelayFinish = bInFollowUp && FollowRelayOrb.Get() == O && Team == ComboTeam;
		O->SetOut();
		AZoogiFX::Spawn(GetWorld(), Edge(L), O->bStar ? FLinearColor(1.f, 0.75f, 0.2f) : FLinearColor(0.5f, 0.85f, 1.f), 180.f, 0.6f, false, 8.f);
		AZoogiFX::Niagara(GetWorld(), O->bStar ? TEXT("NS_Burst_Gold") : TEXT("NS_Burst_Blue"), Edge(L));
		int32 Pts = (O->bStar ? 2 : 1) * ZoogiComboTune::OrbScore + (bRelayFinish ? ZoogiComboTune::RelayFinishBonus : 0);
		FZoogiLedgerEntry E;
		E.Player = Scorer; E.Points = Pts; E.bStar = O->bStar;
		Ledger.Add(E);
		ComboResonance[Team]++;
		ArenaControl[Team] += ZoogiComboTune::ControlOrbOut;
		ComboOrbOuts[Team]++;
		ComboAddEnergy(Team, ZoogiComboTune::EOrbOut, TEXT("ORB OUT"));
		ZoogiUtil::PlaySfx(this, TEXT("Score"), 1.f, O->bStar ? 1.25f : 1.f);
		AddPopup(FString::Printf(TEXT("ORB OUT  +%d"), Pts), Edge(L), TeamColor(Team));
		AddPopup(FString::Printf(TEXT("+1 RESONANCE   +%.0f ARENA CONTROL"), ZoogiComboTune::ControlOrbOut), Edge(L) + FVector(0, 0, 70.f), FLinearColor(0.75f, 0.6f, 1.f));
		ComboCallout(FString::Printf(TEXT("ORB OUT!  +%d   RESONANCE %d   CONTROL %.0f"), Pts, ComboResonance[Team], ArenaControl[Team]), TeamColor(Team));
		if (bRelayFinish) ComboCallout(FString::Printf(TEXT("RELAY FINISH!  +%d"), ZoogiComboTune::RelayFinishBonus), FLinearColor(1.f, 0.85f, 0.3f));
		AddFeed(FString::Printf(TEXT("P%d %s  ORB OUT  +%d  (+1 resonance, +%.0f control)%s"), Scorer + 1, Balls.IsValidIndex(Scorer) ? *Balls[Scorer]->Def.Name : TEXT("?"), Pts, ZoogiComboTune::ControlOrbOut, bRelayFinish ? TEXT("  RELAY FINISH") : TEXT("")), TeamColor(Team));
		UE_LOG(LogZoogi, Log, TEXT("ORB OUT: orb %d by P%d (%s) +%d%s -> resonance %d, arena control %.0f (respawns at Resolve)"), i, Scorer + 1, *TeamName(Team), Pts, bRelayFinish ? TEXT(" RELAY FINISH") : TEXT(""), ComboResonance[Team], ArenaControl[Team]);
		ComboShot(bRelayFinish ? TEXT("relay_finish_orb_out") : FString::Printf(TEXT("orb_out_%d"), ComboOrbOuts[0] + ComboOrbOuts[1]), 0.35f);
	}
	for (AZoogiBall* B : Balls)
	{
		if (!B || B->bOut) continue;
		const FVector L = B->BallLocation();
		if (!(L.Z < ZoogiConst::FallOutZ || FVector2D(L.X, L.Y).Size() > 8000.f)) continue;
		AZoogiFX::Spawn(GetWorld(), Edge(L), FLinearColor(1.f, 0.3f, 0.25f), 220.f, 0.7f, false, 7.f);
		AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), Edge(L), FRotator::ZeroRotator, 1.4f);
		B->SetOut();
		ZoogiUtil::PlaySfx(this, TEXT("Fall"), 0.7f);
		B->RingMarks = FMath::Max(0, B->RingMarks - 1);
		const int32 Att = (Balls.IsValidIndex(B->LastHitPlayer) && TeamOfIndex(B->LastHitPlayer) != B->TeamIndex) ? B->LastHitPlayer : -1;
		if (Att >= 0)
		{
			FZoogiLedgerEntry E;
			E.Player = Att; E.Points = ZoogiComboTune::RingOutScore; E.bKO = true; E.Victim = B->PlayerIndex;
			Ledger.Add(E);
			ArenaControl[TeamOfIndex(Att)] += ZoogiComboTune::ControlRingOut;
			ComboAddEnergy(TeamOfIndex(Att), ZoogiComboTune::ERingOut, TEXT("RING OUT"));
			if (AZoogiPlayerController* PCtl = Cast<AZoogiPlayerController>(GetWorld()->GetFirstPlayerController())) PCtl->OnImpact(1.f);
		}
		const FString Who = FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, *B->Def.Name);
		const FString Marks = B->RingMarks >= 2 ? TEXT("<> <>") : (B->RingMarks == 1 ? TEXT("<> []") : TEXT("[] []"));
		if (B->RingMarks > 0)
		{
			B->bPendingRecovery = true;
			BigText = FString::Printf(TEXT("RING OUT!\n%s  -  RING MARK LOST  -  RECOVERY GATE"), *B->Def.Name.ToUpper());
			ComboCallout(FString::Printf(TEXT("RING OUT  %s  %s  -> RECOVERY GATE"), *Who, *Marks), FLinearColor(1.f, 0.45f, 0.35f));
		}
		else
		{
			B->bKnockedOut = true;
			BigText = FString::Printf(TEXT("KNOCKED OUT!\n%s IS OUT FOR THE ROUND"), *B->Def.Name.ToUpper());
			ComboCallout(FString::Printf(TEXT("KNOCKED OUT  %s  %s"), *Who, *Marks), FLinearColor(1.f, 0.3f, 0.25f));
			if (TeamAlive(B->TeamIndex) == 0 && !bComboMatchOver)
			{
				bComboMatchOver = true;
				ComboWinner = 1 - B->TeamIndex;
				ComboOverReason = FString::Printf(TEXT("both %s Zoogis knocked out"), *TeamName(B->TeamIndex));
			}
		}
		BigColor = Att >= 0 ? ZoogiUtil::PlayerColor(Att) : FLinearColor(1.f, 0.4f, 0.35f);
		BigAge = 0.f;
		AddPopup(FString::Printf(TEXT("RING OUT  %s"), *Marks), Edge(L) + FVector(0, 0, 60.f), FLinearColor(1.f, 0.4f, 0.35f));
		AddFeed(FString::Printf(TEXT("%s  rung out%s  - %d Ring Mark%s left"), *Who, Att >= 0 ? *FString::Printf(TEXT(" by P%d"), Att + 1) : TEXT(""), B->RingMarks, B->RingMarks == 1 ? TEXT("") : TEXT("s")), FLinearColor(1.f, 0.45f, 0.35f));
		UE_LOG(LogZoogi, Log, TEXT("RING OUT: %s (%s) by %s -> Ring Marks %d %s"), *Who, *TeamName(B->TeamIndex), Att >= 0 ? *FString::Printf(TEXT("P%d"), Att + 1) : TEXT("self / ally"), B->RingMarks,
			B->RingMarks > 0 ? TEXT("(Recovery Gate returns it at Resolve)") : TEXT("KNOCKED OUT for the round"));
		ComboShot(FString::Printf(TEXT("ringout_p%d_marks%d"), B->PlayerIndex + 1, B->RingMarks), 0.3f);
	}
}

// ------------------------------------------------------------------ Resolve

void AZoogiGameMode::ComboResolve()
{
	AZoogiBall* Shooter = GetActiveBall();
	// Orb Relay: the shooter sent an orb that ended at (or touched) the partner.
	if (!bInFollowUp && RelayOrb.IsValid() && !RelayOrb->bOut && Shooter)
	{
		AZoogiBall* P = PartnerOf(Shooter);
		const float D = P && !P->bOut ? FVector::Dist2D(P->BallLocation(), RelayOrb->OrbLocation()) : 1e6f;
		if (P && !P->bOut && (bRelayTouched || D < ZoogiComboTune::RelayRadius))
		{
			UE_LOG(LogZoogi, Log, TEXT("ORB RELAY: orb ends %.0f cm from partner P%d%s"), D, P->PlayerIndex + 1, bRelayTouched ? TEXT(" (touched)") : TEXT(""));
			Phase = EZoogiPhase::Rolling; // ComboTrigger is phase agnostic; keep the feed / callouts identical
			ComboTrigger(ZoogiComboTrigger::OrbRelay, ZoogiComboTune::ERelay, RelayOrb->OrbLocation());
			Phase = EZoogiPhase::Resolving;
		}
	}
	// Recovery Gate returns Zoogis that still have a Ring Mark.
	for (AZoogiBall* B : Balls)
	{
		if (!B || !B->bPendingRecovery) continue;
		B->bPendingRecovery = false;
		if (AirState.IsValidIndex(B->PlayerIndex)) AirState[B->PlayerIndex] = FBallAir();
		const FVector P = FindFreeSpot(AZoogiComboArena::GateReturn());
		B->PlaceAt(P, 90.f);
		B->LastHitPlayer = -1;
		if (ComboArena) ComboArena->GateFlash();
		AddPopup(TEXT("RECOVERY GATE"), P + FVector(0, 0, 150.f), FLinearColor(0.4f, 1.f, 0.65f));
		ComboCallout(FString::Printf(TEXT("RECOVERY GATE RETURNS %s  (1 RING MARK)"), *B->Def.Name.ToUpper()), FLinearColor(0.4f, 1.f, 0.65f));
		UE_LOG(LogZoogi, Log, TEXT("Recovery Gate: P%d %s returned at (%.0f, %.0f) with %d Ring Mark"), B->PlayerIndex + 1, *B->Def.Name, P.X, P.Y, B->RingMarks);
		ComboShot(FString::Printf(TEXT("recovery_gate_p%d"), B->PlayerIndex + 1), 0.6f);
	}
	// Neutral Arena Orbs respawn (never destroyed).
	for (int32 i = 0; i < Orbs.Num(); ++i)
	{
		AZoogiOrb* O = Orbs[i];
		if (!O || !O->bOut) continue;
		const FVector Home = ComboOrbHome.IsValidIndex(i) ? ComboOrbHome[i] : FVector(0, 0, ZoogiConst::OrbRadius + 1.f);
		FVector P = FindFreeSpot(Home);
		P.Z = ZoogiConst::OrbRadius + 1.f;
		O->Respawn(P);
		AZoogiFX::Spawn(GetWorld(), P, FLinearColor(0.5f, 0.85f, 1.f), 120.f, 0.5f, true, 8.f);
		AddPopup(TEXT("ORB RESPAWN"), P + FVector(0, 0, 100.f), FLinearColor(0.5f, 0.85f, 1.f));
		UE_LOG(LogZoogi, Log, TEXT("Arena Orb %d respawned at (%.0f, %.0f)"), i, P.X, P.Y);
	}
	for (int32 t = 0; t < 2; ++t)
	{
		// Resonance Energy: every 3 -> +1 artifact draw for the team (Awaken integration).
		while (ComboResonance[t] >= ZoogiComboTune::ResonanceSurge)
		{
			ComboResonance[t] -= ZoogiComboTune::ResonanceSurge;
			for (AZoogiBall* B : Balls) if (B && B->TeamIndex == t && !B->bKnockedOut && B->Artifact) B->Artifact->BonusDraws = FMath::Min(2, B->Artifact->BonusDraws + 1);
			ComboCallout(FString::Printf(TEXT("%s RESONANCE SURGE: +1 ARTIFACT DRAW"), *TeamName(t)), FLinearColor(0.75f, 0.6f, 1.f));
			UE_LOG(LogZoogi, Log, TEXT("Resonance surge: %s +1 artifact draw"), *TeamName(t));
		}
		// Resonance Shift: the arena changes once a team reaches 50 Arena Control.
		if (ArenaControl[t] >= ZoogiComboTune::ShiftAtControl && ComboArena && !ComboArena->IsShifted())
		{
			ComboArena->ResonanceShift();
			BigText = FString::Printf(TEXT("RESONANCE SHIFT!\n%s ARENA CONTROL - THE BUMPERS SWELL"), *TeamName(t));
			BigColor = FLinearColor(0.65f, 0.4f, 1.f);
			BigAge = 0.f;
			ComboCallout(TEXT("RESONANCE SHIFT: BUMPERS SWELL + KICK HARDER"), FLinearColor(0.65f, 0.4f, 1.f));
			AddFeed(FString::Printf(TEXT("%s reached %.0f Arena Control: RESONANCE SHIFT"), *TeamName(t), ArenaControl[t]), FLinearColor(0.65f, 0.4f, 1.f));
			ComboShot(TEXT("resonance_shift"), 1.0f);
		}
		if (ArenaControl[t] >= 100.f)
		{
			ArenaControl[t] -= 100.f;
			ComboAddEnergy(t, 30.f, TEXT("ARENA CONTROL"));
			ComboCallout(FString::Printf(TEXT("%s ARENA CONTROL!  +30 COMBO"), *TeamName(t)), TeamColor(t));
		}
	}
	// Partner Follow-Up offer.
	OfferTriggers = ShotTriggers;
	bFollowUpOffered = false;
	if (!bComboMatchOver && !bInFollowUp && !bFollowUpUsed && ShotTriggers != 0 && Shooter)
	{
		AZoogiBall* P = PartnerOf(Shooter);
		const bool bPartnerOk = P && !P->bKnockedOut && !P->bOut && P->StunTurns <= 0;
		if (bPartnerOk && ComboEnergy[ComboTeam] >= ZoogiComboTune::FollowUpCost)
		{
			bFollowUpOffered = true;
			FollowUpOfferAt = Clock;
			ComboCallout(FString::Printf(TEXT("PARTNER FOLLOW-UP READY  (%s)"), *TriggerNames(ShotTriggers)), FLinearColor(1.f, 0.85f, 0.3f));
		}
		UE_LOG(LogZoogi, Log, TEXT("COMBO resolve: setup %s, %s gauge %.0f / cost %.0f, partner %s -> follow-up %s"), *TriggerNames(ShotTriggers), *TeamName(ComboTeam), ComboEnergy[ComboTeam], ZoogiComboTune::FollowUpCost,
			bPartnerOk ? TEXT("ready") : TEXT("unavailable"), bFollowUpOffered ? TEXT("OFFERED") : TEXT("not offered"));
	}
	else
	{
		UE_LOG(LogZoogi, Log, TEXT("COMBO resolve: %s shot, triggers [%s], gauge A %.0f B %.0f%s"), bInFollowUp ? TEXT("follow-up") : TEXT("main"), *TriggerNames(ShotTriggers), ComboEnergy[0], ComboEnergy[1], bComboMatchOver ? TEXT(", MATCH OVER") : TEXT(""));
	}
	{
		FString Pos;
		for (const AZoogiBall* B : Balls) if (B) Pos += FString::Printf(TEXT(" P%d(%.0f,%.0f)%s"), B->PlayerIndex + 1, B->BallLocation().X, B->BallLocation().Y, B->bOut ? TEXT("out") : TEXT(""));
		UE_LOG(LogZoogi, Log, TEXT("COMBO positions:%s"), *Pos);
		if (ComboDemo > 0) ComboShot(FString::Printf(TEXT("t%d%s_resolve"), TurnCounter, bInFollowUp ? TEXT("fu") : TEXT("")), 0.2f);
	}
	if (ShotTriggers) ResolveNotes += FString::Printf(TEXT("%s%s"), ResolveNotes.IsEmpty() ? TEXT("") : TEXT("     "), *TriggerNames(ShotTriggers));
}

bool AZoogiGameMode::ComboHoldResolve()
{
	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	const bool bBusy = (Anime && Anime->IsBusy()) || ClimaxLeft > 0.f;
	if (bComboMatchOver)
	{
		if (StageTime >= ZoogiStageTune::ResolveMin + 0.8f && !bBusy) ComboFinish(ComboWinner, ComboOverReason);
		return true;
	}
	if (bFollowUpOffered)
	{
		AZoogiBall* P = PartnerOf(GetActiveBall());
		if (ComboDemo > 0) return true; // the demo director decides
		if (P && P->bCPU && StageTime >= 1.1f && !bBusy) ComboAcceptFollowUp();
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ Guard Reaction / Guard Break

bool AZoogiGameMode::ComboGuardBreak(AZoogiBall* Victim, AZoogiBall* Attacker)
{
	if (!Victim || !Attacker || !Victim->Artifact || Attacker != GetActiveBall()) return false;
	if (!(ComboShotForce == EZoogiForce::Launch || bLastHitCrit)) return false;
	const EZoogiArtifact G = Victim->Artifact->Guard;
	Victim->Artifact->ClearGuard();
	RefreshArtifactMass(Victim, false);
	AZoogiFX::Spawn(GetWorld(), Victim->BallLocation(), FLinearColor(1.f, 0.35f, 0.2f), 200.f, 0.45f, false, 14.f, Victim);
	AddPopup(TEXT("GUARD BREAK!"), Victim->BallLocation() + FVector(0, 0, 160.f), FLinearColor(1.f, 0.4f, 0.25f));
	UE_LOG(LogZoogi, Log, TEXT("GUARD BREAK: P%d %s's %s guard shattered by P%d (%s%s)"), Victim->PlayerIndex + 1, *Victim->Def.Name, *ZoogiArtifacts::Name(G), Attacker->PlayerIndex + 1, ForceName(ComboShotForce), bLastHitCrit ? TEXT(", crit") : TEXT(""));
	ComboTrigger(ZoogiComboTrigger::GuardBreak, ZoogiComboTune::EGuardBreak, Victim->BallLocation());
	return true;
}

void AZoogiGameMode::ComboGuardReaction(AZoogiBall* Victim, AZoogiBall* Attacker)
{
	if (!Victim || !Victim->Artifact) return;
	// Reduce knockback (done by the default guard damping), stabilise at the rim, break a follow-up; the guard is consumed.
	Victim->bEdgeSave = true;
	ComboAddEnergy(Victim->TeamIndex, ZoogiComboTune::EGuardReaction, TEXT("GUARD REACTION"));
	FString Extra;
	if (bInFollowUp && Attacker && Attacker == GetActiveBall())
	{
		bFollowUpBroken = true;
		ComboEnergy[ComboTeam] = FMath::Max(0.f, ComboEnergy[ComboTeam] - 10.f);
		const FVector AV = Attacker->GetBallVelocity();
		Attacker->SetPlanarVelocity(FVector(AV.X, AV.Y, 0.f) * 0.4f - (Victim->BallLocation() - Attacker->BallLocation()).GetSafeNormal2D() * 300.f);
		Extra = TEXT("  FOLLOW-UP BROKEN");
	}
	ComboCallout(FString::Printf(TEXT("GUARD REACTION: %s%s"), *Victim->Def.Name.ToUpper(), *Extra), FLinearColor(0.5f, 1.f, 0.8f));
	UE_LOG(LogZoogi, Log, TEXT("GUARD REACTION: P%d %s (knockback damped, edge save armed%s), guard consumed"), Victim->PlayerIndex + 1, *Victim->Def.Name, *Extra);
	Victim->Artifact->ClearGuard();
	RefreshArtifactMass(Victim, false);
	ComboShot(TEXT("guard_reaction"), 0.2f);
}

// ------------------------------------------------------------------ demo director (-ZoogiComboDemo=N)
// Plays every Zoogi through the same calls the HUD buttons / keys use (select, aim, force, confirm, follow-up).

void AZoogiGameMode::DemoStage()
{
	// Test fixture: starting positions for the scripted sequence (logged so the runs are reproducible).
	auto Put = [this](int32 i, float X, float Y) { if (Balls.IsValidIndex(i) && Balls[i]) { Balls[i]->PlaceAt(FVector(X, Y, ZoogiConst::ZoogiRadius + 2.f), FMath::RadiansToDegrees(FMath::Atan2(-Y, -X))); Balls[i]->SpawnLocation = FVector(X, Y, ZoogiConst::ZoogiRadius + 2.f); } };
	auto PutOrb = [this](int32 i, float X, float Y) { if (Orbs.IsValidIndex(i) && Orbs[i]) { const FVector L(X, Y, ZoogiConst::OrbRadius + 1.f); Orbs[i]->Respawn(L); if (ComboOrbHome.IsValidIndex(i)) ComboOrbHome[i] = L; } };
	if (ComboDemo == 1)
	{
		Put(0, -900.f, 100.f); Put(1, -700.f, -640.f); Put(2, 250.f, -1150.f); Put(3, 900.f, 300.f);
		PutOrb(3, 100.f, 850.f); PutOrb(5, 650.f, 100.f);
	}
	else if (ComboDemo == 2)
	{
		Put(0, -1100.f, 200.f); Put(1, -455.f, -427.f); Put(2, 1000.f, -100.f); Put(3, 800.f, 500.f);
		PutOrb(0, -700.f, -50.f); PutOrb(2, 250.f, -250.f);
	}
	else if (ComboDemo == 3)
	{
		Put(0, -1100.f, 400.f); Put(1, -1100.f, 250.f); Put(2, 700.f, 450.f); Put(3, 900.f, -300.f);
	}
	UE_LOG(LogZoogi, Log, TEXT("ComboDemo %d: staged start positions"), ComboDemo);
}

bool AZoogiGameMode::DemoAimBankInto(AZoogiBall* Into, float AngFrom, float AngTo, float R)
{
	AZoogiBall* Me = GetActiveBall();
	if (!Me || !Into) return false;
	const float Z = Me->BallLocation().Z - ZoogiConst::ZoogiRadius;
	float BestA = -1000.f, BestCut = 1000.f;
	for (float A = AngFrom; A <= AngTo; A += 0.25f)
	{
		bFreeAim = true;
		TargetLock.Reset();
		AimCutDeg = 0.f;
		FreeAimPoint = FVector(FMath::Cos(FMath::DegreesToRadians(A)) * R, FMath::Sin(FMath::DegreesToRadians(A)) * R, Z);
		UpdateLockAim();
		UpdatePreview();
		if (Preview.bBank && Preview.bContact && Preview.ContactActor.Get() == Into && Preview.CutDeg < BestCut) { BestCut = Preview.CutDeg; BestA = A; }
	}
	bFreeAim = false;
	if (BestA < -999.f) { UE_LOG(LogZoogi, Warning, TEXT("ComboDemo: no bank line into P%d"), Into->PlayerIndex + 1); return false; }
	SetFreeAim(FVector(FMath::Cos(FMath::DegreesToRadians(BestA)) * R, FMath::Sin(FMath::DegreesToRadians(BestA)) * R, Z));
	UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d aims at the curved wall (%.2f deg) - preview: bank, then contact P%d at cut %.0f deg"), ActivePlayer + 1, BestA, Into->PlayerIndex + 1, BestCut);
	return true;
}

namespace
{
	/** Ring-out line score for Shooter -> Target with an aim cut (lower is better, >= 1e5 = not playable). */
	float RingOutScore(const AZoogiGameMode* G, const AZoogiBall* Me, const AActor* T, float Cut, float Speed)
	{
		const FVector2D S(Me->BallLocation()), P(CPos(T));
		const float TR = Cast<AZoogiOrb>(T) ? ZoogiConst::OrbRadius : ZoogiConst::ZoogiRadius;
		const float RR = ZoogiConst::ZoogiRadius + TR;
		const FVector2D D0 = (P - S).GetSafeNormal();
		const FVector2D D = D0.GetRotated(Cut);
		const float Along = FVector2D::DotProduct(P - S, D);
		const float Perp2 = (P - S).SizeSquared() - Along * Along;
		if (Along <= 0.f || Perp2 > FMath::Square(RR - 6.f)) return 1e6f;
		const FVector2D Ghost = S + D * (Along - FMath::Sqrt(FMath::Max(0.f, RR * RR - Perp2)));
		const FVector2D N = (P - Ghost).GetSafeNormal();
		const float CosC = FVector2D::DotProduct(D, N);
		float Score = G->Arena ? G->Arena->DistanceToEdge(P, N) : 1000.f;
		auto SegBlock = [](const FVector2D& A, const FVector2D& B, const FVector2D& Q, float Rad)
		{
			const FVector2D AB = B - A;
			const float L2 = AB.SizeSquared();
			const float Tt = L2 > 1.f ? FMath::Clamp(FVector2D::DotProduct(Q - A, AB) / L2, 0.f, 1.f) : 0.f;
			return FVector2D::Distance(A + AB * Tt, Q) < Rad;
		};
		const FVector2D Exit = P + N * (Score + 60.f);
		// Blockers: other Zoogis / orbs on either leg, rim rocks, combo props.
		for (const AZoogiBall* B : G->Balls)
		{
			if (!B || B == Me || B == T || B->bOut) continue;
			if (SegBlock(S, Ghost, FVector2D(B->BallLocation()), ZoogiConst::ZoogiRadius * 2.f + 5.f) || SegBlock(P, Exit, FVector2D(B->BallLocation()), RR + 10.f)) Score += 5000.f;
		}
		for (const AZoogiOrb* O : G->Orbs)
		{
			if (!O || O == T || O->bOut) continue;
			if (SegBlock(S, Ghost, FVector2D(O->OrbLocation()), ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 5.f) || SegBlock(P, Exit, FVector2D(O->OrbLocation()), TR + ZoogiConst::OrbRadius + 10.f)) Score += 5000.f;
		}
		if (G->Arena)
			for (const AZoogiRock* Rk : G->Arena->GetRocks())
				if (Rk && Rk->IsStanding() && SegBlock(P, Exit, FVector2D(Rk->GetActorLocation()), Rk->GetRadius() + TR + 15.f)) Score += 5000.f;
		if (G->Arena)
			for (const AZoogiArenaPiece* Pc : G->Arena->GetPieces())
			{
				FVector2D PXY; float PR = 0.f;
				if (Pc && Pc->GetPathBlock(PXY, PR) && (SegBlock(S, Ghost, PXY, PR + ZoogiConst::ZoogiRadius + 5.f) || SegBlock(P, Exit, PXY, PR + TR + 10.f))) Score += 5000.f;
			}
		if (G->ComboArena)
		{
			for (const FVector2D& Bp : AZoogiComboArena::BumperXY())
				if (SegBlock(S, Ghost, Bp, G->ComboArena->BumperRadius() + ZoogiConst::ZoogiRadius + 10.f) || SegBlock(P, Exit, Bp, G->ComboArena->BumperRadius() + TR + 10.f)) Score += 5000.f;
			if (SegBlock(S, Ghost, AZoogiComboArena::PlatformXY(), AZoogiComboArena::PlatformR + 60.f) || SegBlock(P, Exit, AZoogiComboArena::PlatformXY(), AZoogiComboArena::PlatformR + TR + 10.f)) Score += 5000.f;
			for (float A = AZoogiComboArena::WallA0; A <= AZoogiComboArena::WallA1; A += 5.f)
			{
				const FVector2D W(FMath::Cos(FMath::DegreesToRadians(A)) * (AZoogiComboArena::WallR + 20.f), FMath::Sin(FMath::DegreesToRadians(A)) * (AZoogiComboArena::WallR + 20.f));
				if (SegBlock(S, Ghost, W, 80.f) || SegBlock(P, Exit, W, 80.f)) { Score += 5000.f; break; }
			}
			const FVector2D RC = (AZoogiComboArena::RampMin() + AZoogiComboArena::RampMax()) * 0.5f;
			if (SegBlock(S, Ghost, RC, 190.f) || SegBlock(P, Exit, RC, 190.f)) Score += 2000.f;
		}
		// Push transfer falls off with the cut; the shooter's tangential coast must not carry it off the edge.
		const float Decel = G->Arena ? G->Arena->GetRollingDecel() : ZoogiConst::RollingDecel;
		const float VHit = FMath::Sqrt(FMath::Max(0.f, Speed * Speed - 2.f * Decel * FVector2D::Distance(S, Ghost)));
		const float Need = FMath::Sqrt(2.f * Decel * (Score < 4000.f ? Score + 200.f : 4000.f));
		if (VHit * CosC * 1.3f < Need) Score += 2500.f;
		const FVector2D Tang = (D - CosC * N).GetSafeNormal();
		const float VT = VHit * FMath::Sqrt(FMath::Max(0.f, 1.f - CosC * CosC)) + VHit * 0.15f;
		const float TEdge = G->Arena ? G->Arena->DistanceToEdge(Ghost, Tang.IsNearlyZero() ? D : Tang) : 1000.f;
		if (VT * VT / (2.f * Decel) > TEdge - 150.f) Score += 4000.f;
		// Follow-through: the shooter keeps ~18% of the normal speed (heavier LAUNCH); it must stop before the rim.
		const float VF = VHit * CosC * 0.18f;
		const float FEdge = G->Arena ? G->Arena->DistanceToEdge(Ghost, N) : 1000.f;
		if (VF * VF / (2.f * Decel) + 60.f > FEdge - 140.f) Score += 4000.f;
		return Score + FMath::Abs(Cut) * 6.f;
	}
}

bool AZoogiGameMode::DemoAimRingOut(AActor* Target, EZoogiForce F)
{
	AZoogiBall* Me = GetActiveBall();
	if (!Me || !IsValidTarget(Target)) return false;
	ComboForce = F;
	const float Frac = ComboForceFraction(0.4f);
	const float Speed = ZoogiConst::LaunchSpeedForPull(Frac) * ComboShotMul(Me);
	float BestCut = 0.f, Best = 1e9f;
	for (float C = -30.f; C <= 30.f; C += 2.f)
	{
		const float S = RingOutScore(this, Me, Target, C, Speed);
		if (S < Best) { Best = S; BestCut = C; }
	}
	LockOn(Target, false);
	AimCutDeg = BestCut;
	ComboSetForce(F);
	UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d locks %s, %s, cut %+.0f deg (line score %.0f)"), ActivePlayer + 1, *TargetName(Target), ForceName(F), BestCut, Best);
	return Best < 4000.f;
}

void AZoogiGameMode::TickComboDemo(float Dt)
{
	DemoClock += Dt;
	AZoogiBall* Me = GetActiveBall();
	if (!Me || Phase == EZoogiPhase::GameOver) return;
	const int32 Key = TurnCounter * 2 + (bInFollowUp ? 1 : 0);
	const bool bTeamA = ComboTeam == 0;
	const int32 T = TurnCounter; // team turn number (1-based)

	// Awaken: Team B guards in demo 1 (Guard Reaction / Guard Break), everyone else saves.
	if (Phase == EZoogiPhase::Aiming && Stage == EZoogiStage::Awaken && !bAwakenDone && StageTime > 0.5f)
	{
		if (ComboDemo == 1 && !bTeamA)
		{
			if (T == 2 && Balls.IsValidIndex(2) && !Balls[2]->bKnockedOut && ActivePlayer != 2) ComboSelect(2);
			ComboShot(FString::Printf(TEXT("t%d_awaken"), T));
			AwakenGuard(0);
		}
		else AwakenSave();
		return;
	}
	// Resolve: the Partner Follow-Up prompt appears (after any anime card) -> capture it -> accept.
	if (Phase == EZoogiPhase::Resolving && bFollowUpOffered)
	{
		UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
		if ((Anime && Anime->IsBusy()) || ClimaxLeft > 0.f || Clock - FollowUpOfferAt < 0.8f) { DemoFUAt = -1.f; return; }
		if (DemoFUAt < 0.f) { DemoFUAt = Clock; ComboShot(FString::Printf(TEXT("t%d_followup_prompt"), T), 0.25f); }
		else if (Clock - DemoFUAt > 1.0f) { DemoFUAt = -1.f; ComboAcceptFollowUp(); }
		return;
	}
	if (!IsAimStep()) return;
	if (DemoSeenTurn != Key)
	{
		if (StepTime < 0.3f) return;
		DemoSeenTurn = Key;
		bDemoShotFired = false;
		DemoStep = 0;
		auto Enemies = [this]() { TArray<AZoogiBall*> E; for (AZoogiBall* B : Balls) if (B && B->TeamIndex != ComboTeam && !B->bOut && !B->bKnockedOut) E.Add(B); return E; };
		auto Harmless = [this]()
		{
			AZoogiBall* B = GetActiveBall();
			const FVector L = B->BallLocation();
			const FVector P = L + (FVector(0, 0, L.Z) - L).GetSafeNormal2D() * 180.f;
			SetFreeAim(FVector(P.X, P.Y, L.Z - ZoogiConst::ZoogiRadius));
			ComboSetForce(EZoogiForce::Control);
		};
		// Best ring-out among both teammates (switching the selection if the partner has the better line).
		auto RingOutBest = [this, &Enemies](EZoogiForce F, bool bAllowSwitch)
		{
			TArray<AZoogiBall*> E = Enemies();
			TArray<AActor*> Targets;
			for (AZoogiBall* B : E) Targets.Add(B);
			float Best = 1e9f; int32 BestShooter = ActivePlayer; AActor* BestT = nullptr;
			const float Speed = ZoogiConst::LaunchSpeedForPull(F == EZoogiForce::Launch ? ZoogiComboTune::LaunchFrac : ZoogiComboTune::DriveFrac);
			for (AZoogiBall* Sh : Balls)
			{
				if (!Sh || Sh->TeamIndex != ComboTeam || Sh->bOut || Sh->bKnockedOut) continue;
				if (Sh->PlayerIndex != ActivePlayer && !bAllowSwitch) continue;
				for (AActor* Tg : Targets) for (float C = -30.f; C <= 30.f; C += 2.f)
				{
					const float S = RingOutScore(this, Sh, Tg, C, Speed);
					if (S < Best) { Best = S; BestShooter = Sh->PlayerIndex; BestT = Tg; }
				}
			}
			if (!BestT || Best >= 4000.f)
			{
				// No clean line this turn: reposition softly instead of gifting a self ring-out.
				UE_LOG(LogZoogi, Log, TEXT("ComboDemo: no ring-out line (best %.0f), P%d repositions with CONTROL"), Best, ActivePlayer + 1);
				const FVector L = GetActiveBall()->BallLocation();
				const FVector P = L + (FVector(0, 0, L.Z) - L).GetSafeNormal2D() * 220.f;
				SetFreeAim(FVector(P.X, P.Y, L.Z - ZoogiConst::ZoogiRadius));
				ComboSetForce(EZoogiForce::Control);
				return;
			}
			if (BestShooter != ActivePlayer) ComboSelect(BestShooter);
			DemoAimRingOut(BestT, F);
		};
		if (ComboDemo == 1)
		{
			if (bTeamA && T == 1 && !bInFollowUp) { ComboSetForce(EZoogiForce::Drive); DemoAimBankInto(Balls[1], AZoogiComboArena::WallA0 + 1.f, AZoogiComboArena::WallA1 - 1.f, AZoogiComboArena::WallR - 12.f); ComboSetForce(EZoogiForce::Drive); }
			else if (bTeamA && T == 1 && bInFollowUp && Balls.IsValidIndex(2) && IsValidTarget(Balls[2]))
			{
				// Definition of Done: the partner launches at enemy Zoogi C.
				// DRIVE when it is enough (less follow-through), LAUNCH otherwise.
				if (!DemoAimRingOut(Balls[2], EZoogiForce::Drive) && !DemoAimRingOut(Balls[2], EZoogiForce::Launch)) UE_LOG(LogZoogi, Warning, TEXT("ComboDemo: follow-up line on P3 is not clean"));
			}
			else if (bTeamA && T == 3 && !bInFollowUp) RingOutBest(EZoogiForce::Drive, true); // DRIVE into a prepared Guard -> Guard Reaction
			else if (bTeamA) RingOutBest(EZoogiForce::Launch, !bInFollowUp);
			else Harmless();
		}
		else if (ComboDemo == 2)
		{
			if (bTeamA && T == 1 && !bInFollowUp && Orbs.IsValidIndex(0))
			{
				// Orb Relay: send the orb at the partner (pick the cut whose departure points at P2).
				AZoogiBall* P2 = PartnerOf(Me);
				ComboSetForce(EZoogiForce::Control);
				LockOn(Orbs[0], false);
				float BestC = 0.f, BestDot = -2.f;
				for (float C = -20.f; C <= 20.f; C += 1.f)
				{
					AimCutDeg = C;
					UpdateLockAim(); UpdatePreview();
					if (Preview.bContact && Preview.ContactActor.Get() == Orbs[0] && P2)
					{
						const float Dd = FVector::DotProduct(Preview.TargetDir, (P2->BallLocation() - Orbs[0]->OrbLocation()).GetSafeNormal2D());
						if (Dd > BestDot) { BestDot = Dd; BestC = C; }
					}
				}
				AimCutDeg = BestC;
				UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d sends the orb toward partner P2 (CONTROL, cut %+.0f, alignment %.3f)"), ActivePlayer + 1, BestC, BestDot);
			}
			else if (bTeamA)
			{
				// Knock an Arena Orb out: the relayed one during the follow-up, otherwise the best orb line.
				AActor* BestT = nullptr; float Best = 1e9f; int32 BestSh = ActivePlayer;
				const float Speed = ZoogiConst::LaunchSpeedForPull(ZoogiComboTune::DriveFrac);
				for (AZoogiBall* Sh : Balls)
				{
					if (!Sh || Sh->TeamIndex != ComboTeam || Sh->bOut || (bInFollowUp && Sh != Me)) continue;
					for (AZoogiOrb* O : Orbs)
					{
						if (!O || O->bOut) continue;
						if (bInFollowUp && FollowRelayOrb.IsValid() && O != FollowRelayOrb.Get()) continue;
						for (float C = -30.f; C <= 30.f; C += 2.f) { const float S = RingOutScore(this, Sh, O, C, Speed); if (S < Best) { Best = S; BestT = O; BestSh = Sh->PlayerIndex; } }
					}
				}
				if (BestSh != ActivePlayer) ComboSelect(BestSh);
				if (BestT) DemoAimRingOut(BestT, EZoogiForce::Drive); else Harmless();
			}
			else Harmless();
		}
		else // 3: reactive terrain
		{
			const FVector2D RC = (AZoogiComboArena::RampMin() + AZoogiComboArena::RampMax()) * 0.5f;
			if (bTeamA && (T == 1 || T == 3) && !bInFollowUp)
			{
				if (T == 3 && ActivePlayer != 1) ComboSelect(1);
				AZoogiBall* B = GetActiveBall();
				const FVector L = B->BallLocation();
				// Through the ramp centre line, landing on the floor section.
				const FVector2D Dir = (RC - FVector2D(L)).GetSafeNormal();
				const FVector2D P = RC + Dir * 500.f;
				SetFreeAim(FVector(P.X, P.Y, L.Z - ZoogiConst::ZoogiRadius));
				ComboSetForce(EZoogiForce::Drive);
				UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d drives over the launch ramp toward the floor section"), ActivePlayer + 1);
			}
			else if (!bTeamA && T == 4 && ComboArena && ComboArena->GetFloorState() == EZoogiFloorState::Cracked && Balls.IsValidIndex(2) && !Balls[2]->bOut)
			{
				// Use the changed geometry: bank off the raised lip face of the cracked section.
				if (ActivePlayer != 2) ComboSelect(2);
				AZoogiBall* B = GetActiveBall();
				const float FaceX = AZoogiComboArena::SectionMax().X + 8.f;
				float BestY = 400.f, BestScore = 1e9f; bool bHit = false;
				ComboSetForce(EZoogiForce::Drive);
				for (float Y = AZoogiComboArena::SectionMin().Y + 40.f; Y <= AZoogiComboArena::SectionMax().Y - 40.f; Y += 5.f)
				{
					bFreeAim = true; TargetLock.Reset(); AimCutDeg = 0.f;
					FreeAimPoint = FVector(FaceX, Y, B->BallLocation().Z - ZoogiConst::ZoogiRadius);
					UpdateLockAim(); UpdatePreview();
					if (!Preview.bBank) continue;
					const AZoogiBall* CB = Cast<AZoogiBall>(Preview.ContactActor.Get());
					const float S = Preview.bContact ? (CB && CB->TeamIndex != ComboTeam ? 0.f : (Cast<AZoogiOrb>(Preview.ContactActor.Get()) ? 100.f : 1000.f)) + Preview.CutDeg : 2000.f + FMath::Abs(Y - 400.f);
					if (S < BestScore) { BestScore = S; BestY = Y; bHit = Preview.bContact; }
				}
				bFreeAim = false;
				SetFreeAim(FVector(FaceX, BestY, B->BallLocation().Z - ZoogiConst::ZoogiRadius));
				ComboSetForce(EZoogiForce::Drive);
				UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d banks off the CRACKED raised lip (y %.0f, preview contact %s)"), ActivePlayer + 1, BestY, bHit ? TEXT("yes") : TEXT("no"));
			}
			else if (bTeamA && T >= 5 && !bInFollowUp)
			{
				// Launch off the cracked lip if a teammate is on the low side, else a ring-out attempt.
				AZoogiBall* West = nullptr;
				for (AZoogiBall* B : Balls) if (B && B->TeamIndex == 0 && !B->bOut && B->BallLocation().X < AZoogiComboArena::SectionMin().X - 100.f) West = B;
				if (West && ComboArena && ComboArena->GetFloorState() == EZoogiFloorState::Cracked)
				{
					if (West->PlayerIndex != ActivePlayer) ComboSelect(West->PlayerIndex);
					const FVector L = West->BallLocation();
					SetFreeAim(FVector(AZoogiComboArena::SectionMax().X + 300.f, FMath::Clamp(L.Y, 260.f, 540.f), L.Z - ZoogiConst::ZoogiRadius));
					ComboSetForce(EZoogiForce::Drive);
					UE_LOG(LogZoogi, Log, TEXT("ComboDemo: P%d drives across the CRACKED lip (launch)"), ActivePlayer + 1);
				}
				else RingOutBest(EZoogiForce::Drive, true);
			}
			else if (bTeamA) RingOutBest(EZoogiForce::Drive, false);
			else Harmless();
		}
		return;
	}
	if (DemoStep == 0 && StepTime > 1.2f) { DemoStep = 1; ComboShot(FString::Printf(TEXT("t%d%s_aim_%s"), T, bInFollowUp ? TEXT("fu") : TEXT(""), *FString(ForceName(ComboForce)).ToLower())); }
	if (DemoStep == 1 && StepTime > 1.7f)
	{
		DemoStep = 2;
		UE_LOG(LogZoogi, Log, TEXT("ComboDemo: confirm (P%d, %s, preview %s%s)"), ActivePlayer + 1, ForceName(ComboForce), Preview.bBank ? TEXT("bank -> ") : TEXT(""), Preview.bContact ? *TargetName(Preview.ContactActor.Get()) : TEXT("no contact"));
		if (!CommitLock())
		{
			UE_LOG(LogZoogi, Warning, TEXT("ComboDemo: no aim to confirm, harmless CONTROL shot instead"));
			const FVector L = Me->BallLocation();
			const FVector P = L + (FVector(0, 0, L.Z) - L).GetSafeNormal2D() * 180.f;
			SetFreeAim(FVector(P.X, P.Y, L.Z - ZoogiConst::ZoogiRadius));
			ComboSetForce(EZoogiForce::Control);
			CommitLock();
		}
	}
}
