// Oct 29 night (Evan, build 23): action / fun / timing pack - see ZoogiActionFun.h.
#include "ZoogiActionFun.h"
#include "ZoogiShowtime.h" // b243
#include "ZoogiAwardBanner.h" // b238
#include "ZoogiVoyager.h" // b233
#include "ZoogiWallCine.h" // b230
#include "ZoogiGattling.h"
#include "ZoogiBounty.h"
#include "ZoogiBirds.h"
#include "ZoogiEarthEvents.h"
#include "ZoogiDemoReel.h" // b213
#include "ZoogiEpic.h"
#include "ZoogiSkillLab.h"
#include "ZoogiFront.h" // b66 ZoogiControls::FlashMul
#include "ZoogiRedirect.h"
#include "ZoogiGuide.h"
#include "ZoogiBanter.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiP5Command.h"
class UCanvas;
void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);
#include "ZoogiClashSplit.h"
#include "ZoogiPortraitCam.h"
#include "Engine/PostProcessVolume.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiGameMode.h"
#include "ZoogiLoadScreen.h"
#include "ZoogiCosmosBoss.h" // b217 // b207
#include "ZoogiPlayerController.h"
#include "ZoogiArena.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiFX.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIStyle.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiFabFX.h"
#include "ZoogiStandForms.h"
#include "ZoogiSkills.h"
#include "ZoogiVortexRings.h"
#include "ZoogiSpectacle.h" // b268
#include "ZoogiParty.h" // b273
#include "ZoogiP5Style.h"
#include "ZoogiArenaHUDWidget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "SceneView.h"
#include "CanvasTypes.h"
#include "TextureResource.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Misc/App.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "ImageUtils.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "ZoogiReactCam.h"
#include "ZoogiLightning.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiArcCam.h"
#include "ZoogiSkyDiveCam.h"
#include "ZoogiBossAttack.h"
#include "ZoogiRoyaleMix.h"
#include "ZoogiRaid.h"
#include "ZoogiTrickShot.h"
#include "ZoogiThrone.h"
#include "ZoogiFlags.h"
#include "ZoogiAnimeCues.h"
#include "ZoogiSoundscape.h"
#include "ZoogiAurexPuppet.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiCards.h"

namespace ZoogiFunTune
{
	constexpr float RingLead = 0.75f;   // the timing ring shows this long before the predicted contact
	constexpr float Perfect = 0.075f;   // |tap - contact| for PERFECT
	constexpr float Great = 0.17f;      // ... for GREAT
	constexpr float PerfectMul = 1.35f; // strike: target knock speed
	constexpr float GreatMul = 1.15f;
	constexpr float BlockMul = 0.35f;   // parry: defender keeps this much of its knock speed
	constexpr float BraceMul = 0.6f;
	constexpr float HeavyMul = 1.3f;    // HEAVY HIT boost
	constexpr float FireMul = 1.2f;     // on fire
	constexpr float FastDil = 2.f;      // CPU fast-forward
	constexpr float BoostChance = 0.45f;
	constexpr float PickupR = 115.f;
	constexpr float RecHz = 30.f;
	constexpr float RecMax = 9.f;
	constexpr float ReplayRate = 0.5f;
}

static bool FunFlag(const TCHAR* No) { return !FParse::Param(FCommandLine::Get(), No); }

UZoogiFunSys* UZoogiFunSys::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? W->GetSubsystem<UZoogiFunSys>() : nullptr;
}

void UZoogiFunSys::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFunTest"));
	bClashTestOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashTest"));
	bStrikeTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStrikeTest"));
	HudHandle = AHUD::OnHUDPostRender.AddUObject(this, &UZoogiFunSys::DrawHud);
}

void UZoogiFunSys::Deinitialize()
{
	if (HudHandle.IsValid()) AHUD::OnHUDPostRender.Remove(HudHandle);
	HudHandle.Reset();
	EndArenaEvent();
	EndStrike(false);
	EndFighters();
	EndFlung(false);
	if (CurDil != 1.f && GetWorld()) UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
	CurDil = 1.f; bDilApplied = false;
	Super::Deinitialize();
}

float UZoogiFunSys::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f; }

bool UZoogiFunSys::IsActive(const AZoogiGameMode* G) const
{
	static const bool bOn = FunFlag(TEXT("ZoogiNoActionFun"));
	return bOn && G && !G->bComboMode && !G->bRoyaleMode && G->Phase != EZoogiPhase::Menu && G->Balls.Num() > 0;
}

bool UZoogiFunSys::IsFinalRounds(const AZoogiGameMode* G) const
{
	// b66 (playtest): "FINAL ROUNDS" was the first banner of a 2-round match - never in round 1, and a short match only on its last round
	if (!IsActive(G) || G->Phase == EZoogiPhase::GameOver || G->RoundNumber < 2) return false;
	return G->MenuRounds >= 4 ? G->RoundNumber >= G->MenuRounds - 1 : G->RoundNumber >= G->MenuRounds;
}

int32 UZoogiFunSys::OrbPointMul(const AZoogiGameMode* G, int32 Scorer) const
{
	if (!IsActive(G)) return 1;
	int32 M = IsFinalRounds(G) ? 2 : 1;
	if (ActiveBoost == EBoost::Double && Scorer == ActiveBoostPlayer) M *= 2;
	return M;
}

int32 UZoogiFunSys::KOPointMul(const AZoogiGameMode* G, int32 Scorer) const
{
	return (IsActive(G) && ActiveBoost == EBoost::Double && Scorer == ActiveBoostPlayer) ? 2 : 1;
}

static bool P5BannersOn(); // b63: P5 corner banners (defined with the HUD painter)
void UZoogiFunSys::Callout(const FString& Text, const FString& Sub, const FLinearColor& Col, float Life, float Size, int32 Speaker, const FString& Line)
{
	Callouts.Reset(); // one callout at a time: the newest takes the spot (two at once overlapped)
	static int32 GBandFlip = 0; // b116: banners take turns - top band, bottom band
	FCallout C; C.Band = (GBandFlip++) & 1; C.Text = Text; C.Sub = Sub; C.Col = Col; C.Life = Life + (P5BannersOn() && !Line.IsEmpty() ? 0.6f : 0.f); C.Size = Size; C.Speaker = Speaker; C.Line = Line;
	Callouts.Add(C);
	UE_LOG(LogZoogi, Log, TEXT("FUN callout: %s %s"), *Text, *Sub);
}

void UZoogiFunSys::Shot(AZoogiGameMode* G, const FString& Name, float Delay)
{
	if ((!bTest && !bStrikeTest && !bClashTestOn) || !G || G->ShotDir.IsEmpty()) return;
	if (Delay > 0.f) G->TakeShotDelayed(Name, Delay); else G->TakeShot(Name);
}

void UZoogiFunSys::ResetMatch(AZoogiGameMode* G)
{
	UltAimSeen.Reset();
	for (int32 i = 0; i < Players.Num(); ++i) { Extinguish(G, i, true); SetAwakened(G, i, false); }
	Players.Reset();
	EndArenaEvent(); EventSerial = 0; EventRound = -1;
	EndFighters(); EndFlung(false); Clash = FClash(); bUltAim = false; DelayedHits.Reset(); bFinisherPending = false; EndStrike(false);
	ActiveBoost = EBoost::None; ActiveBoostPlayer = -1;
	KillPickup(false);
	bFinalAnnounced = false; bRound1Announced = false; ZoogiBanter::Reset();
	ZoogiRedirect::Reset(); // b66
	Cur.Reset(); Best.Reset(); bRecording = false;
	SetFast(false);
	CpuTurnsSeen = 0;
	UE_LOG(LogZoogi, Log, TEXT("FUN match state reset"));
}

void UZoogiFunSys::SetFast(bool bOn)
{
	bFast = bOn;
	if (!bOn) FastLevel = 1;
}

// ------------------------------------------------------------------ turn hooks

void UZoogiFunSys::OnTurnBegin(AZoogiGameMode* G, int32 Player)
{
	if (!IsActive(G)) return;
	if (G->RoundNumber == 1 && LastRound > 1) ResetMatch(G);
	LastRound = G->RoundNumber;
	if (Players.Num() != G->Balls.Num()) Players.SetNum(G->Balls.Num());
	// b67 (Evan: "AI talks to each other"): a taunt + answer at some turn starts
	if (G->TurnCounter > 1 && FMath::FRand() < 0.45f) { const int32 Rv = ZoogiBanter::RandomRival(G, Player); if (Rv >= 0) ZoogiBanter::Exchange(G, Player, Rv, ZoogiBanter::EKind::Taunt, ZoogiBanter::EKind::TauntReply); }
	if (bTest && G->RoundNumber == 1 && Player == G->RoundStartPlayer && Players.IsValidIndex(0) && Players[0].Streak == 0)
	{
		for (FPlayerFun& P : Players) { P.Streak = 2; P.Meter = 100.f; } // -ZoogiFunTest: fire on the next scoring turn, ultimates ready
	}
	CommitRec();
	bRecording = false;
	// boosts: last turn's boost is spent, this player's pending boost arms now
	ActiveBoost = EBoost::None; ActiveBoostPlayer = -1;
	AZoogiBall* B = G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (Players.IsValidIndex(Player) && Players[Player].Pending != EBoost::None && B)
	{
		ActiveBoost = Players[Player].Pending; ActiveBoostPlayer = Player;
		Players[Player].Pending = EBoost::None;
		const bool bHeavy = ActiveBoost == EBoost::Heavy;
		Callout(bHeavy ? TEXT("HEAVY HIT!") : TEXT("DOUBLE POINTS!"), FString::Printf(TEXT("%s  BOOST ARMED"), *B->Def.Name.ToUpper()),
			bHeavy ? FLinearColor(1.f, 0.42f, 0.18f) : FLinearColor(1.f, 0.82f, 0.2f), 1.5f, 0.85f);
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), bHeavy ? FLinearColor(1.f, 0.4f, 0.15f) : FLinearColor(1.f, 0.8f, 0.2f), 160.f, 0.6f, true, 10.f, B);
	}
	// b66 (playtest): the match opens with a ROUND 1 beat (once, on the first turn)
	if (G->TurnCounter <= 1 && !bRound1Announced && !G->bSoloTraining)
	{
		bRound1Announced = true;
		Callout(TEXT("ROUND 1"), G->MenuRounds > 1 ? FString::Printf(TEXT("OF %d  -  KNOCK THE ORBS OUT"), G->MenuRounds) : FString(TEXT("KNOCK THE ORBS OUT")), FLinearColor(1.f, 0.82f, 0.25f), 1.8f, 1.f);
	}
	// final rounds: orbs worth double
	if (IsFinalRounds(G) && !bFinalAnnounced)
	{
		bFinalAnnounced = true;
		Callout(G->RoundNumber >= G->MenuRounds ? TEXT("FINAL ROUND!") : TEXT("FINAL ROUNDS!"), TEXT("EVERY ORB IS WORTH DOUBLE"), FLinearColor(1.f, 0.78f, 0.18f), 2.2f, 1.1f);
		if (UZoogiShowSys* SS = GetWorld()->GetSubsystem<UZoogiShowSys>()) SS->Fireworks(5, 1.2f);
		ZoogiShow::Event(GetWorld(), ZoogiShow::EEvent::BigScore, FVector(0, 0, 100.f), 1.5f);
		ZoogiUtil::PlaySfx(G, TEXT("Win"), 0.5f, 1.15f);
		Shot(G, TEXT("fun_final"), 0.6f);
	}
	// build 25: awakened form (on fire + full meter), arena event at the start of every second round
	bUltThisTurn = false; bFinisherPending = false;
	for (int32 i = 0; i < Players.Num(); ++i) if (i != Player) SetAwakened(G, i, false);
	if (!ZoogiAwakenMeter() && Players.IsValidIndex(Player) && Players[Player].bFire && Players[Player].Meter >= 100.f) SetAwakened(G, Player, true); // b135: no AWAKENED buff
	if (Player == G->RoundStartPlayer && EventRound != G->RoundNumber)
	{
		EventRound = G->RoundNumber;
		EndArenaEvent();
		if (G->RoundNumber >= 2 && G->RoundNumber % 2 == 0) StartArenaEvent(G);
	}
	// fast-forward lasts through CPU turns only
	if (B && !B->bCPU) SetFast(false);
	if (B && B->bCPU) ++CpuTurnsSeen;
}

void UZoogiFunSys::OnTurnResolved(AZoogiGameMode* G, int32 Player, int32 Points, int32 KOs)
{
	if (!IsActive(G) || !Players.IsValidIndex(Player)) return;
	FPlayerFun& P = Players[Player];
	if (Points > 0)
	{
		++P.Streak;
		if (P.Streak >= (bTest ? 1 : 3) && !P.bFire) Ignite(G, Player); // -ZoogiFunTest: first scoring turn
		else if (P.bFire && G->Balls.IsValidIndex(Player)) Callout(TEXT("STILL ON FIRE!"), FString::Printf(TEXT("%d IN A ROW"), P.Streak), FLinearColor(1.f, 0.5f, 0.15f), 1.1f, 0.8f);
	}
	else
	{
		if (P.bFire) Extinguish(G, Player, false);
		P.Streak = 0;
	}
	// build 25: SUPER meter (scoring, KOs, getting knocked off), ULTIMATE FINISH, awakened ends with the turn
	if (Points > 0 || KOs > 0) AddMeter(G, Player, Points * 0.4f + KOs * 35.f, TEXT("scoring"));
	// b67: banter on the result (KO > big score > miss)
	{
		int32 Victim = -1;
		for (const FZoogiLedgerEntry& LE : G->Ledger) if (LE.bFall && LE.Victim != Player && LE.Victim >= 0) Victim = LE.Victim;
		if (KOs > 0 && Victim >= 0) ZoogiBanter::Exchange(G, Player, Victim, ZoogiBanter::EKind::KOGloat, ZoogiBanter::EKind::KOVictim, true);
		else if (Points >= 100) { const int32 Rv = ZoogiBanter::RandomRival(G, Player); if (Rv >= 0) ZoogiBanter::Exchange(G, Player, Rv, ZoogiBanter::EKind::BigScore, ZoogiBanter::EKind::BigScoreReact); }
		else if (Points <= 0 && FMath::FRand() < 0.5f) { const int32 Rv = ZoogiBanter::RandomRival(G, Player); if (Rv >= 0) ZoogiBanter::Exchange(G, Rv, Player, ZoogiBanter::EKind::MissMock, ZoogiBanter::EKind::MissReply); }
	}
	if (ZoogiAwakenMeter())
	{	// b135b (Evan): getting knocked off resets the AWAKEN meter
		for (const FZoogiLedgerEntry& LE : G->Ledger)
			if (LE.bFall && Players.IsValidIndex(LE.Victim) && Players[LE.Victim].Meter > 0.f)
			{
				UE_LOG(LogZoogi, Log, TEXT("FUN AWAKEN meter P%d %.0f -> 0 (knocked off)"), LE.Victim + 1, Players[LE.Victim].Meter);
				Players[LE.Victim].Meter = 0.f;
			}
	}
	else for (const FZoogiLedgerEntry& LE : G->Ledger) if (LE.bFall && LE.Victim != Player && Players.IsValidIndex(LE.Victim)) AddMeter(G, LE.Victim, 15.f, TEXT("knocked off"));
	if (KOs > 0 && bUltThisTurn) { bFinisherPending = true; UE_LOG(LogZoogi, Log, TEXT("FUN ultimate knockout -> finisher replay")); }
	if (KOs > 0 && bStrikeThisShot && G->Balls.IsValidIndex(Player) && G->Balls[Player] && !G->Balls[Player]->bOut) StartStrike(G, G->Balls[Player], nullptr, 0.f, TEXT("cheer"), true, 1.8f); // build 26: knockout -> the form stands and cheers
	SetAwakened(G, Player, false);
	// the turn's replay score
	if (Cur.Player == Player)
	{
		Cur.Score = Points + 120.f * KOs + (LastGrade == 2 ? 80.f : (LastGrade == 1 ? 30.f : 0.f)) + (Cur.ImpactT >= 0.f ? 15.f : 0.f);
		Cur.bAction = Points > 0 || KOs > 0 || LastGrade == 2;
		if (G->Balls.IsValidIndex(Player) && G->Balls[Player])
		{
			Cur.Title = G->Balls[Player]->Def.Name.ToUpper();
			if (KOs > 0) Cur.Title += TEXT("  KNOCKOUT");
			else if (Points > 0) Cur.Title += FString::Printf(TEXT("  +%d"), Points);
		}
	}
	KillPickup(true);
}

void UZoogiFunSys::Ignite(AZoogiGameMode* G, int32 Player)
{
	FPlayerFun& P = Players[Player];
	AZoogiBall* B = G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (!B) return;
	P.bFire = true;
	Callout(FString::Printf(TEXT("%s IS ON FIRE!"), *B->Def.Name.ToUpper()), FString::Printf(TEXT("%d SCORING TURNS IN A ROW  -  HARDER HITS"), P.Streak), FLinearColor(1.f, 0.45f, 0.1f), 2.f, 1.f);
	if (UNiagaraComponent* T = AZoogiFX::ArrowTrail(GetWorld(), B, TEXT("Fire"), 0.75f, 0.f)) P.FireTrail = T;
	AZoogiFX::Spawn(GetWorld(), B->BallLocation(), FLinearColor(1.f, 0.45f, 0.1f), 260.f, 0.7f, true, 12.f, B);
	AZoogiFX::Niagara(GetWorld(), TEXT("NS_Burst_Red"), B->BallLocation(), FRotator::ZeroRotator, 1.2f);
	ZoogiShow::Event(GetWorld(), ZoogiShow::EEvent::BigScore, B->BallLocation(), 2.f);
	ZoogiUtil::PlaySfx(G, TEXT("CrowdWave1"), 0.8f);
	UE_LOG(LogZoogi, Log, TEXT("FUN P%d %s ON FIRE (streak %d)"), Player + 1, *B->Def.Name, P.Streak);
	Shot(G, TEXT("fun_fire"), 0.5f);
}

void UZoogiFunSys::Extinguish(AZoogiGameMode* G, int32 Player, bool bQuiet)
{
	if (!Players.IsValidIndex(Player)) return;
	FPlayerFun& P = Players[Player];
	if (P.FireTrail.IsValid()) AZoogiFX::ReleaseArrowTrail(P.FireTrail.Get());
	P.FireTrail.Reset();
	if (!P.bFire) return;
	P.bFire = false;
	if (bQuiet || !G) return;
	if (AZoogiBall* B = G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr)
	{
		Callout(TEXT("FIRE'S OUT"), B->Def.Name.ToUpper(), FLinearColor(0.6f, 0.75f, 0.9f), 1.1f, 0.7f);
		AZoogiFX::Spawn(GetWorld(), B->BallLocation(), FLinearColor(0.7f, 0.7f, 0.75f), 150.f, 0.6f, false, 5.f);
	}
}

// ------------------------------------------------------------------ input

void UZoogiFunSys::UltAimRelease(const FVector2D& Screen)
{	// b120: build 25 bullet-time target pick, on release
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!bUltAim || !G) return;
	AZoogiBall* B = G->GetActiveBall();
	APlayerController* PC = W->GetFirstPlayerController();
	if (!B || !PC || Screen.X < 0.f) return;
	AActor* PickA = nullptr; float BestD = 1e9f;
	for (AActor* T : G->LiveTargets(B))
	{
		FVector2D S;
		const FVector TL = Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation();
		if (!PC->ProjectWorldLocationToScreen(TL, S, false)) continue;
		const float D = FVector2D::Distance(S, Screen);
		if (D < BestD) { BestD = D; PickA = T; }
	}
	int32 Vx = 0, Vy = 0; PC->GetViewportSize(Vx, Vy);
	if (PickA && BestD < 0.20f * FMath::Max(1, FMath::Min(Vx, Vy)) + 80.f) { bUltAim = false; FireUltimate(G, B, UltPower, PickA); } // b125: bigger window (was 0.12 + 40)
	else UE_LOG(LogZoogi, Log, TEXT("ULT tap missed (nearest target %.0f px away)"), BestD);
}

bool UZoogiFunSys::OnTap(const FVector2D& Screen)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (G && ZoogiEdgeHang::Active()) return ZoogiEdgeHang::Tap(G); // b58: edge hang pull-up taps
	if (!IsActive(G)) return false;
	if (DefWin.bOn) { DefWinTap(G, Screen); return true; } // b106 / b107b: the ring (turn player), DIRECT / AROUND, GUARD (rival)
	if (Clash.bOn) // build 25: power clash tap-off
	{
		if (Clash.bFinish) return true;
		if (Clash.bDuel) // b66: duel - menu buttons by touch, REACTION = skill / timing tap
		{
			if (Clash.bV2 && ((Clash.DStage == 11 && Clash.bHumanAtt) || (Clash.DStage == 12 && Clash.bHumanDef))) // b90: the move / answer menus
			{
				if (Screen.X >= 0.f) for (int32 i = 0; i < 4; ++i) if (DuelBtn[i].bIsValid && DuelBtn[i].IsInside(Screen)) { DuelV2Pick(G, i); return true; }
				return true;
			}
			if (Clash.DStage == 1 && Clash.bHumanAtt && Screen.X >= 0.f)
			{
				for (int32 i = 0; i < 4; ++i) if (DuelBtn[i].bIsValid && DuelBtn[i].IsInside(Screen)) { DuelChoose(G, i); return true; }
				return true;
			}
			DuelTap(G);
			return true;
		}
		if (Clash.bHumanAtt) ClashPush(true); else if (Clash.bHumanDef) ClashPush(false);
		return true;
	}
	if (bUltAim) return true; // b120: the bullet-time pick happens on release (UltAimRelease) so a drag can turn the camera
	if (Rep.bOn)
	{
		if (Rep.Age > 0.4f) { UE_LOG(LogZoogi, Log, TEXT("FUN replay skipped by tap")); EndReplay(G); }
		return true;
	}
	static const bool bTapOn = FunFlag(TEXT("ZoogiNoTimingTap"));
	const float T = Now();
	if (bTapOn && Mode != 0 && !bTapUsed && G->Phase == EZoogiPhase::Rolling)
	{
		const bool bBefore = ContactT < 0.f && PredT >= 0.f && PredT <= ZoogiFunTune::RingLead;
		const bool bAfter = ContactT >= 0.f && T - ContactT <= ZoogiFunTune::Great && !bGraded;
		if (bBefore || bAfter)
		{
			bTapUsed = true;
			TapT = T;
			if (bAfter) Grade(G, ContactT - TapT);
			return true;
		}
	}
	// b66 (Evan): REACTION mid-roll = redirect; like every REACTION moment, a tap anywhere counts as the press
	if (ZoogiRedirect::Available(G) && ZoogiRedirect::Begin(G)) return true;
	// CPU turn: speed it up
	AZoogiBall* A = G->GetActiveBall();
	if (A && A->bCPU && !bFast && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving))
	{
		SetFast(true);
		Callout(TEXT(">> 2X"), TEXT("CPU TURN FAST-FORWARD"), FLinearColor(0.55f, 0.85f, 1.f), 0.9f, 0.6f);
		UE_LOG(LogZoogi, Log, TEXT("FUN CPU fast-forward on"));
		Shot(G, TEXT("fun_fast"), 0.4f);
		return true;
	}
	// b192 (Evan: "add another tap for 3x speed to speed up AI turns"): tap again while at 2x
	if (A && A->bCPU && bFast && FastLevel < 2 && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving))
	{
		FastLevel = 2;
		Callout(TEXT(">>> 3X"), TEXT("CPU TURN FAST-FORWARD"), FLinearColor(0.55f, 0.85f, 1.f), 0.9f, 0.6f);
		UE_LOG(LogZoogi, Log, TEXT("FUN CPU fast-forward 3x"));
		return true;
	}
	return false;
}

int32 UZoogiFunSys::TapButtonMode(const AZoogiGameMode* G) const
{
	// build 27 (Evan): the REACTION button is what players tap, so it shows up for every tap moment
	if (G && ZoogiEdgeHang::Active()) return ZoogiEdgeHang::WantsTaps() ? 5 : 0; // b58: PULL! (edge hang)
	if (!G || !IsActive(G) || Rep.bOn) return 0;
	if (Clash.bOn) return (Clash.bHumanAtt || Clash.bHumanDef) && !Clash.bFinish && (!Clash.bDuel || (Clash.bV2 ? (Clash.DStage <= 10 && Clash.bHumanAtt) : Clash.DStage <= 2)) ? 1 : 0; // b90: REACTION = the circle match tap
	static const bool bTapOn = FunFlag(TEXT("ZoogiNoTimingTap"));
	if (!bTapOn || Mode == 0 || bTapUsed || G->Phase != EZoogiPhase::Rolling) return 0;
	static const bool bOldBoost = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoostTap"));
	if (Mode == 1 && !bOldBoost) return 0; // b66 (Evan): no more react-to-boost after launch - every human hit lands boosted
	const bool bBefore = ContactT < 0.f && PredT >= 0.f && PredT <= ZoogiFunTune::RingLead;
	const bool bAfter = ContactT >= 0.f && Now() - ContactT <= ZoogiFunTune::Great && !bGraded;
	if (!bBefore && !bAfter) return 0;
	return Mode == 1 ? 2 : 3; // b58 (Evan, 20:28): "Keep the reaction for the extra hit" - the strike tap asks for REACTION again (b57 removal undone)
}

bool UZoogiFunSys::IsClashFighter(int32 Player) const
{
	if (!Clash.bOn) return false;
	const AZoogiBall* A = Clash.Att.Get(); const AZoogiBall* D = Clash.Def.Get();
	return (A && A->PlayerIndex == Player) || (D && D->PlayerIndex == Player);
}

// ------------------------------------------------------------------ contacts

void UZoogiFunSys::OnBallContact(AZoogiBall* Self, AActor* Other)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!IsActive(G) || G->Phase != EZoogiPhase::Rolling || !Self || !Other || Rep.bOn) return;
	if (!Cast<AZoogiBall>(Other) && !Cast<AZoogiOrb>(Other)) return;
	AZoogiBall* Sh = Shooter.Get();
	if (!Sh) return;
	AActor* Hit = nullptr;
	if (Self == Sh) Hit = Other;
	else if (Other == Sh) Hit = Self;
	if (!Hit) return;
	const bool bFirstContact = ContactT < 0.f;
	SuperContact(G, Sh, Hit, bFirstContact); // build 25: clash / fusion / ultimate contacts
	if (!bFirstContact) return;
	ContactT = Now();
	ContactOther = Hit;
	if (Cur.ImpactT < 0.f) { Cur.ImpactT = RecT; Cur.Other = Hit; }
	// first contact boosts: HEAVY HIT, on fire
	float Mul = 1.f;
	if (ActiveBoost == EBoost::Heavy && Sh->PlayerIndex == ActiveBoostPlayer) Mul *= ZoogiFunTune::HeavyMul;
	if (Players.IsValidIndex(Sh->PlayerIndex) && Players[Sh->PlayerIndex].bFire) Mul *= ZoogiFunTune::FireMul;
	if (Players.IsValidIndex(Sh->PlayerIndex) && Players[Sh->PlayerIndex].bAwake) Mul *= 1.25f;
	if (Mul > 1.01f && !bContactBoosted)
	{
		bContactBoosted = true;
		FPendMul PM; PM.A = Hit; PM.Mul = Mul; PendMul.Add(PM);
		AZoogiFX::Spawn(W, Hit->GetActorLocation(), FLinearColor(1.f, 0.45f, 0.15f), 200.f, 0.4f, true, 12.f);
		UE_LOG(LogZoogi, Log, TEXT("FUN contact boost x%.2f on %s"), Mul, *Hit->GetName());
	}
	// b66 (Evan): "now it hits as if it were tapped and boosted" - a human shooter's first hit is graded PERFECT automatically
	static const bool bOldBoostC = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoostTap"));
	if (!bOldBoostC && !Sh->bCPU && !bGraded && TapT < 0.f) { Mode = 1; TapT = ContactT; bTapUsed = true; }
	if (TapT >= 0.f && !bGraded) Grade(G, ContactT - TapT);
}

void UZoogiFunSys::ParryShield(UWorld* W, AActor* Def)
{
	if (!W || !Def) return;
	float Sc = 0.2f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiParryShieldScale="), Sc); // the dome closes over the defender (~1.5 m), not the court
	const FVector Feet(0.f, 0.f, -ZoogiConst::ZoogiRadius);
	if (AZoogiFabFX* A = AZoogiFabFX::Play(W, TEXT("/Game/StylizedToonVFX3/Effects/NS_Wood_shield.NS_Wood_shield"), Def->GetActorLocation() + Feet, Sc, 1.3f, Def, Feet))
		A->StopAfter = 0.9f; // ends with the dome: no wide debris crater around it
	AZoogiFabFX::Sound(W, TEXT("/Game/StylizedToonVFX3/Audio/Cues/S_Wood_shield_Cue.S_Wood_shield_Cue"), Def->GetActorLocation(), 0.9f);
}

void UZoogiFunSys::Grade(AZoogiGameMode* G, float DTap)
{
	bGraded = true;
	UWorld* W = GetWorld();
	const float A = FMath::Abs(DTap);
	AActor* Hit = ContactOther.Get();
	AZoogiBall* Def = Cast<AZoogiBall>(PredTarget.Get());
	const bool bParry = Mode == 2;
	const bool bRightTarget = !bParry || (Hit && Hit == Def);
	const int32 Gr = !bRightTarget ? 0 : (A <= ZoogiFunTune::Perfect ? 2 : (A <= ZoogiFunTune::Great ? 1 : 0));
	LastGrade = Gr;
	UE_LOG(LogZoogi, Log, TEXT("FUN grade %s: tap %+.3f s vs contact -> %s"), bParry ? TEXT("PARRY") : TEXT("STRIKE"), DTap, Gr == 2 ? TEXT("PERFECT") : (Gr == 1 ? TEXT("GREAT") : TEXT("MISS")));
	if (Gr == 0)
	{
		Callout(!bRightTarget ? TEXT("NO BLOCK") : (DTap > 0.f ? TEXT("TOO EARLY") : TEXT("TOO LATE")), FString(), FLinearColor(0.65f, 0.7f, 0.8f), 0.8f, 0.6f);
		Shot(G, FString::Printf(TEXT("fun_grade_%02d_miss"), ++GradeShots), 0.05f);
		return;
	}
	const FVector At = Hit ? Hit->GetActorLocation() : FVector::ZeroVector;
	if (!bParry && Gr == 2) if (AZoogiBall* Gv = Cast<AZoogiBall>(Hit)) if (const AZoogiBall* Sh = G->GetActiveBall()) ZoogiSkills::GuardHit(G, Gv, Sh->BallLocation(), 1.f, TEXT("PERFECT")); // b83
	FPendMul PM; PM.A = bParry ? (AActor*)Def : Hit;
	if (bParry)
	{
		PM.Mul = Gr == 2 ? ZoogiFunTune::BlockMul : ZoogiFunTune::BraceMul;
		Callout(Gr == 2 ? TEXT("BLOCKED!") : TEXT("BRACED!"), Gr == 2 ? TEXT("PERFECT TIMING") : TEXT("GOOD TIMING"), FLinearColor(0.35f, 0.85f, 1.f), 1.2f, Gr == 2 ? 1.f : 0.8f, Def ? Def->PlayerIndex : -1, Gr == 2 ? TEXT("Not today!") : TEXT("Hah - is that all?"));
		if (Def)
		{
			// b63 (Evan: "For parry use the effect, resize so it covers primarily just the defending player"): the Hovl wood
			// shield dome closes over the defender only (-ZoogiParryShieldScale=, -ZoogiOldParryFx = the old blue ring)
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldParryFx")))
			{
				AZoogiFX::Spawn(W, Def->BallLocation(), FLinearColor(0.4f, 0.85f, 1.f), 190.f, 0.55f, true, 14.f, Def);
				AZoogiFX::Niagara(W, TEXT("NS_Burst_Blue"), Def->BallLocation(), FRotator::ZeroRotator, 0.9f);
			}
			else
			{
				ParryShield(W, Def);
				const float Gd = W->GetWorldSettings() ? FMath::Max(0.001f, W->GetWorldSettings()->GetEffectiveTimeDilation()) : 1.f;
				Shot(G, TEXT("fun_parry_shield_a"), 0.35f * Gd); Shot(G, TEXT("fun_parry_shield_b"), 0.9f * Gd);
			}
		}
		ZoogiUtil::PlaySfx(G, TEXT("HitClick"), 0.9f, 0.7f);
	}
	else
	{
		PM.Mul = Gr == 2 ? ZoogiFunTune::PerfectMul : ZoogiFunTune::GreatMul;
		Callout(Gr == 2 ? TEXT("PERFECT HIT!") : TEXT("GREAT HIT!"), Gr == 2 ? TEXT("+35% KNOCK") : TEXT("+15% KNOCK"), Gr == 2 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(0.75f, 1.f, 0.45f), 1.2f, Gr == 2 ? 1.f : 0.8f, Shooter.IsValid() ? Shooter->PlayerIndex : -1, Gr == 2 ? TEXT("Bullseye!") : TEXT("Right on target!"));
		AZoogiFX::Spawn(W, At, FLinearColor(1.f, 0.9f, 0.4f), Gr == 2 ? 260.f : 180.f, 0.4f, true, 14.f);
		if (Gr == 2) if (AZoogiBall* Mv = Cast<AZoogiBall>(Hit)) if (AZoogiBall* Ms = Shooter.Get()) ZoogiWallCine::Offer(G, Ms, Mv, 5); // b238: the MEGA HIT moment (a PERFECT HIT on a rival)
		if (Gr == 2 && !Clash.bOn) // b66: no impact frame frozen over a duel / clash
		{
			ZoogiShow::ImpactFrame(W, At, 8);
			ZoogiShow::Event(W, ZoogiShow::EEvent::HardHit, At, 1.5f);
			if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(W->GetFirstPlayerController())) PC->OnImpact(1.f);
		}
		ZoogiUtil::PlaySfx(G, TEXT("Score"), 0.7f, Gr == 2 ? 1.5f : 1.25f);
	}
	if (PM.A.IsValid()) PendMul.Add(PM);
	if (bParry && Def) AddMeter(G, Def->PlayerIndex, Gr == 2 ? 30.f : 12.f, TEXT("block"));
	else if (!bParry && Shooter.IsValid()) AddMeter(G, Shooter->PlayerIndex, Gr == 2 ? 30.f : 12.f, TEXT("timing hit"));
	Shot(G, FString::Printf(TEXT("fun_grade_%02d_%s_%s"), ++GradeShots, bParry ? TEXT("parry") : TEXT("strike"), Gr == 2 ? TEXT("perfect") : TEXT("great")), 0.06f);
}

// ------------------------------------------------------------------ prediction

static float TimeToContact(const FVector2D& P, const FVector2D& V, float R)
{
	// |P - V t| = R, P = target - shooter, V = shooter velocity - target velocity
	const float A = FVector2D::DotProduct(V, V);
	if (A < 1.f) return -1.f;
	const float B = -2.f * FVector2D::DotProduct(P, V);
	const float C = FVector2D::DotProduct(P, P) - R * R;
	if (C <= 0.f) return 0.f;
	const float Disc = B * B - 4.f * A * C;
	if (Disc < 0.f) return -1.f;
	const float T = (-B - FMath::Sqrt(Disc)) / (2.f * A);
	return T >= 0.f ? T : -1.f;
}

void UZoogiFunSys::Predict(AZoogiGameMode* G)
{
	Mode = 0; PredT = -1.f; PredTarget.Reset();
	AZoogiBall* Sh = Shooter.Get();
	if (!Sh || Sh->bOut || ContactT >= 0.f || bGraded) return;
	const FVector Vs3 = Sh->GetBallVelocity();
	const FVector2D Vs(Vs3.X, Vs3.Y);
	if (Vs.Size() < 150.f) return;
	const FVector Ls = Sh->BallLocation();
	float BestT = 1e9f; AActor* BestA = nullptr;
	auto Try = [&](AActor* A, const FVector& L, const FVector& V, float R)
	{
		const float T = TimeToContact(FVector2D(L.X - Ls.X, L.Y - Ls.Y), Vs - FVector2D(V.X, V.Y), R + ZoogiConst::ZoogiRadius);
		if (T >= 0.f && T < BestT && FMath::Abs(L.Z - Ls.Z) < 150.f) { BestT = T; BestA = A; }
	};
	for (AZoogiBall* B : G->Balls) if (B && B != Sh && !B->bOut && !B->bParked) Try(B, B->BallLocation(), B->GetBallVelocity(), ZoogiConst::ZoogiRadius);
	for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && O->Sphere) Try(O, O->OrbLocation(), O->Sphere->GetPhysicsLinearVelocity(), ZoogiConst::OrbRadius);
	if (!BestA || BestT > 2.f) return;
	PredT = BestT; PredTarget = BestA;
	if (!Sh->bCPU) Mode = 1;
	else if (AZoogiBall* D = Cast<AZoogiBall>(BestA)) { if (!D->bCPU) Mode = 2; }
}

// ------------------------------------------------------------------ per-shot state / tick

void UZoogiFunSys::UpdateReact(AZoogiGameMode* G, float RealDt)
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactSlow"));
	UWorld* W = GetWorld();
	AZoogiBall* Att = Shooter.Get();
	AZoogiBall* Vic = Cast<AZoogiBall>(PredTarget.Get());
	const bool bIncoming = !bOff && W && G && Mode == 2 && PredT >= 0.f && PredT <= ZoogiFunTune::RingLead && !bTapUsed && ContactT < 0.f
		&& !Clash.bOn && !Rep.bOn && Att && Vic && G->Phase == EZoogiPhase::Rolling;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	const float RNow = W ? W->GetRealTimeSeconds() : 0.f;
	if (bIncoming)
	{
		const bool bStart = ReactT0 < 0.f;
		if (bStart)
		{
			ReactT0 = RNow; ReactCloseShot[0] = ReactCloseShot[1] = false;
			if (!ReactCam.IsValid()) { FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; ReactCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP); }
			if (PC && ReactCam.IsValid()) { ReactPrev = PC->GetViewTarget(); PC->SetViewTargetWithBlend(ReactCam.Get(), 0.f); } // instant: a blend would crawl under the slow-mo
			Callout(TEXT("INCOMING!"), TEXT("TAP REACTION TO BLOCK"), FLinearColor(0.3f, 0.85f, 1.f), 1.4f, 0.8f);
			// b58 (Evan: "bring back aurex transformation prior to hit so it's clearly visible ... during the parry slow cut"):
			// the Zoogi about to be hit bursts into its Aurex and squares up in a guard until contact
			static const bool bNoAurex = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiReactAurex")); // b67 (Evan: "stop all transform prior to a hit") - -ZoogiReactAurex = the b58 guard form
			if (!bNoAurex && !ReactPup.IsValid())
				if (AZoogiAurexPuppet* P = AZoogiAurexPuppet::SpawnFor(W, Vic, Vic->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius), (Att->BallLocation() - Vic->BallLocation()).Rotation().Yaw, EZoogiPuppetPose::Guard))
				{ P->bFollowBall = true; ReactPup = P; }
			UE_LOG(LogZoogi, Log, TEXT("FUN incoming: P%d's shot on P%d - slow-mo close-up, %.0f s to block"), Att->PlayerIndex + 1, Vic->PlayerIndex + 1, ZoogiReactCam::Window);
			if (bTest || FParse::Param(FCommandLine::Get(), TEXT("ZoogiReactShots"))) { if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("react_block_start"), 0.1f); G->TakeShotDelayed(TEXT("react_block_mid"), 0.25f); } }
		}
		if (AZoogiAurexPuppet* P = ReactPup.Get()) { const FVector Fd = (Att->BallLocation() - Vic->BallLocation()).GetSafeNormal2D(); if (!Fd.IsNearlyZero()) P->FaceDir = Fd; }
		// b181 (Evan: "bring back the action close-up shots of the opposing right before the side view slow motion reaction call"):
		// two quick hard-cut close-ups of the attacker (head-on, then low side) before the side-on view. -ZoogiNoReactCloseups = off
		static const bool bNoCloseups = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactCloseups"));
		static float CloseSec = -1.f; if (CloseSec < 0.f) { CloseSec = 1.0f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiReactCloseupSec="), CloseSec); }
		const float RAge = RNow - ReactT0;
		ACameraActor* Rc = ReactCam.Get();
		if (!bNoCloseups && Rc && RAge < CloseSec)
		{
			const FVector A0 = Att->BallLocation();
			FVector Fw = Att->GetBallVelocity().GetSafeNormal2D(); if (Fw.IsNearlyZero()) Fw = (Vic->BallLocation() - A0).GetSafeNormal2D(); if (Fw.IsNearlyZero()) Fw = FVector(1, 0, 0);
			const FVector Sd = FVector::CrossProduct(FVector::UpVector, Fw);
			const bool bFirst = RAge < CloseSec * 0.5f;
			const FVector CamAt = bFirst ? A0 + Fw * 260.f + FVector(0, 0, 45.f) // head-on: it rushes straight at the lens
				: A0 + Sd * 210.f + Fw * 90.f + FVector(0, 0, 25.f);              // low side, a little ahead
			Rc->SetActorLocation(CamAt);
			Rc->SetActorRotation((A0 + FVector(0, 0, 15.f) - CamAt).Rotation());
			Rc->GetCameraComponent()->SetFieldOfView(bFirst ? 48.f : 55.f);
			if (bTest && !G->ShotDir.IsEmpty() && !ReactCloseShot[bFirst ? 0 : 1]) { ReactCloseShot[bFirst ? 0 : 1] = true; G->TakeShot(bFirst ? TEXT("react_closeup_a") : TEXT("react_closeup_b")); }
		}
		else ZoogiReactCam::Frame(Rc, Att->BallLocation(), Vic->BallLocation() + FVector(0.f, 0.f, ReactPup.IsValid() ? 60.f : 0.f), RealDt, bStart || (!bNoCloseups && RAge - RealDt < CloseSec));
		const float Left = FMath::Max(ZoogiReactCam::Window - (RNow - ReactT0), 0.25f);
		ReactDil = FMath::Clamp(PredT / Left, 0.02f, 1.f);
	}
	else if (ReactT0 >= 0.f)
	{
		ReactT0 = -1.f; ReactDil = 1.f;
		if (AZoogiAurexPuppet* P = ReactPup.Get()) P->Dismiss(true); // back into the ball at contact / when the window closes
		ReactPup.Reset();
		if (PC && ReactCam.IsValid() && PC->GetViewTarget() == ReactCam.Get()) PC->SetViewTargetWithBlend(ReactPrev.IsValid() ? ReactPrev.Get() : PC->GetPawn(), 0.25f);
		ReactPrev.Reset();
	}
}

void UZoogiFunSys::NewShot(AZoogiGameMode* G)
{
	Shooter = G->GetActiveBall();
	Mode = 0; PredT = -1.f; PredTarget.Reset();
	TapT = -1.f; ContactT = -1.f; ContactOther.Reset();
	bTapUsed = false; bGraded = false; bContactBoosted = false; LastGrade = 0;
	bUltThisShot = false; bUltAim = false; UltBounces = 0; UltRedirectAt = -1.f; UltHoming.Reset(); UltHitMul = 1.f; bClashThisShot = false; bFusedThisShot = false; bDefWinThisShot = false;
	bStrikeThisShot = false; if (Strike.bOn) EndStrike(false);
	EndFlung(false);
	bResolveAfterClash = false;
	bCpuStrikeRolled = false; bCpuStrikeYes = false;
	ShotAge = 0.f;
	// recording for the round highlight
	CommitRec();
	Cur.Reset();
	Cur.Player = G->ActivePlayer;
	Cur.Shooter = Shooter.Get();
	for (AZoogiBall* B : G->Balls) if (B) Cur.Actors.Add(B);
	for (AZoogiOrb* O : G->Orbs) if (O) Cur.Actors.Add(O);
	bRecording = true; RecT = 0.f; RecNext = 0.f;
	// boost pickup spawns mid-roll
	static const bool bBoostOn = FunFlag(TEXT("ZoogiNoBoosts"));
	const float Chance = (bTest && BoostShots < 4) ? 1.f : ZoogiFunTune::BoostChance;
	PickupSpawnAt = (bBoostOn && !Pickup && FMath::FRand() < Chance) ? 0.35f : -1.f;
}

void UZoogiFunSys::FunTick(float Dt)
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const float RealDt = FMath::Min((float)FApp::GetDeltaTime(), 0.05f);
	for (FCallout& C : Callouts) C.Age += RealDt;
	ZoogiBanter::Tick(RealDt); // b67
	Callouts.RemoveAll([](const FCallout& C) { return C.Age > C.Life; });
	if (!G) return;
	if (G->Phase == EZoogiPhase::Menu && (Players.Num() > 0 || bFinalAnnounced)) { ResetMatch(G); LastRound = 0; }
	const bool bAct = IsActive(G);
	ZoogiEdgeHang::Tick(G, RealDt); // b58 (Evan): edge hang mini-game
	ZoogiStandForms::Tick(G, RealDt); // b63 (Evan): everyone starts standing in its ultimate form
	ZoogiSkills::Tick(G, RealDt); // b63: Fab skills (scheduled hits, ice walls, apple mines, shields)
	ZoogiBounty::Tick(G, RealDt); // b179: bounty + turn challenges
	ZoogiGattling::Tick(G, RealDt); // b182: SHOWTIME (Gattling)
	ZoogiBirds::Tick(G, RealDt); // b179: Meadow songbirds
	ZoogiEarthEvents::Tick(G, RealDt); // b180: Earthbender map events
	ZoogiDemoReel::Tick(G, RealDt); // b213: the previs reel (-ZoogiDemoReel only)
	ZoogiWallCine::Tick(G, RealDt); // b230: the wall-smash cinematic
	ZoogiShowtime::DarkTick(G); // b243: Settings > DARK ARENA
	ZoogiVoyager::Tick(G, RealDt); // b233: the Voyager raid crew
	ZoogiRedirect::Tick(G, RealDt); // b66 (Evan): REACTION mid-roll redirects (crystals), combo, CPU spends earned crystals
	ZoogiGuide::Tick(G, RealDt); // b66: guided first turn
	ZoogiVortexRings::Tick(G, Dt); // b65 (Evan): Frozen rings suck things in and fire them back toward the centre
	ZoogiArcCam::Tick(G, Dt); // b59: ARC launch flight camera
	ZoogiBossAttack::Tick(G, RealDt); // b146 (Evan): the Cosmos boss attacks in the closing rounds
	ZoogiSkyDiveCam::Tick(G, RealDt);
	ZoogiRoyaleMix::Tick(G, RealDt); // b149: the royale-mix action round clock
	ZoogiTrickShot::Tick(G, RealDt); // b156: DAILY TRICK SHOT
	ZoogiFlags::Tick(G, RealDt); // b157: the Z flags waving on every arena
	ZoogiParty::Tick(G, RealDt); // b273 PARTY MODES
	ZoogiSpectacle::Tick(G, RealDt); // b268 (Evan: "Yes to all four ideas"): KO moment, shot cam, chain lights, final round pulse
	ZoogiAnimeCues::Tick(G, RealDt); // b164: ANIME DIRECTOR cues (round / final / win / lose, F6)
	ZoogiSoundscape::Tick(G, RealDt); // b163 (Evan): ambience per map, crowd build-up, final drumroll, miss groan
	ZoogiThrone::Tick(G); // b156: the Zoogi King's throne on Ancient Meadows
	ZoogiRaid::Tick(G, RealDt); // b151: RAID mode // b144c (Evan): SKY DIVE - a Zoogi thrown high gets a swoop + slow-mo + ride-down camera
	// time dilation: power clash (near freeze) > ultimate bullet-time aim > CPU fast-forward (never during a replay)
	{
		AZoogiBall* A = G->GetActiveBall();
		float Want = 1.f;
		AZoogiFX::bMuted = false;
		if (bAct && !Rep.bOn)
		{
			AZoogiFX::bMuted = Clash.bOn && !Clash.bField; // b57 (Evan): no sparks during a clash; b104: a field duel shows its hits
			if (ZoogiAnimeCues::WantDilation() > 0.f) Want = ZoogiAnimeCues::WantDilation(); // b164: a director cue pauses / slows the game
			else if (ZoogiEdgeHang::Active() && !ZoogiEdgeHang::View().bVisual) Want = ZoogiEdgeHang::Dilation; // b67: no slow-mo for the visual-only hang // b58: the world waits while the Zoogi hangs on the lip
			else if (ZoogiSkills::DashSlowMo()) Want = 0.3f; // b228: ORB RELAY - a short slow-mo after each dash
			else if (ZoogiSkills::RapidActive()) Want = 0.0001f; // b197 (Evan): GATTLING RAPID pauses the world like a REACTION - the bolts hang, then fly
			else if (ZoogiSkills::Picking()) Want = 0.0001f; // b63: the world waits while a skill is aimed (b67: stops - up to 30 s)
			else if (ZoogiRedirect::Picking()) Want = 0.0001f; // b66: the world waits while a redirect target is picked (b67: stops - up to 30 s)
			else if (ZoogiGuide::WantSlow() && ZoogiRedirect::Available(G)) Want = 0.3f; // b120h: only with a target in the arc range // b66: guided turn - the roll slows while it asks for REACTION
			else if (UZoogiAnimeSubsystem* AnC = W->GetSubsystem<UZoogiAnimeSubsystem>(); AnC && AnC->CrashOn() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeSlow"))) Want = AnC->PowerIntroOn() ? 0.0001f : 0.03f; // b85 (Evan: "remove the gameplay pause during animation"): only with -ZoogiAnimeSlow; // b70: a skill's clip stops the game // b59: the world holds under the crash clip
			else if (DefWin.bOn && !DefWin.bContact) Want = DefWinDilation(); // b106: the defend window (b107b: by stage)
			else if (Clash.bOn) Want = ClashDilation; // build 27: near freeze while the forms fight (they run at 1 / ClashDilation)
			else if (bUltAim) Want = 0.0001f; // b67 (Evan): "game play stops until player chooses targets, has 30 sec" // Oct 30 (Evan): near-freeze while the player reads the card and picks a target
			else if (ReactT0 >= 0.f) Want = ReactDil; // b55: incoming hit - slowed so the player gets ~5 s to BLOCK
			else if (ZoogiBossAttack::SlowMo() > 0.f) Want = ZoogiBossAttack::SlowMo(); // b146: TAP TO BRACE
			else if (ZoogiSkyDiveCam::SlowMo() > 0.f) Want = ZoogiSkyDiveCam::SlowMo(); // b144c: SKY HIGH! swoop
			else if (ZoogiSpectacle::SlowMo() > 0.f) Want = ZoogiSpectacle::SlowMo(); // b268: the knockout moment
			else if (bFast && A && A->bCPU && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving)) Want = ZoogiFunTune::FastDil * (FastLevel >= 2 ? 1.5f : 1.f); // b192: 3x on the second tap
		}
		if (FMath::Abs(Want - CurDil) > 0.001f) { UGameplayStatics::SetGlobalTimeDilation(W, Want); CurDil = Want; }
		bDilApplied = CurDil != 1.f;
	}
	// b67 (Evan: "When awaken is activated the arena glows are brighter"): an unbound post-process volume blends in more bloom,
	// a touch of exposure and saturation while a Zoogi is awakened / an Awaken infuse is running (-ZoogiNoAwakenGlow)
	{
		static const bool bGlowOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAwakenGlow"));
		bool bAwakeNow = bAct && G->bStandbyAwaken;
		for (const FPlayerFun& Pf : Players) if (Pf.bAwake) bAwakeNow = true;
		const float WantK = (!bGlowOff && bAwakeNow) ? 1.f : 0.f;
		AwakenGlowK = FMath::FInterpConstantTo(AwakenGlowK, WantK, RealDt, 1.6f);
		APostProcessVolume* V = AwakenGlowVol.Get();
		if (!V && AwakenGlowK > 0.f)
		{
			FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			V = W->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, SP);
			if (V)
			{
				V->bUnbound = true; V->Priority = 50.f;
				FPostProcessSettings& S = V->Settings;
				S.bOverride_BloomIntensity = true; S.BloomIntensity = 2.2f;
				S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.55f;
				S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.18f, 1.18f, 1.18f, 1.f);
				S.bOverride_SceneColorTint = true; S.SceneColorTint = FLinearColor(1.f, 0.96f, 0.86f);
				AwakenGlowVol = V;
				UE_LOG(LogZoogi, Log, TEXT("AWAKEN GLOW volume up"));
			}
		}
		if (V) V->BlendWeight = AwakenGlowK;
	}
	if (!bAct) { PrevPhase = (int32)G->Phase; return; }
	if (Players.Num() != G->Balls.Num()) Players.SetNum(G->Balls.Num());
	if (!bFormsLoaded) PreloadForms(); // build 26

	if (Rep.bOn && Strike.bOn) EndStrike(false);
	// build 29 (Evan): during an anime cut-in the focused Zoogi is out of its ball, animating in its ultimate form
	if (UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>())
	{
		AZoogiBall* Fb = (An->bPlaying && !Clash.bOn && !Rep.bOn) ? Cast<AZoogiBall>(An->Current.Focus.Get()) : nullptr;
		if (Fb && !Fb->bOut && FormIndex(Fb) >= 0 && Flung.Ball.Get() != Fb && !(Strike.bOn && Strike.Ball.Get() == Fb))
		{
			const FName Ev = An->Current.Event;
			const FString Move = Ev == TEXT("KO") ? FString(TEXT("cheer")) : (Ev == TEXT("GuardBlock") || Ev == TEXT("Guard") ? FString(TEXT("box_01")) : (Ev == TEXT("Miss") ? FString(TEXT("hit_to_body_01")) : FormSignature(FormIndex(Fb))));
			StartStrike(G, Fb, nullptr, 0.f, Move, true, FMath::Max(0.8f, An->Length - An->Age + 0.25f));
			bCutInStrike = Strike.bOn;
			if (bCutInStrike) HideFxAround(Fb->BallLocation(), 1600.f, Fb, Strike.Actor.Get()); // the form must read, not the bursts
			if (bCutInStrike) UE_LOG(LogZoogi, Log, TEXT("FUN cut-in %s: P%d %s shown in its ultimate form (%s)"), *Ev.ToString(), Fb->PlayerIndex + 1, *Fb->Def.Name, *Move);
		}
		else if (bCutInStrike && !An->bPlaying) { if (Strike.bOn) EndStrike(true); bCutInStrike = false; if (!Clash.bOn) RestoreHiddenFx(); }
	}
	if (Rep.bOn) { TickReplay(G, RealDt); PrevPhase = (int32)G->Phase; return; }
	if (Clash.bOn) TickClash(G, RealDt);

	const bool bRolling = G->Phase == EZoogiPhase::Rolling;
	if (bRolling && PrevPhase != (int32)EZoogiPhase::Rolling) NewShot(G);
	PrevPhase = (int32)G->Phase;
	// -ZoogiNoAimTest (proofs): the human presses DRIVE on the aim step before aiming at anything -> must launch along the arrow
	{
		static const bool bNoAimTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAimTest"));
		if (bNoAimTest && G->Phase == EZoogiPhase::Aiming && G->IsHumanAiming() && G->IsAimStep() && !G->HasAim())
		{
			UE_LOG(LogZoogi, Log, TEXT("FUN no-aim test: pressing DRIVE with no aim"));
			G->AimPreset(2);
			Shot(G, TEXT("noaim_launch"), 0.4f);
		}
	}
	// -ZoogiP5CritTest (P5 brief step 2 proofs): Blue lands a critical 18 s in -> the P5 character cut-in (frames p5crit_NN)
	{
		static const bool bCritTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CritTest"));
		static bool bCritDone = false;
		if (bCritTest && !bCritDone && GetWorld()->GetRealTimeSeconds() > 18.f)
			for (AZoogiBall* Cb : G->Balls)
				if (Cb && !Cb->bOut && Cb->Def.Name == TEXT("Blue"))
				{
					bCritDone = true;
					UE_LOG(LogZoogi, Log, TEXT("P5 crit test: Blue critical"));
					G->EmitAnime(TEXT("Critical"), Cb, EZoogiArtifact::Frost, Cb, Cb->BallLocation(), 2.f, true);
					// b57 proof: the critical-hit lightning (player colour from the Zoogi, blue from the nearest orbs)
					AZoogiLightning::Burst(GetWorld(), Cb, ZoogiUtil::PlayerColor(Cb->PlayerIndex), 7, 380.f, 1.6f);
					{ int32 Nn = 0; for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist(O->OrbLocation(), Cb->BallLocation()) < 900.f && Nn < 3) { ++Nn; AZoogiLightning::Burst(GetWorld(), O, FLinearColor(0.2f, 0.55f, 1.f), 5, 260.f, 1.6f); } }
					G->TakeShotDelayed(TEXT("crit_lightning_a"), 2.4f); G->TakeShotDelayed(TEXT("crit_lightning_b"), 2.8f);
					break;
				}
	}
	// -ZoogiArcTest (proofs): the human turns ARC on at every aim step
	{
		static const bool bArcTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiArcTest"));
		static int32 ArcShots = 0;
		if (bArcTest && G->Phase == EZoogiPhase::Aiming && G->IsHumanAiming() && G->IsAimStep() && !G->bArcShot)
		{
			G->MenuAction(TEXT("AimArc"), 0);
			++ArcShots;
			Shot(G, FString::Printf(TEXT("arc_%d_on"), ArcShots), 0.6f);
		}
	}
	// -ZoogiClashTest (proof captures only): every shot clashes with the nearest rival 2.2 s into the roll (after the pad flight) (human and CPU turns)
	if (bClashTestOn && bRolling && !bClashThisShot && !Clash.bOn && ShotAge > 2.2f && G->Phase == EZoogiPhase::Rolling)
		if (AZoogiBall* Sh = Shooter.Get())
		{
			AZoogiBall* Near = nullptr; float NearD = 1e12f;
			for (AZoogiBall* Rb : G->Balls)
				if (Rb && Rb != Sh && !Rb->bOut && !G->IsAlly(Rb, Sh) && FMath::Abs(Rb->BallLocation().Z - Sh->BallLocation().Z) < 250.f) { const float Dd = FVector::DistSquared(Rb->BallLocation(), Sh->BallLocation()); if (Dd < NearD) { NearD = Dd; Near = Rb; } }
			if (Near && !Sh->bOut) StartClash(G, Sh, Near);
		}

	// pending knock-speed changes (applied the frame after the contact: post-collision velocities)
	for (const FPendMul& PM : PendMul)
	{
		AActor* A = PM.A.Get();
		UPrimitiveComponent* Pr = A ? Cast<UPrimitiveComponent>(A->GetRootComponent()) : nullptr;
		if (!Pr || !Pr->IsSimulatingPhysics()) continue;
		const FVector V = Pr->GetPhysicsLinearVelocity();
		Pr->SetPhysicsLinearVelocity(FVector(V.X * PM.Mul, V.Y * PM.Mul, V.Z));
		UE_LOG(LogZoogi, Log, TEXT("FUN knock x%.2f on %s (%.0f -> %.0f cm/s)"), PM.Mul, *A->GetName(), FVector2D(V.X, V.Y).Size(), FVector2D(V.X, V.Y).Size() * PM.Mul);
	}
	PendMul.Reset();

	if (bRolling)
	{
		ShotAge += Dt;
		Predict(G);
		MaybeStrike(G); // build 26: ultimate strike
		if (Mode != 0 && PredT >= 0.f && PredT <= ZoogiFunTune::RingLead && !bTapUsed)
		{
			if (PredT < 0.55f && PredT > 0.15f && RingShots < 3) Shot(G, FString::Printf(TEXT("fun_ring_%02d_%s"), ++RingShots, Mode == 1 ? TEXT("strike") : TEXT("parry")));
			if (bTest && PredT <= 0.035f) OnTap(); // test: tap on the contact
		}
		// a tap long before a contact that never came
		if (TapT >= 0.f && !bGraded && ContactT < 0.f && Now() - TapT > 0.45f)
		{
			bGraded = true;
			Callout(TEXT("TOO EARLY"), FString(), FLinearColor(0.65f, 0.7f, 0.8f), 0.8f, 0.6f);
		}
		if (bTest && CpuTurnsSeen == 2 && !bFast && Mode == 0 && ShotAge > 0.3f && TestCpuTaps == 0 && G->GetActiveBall() && G->GetActiveBall()->bCPU) { ++TestCpuTaps; OnTap(); }
	}
	else { Mode = 0; PredT = -1.f; }
	UpdateReact(G, RealDt);

	{
		// b63 test: -ZoogiParryShieldTest puts the parry wood shield on the human Zoogi at its first aim (size check shots)
		static const bool bPST = FParse::Param(FCommandLine::Get(), TEXT("ZoogiParryShieldTest"));
		static bool bPSTDone = false;
		AZoogiBall* Hb = G->GetActiveBall();
		if (bPST && !bPSTDone && Hb && !Hb->bCPU && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 1.5f)
		{
			bPSTDone = true; ParryShield(W, Hb);
			if (!G->ShotDir.IsEmpty()) { G->TakeShotDelayed(TEXT("parry_shield_t05"), 0.5f); G->TakeShotDelayed(TEXT("parry_shield_t07"), 0.75f); G->TakeShotDelayed(TEXT("parry_shield_t10"), 1.0f); G->TakeShotDelayed(TEXT("parry_shield_t14"), 1.4f); }
		}
	}
	TickBoostPickup(G, Dt);
	TickStrike(G, Dt);
	if (Flung.Actor.IsValid()) TickFlung(G, Dt); // build 28
	if (Players.IsValidIndex(G->ActivePlayer) && !Clash.bOn) Players[G->ActivePlayer].DuelReserve = 2; // b96: the duel reserve refills on your own turn
	{	// b95c (Evan: "game stalls after the new duel"): real-time watchdogs - nothing may hold a match up for long
		static double ClashSince = 0.0, FlungSince = 0.0, PhaseSince = 0.0; static EZoogiPhase WdPh = EZoogiPhase::Menu; static int32 WdTurn = -1;
		const double Nw = FPlatformTime::Seconds();
		if (!Clash.bOn) ClashSince = Nw; else if (Nw - ClashSince > 40.0) { UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: clash/duel on %.0f s -> ended"), Nw - ClashSince); ClashSince = Nw; if (Clash.bV2) DuelV2End(G, false); else EndClash(G); }
		if (!Flung.Actor.IsValid()) FlungSince = Nw; else if (Nw - FlungSince > 8.0) { UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: flung loser %.0f s -> landed"), Nw - FlungSince); FlungSince = Nw; EndFlung(true); }
		if (G->Phase != WdPh || G->TurnCounter != WdTurn) { WdPh = G->Phase; WdTurn = G->TurnCounter; PhaseSince = Nw; }
		else if (G->Phase == EZoogiPhase::Rolling && !Clash.bOn && Nw - PhaseSince > 20.0)
		{
			UE_LOG(LogZoogi, Warning, TEXT("WATCHDOG: P%d's shot rolling %.0f s (resolve pending %d) -> resolved"), G->ActivePlayer + 1, Nw - PhaseSince, bResolveAfterClash ? 1 : 0);
			PhaseSince = Nw; bResolveAfterClash = false; ResolveHoldT = 0.f;
			G->ForceResolveNow(TEXT("watchdog: shot stuck"));
		}
	}
	if (bResolveAfterClash && !Clash.bOn && !Flung.Actor.IsValid())
	{
		UZoogiAnimeSubsystem* AnR = W->GetSubsystem<UZoogiAnimeSubsystem>();
		if (AnR && (AnR->CrashOn() || AnR->IsBusy())) ResolveHoldT = 0.f;
		else ResolveHoldT += RealDt;
		if (ResolveHoldT > 0.25f && !ZoogiEdgeHang::Active())
		{
			UE_LOG(LogZoogi, Log, TEXT("FUN clash scene finished (crash %d busy %d) -> resolve"), AnR && AnR->CrashOn() ? 1 : 0, AnR && AnR->IsBusy() ? 1 : 0);
			bResolveAfterClash = false; ResolveHoldT = 0.f;
			G->ForceResolveNow(TEXT("clash scene over -> next turn"));
		}
	}
	TickUltimate(G, Dt, RealDt);
	ZoogiEpic::Tick(G, RealDt); // b103
	TickDefendWindow(G, RealDt); // b106
	TickArenaEvent(G, Dt);

	// on-fire flames + crowd hype
	FireFxT += Dt;
	bool bAnyFire = false;
	if (FireFxT > 0.3f)
	{
		FireFxT = 0.f;
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			if ((!Players[i].bFire && !Players[i].bAwake) || !G->Balls.IsValidIndex(i)) continue;
			AZoogiBall* B = G->Balls[i];
			if (!B || B->bOut || B->IsHidden()) continue;
			if (Players[i].bAwake) AZoogiFX::Spawn(W, B->BallLocation(), FLinearColor(1.f, 0.85f, 0.3f), 150.f, 0.4f, true, 3.5f, B); // awakened aura (b157: fainter ring)
			if (!Players[i].bFire) continue;
			bAnyFire = true;
			const FVector L = B->BallLocation();
			AZoogiFX::Spawn(W, L - FVector(0, 0, 20.f), FMath::RandBool() ? FLinearColor(1.f, 0.45f, 0.08f) : FLinearColor(1.f, 0.75f, 0.15f), 105.f, 0.45f, false, 9.f, B);
			if (FMath::RandBool()) AZoogiFX::Niagara(W, FMath::RandBool() ? TEXT("NS_Burst_Red") : TEXT("NS_Burst_Yellow"), L + FVector(0, 0, 30.f), FRotator::ZeroRotator, 0.2f); // b157 (Evan: "thin the circle particle"): smaller, half as often
		}
		if (bAnyFire) if (UZoogiShowSys* SS = W->GetSubsystem<UZoogiShowSys>()) if (SS->Crowd.IsValid()) SS->Crowd->Hype = FMath::Max(SS->Crowd->Hype, 0.55f);
	}

	Record(G);
	if (bTest && CpuTurnsSeen >= 2 && !bFast && TestCpuTaps == 0 && G->Phase == EZoogiPhase::Aiming && G->PhaseTime > 0.25f && G->GetActiveBall() && G->GetActiveBall()->bCPU) { ++TestCpuTaps; OnTap(); }
}

// ------------------------------------------------------------------ boost pickup

void UZoogiFunSys::SpawnBoostPickup(AZoogiGameMode* G)
{
	UWorld* W = GetWorld();
	AZoogiBall* Sh = Shooter.Get();
	if (!W || !Sh) return;
	const FVector Ls = Sh->BallLocation();
	FVector V = Sh->GetBallVelocity(); V.Z = 0.f;
	FVector D = V.Size() > 50.f ? V.GetSafeNormal() : FRotator(0.f, Sh->AimYaw, 0.f).Vector();
	const float KR = G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
	for (int32 Try = 0; Try < 24; ++Try)
	{
		const float Ang = FMath::FRandRange(-22.f, 22.f), Dist = FMath::FRandRange(300.f, 720.f);
		const FVector P = Ls + FRotator(0.f, Ang, 0.f).RotateVector(D) * Dist;
		const FVector2D P2(P.X, P.Y);
		if ((G->Arena && G->Arena->IsOffCourt(P2)) || AZoogiArena::IsRingOut(P) || P2.Size() > KR - 260.f) continue;
		bool bClear = true;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut && FVector::Dist2D(B->BallLocation(), P) < 160.f) bClear = false;
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist2D(O->OrbLocation(), P) < 140.f) bClear = false;
		if (!bClear) continue;
		PickupKind = FMath::RandBool() ? EBoost::Heavy : EBoost::Double;
		const FLinearColor Col = PickupKind == EBoost::Heavy ? FLinearColor(1.f, 0.35f, 0.1f) : FLinearColor(1.f, 0.8f, 0.15f);
		PickupAt = FVector(P.X, P.Y, Ls.Z + 30.f);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(PickupAt), SP);
		if (!A) return;
		USceneComponent* Root = NewObject<USceneComponent>(A, TEXT("Root"));
		Root->RegisterComponent(); A->SetRootComponent(Root); A->SetActorLocation(PickupAt);
		auto Mesh = [&](const TCHAR* Shape, const FVector& Sc, float Glow, const FRotator& R)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
			C->SetupAttachment(Root);
			C->SetStaticMesh(ZoogiUtil::BasicShape(Shape));
			C->SetMaterial(0, ZoogiUtil::GlowMID(C, Col, Glow));
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->SetRelativeScale3D(Sc); C->SetRelativeRotation(R);
			C->RegisterComponent();
		};
		Mesh(TEXT("Cube"), FVector(0.6f), 9.f, FRotator(45.f, 0.f, 45.f));
		Mesh(TEXT("Sphere"), FVector(0.9f), 2.5f, FRotator::ZeroRotator);
		Pickup = A; PickupAge = 0.f;
		AZoogiFX::Spawn(W, PickupAt, Col, 140.f, 0.5f, true, 10.f);
		AZoogiFX::Niagara(W, PickupKind == EBoost::Heavy ? TEXT("NS_Burst_Red") : TEXT("NS_Burst_Yellow"), PickupAt, FRotator::ZeroRotator, 0.6f);
		ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.45f, 1.8f);
		++BoostShots;
		UE_LOG(LogZoogi, Log, TEXT("FUN boost pickup %s at (%.0f, %.0f)"), PickupKind == EBoost::Heavy ? TEXT("HEAVY") : TEXT("DOUBLE"), P.X, P.Y);
		Shot(G, FString::Printf(TEXT("fun_boost_spawn_%02d"), BoostShots), 0.3f);
		return;
	}
}

void UZoogiFunSys::KillPickup(bool bPoof)
{
	if (Pickup)
	{
		if (bPoof && GetWorld()) AZoogiFX::Spawn(GetWorld(), Pickup->GetActorLocation(), FLinearColor(0.8f, 0.8f, 0.85f), 90.f, 0.35f, false, 5.f);
		Pickup->Destroy();
	}
	Pickup = nullptr;
	PickupSpawnAt = -1.f;
}

void UZoogiFunSys::TickBoostPickup(AZoogiGameMode* G, float Dt)
{
	if (PickupSpawnAt >= 0.f && G->Phase == EZoogiPhase::Rolling && ShotAge >= PickupSpawnAt) { PickupSpawnAt = -1.f; SpawnBoostPickup(G); }
	if (!Pickup) return;
	PickupAge += Dt;
	const float Pop = FMath::Clamp(PickupAge / 0.25f, 0.f, 1.f);
	Pickup->SetActorScale3D(FVector(FMath::Max(0.05f, Pop * (1.f + 0.08f * FMath::Sin(PickupAge * 7.f)))));
	Pickup->SetActorLocation(PickupAt + FVector(0, 0, 18.f * FMath::Sin(PickupAge * 3.2f)));
	Pickup->SetActorRotation(FRotator(0.f, PickupAge * 140.f, 0.f));
	for (AZoogiBall* B : G->Balls)
	{
		if (!B || B->bOut || B->bParked) continue;
		const FVector L = B->BallLocation();
		if (FVector::Dist2D(L, PickupAt) > ZoogiFunTune::PickupR || FMath::Abs(L.Z - PickupAt.Z) > 160.f) continue;
		const int32 P = B->PlayerIndex;
		if (Players.IsValidIndex(P)) Players[P].Pending = PickupKind;
		const bool bHeavy = PickupKind == EBoost::Heavy;
		const FLinearColor Col = bHeavy ? FLinearColor(1.f, 0.4f, 0.12f) : FLinearColor(1.f, 0.82f, 0.2f);
		G->AddPopup(bHeavy ? TEXT("HEAVY HIT  NEXT TURN") : TEXT("DOUBLE POINTS  NEXT TURN"), PickupAt + FVector(0, 0, 120.f), Col);
		Callout(bHeavy ? TEXT("HEAVY HIT BOOST") : TEXT("DOUBLE POINTS BOOST"), FString::Printf(TEXT("%s  GRABBED IT  -  NEXT TURN"), *B->Def.Name.ToUpper()), Col, 1.4f, 0.8f);
		AZoogiFX::Spawn(GetWorld(), PickupAt, Col, 220.f, 0.5f, true, 12.f);
		AZoogiFX::SpawnConfetti(GetWorld(), PickupAt, Col, 18);
		ZoogiUtil::PlaySfx(G, TEXT("Score"), 0.7f, 1.6f);
		UE_LOG(LogZoogi, Log, TEXT("FUN P%d %s grabbed the %s boost"), P + 1, *B->Def.Name, bHeavy ? TEXT("HEAVY") : TEXT("DOUBLE"));
		Shot(G, FString::Printf(TEXT("fun_boost_get_%02d"), BoostShots), 0.12f);
		KillPickup(false);
		return;
	}
}

// ------------------------------------------------------------------ highlight recording + replay

void UZoogiFunSys::Record(AZoogiGameMode* G)
{
	if (!bRecording) return;
	if (G->Phase != EZoogiPhase::Rolling && G->Phase != EZoogiPhase::Resolving) { return; }
	RecT += GetWorld()->GetDeltaSeconds();
	if (RecT > ZoogiFunTune::RecMax) { bRecording = false; return; }
	if (RecT < RecNext) return;
	RecNext = RecT + 1.f / ZoogiFunTune::RecHz;
	FRecFrame F; F.T = RecT;
	F.X.SetNum(Cur.Actors.Num()); F.Vis.SetNum(Cur.Actors.Num());
	for (int32 i = 0; i < Cur.Actors.Num(); ++i)
	{
		AActor* A = Cur.Actors[i].Get();
		F.X[i] = A ? A->GetActorTransform() : FTransform::Identity;
		F.Vis[i] = (A && !A->IsHidden()) ? 1 : 0;
	}
	if (Cur.Frames.Num() < 3)
		if (AZoogiBall* Sh = Shooter.Get()) { FVector V = Sh->GetBallVelocity(); V.Z = 0.f; if (V.Size() > 100.f) Cur.Dir = V.GetSafeNormal(); }
	Cur.Frames.Add(MoveTemp(F));
}

void UZoogiFunSys::CommitRec()
{
	if (Cur.Frames.Num() > 10 && Cur.bAction && Cur.Score > Best.Score) { Best = Cur; UE_LOG(LogZoogi, Log, TEXT("FUN round highlight now: %s (score %.0f)"), *Best.Title, Best.Score); }
	Cur.Reset();
}

bool UZoogiFunSys::StartReplay(AZoogiGameMode* G, bool bRoundEnd)
{
	static const bool bOn = FunFlag(TEXT("ZoogiNoReplay"));
	if (!IsActive(G) || Rep.bOn) return false;
	// build 25 #8: a knockout during an ultimate replays this turn right away (ULTIMATE FINISH)
	const bool bFinisher = bOn && bFinisherPending && Cur.Frames.Num() > 10;
	FRec FinClip;
	if (bFinisher) { FinClip = Cur; FinClip.Title = FString::Printf(TEXT("ULTIMATE FINISH  -  %s"), *Cur.Title); }
	bFinisherPending = false;
	bRecording = false;
	if (bFinisher) Cur.Reset(); else CommitRec(); // the finisher clip never comes back as the round's best moment
	if (!bFinisher && (!bOn || !Best.bAction || Best.Frames.Num() < 10)) { if (bRoundEnd) Best.Reset(); return false; }
	if (!ZoogiControls::ReplayCam()) { if (bRoundEnd) Best.Reset(); return false; } // b70 (Evan): replay cam off unless switched on in Settings
	UWorld* W = GetWorld();
	APlayerController* PC = W->GetFirstPlayerController();
	if (!PC) { if (bRoundEnd) Best.Reset(); return false; }
	Rep = FReplay();
	if (bFinisher)
	{
		Rep.Clip = MoveTemp(FinClip); Rep.bFinisher = true;
		if (bRoundEnd) Best.Reset();
		if (UZoogiShowSys* SS = W->GetSubsystem<UZoogiShowSys>()) SS->Fireworks(6, 1.3f);
		ZoogiUtil::PlaySfx(G, TEXT("CrowdWave1"), 0.9f);
	}
	else { Rep.Clip = MoveTemp(Best); Best.Reset(); }
	const float Dur = Rep.Clip.Frames.Last().T;
	const float Peak = Rep.Clip.ImpactT >= 0.f ? Rep.Clip.ImpactT : 0.6f;
	Rep.T0 = FMath::Max(0.f, Peak - 0.7f);
	Rep.T1 = FMath::Min(Dur, Peak + 2.1f);
	if (Rep.T1 - Rep.T0 < 0.8f) { Rep = FReplay(); return false; }
	Rep.T = Rep.T0;
	// snapshot the live state, then hand every recorded body to the replay (kinematic, no collision)
	for (const TWeakObjectPtr<AActor>& WA : Rep.Clip.Actors)
	{
		AActor* A = WA.Get();
		if (!A) continue;
		FSnap S; S.A = A; S.X = A->GetActorTransform(); S.bHidden = A->IsHidden();
		if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(A->GetRootComponent()))
		{
			S.bSim = Pr->IsSimulatingPhysics(); S.Coll = Pr->GetCollisionEnabled();
			if (S.bSim) { S.V = Pr->GetPhysicsLinearVelocity(); S.W = Pr->GetPhysicsAngularVelocityInRadians(); }
			Pr->SetSimulatePhysics(false);
			Pr->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Rep.Snaps.Add(S);
	}
	Rep.PrevView = PC->GetViewTarget();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Cam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FVector(0, 0, 2000.f)), SP);
	if (Cam) { Cam->GetCameraComponent()->SetFieldOfView(78.f); Cam->GetCameraComponent()->bConstrainAspectRatio = false; }
	Rep.Cam = Cam;
	Rep.bOn = true;
	Rep.Age = 0.f;
	if (CurDil != 1.f) { UGameplayStatics::SetGlobalTimeDilation(W, 1.f); CurDil = 1.f; bDilApplied = false; }
	EndFighters(); EndFlung(false); Clash = FClash(); bUltAim = false;
	KillPickup(false);
	UE_LOG(LogZoogi, Log, TEXT("FUN REPLAY start: %s, %.2f..%.2f s of %.2f (impact %.2f)"), *Rep.Clip.Title, Rep.T0, Rep.T1, Dur, Rep.Clip.ImpactT);
	ZoogiUtil::PlaySfx(G, TEXT("Inhale"), 0.5f, 0.8f);
	TickReplay(G, 0.f);
	return true;
}

void UZoogiFunSys::TickReplay(AZoogiGameMode* G, float RealDt)
{
	UWorld* W = GetWorld();
	Rep.Age += RealDt;
	Rep.T += RealDt * ZoogiFunTune::ReplayRate;
	const TArray<FRecFrame>& Fr = Rep.Clip.Frames;
	if (Rep.T >= Rep.T1 || Fr.Num() < 2) { EndReplay(G); return; }
	int32 I = 0;
	while (I < Fr.Num() - 2 && Fr[I + 1].T < Rep.T) ++I;
	const FRecFrame& F0 = Fr[I]; const FRecFrame& F1 = Fr[I + 1];
	const float A = FMath::Clamp((Rep.T - F0.T) / FMath::Max(F1.T - F0.T, 0.001f), 0.f, 1.f);
	FVector ShP = FVector::ZeroVector, OtP = FVector::ZeroVector; bool bSh = false, bOt = false;
	for (int32 i = 0; i < Rep.Clip.Actors.Num(); ++i)
	{
		AActor* Ac = Rep.Clip.Actors[i].Get();
		if (!Ac || !F0.X.IsValidIndex(i) || !F1.X.IsValidIndex(i)) continue;
		FTransform X;
		X.Blend(F0.X[i], F1.X[i], A);
		Ac->SetActorTransform(X, false, nullptr, ETeleportType::TeleportPhysics);
		const bool bVis = (A < 0.5f ? F0.Vis[i] : F1.Vis[i]) != 0;
		Ac->SetActorHiddenInGame(!bVis);
		if (Ac == Rep.Clip.Shooter.Get()) { ShP = X.GetLocation(); bSh = bVis; }
		if (Ac == Rep.Clip.Other.Get()) { OtP = X.GetLocation(); bOt = bVis; }
	}
	// wide side camera at the after-launch overhead distance (Evan: different angles, not close-ups)
	const float Imp = Rep.Clip.ImpactT >= 0.f ? Rep.Clip.ImpactT : 1e9f;
	const float K = FMath::Clamp((Rep.T - Imp) / 0.8f, 0.f, 1.f) * (bOt ? 0.6f : 0.f);
	FVector Want = bSh ? FMath::Lerp(ShP, OtP, K) : (bOt ? OtP : Rep.Focus);
	if (!Rep.bCamInit) Rep.Focus = Want; else Rep.Focus = FMath::Lerp(Rep.Focus, Want, 1.f - FMath::Exp(-4.f * RealDt));
	FVector D = Rep.Clip.Dir; D.Z = 0.f; D = D.GetSafeNormal(); if (D.IsNearlyZero()) D = FVector::ForwardVector;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, D);
	if (!Rep.bCamInit)
	{
		const FVector2D Pa(Rep.Focus + Side * 1400.f), Pb(Rep.Focus - Side * 1400.f);
		Rep.SideSign = Pa.Size() <= Pb.Size() ? 1.f : -1.f;
	}
	const float Orbit = FMath::DegreesToRadians(8.f * (Rep.T - Rep.T0) / ZoogiFunTune::ReplayRate * 0.5f);
	const FVector SideR = (Side * FMath::Cos(Orbit) - D * FMath::Sin(Orbit)) * Rep.SideSign;
	const FVector CamWant = Rep.Focus + SideR * 1450.f - D * 250.f + FVector(0, 0, 1350.f);
	if (!Rep.bCamInit) Rep.CamP = CamWant; else Rep.CamP = FMath::Lerp(Rep.CamP, CamWant, 1.f - FMath::Exp(-3.f * RealDt));
	Rep.bCamInit = true;
	if (ACameraActor* Cam = Cast<ACameraActor>(Rep.Cam.Get()))
	{
		FVector Eye = Rep.CamP; // b67: portrait pulls the replay camera back
		if (Cam->GetCameraComponent()) Cam->GetCameraComponent()->SetFieldOfView(ZoogiPortrait::SceneFit(W, Rep.Focus, 90.f, Eye));
		Cam->SetActorLocationAndRotation(Eye, (Rep.Focus - Eye).Rotation());
		if (APlayerController* PC = W->GetFirstPlayerController()) if (PC->GetViewTarget() != Cam) PC->SetViewTarget(Cam);
	}
	const float U = (Rep.T - Rep.T0) / FMath::Max(Rep.T1 - Rep.T0, 0.01f);
	if (Rep.Shots == 0 && U > 0.15f) { ++Rep.Shots; Shot(G, TEXT("fun_replay_a")); }
	if (Rep.Shots == 1 && U > 0.5f) { ++Rep.Shots; Shot(G, TEXT("fun_replay_b")); }
	if (Rep.Shots == 2 && U > 0.85f) { ++Rep.Shots; Shot(G, TEXT("fun_replay_c")); }
}

void UZoogiFunSys::EndReplay(AZoogiGameMode* G)
{
	UWorld* W = GetWorld();
	for (const FSnap& S : Rep.Snaps)
	{
		AActor* A = S.A.Get();
		if (!A) continue;
		A->SetActorTransform(S.X, false, nullptr, ETeleportType::TeleportPhysics);
		A->SetActorHiddenInGame(S.bHidden);
		if (UPrimitiveComponent* Pr = Cast<UPrimitiveComponent>(A->GetRootComponent()))
		{
			Pr->SetCollisionEnabled(S.Coll);
			if (S.bSim)
			{
				Pr->SetSimulatePhysics(true);
				Pr->SetPhysicsLinearVelocity(S.V);
				Pr->SetPhysicsAngularVelocityInRadians(S.W);
			}
		}
	}
	if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		if (AActor* Prev = Rep.PrevView.Get()) PC->SetViewTarget(Prev);
	if (AActor* Cam = Rep.Cam.Get()) Cam->Destroy();
	UE_LOG(LogZoogi, Log, TEXT("FUN REPLAY end (%.1f s real)"), Rep.Age);
	Rep = FReplay();
}

// ------------------------------------------------------------------ HUD

static FSlateFontInfo FunFont(float Px)
{
	UFont* F = GEngine->GetLargeFont();
	FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
	Fi.Size = FMath::Max(6.f, FMath::RoundToFloat(Px));
	Fi.TypefaceFontName = FName(TEXT("Bold"));
	ZoogiUIStyle::AddOutline(Fi, FMath::Max(1.5f, Px * 0.09f));
	return Fi;
}

static void FunText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, bool bCenter = true)
{
	if (S.IsEmpty()) return;
	const FSlateFontInfo Fi = FunFont(Px);
	FVector2D Sz(0.f, 0.f);
	if (bCenter && FSlateApplication::IsInitialized()) Sz = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi);
	FCanvasTextItem It(FVector2D(X - Sz.X * 0.5f, Y - Sz.Y * 0.5f), FText::FromString(S), Fi, Col);
	It.EnableShadow(FLinearColor(0.f, 0.f, 0.06f, 0.7f * Col.A), FVector2D(2.f, 3.f));
	C->DrawItem(It);
}

namespace ZoogiModeSel { FSlateFontInfo FontInfo(float Size, float Outline); } // ZoogiModeSelect.cpp: Lilita One, dark outline

// Oct 30: canvas-drawable font: Lilita One (-1) or the P5-style fonts (0 Anton, 1 Archivo Black, 2 Abril Fatface, 3 Bangers),
// each wrapped once in a runtime-cached UFont (the canvas cannot draw a bare composite font)
static FSlateFontInfo FunFontP(int32 Which, float Px)
{
	static UFont* Fonts[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
	static bool bTried[5] = { false, false, false, false, false };
	const int32 I = FMath::Clamp(Which + 1, 0, 4);
	if (!bTried[I])
	{
		bTried[I] = true;
		const FSlateFontInfo F0 = Which < 0 ? ZoogiModeSel::FontInfo(24.f, 0.f) : ZoogiP5::Font(Which, 24.f, 0.f);
		if (F0.CompositeFont.IsValid())
		{
			Fonts[I] = NewObject<UFont>(GetTransientPackage(), *FString::Printf(TEXT("ZoogiCanvasFont%d"), I));
			Fonts[I]->FontCacheType = EFontCacheType::Runtime;
			Fonts[I]->CompositeFont = *F0.CompositeFont;
			Fonts[I]->AddToRoot();
		}
	}
	if (!Fonts[I]) return FunFont(Px);
	FSlateFontInfo Fi(Fonts[I], FMath::Max(6.f, FMath::RoundToFloat(Px)), FName(TEXT("Regular")));
	Fi.OutlineSettings.OutlineSize = FMath::RoundToInt(FMath::Max(1.5f, Px * 0.1f));
	Fi.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
	return Fi;
}

static void FunTextF(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which)
{
	if (S.IsEmpty() || Col.A <= 0.f) return;
	const FSlateFontInfo Fi = FunFontP(Which, Px);
	FVector2D Sz(0.f, 0.f);
	if (FSlateApplication::IsInitialized()) Sz = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi);
	const float Xo = Align == 0 ? X : (Align == 1 ? X - Sz.X * 0.5f : X - Sz.X);
	FCanvasTextItem It(FVector2D(Xo, Y - Sz.Y * 0.5f), FText::FromString(S), Fi, Col);
	C->DrawItem(It);
}

// build 28: the clash panel's chunky comic text (Lilita One). Align 0 = left, 1 = centre, 2 = right.
static void FunTextL(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align = 1)
{
	if (S.IsEmpty() || Col.A <= 0.f) return;
	// the canvas can only draw fonts that have a UFont, so wrap the Lilita One composite in a runtime-cached UFont once
	static UFont* LFont = nullptr; static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const FSlateFontInfo F0 = ZoogiModeSel::FontInfo(24.f, 0.f);
		if (F0.CompositeFont.IsValid())
		{
			LFont = NewObject<UFont>(GetTransientPackage(), TEXT("ZoogiClashLilita"));
			LFont->FontCacheType = EFontCacheType::Runtime;
			LFont->CompositeFont = *F0.CompositeFont;
			LFont->AddToRoot();
		}
		UE_LOG(LogZoogi, Log, TEXT("FUN clash panel font: %s"), LFont ? TEXT("Lilita One (runtime UFont)") : TEXT("fallback (engine bold)"));
	}
	FSlateFontInfo Fi = LFont ? FSlateFontInfo(LFont, FMath::Max(6.f, FMath::RoundToFloat(Px)), FName(TEXT("Regular"))) : FunFont(Px);
	if (LFont) { Fi.OutlineSettings.OutlineSize = FMath::RoundToInt(FMath::Max(1.5f, Px * 0.12f)); Fi.OutlineSettings.OutlineColor = FLinearColor(0.06f, 0.04f, 0.1f, 1.f); }
	FVector2D Sz(0.f, 0.f);
	if (FSlateApplication::IsInitialized()) Sz = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi);
	const float Xo = Align == 0 ? X : (Align == 1 ? X - Sz.X * 0.5f : X - Sz.X);
	FCanvasTextItem It(FVector2D(Xo, Y - Sz.Y * 0.5f), FText::FromString(S), Fi, Col);
	It.EnableShadow(FLinearColor(0.f, 0.f, 0.06f, 0.75f * Col.A), FVector2D(2.f, 3.f));
	C->DrawItem(It);
}

static void FunRing(UCanvas* C, const FVector2D& Ctr, float R, float Th, const FLinearColor& Col)
{
	const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
	const int32 N = 48;
	TArray<FCanvasUVTri> Tris; Tris.Reserve(N * 2);
	for (int32 i = 0; i < N; ++i)
	{
		const float A0 = 2.f * PI * i / N, A1 = 2.f * PI * (i + 1) / N;
		const FVector2D D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
		FCanvasUVTri T1, T2;
		T1.V0_Pos = Ctr + D0 * (R - Th * 0.5f); T1.V1_Pos = Ctr + D0 * (R + Th * 0.5f); T1.V2_Pos = Ctr + D1 * (R + Th * 0.5f);
		T2.V0_Pos = Ctr + D0 * (R - Th * 0.5f); T2.V1_Pos = Ctr + D1 * (R + Th * 0.5f); T2.V2_Pos = Ctr + D1 * (R - Th * 0.5f);
		T1.V0_Color = T1.V1_Color = T1.V2_Color = Col; T2.V0_Color = T2.V1_Color = T2.V2_Color = Col;
		Tris.Add(T1); Tris.Add(T2);
	}
	FCanvasTriangleItem TI(Tris, WhiteT);
	TI.BlendMode = SE_BLEND_Translucent;
	C->DrawItem(TI);
}

static void FunRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col)
{
	const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
	FCanvasTileItem T(FVector2D(X, Y), WhiteT, FVector2D(W, H), Col);
	T.BlendMode = SE_BLEND_Translucent;
	C->DrawItem(T);
}

// b58 (Evan): edge hang mini-game panel - HANG ON! title, PULL bar in the player's colour, countdown, then SAVED / LET GO
static void DrawEdgeHang(UCanvas* C, float Wd, float Ht, float U)
{
	const ZoogiEdgeHang::FView& V = ZoogiEdgeHang::View();
	const bool bResult = !V.bOn && V.Result > 0 && V.ResultAge >= 0.f && V.ResultAge < 1.4f;
	if (!V.bOn && !bResult) return;
	const FLinearColor Pc = ZoogiUtil::PlayerColor(V.Player);
	const float Tn = FPlatformTime::Seconds();
	auto Al = [](const FLinearColor& Cc, float A) { FLinearColor R = Cc; R.A = A; return R; };
	if (bResult)
	{
		const float K = FMath::Clamp(V.ResultAge / 0.12f, 0.f, 1.f) * FMath::Clamp((1.4f - V.ResultAge) / 0.3f, 0.f, 1.f);
		const float Pop = 1.f + 0.3f * FMath::Max(0.f, 1.f - V.ResultAge / 0.18f);
		FunRect(C, 0.f, Ht * 0.17f - 70.f * U, Wd, 140.f * U, FLinearColor(0.02f, 0.f, 0.05f, 0.6f * K));
		FunTextL(C, V.Result == 1 ? TEXT("SAVED!") : (V.bVisual ? TEXT("KNOCKOUT!") : TEXT("LET GO!")), Wd * 0.5f, Ht * 0.17f - 8.f * U, 92.f * U * Pop, Al(V.Result == 1 ? FLinearColor(1.f, 0.86f, 0.3f) : FLinearColor(1.f, 0.35f, 0.3f), K));
		FunTextL(C, V.Result == 1 ? (V.bFree ? FString(TEXT("BACK ON THE COURT")) : FString::Printf(TEXT("BACK ON THE COURT   -%d SP%s"), ZoogiEdgeHang::Cost, V.bNextTurn ? TEXT(" NEXT TURN") : TEXT(""))) : FString(TEXT("FELL OFF THE EDGE")), Wd * 0.5f, Ht * 0.17f + 48.f * U, 30.f * U, Al(FLinearColor::White, K));
		return;
	}
	const float In = FMath::Clamp((V.Phase == 0 ? V.PhaseAge : 1.f) / 0.2f, 0.f, 1.f);
	// title plate
	const float TY = Ht * 0.13f;
	FunRect(C, 0.f, TY - 66.f * U, Wd, 132.f * U, FLinearColor(0.02f, 0.f, 0.05f, 0.62f * In));
	FunRect(C, 0.f, TY + 62.f * U, Wd, 6.f * U, Al(Pc, In));
	const FString Title = V.Phase == 2 || V.Phase == 4 ? FString(TEXT("PULLED UP!")) : ((V.Phase == 3 || V.bVisual) ? FString(TEXT("LOSING GRIP!")) : FString(TEXT("HANG ON!")));
	const float Shake = V.Phase == 1 ? 3.f * U * FMath::Sin(Tn * 40.f) : 0.f;
	FunTextL(C, Title, Wd * 0.5f + Shake, TY - 12.f * U, 84.f * U * (1.f + 0.2f * (1.f - In)), Al(FMath::Lerp(FLinearColor(1.f, 0.9f, 0.35f), FLinearColor::White, 0.25f + 0.25f * FMath::Sin(Tn * 9.f)), In));
	const FString Sub = V.bVisual ? FString::Printf(TEXT("P%d %s IS SLIPPING OFF THE EDGE"), V.Player + 1, *V.Name.ToUpper()) : V.bHuman ? (V.bFree ? FString::Printf(TEXT("P%d %s  -  TAP TO PULL UP"), V.Player + 1, *V.Name.ToUpper()) : FString::Printf(TEXT("P%d %s  -  TAP TO PULL UP  (%d SP%s)"), V.Player + 1, *V.Name.ToUpper(), ZoogiEdgeHang::Cost, V.bNextTurn ? TEXT(" NEXT TURN") : TEXT("")))
		: FString::Printf(TEXT("P%d %s IS HANGING ON"), V.Player + 1, *V.Name.ToUpper());
	FunTextL(C, Sub, Wd * 0.5f, TY + 40.f * U, 28.f * U, Al(FLinearColor::White, In));
	if (V.Phase > 1 || V.bVisual) return; // b67: no PULL bar - it always falls
	// PULL bar
	const float BW = FMath::Min(Wd * 0.8f, 760.f * U), BH = 40.f * U, BX = Wd * 0.5f - BW * 0.5f, BY = Ht * 0.8f;
	FunRect(C, BX - 6.f * U, BY - 6.f * U, BW + 12.f * U, BH + 12.f * U, FLinearColor(0.02f, 0.f, 0.05f, 0.85f * In));
	FunRect(C, BX, BY, BW, BH, FLinearColor(0.12f, 0.1f, 0.16f, 0.9f * In));
	FunRect(C, BX, BY, BW * V.Prog, BH, Al(FMath::Lerp(Pc, FLinearColor::White, 0.15f + 0.15f * FMath::Sin(Tn * 14.f)), In));
	FunRect(C, BX + BW - 4.f * U, BY - 10.f * U, 4.f * U, BH + 20.f * U, Al(FLinearColor(1.f, 0.86f, 0.3f), In)); // the top of the ledge
	FunTextL(C, TEXT("PULL"), BX - 18.f * U, BY + BH * 0.5f, 34.f * U, Al(FLinearColor::White, In), 2);
	const FLinearColor TimeC = V.Left < 1.f ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor::White;
	FunTextL(C, FString::Printf(TEXT("%.1f"), V.Left), BX + BW + 22.f * U, BY + BH * 0.5f, 40.f * U, Al(TimeC, In), 0);
}

// b63 (Evan: "The horizontal banners like homing strike and meteor strike moved off center to diagonal corner in p5 style with
// that player speech bubble commented at top"): callouts and the ultimate banner are a slanted P5 plate in the top-left corner;
// when a Zoogi is behind one, its speech bubble (face + one line) pops at the top. -ZoogiOldBanners = the centred bands.
static bool P5BannersOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBanners")); return b; }
static void P5Tri(UCanvas* C, const FVector2D& A, const FVector2D& B, const FVector2D& D, const FLinearColor& Col)
{
	const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
	TArray<FCanvasUVTri> Tris; FCanvasUVTri T; T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = D; T.V0_Color = T.V1_Color = T.V2_Color = Col; Tris.Add(T);
	FCanvasTriangleItem TI(Tris, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
}
static void P5Quad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col)
{
	const float A = FMath::DegreesToRadians(AngDeg);
	const FVector2D Ax(FMath::Cos(A), FMath::Sin(A)), Dn(-FMath::Sin(A), FMath::Cos(A));
	const FVector2D P0 = Ctr - Ax * (Len * 0.5f + Skew) + Dn * (Hgt * 0.5f), P1 = Ctr - Ax * (Len * 0.5f - Skew) - Dn * (Hgt * 0.5f);
	const FVector2D P2 = Ctr + Ax * (Len * 0.5f + Skew) - Dn * (Hgt * 0.5f), P3 = Ctr + Ax * (Len * 0.5f - Skew) + Dn * (Hgt * 0.5f);
	P5Tri(C, P0, P1, P2, Col); P5Tri(C, P0, P2, P3, Col);
}
static FVector2D P5Measure(const FString& S, const FSlateFontInfo& Fi)
{
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Fi) : FVector2D(S.Len() * Fi.Size * 0.6f, Fi.Size);
}
static void P5CornerBanner(UCanvas* C, float Wd, float Ht, float U, const FString& Text, const FString& Sub, const FLinearColor& Col, float Age, float Life, float Size, int32 Band = 0)
{
	const bool bPort = Ht > Wd;
	const float In = FMath::Clamp(Age / 0.16f, 0.f, 1.f), Out = FMath::Clamp((Life - Age) / 0.25f, 0.f, 1.f);
	if (Out <= 0.f) return;
	const float Ease = 1.f - FMath::Pow(1.f - In, 3.f);
	const float Ang = -8.f, Rad = FMath::DegreesToRadians(Ang);
	const FVector2D Ax(FMath::Cos(Rad), FMath::Sin(Rad)), Dn(-FMath::Sin(Rad), FMath::Cos(Rad));
	float Px = 60.f * U * FMath::Clamp(Size, 0.7f, 1.1f) / 0.9f * (bPort ? 1.25f : 1.f);
	const float MaxLen = Band == 1 ? (bPort ? Wd * 0.70f : Wd * 0.42f) : (bPort ? Wd * 0.94f : FMath::Min(Wd * 0.48f, Wd * 0.5f - 150.f * U)); // b116: the bottom band stays clear of SUPERPOWER; b132: the top band stops short of the centre ROUND header
	FVector2D TS = P5Measure(Text, FunFontP(0, Px));
	if (TS.X > MaxLen - 90.f * U) { Px *= (MaxLen - 90.f * U) / FMath::Max(1.f, TS.X); TS = P5Measure(Text, FunFontP(0, Px)); }
	const float Len = FMath::Min(MaxLen, TS.X + 120.f * U), Hgt = Px * 1.5f, Sk = 16.f * U;
	const float Slide = (1.f - Ease) * (Len + 240.f * U) + (1.f - Out) * Len * 0.5f;
	// b63 (Evan 11:38: "Banners move from top to bottom right"): bottom-right, above the SUPERPOWER button, sliding in from the right
	static const bool bRightBanners = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBannersRight"));
	const FVector2D Ctr = bRightBanners ? FVector2D(Wd - Len * 0.5f - 24.f * U + Slide, Ht * (bPort ? 0.66f : 0.57f)) // clear of the SUPERPOWER button
		: FVector2D(Len * 0.5f + 24.f * U - Slide, Ht * (Band == 1 ? (bPort ? 0.875f : 0.86f) : (bPort ? 0.20f : 0.19f))); // b79: left side, sliding in from the left; b116 (Evan): the top band or the bottom band - never the middle of the field
	FLinearColor Cc = Col; Cc.A = 0.95f * Out;
	P5Quad(C, Ctr + FVector2D(14.f * U, 14.f * U), Len, Hgt, Ang, Sk, Cc);                                   // colour slab behind
	P5Quad(C, Ctr, Len, Hgt, Ang, Sk, FLinearColor(0.02f, 0.02f, 0.05f, 0.95f * Out));                       // black plate
	P5Quad(C, Ctr + Dn * (Hgt * 0.5f - 5.f * U), Len - 10.f * U, 6.f * U, Ang, Sk, FLinearColor(1.f, 1.f, 1.f, Out)); // white edge
	FunTextF(C, Text, Ctr.X, Ctr.Y - 2.f * U, Px, FLinearColor(1.f, 1.f, 1.f, Out), 1, 0);
	if (!Sub.IsEmpty())
	{
		float Sp = Px * 0.4f;
		FVector2D SS = P5Measure(Sub, FunFontP(1, Sp)) * 1.15f; // b131: the measure runs ~10% short of the drawn text
		if (SS.X > Len - 40.f * U) { Sp *= (Len - 40.f * U) / FMath::Max(1.f, SS.X); SS = P5Measure(Sub, FunFontP(1, Sp)) * 1.15f; }
		const float SL = SS.X + 44.f * U, SH = Sp * 1.6f;
		FVector2D SC = Ctr + Dn * (Hgt * 0.5f + SH * 0.5f + 6.f * U) + Ax * (Len * 0.5f - SL * 0.5f - 40.f * U);
		SC.X = FMath::Max(SC.X, SL * 0.5f + 12.f * U); // b131: never cut off on the left
		P5Quad(C, SC, SL, SH, Ang, 8.f * U, FLinearColor(1.f, 1.f, 1.f, 0.97f * Out));
		FunTextF(C, Sub, SC.X, SC.Y, Sp, FLinearColor(0.02f, 0.02f, 0.05f, Out), 1, 1);
	}
}
static void P5SpeechBubble(UCanvas* C, float Wd, float Ht, float U, UTexture2D* Face, const FString& Name, const FString& Line, const FLinearColor& Col, float Age, float Life, int32 Slot = 0)
{
	if (Line.IsEmpty()) return;
	const bool bPort = Ht > Wd;
	const float In = FMath::Clamp((Age - 0.08f) / 0.14f, 0.f, 1.f), Out = FMath::Clamp((Life + 0.35f - Age) / 0.25f, 0.f, 1.f);
	const float K = In * Out;
	if (K <= 0.f) return;
	const float Pop = In < 1.f ? FMath::Lerp(0.6f, 1.f, In) : 1.f;
	float Px = 30.f * U * (bPort ? 1.3f : 1.f);
	const float F = (bPort ? 120.f : 96.f) * U * Pop;
	const float MaxW = (bPort ? Wd * 0.62f : Wd * 0.36f) - F - 30.f * U; // b116: narrower - it hugs one side
	FVector2D LS = P5Measure(Line, FunFontP(-1, Px));
	if (LS.X > MaxW - 40.f * U) { Px = FMath::Max(14.f * U, Px * (MaxW - 40.f * U) / FMath::Max(1.f, LS.X)); LS = P5Measure(Line, FunFontP(-1, Px)); }
	const float BW = (LS.X + 44.f * U) * Pop, BH = (LS.Y + 30.f * U) * Pop;
	const float Tot = F + 26.f * U + BW;
	// b116 (Evan: "comment pop-ups all around the screen, just not directly centre"): six edge spots (left / right, top to bottom),
	// picked from the line itself (+ the slot) so a comment keeps its spot for its whole life
	static const float SpotY[6] = { 0.50f, 0.50f, 0.61f, 0.61f, 0.72f, 0.72f }; // b266 (Evan: nothing covered): below the MENU button, timer and AWAKEN bar (they sat on them at 0.34 / 0.42)
	const int32 Spot = (int32)((GetTypeHash(Line) + (uint32)Slot * 7u) % 6u);
	const bool bRightSide = (Spot % 2) == 1;
	const float X0 = bRightSide ? Wd - 24.f * U - Tot : 24.f * U, Y0 = Ht * SpotY[Spot] - F * 0.5f;
	const FLinearColor Ink(0.02f, 0.02f, 0.05f, K), White(1.f, 1.f, 1.f, K);
	FLinearColor Cc = Col; Cc.A = K;
	// face: colour frame, black frame, the head art
	FunRect(C, X0 - 6.f * U, Y0 - 6.f * U, F + 12.f * U, F + 12.f * U, Cc);
	FunRect(C, X0, Y0, F, F, Ink);
	if (Face && Face->GetResource())
	{
		FCanvasTileItem Ti(FVector2D(X0 + 4.f * U, Y0 + 4.f * U), Face->GetResource(), FVector2D(F - 8.f * U, F - 8.f * U), FLinearColor(1.f, 1.f, 1.f, K));
		Ti.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ti);
	}
	if (!Name.IsEmpty())
	{
		const float Np = 18.f * U * (bPort ? 1.25f : 1.f);
		const FVector2D NS = P5Measure(Name, FunFontP(0, Np));
		FunRect(C, X0 - 6.f * U, Y0 + F - 4.f * U, NS.X + 16.f * U, NS.Y + 4.f * U, Ink);
		FunTextF(C, Name, X0 + 2.f * U, Y0 + F - 2.f * U + NS.Y * 0.5f, Np, White, 0, 0);
	}
	// bubble with a tail pointing at the face
	const float BX = X0 + F + 26.f * U, BY = Y0 + F * 0.5f - BH * 0.5f;
	FunRect(C, BX - 4.f * U, BY - 4.f * U, BW + 8.f * U, BH + 8.f * U, Ink);
	P5Tri(C, FVector2D(BX + 2.f * U, BY + BH * 0.35f - 4.f * U), FVector2D(BX + 2.f * U, BY + BH * 0.7f + 4.f * U), FVector2D(BX - 26.f * U, BY + BH * 0.62f), Ink);
	FunRect(C, BX, BY, BW, BH, White);
	P5Tri(C, FVector2D(BX + 4.f * U, BY + BH * 0.38f), FVector2D(BX + 4.f * U, BY + BH * 0.66f), FVector2D(BX - 18.f * U, BY + BH * 0.6f), White);
	FunTextF(C, Line, BX + BW * 0.5f, BY + BH * 0.5f, Px * Pop, Ink, 1, -1);
}

// b63: canvas helpers shared with ZoogiSkills.cpp (P5 fonts / plates)
void ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which) { FunTextF(C, S, X, Y, Px, Col, Align, Which); }
FVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which) { return P5Measure(S, FunFontP(Which, Px)); }
void ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col) { P5Quad(C, Ctr, Len, Hgt, AngDeg, Skew, Col); }
void ZoogiFunCanvasRect(UCanvas* C, float X, float Y, float W, float H, const FLinearColor& Col) { FunRect(C, X, Y, W, H, Col); }
double GZoogiAttackJoltAt = -10.0; // b193: the ATTACK jolt (set by the P5 menu)
void ZoogiFunCanvasBubble(UCanvas* C, float Wd, float Ht, float U, UTexture2D* Face, const FString& Name, const FString& Line, const FLinearColor& Col, float Age, float Life, int32 Slot) { P5SpeechBubble(C, Wd, Ht, U, Face, Name, Line, Col, Age, Life, Slot); }

// b75 (Evan: "turn them back into their ball form, with the avatar icon above their marble on the field - for CPU, not the user")
// b77 (Evan: "the avi icons above the CPU can't overlay anything - they should not block any text or menu"): drawn first on the main HUD
// canvas (under the UMG / Slate menus and every later canvas text), not on this post-render canvas that sits over everything
static bool GFunIconsOff = true;
static bool GFinderOff = false; // b173b: the marble finder only steps aside for a clash / replay
void ZoogiFunCpuIcons(UCanvas* C, AZoogiGameMode* G, APlayerController* PC)
{
	static const bool bNoCpuIcons = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCpuIcons"));
	if (!C || !G || !PC || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) return;
	// b173b (Evan: "Trail not following players and you icon don't show on launch"): project with the canvas's own view (like the aim
	// arrow) - the player-controller projection is offset by the iPhone safe-area inset, so the ring / pin / trail sat beside the marble
	const FSceneView* View = C->SceneView;
	auto Proj = [&](const FVector& W, FVector2D& Out) -> bool
	{
		if (View)
		{
			if (FVector::DotProduct(W - View->ViewMatrices.GetViewOrigin(), View->GetViewDirection()) < 1.f) return false;
			const FVector Sv = C->Project(W, false); Out = FVector2D(Sv.X, Sv.Y); return true;
		}
		return PC->ProjectWorldLocationToScreen(W, Out, true);
	};
	const bool bCpuIconsOn = !bNoCpuIcons && !GFunIconsOff;
	if (ZoogiSkills::Picking()) return;
	const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
	for (const AZoogiBall* Bc : G->Balls)
	{
		if (!Bc || !Bc->bCPU || Bc->bOut || Bc->IsHidden()) continue;
		FVector2D Sp;
		if (!Proj(Bc->BallLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 75.f), Sp)) continue;
		if (Sp.X < -50.f || Sp.Y < -50.f || Sp.X > Wd + 50.f || Sp.Y > Ht + 50.f) continue;
		if (Sp.Y < Ht * 0.17f) continue; // never up in the profile / banner band
		const float Sz = 44.f * U * (Ht > Wd ? 1.25f : 1.f);
		const FLinearColor Pcol = ZoogiUtil::PlayerColor(Bc->PlayerIndex);
		FCanvasTileItem Back(Sp - FVector2D(Sz * 0.5f + 4.f * U, Sz * 0.5f + 4.f * U), FVector2D(Sz + 8.f * U, Sz + 8.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Back.BlendMode = SE_BLEND_Translucent; C->DrawItem(Back);
		FCanvasTileItem Edge(Sp - FVector2D(Sz * 0.5f + 2.f * U, Sz * 0.5f + 2.f * U), FVector2D(Sz + 4.f * U, Sz + 4.f * U), FLinearColor(0.f, 0.f, 0.f, 0.85f)); Edge.BlendMode = SE_BLEND_Translucent; C->DrawItem(Edge);
		if (UTexture2D* Tx = ZoogiP5::Portrait(Bc->Def.Name))
		{
			FCanvasTileItem Ic(Sp - FVector2D(Sz, Sz) * 0.5f, Tx->GetResource(), FVector2D(Sz, Sz), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic);
		}
		FCanvasTileItem Tip(FVector2D(Sp.X - 3.f * U, Sp.Y + Sz * 0.5f + 4.f * U), FVector2D(6.f * U, 12.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Tip.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tip);
	}
	// b172b (Evan: "better ways to identify your marble from top view in the midst of chaos" -> picked A + B + C): each human marble gets
	// a pulsing ring on the floor in its colour (A), its face on a pin with a gold arrow and YOU / P# (A + B) and a comet trail while it
	// rolls (C). -ZoogiNoMarbleFinder = off.
	static const bool bNoFinder = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoMarbleFinder"));
	if (bNoFinder || GFinderOff) return;
	static TArray<FVector> GTrail[4];
	int32 NHuman = 0; for (const AZoogiBall* Bh : G->Balls) if (Bh && !Bh->bCPU) ++NHuman;
	const double Tn = FPlatformTime::Seconds();
	for (const AZoogiBall* Bh : G->Balls)
	{
		if (!Bh || Bh->bCPU || Bh->PlayerIndex < 0 || Bh->PlayerIndex > 3) continue;
		TArray<FVector>& Tr = GTrail[Bh->PlayerIndex];
		if (Bh->bOut || Bh->IsHidden()) { Tr.Reset(); continue; }
		const FVector BL = Bh->BallLocation();
		const FLinearColor Pc = ZoogiUtil::PlayerColor(Bh->PlayerIndex);
		// C: the trail (last ~24 spots, one every 18 cm), only while it really moves
		const bool bMoving = Bh->GetBallVelocity().Size2D() > 120.f;
		if (bMoving) { if (Tr.Num() == 0 || FVector::Dist(Tr.Last(), BL) > 18.f) { Tr.Add(BL); if (Tr.Num() > 24) Tr.RemoveAt(0); } }
		else if (Tr.Num() > 0) Tr.RemoveAt(0);
		static const bool bTrailOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFinderTrail")); // b176d (Evan: "Remove trail")
		for (int32 k = 1; bTrailOn && k < Tr.Num(); ++k)
		{
			FVector2D A0, A1;
			if (!Proj(Tr[k - 1], A0) || !Proj(Tr[k], A1)) continue;
			const float F = (float)k / Tr.Num();
			FCanvasLineItem Ln(A0, A1); Ln.LineThickness = (4.f + 18.f * F) * U; Ln.SetColor(FLinearColor(Pc.R, Pc.G, Pc.B, 0.15f + 0.6f * F)); Ln.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ln);
		}
		// A: a pulsing ring on the floor around the marble (projected circle) - b176f (Evan: "Remove the ring leave the YOU"): -ZoogiFinderRing
		static const bool bRingOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFinderRing"));
		if (bRingOn)
		{
			const float Pulse = 1.f + 0.08f * FMath::Sin((float)Tn * 6.f);
			const float Rr = ZoogiConst::ZoogiRadius * 1.9f * Pulse;
			const FVector Floor = BL - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 4.f);
			FVector2D Prev; bool bPrev = false;
			for (int32 k = 0; k <= 28; ++k)
			{
				const float Ang = k * 2.f * PI / 28.f; FVector2D Pk;
				const bool bOk = Proj(Floor + FVector(FMath::Cos(Ang) * Rr, FMath::Sin(Ang) * Rr, 0.f), Pk);
				if (bOk && bPrev) { FCanvasLineItem Ln(Prev, Pk); Ln.LineThickness = 5.f * U; Ln.SetColor(FLinearColor(Pc.R, Pc.G, Pc.B, 0.95f)); Ln.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ln); }
				Prev = Pk; bPrev = bOk;
			}
		}
		// A + B: the face pin above the marble, a gold arrow down to it, YOU (one human) or P# over the face
		FVector2D Sp;
		if (!Proj(BL + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 110.f), Sp)) continue;
		if (Sp.X < -50.f || Sp.Y < -50.f || Sp.X > Wd + 50.f || Sp.Y > Ht + 50.f || Sp.Y < Ht * 0.1f) continue;
		const float Sz = 58.f * U * (Ht > Wd ? 1.25f : 1.f);
		{ FVector2D Bs; if (Proj(BL, Bs) && Sp.Y > Bs.Y - (Sz + 40.f * U)) Sp.Y = Bs.Y - (Sz + 40.f * U); } // b175: the pin never covers its own marble (top-down view)
		const FLinearColor Gold(1.f, 0.75f, 0.05f, 1.f);
		FCanvasTileItem Bk(Sp - FVector2D(Sz * 0.5f + 5.f * U, Sz * 0.5f + 5.f * U), FVector2D(Sz + 10.f * U, Sz + 10.f * U), FLinearColor(1.f, 1.f, 1.f, 0.95f)); Bk.BlendMode = SE_BLEND_Translucent; C->DrawItem(Bk);
		FCanvasTileItem Fr(Sp - FVector2D(Sz * 0.5f + 2.f * U, Sz * 0.5f + 2.f * U), FVector2D(Sz + 4.f * U, Sz + 4.f * U), FLinearColor(Pc.R, Pc.G, Pc.B, 1.f)); Fr.BlendMode = SE_BLEND_Translucent; C->DrawItem(Fr);
		if (UTexture2D* Tx = ZoogiP5::Portrait(Bh->Def.Name)) { FCanvasTileItem Ic(Sp - FVector2D(Sz, Sz) * 0.5f, Tx->GetResource(), FVector2D(Sz, Sz), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic); }
		const float Bob = 4.f * U * FMath::Sin((float)Tn * 5.f);
		const FVector2D At(Sp.X, Sp.Y + Sz * 0.5f + 10.f * U + Bob);
		for (int32 k = 0; k < 6; ++k) { const float Hw = (14.f - 2.3f * k) * U; FCanvasTileItem Ar(FVector2D(At.X - Hw, At.Y + 3.f * U * k), FVector2D(Hw * 2.f, 3.2f * U), Gold); Ar.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ar); } // gold arrow (stepped)
		FunText(C, NHuman <= 1 ? FString(TEXT("YOU")) : FString::Printf(TEXT("P%d"), Bh->PlayerIndex + 1), Sp.X, Sp.Y - Sz * 0.5f - 34.f * U, 30.f * U, FLinearColor::White);
	}
}

void UZoogiFunSys::DrawHud(AHUD* HUD, UCanvas* C)
{
	GFunIconsOff = true;
	GFinderOff = Clash.bOn || Rep.bOn; // b173b: set before any early return (a cut-in at launch used to hide the finder)
	if (!HUD || !C || HUD->GetWorld() != GetWorld()) return;
	if (UGameplayStatics::IsGamePaused(GetWorld())) return; // b79 (Evan: "nothing overlays the menu"): the pause menu is clear
	if (ZoogiLoadScreen::Holding()) return; // b207: nothing over the loading screen
	if (ZoogiWallCine::Active()) { ZoogiWallCine::DrawHud(C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunText(C, Ts, Tx, Ty, Tp, Tc); }); return; } // b230
	if (ZoogiCosmosBoss::BossCamOn() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBossCamHud"))) return; // b217 (Evan: "hide banners when showing golem")
	if (ZoogiP5::CutInBusy() || ZoogiP5::ResultsOn()) return; // this post-render canvas sits above the viewport widgets: stay off while the P5 cut-in plays
	AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	if (!IsActive(G)) return;
	ZoogiWallCine::DrawPip(C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunTextL(C, Ts, Tx, Ty, Tp, Tc, 1); }); // b245
	ZoogiAwardBanner::Draw(G, C, [C](const FString& Ts, float Tx, float Ty, float Tp, const FLinearColor& Tc) { FunTextL(C, Ts, Tx, Ty, Tp, Tc, 1); }); // b238
	APlayerController* PC = HUD->GetOwningPlayerController();
	const float Wd = C->ClipX, Ht = C->ClipY, Mn = FMath::Min(Wd, Ht);
	const float U = Mn / 1080.f;
	GFunIconsOff = Clash.bOn || Rep.bOn; // b77: read by ZoogiFunCpuIcons on the next main HUD pass

	if (Rep.bOn)
	{
		const float Bar = Ht * 0.1f;
		FunRect(C, 0.f, 0.f, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
		FunRect(C, 0.f, Ht - Bar, Wd, Bar, FLinearColor(0.f, 0.f, 0.f, 0.92f));
		if (FMath::Fmod(Rep.Age, 0.9f) < 0.55f) FunRing(C, FVector2D(Wd * 0.05f + 12.f * U, Bar * 0.5f), 6.f * U, 12.f * U, FLinearColor(1.f, 0.2f, 0.15f));
		FunText(C, TEXT("REPLAY"), Wd * 0.05f + 32.f * U, Bar * 0.5f - 22.f * U, 40.f * U, FLinearColor(1.f, 0.25f, 0.2f), false);
		FunText(C, Rep.bFinisher ? TEXT("ULTIMATE FINISH!") : TEXT("BEST OF THE ROUND"), Wd * 0.5f, Ht - Bar * 0.78f, 22.f * U, Rep.bFinisher ? FLinearColor(1.f, 0.45f, 0.9f) : FLinearColor(1.f, 0.85f, 0.3f));
		FunText(C, Rep.Clip.Title, Wd * 0.5f, Ht - Bar * 0.5f, 34.f * U, FLinearColor::White);
		FunText(C, TEXT("SLOW-MO  -  TAP TO SKIP"), Wd * 0.5f, Ht - Bar * 0.2f, 18.f * U, FLinearColor(0.75f, 0.8f, 0.9f));
		if (Rep.Age < 0.15f) FunRect(C, 0.f, 0.f, Wd, Ht, FLinearColor(1.f, 1.f, 1.f, 0.7f * ZoogiControls::FlashMul() * (1.f - Rep.Age / 0.15f)));
		return;
	}

	// build 25: blackout (Dark Zoogy's ultimate)
	if (BlackoutAge < 2.4f)
	{
		const float Ab = BlackoutAge < 0.15f ? BlackoutAge / 0.15f : FMath::Clamp((2.4f - BlackoutAge) / 0.5f, 0.f, 1.f);
		FunRect(C, 0.f, 0.f, Wd, Ht, FLinearColor(0.02f, 0.f, 0.06f, 0.78f * Ab));
	}
	// timing ring
	if ((Mode == 2 || (Mode == 1 && FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBoostTap")))) && PredT >= 0.f && PredT <= ZoogiFunTune::RingLead && !bTapUsed && PC) // b66: strike ring gone (auto boost); BLOCK ring stays
	{
		AActor* A = PredTarget.Get();
		FVector2D S;
		if (A && PC->ProjectWorldLocationToScreen(A->GetActorLocation(), S, true))
		{
			float R0 = 46.f * U;
			if (PC->PlayerCameraManager)
			{
				const FVector Right = PC->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
				FVector2D S2;
				if (PC->ProjectWorldLocationToScreen(A->GetActorLocation() + Right * (Mode == 1 ? ZoogiConst::OrbRadius + 25.f : ZoogiConst::ZoogiRadius + 25.f), S2, true))
					R0 = FMath::Clamp(FVector2D::Distance(S, S2), 22.f * U, 140.f * U);
			}
			const float K = FMath::Clamp(PredT / ZoogiFunTune::RingLead, 0.f, 1.f);
			const bool bNow = PredT <= ZoogiFunTune::Perfect;
			const FLinearColor Base = Mode == 1 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(0.3f, 0.85f, 1.f);
			FunRing(C, S, R0, 5.f * U, FLinearColor(1.f, 1.f, 1.f, 0.85f));
			FunRing(C, S, R0 * (1.f + 3.2f * K), (7.f + 5.f * (1.f - K)) * U, bNow ? FLinearColor(1.f, 1.f, 1.f, 1.f) : FLinearColor(Base.R, Base.G, Base.B, 0.5f + 0.5f * (1.f - K)));
			if (bNow) FunRing(C, S, R0 * 0.6f, R0 * 0.9f, FLinearColor(Base.R, Base.G, Base.B, 0.45f));
			FunText(C, Mode == 1 ? TEXT("TAP!") : TEXT("BLOCK!"), S.X, S.Y + R0 * (1.f + 3.2f * K) + 34.f * U, 34.f * U, Base);
		}
	}

	// labels in the world: boost pickup, on-fire Zoogis
	if (PC)
	{
		FVector2D S;
		if (Pickup && PC->ProjectWorldLocationToScreen(Pickup->GetActorLocation() + FVector(0, 0, 95.f), S, true))
			FunText(C, PickupKind == EBoost::Heavy ? TEXT("HEAVY") : TEXT("x2"), S.X, S.Y, 30.f * U, PickupKind == EBoost::Heavy ? FLinearColor(1.f, 0.5f, 0.2f) : FLinearColor(1.f, 0.85f, 0.25f));
		for (int32 i = 0; i < Players.Num(); ++i)
		{
			if (!Players[i].bFire || !G->Balls.IsValidIndex(i) || !G->Balls[i] || G->Balls[i]->bOut || G->Balls[i]->IsHidden()) continue;
			if (PC->ProjectWorldLocationToScreen(G->Balls[i]->BallLocation() + FVector(0, 0, 120.f), S, true))
				FunText(C, TEXT("ON FIRE"), S.X, S.Y, 24.f * U, FLinearColor(1.f, 0.5f + 0.2f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 9.f), 0.1f));
		}
	}

	// build 25: bullet-time ultimate aim
	if (bUltAim && PC)
	{
		if (AZoogiBall* Sh = G->GetActiveBall())
		{
			const FLinearColor Uc = UltColor(UltPower);
			const float Pulse = 1.f + 0.12f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 10.f);
			for (AActor* T : G->LiveTargets(Sh))
			{
				FVector2D S;
				const FVector TL = Cast<AZoogiBall>(T) ? Cast<AZoogiBall>(T)->BallLocation() : T->GetActorLocation();
				if (PC->ProjectWorldLocationToScreen(TL, S, true)) FunRing(C, S, (Cast<AZoogiBall>(T) ? 46.f : 34.f) * U * Pulse, 5.f * U, FLinearColor(Uc.R, Uc.G, Uc.B, 0.9f));
			}
			// Oct 30 (Evan: "players need time to understand"): P5-style card - what this superpower does, then the pick
			{
				const FString Nm = UltName(UltPower);
				const FString Desc = UltPower == EZoogiPower::Ricochet ? TEXT("TAP A TARGET  -  IT BOUNCES ON TO 5 MORE") : TEXT("TAP A TARGET  -  YOU HOME IN FOR A HEAVY HIT");
				const float CY = Ht * 0.47f, CW = FMath::Min(Wd * 0.86f, 1100.f * U), CH = 150.f * U, CX = Wd * 0.5f - CW * 0.5f;
				const float In = FMath::Clamp(UltAimAge / 0.18f, 0.f, 1.f);
				const float Sx = (1.f - In) * Wd * 0.6f;
				FunRect(C, CX + Sx + 14.f * U, CY + 12.f * U, CW, CH, FLinearColor(1.f, 0.07f, 0.5f, 0.95f));      // magenta offset block
				FunRect(C, CX + Sx, CY, CW, CH, FLinearColor(0.02f, 0.02f, 0.03f, 0.94f));                          // black card
				FunRect(C, CX + Sx, CY, 18.f * U, CH, Uc);                                                           // power colour edge
				FunTextF(C, Nm, Wd * 0.5f + Sx, CY + 50.f * U, 66.f * U, FLinearColor::White, 1, 3);
				FunTextF(C, Desc, Wd * 0.5f + Sx, CY + 112.f * U, 30.f * U, FLinearColor(1.f, 0.85f, 0.35f), 1, 0);
			}
			FunTextL(C, FString::Printf(TEXT("TAP A GLOWING TARGET!  %d"), FMath::Max(0, FMath::CeilToInt(UltAimLimit - UltAimAge))), Wd * 0.5f, Ht * 0.62f + 30.f * U, 40.f * U * (1.f + 0.05f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 8.f)), Uc);
			const float Left = FMath::Clamp(1.f - UltAimAge / FMath::Max(0.5f, UltAimLimit), 0.f, 1.f);
			FunRect(C, Wd * 0.3f, Ht * 0.66f, Wd * 0.4f, 8.f * U, FLinearColor(0.f, 0.f, 0.f, 0.5f));
			FunRect(C, Wd * 0.3f, Ht * 0.66f, Wd * 0.4f * Left, 8.f * U, Uc);
		}
	}
	// build 27: power clash fight panel (split energy, ULTIMATE STRIKE / POWER CLASH title, tug bar with tap icons)
	if (Clash.bOn) { if (Clash.bDuel) DrawDuel(C, G, Wd, Ht, U); else DrawClash(C, G, Wd, Ht, U); } // b66: duel cutscene UI
	if (DefWin.bOn) DrawDefendWindow(C, G, Wd, Ht, U); // b107b
	DrawEdgeHang(C, Wd, Ht, U); // b58
	// build 25: ultimate banner (a slanted band sweeping across)
	if (UltBannerAge < 1.3f && !Clash.bOn && !P5BannersOn()) // build 29: never over the clash panel (b63: P5 corner banner instead)
	{
		const float In = FMath::Clamp(UltBannerAge / 0.18f, 0.f, 1.f), Out = FMath::Clamp((1.3f - UltBannerAge) / 0.25f, 0.f, 1.f);
		const float Ease = 1.f - FMath::Square(1.f - In);
		const float Off = (1.f - Ease) * Wd - (1.f - Out) * Wd;
		const float Yc = Ht * 0.5f, Hb = Ht * 0.17f, Sk = Hb * 0.5f;
		const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
		FLinearColor Bc = UltBannerCol; Bc.A = 0.88f;
		TArray<FCanvasUVTri> Tris;
		FCanvasUVTri T1, T2;
		const FVector2D P0(Off - Sk, Yc + Hb * 0.5f), P1(Off + Sk, Yc - Hb * 0.5f), P2(Off + Wd + Sk, Yc - Hb * 0.5f), P3(Off + Wd - Sk, Yc + Hb * 0.5f);
		T1.V0_Pos = P0; T1.V1_Pos = P1; T1.V2_Pos = P2; T2.V0_Pos = P0; T2.V1_Pos = P2; T2.V2_Pos = P3;
		T1.V0_Color = T1.V1_Color = T1.V2_Color = Bc; T2.V0_Color = T2.V1_Color = T2.V2_Color = Bc;
		Tris.Add(T1); Tris.Add(T2);
		FCanvasTriangleItem TI(Tris, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
		FunText(C, UltBannerText, Wd * 0.5f + Off, Yc - 10.f * U, 70.f * U, FLinearColor::White);
		FunText(C, UltBannerSub, Wd * 0.5f + Off, Yc + Hb * 0.32f, 24.f * U, FLinearColor(1.f, 1.f, 1.f, 0.9f));
	}
	// build 25: SUPER meter of the (first) human Zoogi, under its portrait
	for (int32 i = 0; i < G->Balls.Num(); ++i)
	{
		const AZoogiBall* Hb = G->Balls[i];
		if (!Hb || Hb->bCPU || !Players.IsValidIndex(i)) continue;
		const float M = Players[i].Meter / 100.f;
		float DetBottom = 0.f;
		const float DetRev = ZoogiArenaHUD::DetailsReveal(DetBottom);
		const float Y0m = Ht * 0.165f;
		static const bool bMeterLeft = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAwakenLeft"));
		const float X = (Wd > Ht && !bMeterLeft) ? Wd * 0.025f + 150.f * U : Wd * 0.025f, Y = FMath::Lerp(Y0m, FMath::Max(Y0m, DetBottom + 14.f * U), DetRev), Bw = Mn * 0.2f, Bh = 12.f * U; // Oct 30 (Evan): below the avatars; slides down under the details when an avatar tap reveals them
		const EZoogiPower Sig = SignaturePower(Hb);
		const FLinearColor Mc = M >= 1.f ? FLinearColor::LerpUsingHSV(UltColor(Sig), FLinearColor::White, 0.35f + 0.35f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 8.f)) : UltColor(Sig);
		const float BhL = Bh + 6.f * U; // b131: a little taller, the label inside it
		static const bool bSmallMeter = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSmallAwakenMeter"));
		const TCHAR* MLbl = ZoogiAwakenMeter() ? (M >= 1.f ? TEXT("AWAKEN!") : TEXT("AWAKEN")) : (M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"));
		if (bSmallMeter)
		{
			FunRect(C, X - 2.f * U, Y - 2.f * U, Bw + 4.f * U, BhL + 4.f * U, FLinearColor(0.f, 0.f, 0.05f, 0.8f));
			FunRect(C, X, Y, Bw * FMath::Clamp(M, 0.f, 1.f), BhL, Mc);
			FunText(C, MLbl, X + 6.f * U, Y + 1.f * U, 13.f * U, M >= 1.f ? FLinearColor(0.05f, 0.03f, 0.1f) : FLinearColor::White, false);
		}
		else
		{	// b178l (Evan picked A): a big segmented bar - AWAKEN on the left, 10 slanted cells, the % (or READY!) in yellow on the right
			const float Bw2 = Mn * 0.3f, Bh2 = 40.f * U, Yc = Y + Bh2 * 0.5f;
			P5Quad(C, FVector2D(X + Bw2 * 0.5f + 6.f * U, Yc + 6.f * U), Bw2, Bh2, -4.f, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			P5Quad(C, FVector2D(X + Bw2 * 0.5f, Yc), Bw2, Bh2, -4.f, 10.f * U, FLinearColor(0.08f, 0.06f, 0.16f, 0.94f));
			const float LPx = 22.f * U;
			const FString Pct = M >= 1.f ? FString(TEXT("READY!")) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(M, 0.f, 1.f) * 100.f));
			const float LW = P5Measure(MLbl, FunFontP(0, LPx)).X, PW = P5Measure(Pct, FunFontP(0, LPx)).X;
			FunTextF(C, MLbl, X + 12.f * U, Yc, LPx, FLinearColor::White, 0, 0);
			FunTextF(C, Pct, X + Bw2 - 12.f * U, Yc, LPx, FLinearColor(1.f, 0.84f, 0.16f), 2, 0);
			const float Sx0 = X + 12.f * U + LW + 12.f * U, Sx1 = X + Bw2 - 12.f * U - PW - 12.f * U;
			const int32 NC = 10; const float Cw = FMath::Max(2.f, (Sx1 - Sx0) / NC);
			for (int32 k = 0; k < NC; ++k)
			{
				const float Fill = FMath::Clamp(M * NC - k, 0.f, 1.f);
				const FVector2D Cc(Sx0 + Cw * (k + 0.5f), Yc + 2.f * U);
				P5Quad(C, Cc, Cw - 4.f * U, Bh2 * 0.46f, -4.f, 4.f * U, FLinearColor(0.27f, 0.23f, 0.4f, 1.f));
				if (Fill > 0.f) P5Quad(C, Cc, Cw - 4.f * U, Bh2 * 0.46f, -4.f, 4.f * U, FLinearColor(Mc.R, Mc.G, Mc.B, FMath::Lerp(0.35f, 1.f, Fill)));
			}
		}
		// b80 (Evan): the ULTIMATE READY banner sits on the left, right under this meter (was a lower-left P5 note)
		if (M >= 1.f && !bUltThisShot && G->ActivePlayer == i && (ZoogiAwakenMeter() ? G->Phase == EZoogiPhase::Aiming : (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling)) && !Clash.bOn)
		{
			const FString Rd = ZoogiAwakenMeter() ? FString(TEXT("AWAKEN READY  -  TAP AWAKEN")) : FString::Printf(TEXT("%s READY  -  TAP SUPERPOWER WHILE ROLLING"), UltName(Sig));
			float Px = 20.f * U;
			FVector2D TS = P5Measure(Rd, FunFontP(0, Px));
			const float MaxL = Wd * 0.9f;
			if (TS.X + 50.f * U > MaxL) { Px *= (MaxL - 50.f * U) / FMath::Max(1.f, TS.X); TS = P5Measure(Rd, FunFontP(0, Px)); }
			const float Len = TS.X + 50.f * U, Hg = Px * 1.8f;
			const FVector2D Ct(X + Len * 0.5f, Y + (bSmallMeter ? Bh : 40.f * U) + 26.f * U + Hg * 0.5f); // b178l: under the bigger bar
			P5Quad(C, Ct + FVector2D(6.f, 6.f) * U, Len, Hg, -4.f, 10.f * U, FLinearColor(Mc.R, Mc.G, Mc.B, 0.95f));
			P5Quad(C, Ct, Len, Hg, -4.f, 10.f * U, FLinearColor(0.02f, 0.02f, 0.05f, 0.95f));
			FunTextF(C, Rd, Ct.X, Ct.Y, Px, FLinearColor::White, 1, 0);
		}
		break;
	}
	// status badges
	// Oct 30 (Evan): instructions / status notes in Persona-style sharp angled panels, stacked in the lower left (they used to sit
	// in the middle of the top of the screen over the arena). Each slams in from the left the first time it shows.
	if (!Clash.bOn)
	{
		struct FNote { FString Text; FLinearColor Col; };
		TArray<FNote> Notes;
		static const bool bNotesRolling = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNotesRolling"));
		const bool bPlay = G->Phase == EZoogiPhase::Aiming || (bNotesRolling && G->Phase == EZoogiPhase::Rolling); // b79 (Evan: "P5 notes only while aiming"): gone after the launch
		if (IsFinalRounds(G) && bPlay) Notes.Add({ TEXT("FINAL ROUNDS  -  ORBS x2"), FLinearColor(1.f, 0.8f, 0.2f) });
		if (ActiveBoost != EBoost::None && ActiveBoostPlayer == G->ActivePlayer && bPlay) Notes.Add({ ActiveBoost == EBoost::Heavy ? TEXT("HEAVY HIT ARMED") : TEXT("DOUBLE POINTS ARMED"), ActiveBoost == EBoost::Heavy ? FLinearColor(1.f, 0.45f, 0.15f) : FLinearColor(1.f, 0.82f, 0.2f) });
		if (AZoogiBall* A = G->GetActiveBall())
		{
			if (A->bCPU && bFast && bPlay) Notes.Add({ TEXT(">> 2X  CPU TURN"), FLinearColor(0.55f, 0.85f, 1.f) });
			else if (A->bCPU && CpuTurnsSeen <= 3 && bPlay) Notes.Add({ TEXT("TAP TO SPEED UP"), FLinearColor(0.85f, 0.88f, 0.95f) });
			else if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiUltNote")) && !A->bCPU && Meter(A->PlayerIndex) >= 100.f && !bUltThisShot && bPlay) Notes.Add({ FString::Printf(TEXT("%s READY  -  TAP SUPERPOWER WHILE ROLLING"), UltName(SignaturePower(A))), UltColor(SignaturePower(A)) });
			if (!A->bCPU && G->ActiveInOuterRing() && G->Phase == EZoogiPhase::Aiming) Notes.Add({ G->bArcShot ? FString(TEXT("OUTER RING  -  JUMP IN ARMED, AIM INTO THE COURT")) : FString(TEXT("OUTER RING  -  TAP JUMP IN, OR ROLL UNDER A HALO")), FLinearColor(0.4f, 0.9f, 1.f) });
			else if (!A->bCPU && G->bArcShot && G->Phase == EZoogiPhase::Aiming) Notes.Add({ FString::Printf(TEXT("ARC ON  -  UP AND OVER, MAX %d M"), FMath::RoundToInt(G->ArcRange() / 100.f)), FLinearColor(0.4f, 0.9f, 1.f) });
		}
		if (!EventName.IsEmpty() && bPlay) Notes.Add({ EventName, FLinearColor(1.f, 0.55f, 0.88f) });
		// b131 (Evan: "fix overlapping text and banners"): the skills played this turn are a note in this stack (was a plate on top of it)
		if (G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
			if (const AZoogiBall* Hb = G->GetActiveBall())
				if (!Hb->bCPU)
					if (const UZoogiCardSubsystem* CSs = UZoogiCardSubsystem::Get(G))
						if (const FZoogiCardPlayerState* PSs = CSs->Rules().Player(Hb->PlayerIndex))
							if (PSs->Committed.Num() > 0 && PSs->Turn == G->TurnCounter)
							{
								TArray<FString> Nm;
								for (const FZoogiCardCommitted& Cm : PSs->Committed) { const FZoogiCardDef* D = CSs->Def(Cm.Card.CardId); Nm.Add(D ? D->Name.ToUpper() : Cm.Card.CardId.ToString().ToUpper()); }
								Notes.Insert({ FString(TEXT("SKILL  ")) + FString::Join(Nm, TEXT("  +  ")), FLinearColor(0.95f, 0.1f, 0.2f) }, 0);
							}
		{	// b131 (Evan: "fix overlapping text and banners"): a bottom-band corner banner owns the bottom-left while it shows
			bool bBandLow = false;
			for (const FCallout& Co : Callouts) if (Co.Band == 1 && Co.Age < Co.Life) bBandLow = true;
			if (bBandLow && P5BannersOn()) Notes.Reset();
		}
		const float Rt = GetWorld()->GetRealTimeSeconds();
		for (auto It = NoteSeen.CreateIterator(); It; ++It) { bool bLive = false; for (const FNote& N : Notes) bLive |= N.Text == It.Key(); if (!bLive) It.RemoveCurrent(); }
		const bool bPortraitV = Ht > Wd;
		const float PH = 46.f * U, Sk = 16.f * U, Gap = 10.f * U;
		float Y = bPortraitV ? Ht - 250.f * U : Ht - 150.f * U; // above the aim bar (portrait: it sits bottom-left)
		const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
		for (int32 k = 0; k < Notes.Num(); ++k)
		{
			const FNote& N = Notes[k];
			if (!NoteSeen.Contains(N.Text)) NoteSeen.Add(N.Text, Rt);
			const float Age = Rt - NoteSeen[N.Text];
			const float K = FMath::Clamp(Age / 0.22f, 0.f, 1.f);
			const float Back = 1.f + 2.7f * FMath::Pow(K - 1.f, 3.f) + 1.7f * FMath::Pow(K - 1.f, 2.f); // ease-out-back: whip in, overshoot, settle
			const FSlateFontInfo Fi = FunFontP(0, 28.f * U);
			FVector2D Ts(300.f * U, 30.f * U);
			if (FSlateApplication::IsInitialized()) Ts = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(N.Text, Fi);
			const float PW = Ts.X + 70.f * U;
			const float X0 = 18.f * U - (1.f - Back) * (PW + 60.f * U) + k * 10.f * U;
			auto Para = [&](float X, float Yt, float W, float H, const FLinearColor& Cc)
			{
				TArray<FCanvasUVTri> Ts2; FCanvasUVTri A1, A2;
				A1.V0_Pos = FVector2D(X + Sk, Yt); A1.V1_Pos = FVector2D(X + W + Sk, Yt); A1.V2_Pos = FVector2D(X + W, Yt + H);
				A2.V0_Pos = FVector2D(X + Sk, Yt); A2.V1_Pos = FVector2D(X + W, Yt + H); A2.V2_Pos = FVector2D(X, Yt + H);
				A1.V0_Color = A1.V1_Color = A1.V2_Color = A2.V0_Color = A2.V1_Color = A2.V2_Color = Cc;
				Ts2.Add(A1); Ts2.Add(A2);
				FCanvasTriangleItem TI(Ts2, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI);
			};
			const float Yt = Y - PH - k * (PH + Gap);
			Para(X0 + 7.f * U, Yt + 6.f * U, PW, PH, FLinearColor(N.Col.R, N.Col.G, N.Col.B, 0.95f));      // colour offset block
			Para(X0, Yt, PW, PH, FLinearColor(0.02f, 0.02f, 0.03f, 0.94f));                                 // black panel
			Para(X0 + 8.f * U, Yt + 6.f * U, 30.f * U, PH - 12.f * U, FLinearColor(1.f, 1.f, 1.f, 1.f));    // white tab
			FunTextF(C, N.Text, X0 + 50.f * U, Yt + PH * 0.5f, 28.f * U, FLinearColor::White, 0, 0);
		}
	}
	else NoteSeen.Reset();

	// Oct 30 (Evan): ARC shot range - a ring on the ground around the shooter while ARC is on (aim step)
	if (PC && G->bArcShot && G->Phase == EZoogiPhase::Aiming && !G->ActiveInOuterRing())
		if (AZoogiBall* A = G->GetActiveBall())
			if (!A->bCPU)
			{
				const FVector Ctr = A->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius - 4.f);
				const float R = G->ArcRange();
				FVector2D Prev(0.f, 0.f); bool bPrev = false; int32 Drawn = 0;
				for (int32 k = 0; k <= 48; ++k)
				{
					const float Ang = UE_TWO_PI * k / 48.f;
					FVector2D Sp;
					const bool bOk = PC->ProjectWorldLocationToScreen(Ctr + FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 0.f), Sp, true);
					if (bOk && bPrev && (k % 2 == 0))
					{
						FCanvasLineItem Lb(Prev, Sp); Lb.LineThickness = 9.f * U; Lb.SetColor(FLinearColor(0.f, 0.f, 0.f, 0.8f)); C->DrawItem(Lb); ++Drawn;
						FCanvasLineItem Li(Prev, Sp); Li.LineThickness = 5.f * U; Li.SetColor(ZoogiP5::Accent()); C->DrawItem(Li);
					}
					Prev = Sp; bPrev = bOk;
				}
				static int32 RingLogs = 0;
				if (RingLogs++ < 3) UE_LOG(LogZoogi, Log, TEXT("ARC ring: r %.0f around (%.0f, %.0f), %d segments on screen"), R, Ctr.X, Ctr.Y, Drawn);
			}

	// callouts
	for (const FCallout& Co : Callouts)
	{
		const float In = FMath::Clamp(Co.Age / 0.12f, 0.f, 1.f);
		const float Out = FMath::Clamp((Co.Life - Co.Age) / 0.25f, 0.f, 1.f);
		const float Pop = In < 1.f ? FMath::Lerp(1.6f, 1.f, In) : 1.f + 0.05f * FMath::Max(0.f, 1.f - (Co.Age - 0.12f) / 0.2f);
		FLinearColor Cc = Co.Col; Cc.A = Out;
		if (P5BannersOn() && !Clash.bOn)
		{
			P5CornerBanner(C, Wd, Ht, U, Co.Text, Co.Sub, Co.Col, Co.Age, Co.Life, Co.Size, Co.Band);
			if (Co.Band == 0 && Co.Age < Co.Life) ZoogiArenaHUD::NoteTopBanner(); // the profiles slide out of the way
			if (Co.Speaker >= 0 && G->Balls.IsValidIndex(Co.Speaker) && G->Balls[Co.Speaker])
			{
				const AZoogiBall* Sb = G->Balls[Co.Speaker];
				P5SpeechBubble(C, Wd, Ht, U, ZoogiP5::Portrait(Sb->Def.Name), Sb->Def.Name.ToUpper(), Co.Line, ZoogiUtil::PlayerColor(Co.Speaker), Co.Age, Co.Life);
			}
			continue;
		}
		const float Y = Clash.bOn ? Ht * 0.67f : Ht * 0.42f; // below the comic trick-shot callouts (~0.2-0.3); under the fighters during a clash
		FunText(C, Co.Text, Wd * 0.5f, Y, 64.f * U * Co.Size * Pop, Cc);
		FLinearColor Sc(1.f, 1.f, 1.f, 0.9f * Out);
		FunText(C, Co.Sub, Wd * 0.5f, Y + 62.f * U * Co.Size, 24.f * U, Sc);
	}
	if (false)
		for (const AZoogiBall* Bc : G->Balls)
		{
			if (!Bc || !Bc->bCPU || Bc->bOut || Bc->IsHidden()) continue;
			FVector2D Sp;
			if (!PC->ProjectWorldLocationToScreen(Bc->BallLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 75.f), Sp, true)) continue;
			if (Sp.X < -50.f || Sp.Y < -50.f || Sp.X > Wd + 50.f || Sp.Y > Ht + 50.f) continue;
			const float Sz = 54.f * U * (Ht > Wd ? 1.25f : 1.f);
			const FLinearColor Pcol = ZoogiUtil::PlayerColor(Bc->PlayerIndex);
			FCanvasTileItem Back(Sp - FVector2D(Sz * 0.5f + 5.f * U, Sz * 0.5f + 5.f * U), FVector2D(Sz + 10.f * U, Sz + 10.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.95f)); Back.BlendMode = SE_BLEND_Translucent; C->DrawItem(Back);
			FCanvasTileItem Edge(Sp - FVector2D(Sz * 0.5f + 2.f * U, Sz * 0.5f + 2.f * U), FVector2D(Sz + 4.f * U, Sz + 4.f * U), FLinearColor(0.f, 0.f, 0.f, 0.9f)); Edge.BlendMode = SE_BLEND_Translucent; C->DrawItem(Edge);
			if (UTexture2D* Tx = ZoogiP5::Portrait(Bc->Def.Name))
			{
				FCanvasTileItem Ic(Sp - FVector2D(Sz, Sz) * 0.5f, Tx->GetResource(), FVector2D(Sz, Sz), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic);
			}
			// a little pointer down to the marble
			FCanvasTileItem Tip(FVector2D(Sp.X - 3.f * U, Sp.Y + Sz * 0.5f + 5.f * U), FVector2D(6.f * U, 14.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.95f)); Tip.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tip);
		}
	{ bool bTop = false; for (const FCallout& Co : Callouts) if (Co.Speaker >= 0 && !Co.Line.IsEmpty()) bTop = true; ZoogiBanter::Draw(G, C, Wd, Ht, U, bTop); } // b67: P5 banter between the Zoogis
	ZoogiSkillLab::Draw(G, C, PC); // b107
	ZoogiSkills::Draw(G, C, PC); // b63: skill target pick (arc range ring, circle / line / targets, FIRE / CANCEL)
	ZoogiRaid::Draw(G, C); // b151: the raid boss health bar + team lives
	ZoogiTrickShot::Draw(G, C); // b156: try / best stars
	// b70 (Evan: "skill stays visible during aiming"): the skills played this turn, on a plate while the human aims
	static const bool bOldSkillPlate = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkillPlate")); // b131: the note stack shows it now
	if (bOldSkillPlate && G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
		if (const AZoogiBall* Hb = G->GetActiveBall())
			if (!Hb->bCPU)
				if (const UZoogiCardSubsystem* CSs = UZoogiCardSubsystem::Get(G))
					if (const FZoogiCardPlayerState* PSs = CSs->Rules().Player(Hb->PlayerIndex))
						if (PSs->Committed.Num() > 0 && PSs->Turn == G->TurnCounter)
						{
							TArray<FString> Nm;
							for (const FZoogiCardCommitted& Cm : PSs->Committed) { const FZoogiCardDef* D = CSs->Def(Cm.Card.CardId); Nm.Add(D ? D->Name.ToUpper() : Cm.Card.CardId.ToString().ToUpper()); }
							const FString Line = FString::Join(Nm, TEXT("  +  "));
							const bool bPortS = Ht > Wd;
							const float Px = (bPortS ? 30.f : 32.f) * U;
							const FVector2D Ms = ZoogiFunCanvasMeasure(Line, Px, 0);
							const float BoxW = FMath::Min(Ms.X + 200.f * U, Wd * 0.94f), BoxH = Px * 1.9f;
							const FVector2D Ctr(bPortS ? Wd * 0.5f : 40.f * U + BoxW * 0.5f, Ht * (bPortS ? 0.74f : 0.76f));
							ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, BoxW, BoxH, -3.f, 12.f * U, FLinearColor(0.85f, 0.05f, 0.12f, 0.95f));
							ZoogiFunCanvasQuad(C, Ctr, BoxW, BoxH, -3.f, 12.f * U, FLinearColor(0.02f, 0.02f, 0.04f, 0.92f));
							ZoogiFunCanvasText(C, TEXT("SKILL"), Ctr.X - BoxW * 0.5f + 70.f * U, Ctr.Y, Px * 0.8f, FLinearColor(1.f, 0.82f, 0.25f), 1, 0);
							ZoogiFunCanvasText(C, Line, Ctr.X + 50.f * U, Ctr.Y, Px, FLinearColor::White, 1, 0);
						}
	if (!ZoogiGattling::Active() && !ZoogiBossAttack::DuelActive()) ZoogiBounty::Draw(G, C, PC); // b179; b191: not over a boss grab / duel
	ZoogiGattling::Draw(G, C, PC); // b182
	ZoogiBossAttack::Draw(G, C, PC); // b190
	{	// b197 GATTLING RAPID: the count + the clock while time stands still
		int32 Rn = 0; float Rl = 0.f;
		if (ZoogiSkills::RapidActive(&Rn, &Rl))
		{
			const float Pulse = 1.f + 0.06f * FMath::Sin((float)FPlatformTime::Seconds() * 22.f);
			{	// b198 (Evan: "like Bolt Balls, an icon for rapid fire"): the skill art beside the count
				static UTexture2D* RapidIcon = nullptr; static bool bRiTried = false;
				if (!bRiTried) { bRiTried = true; const FString Pi = FPaths::ProjectContentDir() / TEXT("Data/CardArt/Icons/gattling_beat.png"); if (FPaths::FileExists(Pi)) RapidIcon = FImageUtils::ImportFileAsTexture2D(Pi); if (RapidIcon) RapidIcon->AddToRoot(); }
				if (RapidIcon && RapidIcon->GetResource())
				{
					const float IH = 120.f * U, IW = IH * 1.2f; const FVector2D IP(Wd * 0.5f - IW * 0.5f, Ht * 0.24f - 70.f * U - IH);
					ZoogiFunCanvasRect(C, IP.X - 4.f * U, IP.Y - 4.f * U, IW + 8.f * U, IH + 8.f * U, FLinearColor(1.f, 0.82f, 0.25f, 1.f));
					FCanvasTileItem It(IP, RapidIcon->GetResource(), FVector2D(IW, IH), FLinearColor::White); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It);
				}
			}
			ZoogiFunCanvasText(C, FString::Printf(TEXT("RAPID FIRE!  %d"), Rn), Wd * 0.5f, Ht * 0.24f, 64.f * U * Pulse, FLinearColor(1.f, 0.82f, 0.25f, 1.f), 1, 0);
			const float BW = 520.f * U, BH = 22.f * U, X0 = Wd * 0.5f - BW * 0.5f, Y0 = Ht * 0.24f + 52.f * U;
			ZoogiFunCanvasRect(C, X0 - 3.f * U, Y0 - 3.f * U, BW + 6.f * U, BH + 6.f * U, FLinearColor(0.04f, 0.03f, 0.06f, 0.9f));
			ZoogiFunCanvasRect(C, X0, Y0, BW * FMath::Clamp(Rl / 6.f, 0.f, 1.f), BH, FLinearColor(0.62f, 0.32f, 1.f, 1.f));
			ZoogiFunCanvasText(C, TEXT("TAP TAP TAP!"), Wd * 0.5f, Y0 + BH + 30.f * U, 30.f * U, FLinearColor::White, 1, 0);
		}
		TArray<FVector> Mp; TArray<float> Mk;
		if (ZoogiSkills::RapidMarks(Mp, Mk))
		{	// b200: the tapped circles - a gold target ring that tightens while the bolt flies, then pops
			for (int32 i = 0; i < Mp.Num(); ++i)
			{
				const FVector Sp = C->Project(Mp[i]); if (Sp.Z <= 0.f) continue;
				const bool bPop = Mk[i] >= 0.f;
				const float Kp = bPop ? Mk[i] / 0.35f : 0.f;
				const float Rr = (bPop ? 46.f + 70.f * Kp : 46.f + 18.f * FMath::Clamp(-Mk[i] / 0.4f, 0.f, 1.f)) * U;
				const FLinearColor Cc = bPop ? FLinearColor(1.f, 0.95f, 0.7f, 1.f - Kp) : FLinearColor(1.f, 0.82f, 0.25f, 1.f);
				const int32 Ns = 28;
				for (int32 s = 0; s < Ns; ++s)
				{
					const float A0 = 2.f * PI * s / Ns, A1 = 2.f * PI * (s + 1) / Ns;
					FCanvasLineItem Li(FVector2D(Sp.X + FMath::Cos(A0) * Rr, Sp.Y + FMath::Sin(A0) * Rr * 0.75f), FVector2D(Sp.X + FMath::Cos(A1) * Rr, Sp.Y + FMath::Sin(A1) * Rr * 0.75f));
					Li.SetColor(Cc); Li.LineThickness = (bPop ? 4.f : 6.f) * U; Li.BlendMode = SE_BLEND_Translucent; C->DrawItem(Li);
				}
				if (!bPop)
					for (int32 q = 0; q < 4; ++q)
					{	// crosshair ticks
						const FVector2D Dq(FMath::Cos(q * PI * 0.5f), FMath::Sin(q * PI * 0.5f) * 0.75f);
						FCanvasLineItem Li(FVector2D(Sp.X, Sp.Y) + Dq * Rr * 0.45f, FVector2D(Sp.X, Sp.Y) + Dq * Rr * 1.25f);
						Li.SetColor(FLinearColor(1.f, 0.3f, 0.25f, 1.f)); Li.LineThickness = 4.f * U; C->DrawItem(Li);
					}
			}
		}
	}
	{	// b205 (Evan picked F: "edge flash + side card"): a skill went off - the screen edges flash in the caster's colour and a skill
		// card slides in from the right: icon, name, who cast it. -ZoogiNoSkillBanner = off
		static const bool bNoSb = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSkillBanner"));
		FName Sid; int32 Sp = -1; double Sat = 0.0;
		const double Sa = ZoogiSkills::LastCast(Sid, Sp, Sat) ? FPlatformTime::Seconds() - Sat : 100.0;
		if (!bNoSb && Sa >= 0.0 && Sa < 2.6)
		{
			const FLinearColor Pc = ZoogiUtil::PlayerColor(FMath::Max(0, Sp));
			if (Sa < 0.55)
			{	// edge flash
				const float Fk = (float)(1.0 - Sa / 0.55); const int32 Ns = 10; const float Sw = 8.f * U;
				for (int32 i = 0; i < Ns; ++i)
				{
					const float Al = 0.5f * Fk * FMath::Pow(1.f - (float)i / Ns, 1.5f); const FLinearColor Ec(Pc.R, Pc.G, Pc.B, Al);
					ZoogiFunCanvasRect(C, 0.f, i * Sw, Wd, Sw, Ec); ZoogiFunCanvasRect(C, 0.f, Ht - (i + 1) * Sw, Wd, Sw, Ec);
					ZoogiFunCanvasRect(C, i * Sw, 0.f, Sw, Ht, Ec); ZoogiFunCanvasRect(C, Wd - (i + 1) * Sw, 0.f, Sw, Ht, Ec);
				}
			}
			// side card: in 0.22 s, holds, out from 2.2 s
			const float In = FMath::Clamp((float)(Sa / 0.22), 0.f, 1.f), Out = FMath::Clamp((float)((Sa - 2.2) / 0.3), 0.f, 1.f);
			const float Ei = 1.f - FMath::Pow(1.f - In, 3.f), Eo = Out * Out;
			const float CW = 500.f * U, CH = 150.f * U;
			const float X0 = Wd - 24.f * U - CW + (1.f - Ei + Eo) * (CW + 40.f * U), Y0 = Ht * (Ht > Wd ? 0.30f : 0.50f); // b266: below the MENU button + timer (0.40 sat on them)
			ZoogiFunCanvasRect(C, X0 + 8.f * U, Y0 + 8.f * U, CW, CH, FLinearColor(Pc.R, Pc.G, Pc.B, 0.95f)); // colour shadow
			ZoogiFunCanvasRect(C, X0 - 4.f * U, Y0 - 4.f * U, CW + 8.f * U, CH + 8.f * U, FLinearColor::White);
			ZoogiFunCanvasRect(C, X0, Y0, CW, CH, FLinearColor(0.03f, 0.03f, 0.05f, 0.97f));
			static TMap<FName, UTexture2D*> SbIcons;
			UTexture2D* Ic = nullptr;
			if (UTexture2D** Fd = SbIcons.Find(Sid)) Ic = *Fd;
			else { const FString Pth = FPaths::ProjectContentDir() / FString::Printf(TEXT("Data/CardArt/Icons/%s.png"), *Sid.ToString()); if (FPaths::FileExists(Pth)) Ic = FImageUtils::ImportFileAsTexture2D(Pth); if (Ic) Ic->AddToRoot(); SbIcons.Add(Sid, Ic); }
			const float IS = CH - 24.f * U;
			if (Ic && Ic->GetResource()) { FCanvasTileItem It(FVector2D(X0 + 12.f * U, Y0 + 12.f * U), Ic->GetResource(), FVector2D(IS, IS), FLinearColor::White); It.BlendMode = SE_BLEND_Opaque; C->DrawItem(It); }
			FString Nm = Sid.ToString();
			for (const FZoogiCardDef& Cd : UZoogiFrontProfileSubsystem::CardDefs()) if (Cd.Id == Sid) { Nm = Cd.Name; break; }
			Nm = Nm.Replace(TEXT("_"), TEXT(" ")).ToUpper();
			FString Who = FString::Printf(TEXT("P%d"), Sp + 1);
			if (G->Balls.IsValidIndex(Sp) && G->Balls[Sp]) Who = FString::Printf(TEXT("P%d %s"), Sp + 1, *G->Balls[Sp]->Def.Name.ToUpper());
			const float Tx = X0 + 24.f * U + IS;
			float NPx = 42.f * U; { const float Room = X0 + CW - 14.f * U - Tx; const float Mw = ZoogiFunCanvasMeasure(Nm, NPx, 0).X; if (Mw > Room && Mw > 1.f) NPx *= Room / Mw; } // fits the card
			ZoogiFunCanvasText(C, Nm, Tx, Y0 + 44.f * U, NPx, FLinearColor::White, 0, 0);
			ZoogiFunCanvasText(C, Who, Tx, Y0 + 88.f * U, 26.f * U, FLinearColor(Pc.R, Pc.G, Pc.B, 1.f), 0, 0);
			ZoogiFunCanvasRect(C, Tx, Y0 + 108.f * U, 150.f * U, 30.f * U, FLinearColor(1.f, 0.82f, 0.2f, 1.f));
			ZoogiFunCanvasText(C, TEXT("ACTIVATED!"), Tx + 75.f * U, Y0 + 123.f * U, 22.f * U, FLinearColor(0.03f, 0.03f, 0.05f, 1.f), 1, 0);
		}
	}
	{	// b193 ATTACK jolt: tunnel vision - the edges close in dark and speed streaks rush to the centre (0.75 s)
		const double Ja = FPlatformTime::Seconds() - GZoogiAttackJoltAt;
		if (Ja >= 0.0 && Ja < 0.75)
		{
			const float Jk = 1.f - (float)(Ja / 0.75);
			for (int32 r = 0; r < 6; ++r)
			{	// nested dark frames = a soft vignette
				const float In = (0.04f + 0.035f * r) * Mn;
				const FLinearColor Dk(0.f, 0.f, 0.f, 0.16f * Jk);
				ZoogiFunCanvasRect(C, 0.f, 0.f, Wd, In, Dk); ZoogiFunCanvasRect(C, 0.f, Ht - In, Wd, In, Dk);
				ZoogiFunCanvasRect(C, 0.f, In, In, Ht - 2.f * In, Dk); ZoogiFunCanvasRect(C, Wd - In, In, In, Ht - 2.f * In, Dk);
			}
			FRandomStream Rs(int32(GZoogiAttackJoltAt * 1000.0) & 0x7fffffff);
			for (int32 i = 0; i < 28; ++i)
			{	// streaks from the edges toward the centre
				const float A = Rs.FRandRange(0.f, 2.f * PI);
				const float R0 = 0.62f * FMath::Max(Wd, Ht) - 0.25f * Mn * (1.f - Jk) * Rs.FRandRange(0.6f, 1.4f);
				const float L = Rs.FRandRange(0.12f, 0.24f) * Mn;
				const FVector2D Ctr(Wd * 0.5f + FMath::Cos(A) * (R0 - L * 0.5f), Ht * 0.5f + FMath::Sin(A) * (R0 - L * 0.5f));
				ZoogiFunCanvasQuad(C, Ctr, L, FMath::Max(2.f, 5.f * Jk), FMath::RadiansToDegrees(A), 0.f, FLinearColor(1.f, 1.f, 1.f, 0.55f * Jk));
			}
		}
	}
	ZoogiEarthEvents::Draw(G, C, PC); // b180: tremor warnings
	ZoogiRedirect::Draw(G, C, PC); // b66: redirect pick (range ring, glowing targets) + combo counter
	ZoogiGuide::Draw(G, C, PC); // b66: guided first turn cards
}

// ------------------------------------------------------------------ build 27: power clash panel (Evan's mockup)

void UZoogiFunSys::DrawClash(UCanvas* C, AZoogiGameMode* G, float Wd, float Ht, float U)
{
	const AZoogiBall* A = Clash.Att.Get(); const AZoogiBall* D = Clash.Def.Get();
	const float Tn = GetWorld()->GetRealTimeSeconds();
	const FTexture* WhiteT = C->DefaultTexture ? C->DefaultTexture->GetResource() : nullptr;
	// screen left / right (the camera may sit on the other side when one side is walled in)
	const bool bFl = Clash.bCamFlip;
	const AZoogiBall* LB = bFl ? D : A; const AZoogiBall* RB = bFl ? A : D;
	const FLinearColor Ca = FormColor(Clash.F[bFl ? 1 : 0].Form), Cd = FormColor(Clash.F[bFl ? 0 : 1].Form);
	const float LRatio = bFl ? 1.f - Clash.Ratio : Clash.Ratio;
	const bool bLeftWins = bFl ? !Clash.bAttWins : Clash.bAttWins;
	const float In = FMath::Clamp(Clash.Age / 0.25f, 0.f, 1.f);
	const float Fade = Clash.bFinish ? FMath::Clamp(1.f - (Clash.FinishAge - (ClashFinishSecs - 0.45f)) / 0.45f, 0.f, 1.f) : 1.f;
	const float K = In * Fade;
	if (K <= 0.f) return;
	if (ZoogiClashSplit::On())
	{	// b68 (Evan): two awakening clips, one per side, torn down the middle at the tug; the winner's side sweeps to full screen
		float Sp = LRatio;
		if (Clash.bFinish) { const float Fk = FMath::Clamp(Clash.FinishAge / 0.7f, 0.f, 1.f); Sp = FMath::Lerp(LRatio, bLeftWins ? 1.f : 0.f, 1.f - FMath::Pow(1.f - Fk, 3.f)); }
		ZoogiClashSplit::Draw(C, Sp, K, Tn, Ca, Cd);
	}
	auto Al = [](const FLinearColor& Cc, float Aa) { FLinearColor R = Cc; R.A = Aa; return R; };
	auto Tri = [](TArray<FCanvasUVTri>& Ts, const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2)
	{
		FCanvasUVTri T; T.V0_Pos = P0; T.V1_Pos = P1; T.V2_Pos = P2; T.V0_Color = C0; T.V1_Color = C1; T.V2_Color = C2; Ts.Add(T);
	};
	auto Flush = [&](TArray<FCanvasUVTri>& Ts) { if (!Ts.Num()) return; FCanvasTriangleItem TI(Ts, WhiteT); TI.BlendMode = SE_BLEND_Translucent; C->DrawItem(TI); Ts.Reset(); };
	auto Quad = [&](TArray<FCanvasUVTri>& Q, const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2, const FLinearColor& C3)
	{
		Tri(Q, P0, P1, P2, C0, C1, C2); Tri(Q, P0, P2, P3, C0, C2, C3);
	};

	// 1) split energy: each fighter's colour washes in from its side, brightest at a crackling seam that follows the tug
	const bool bWinSide = Clash.bFinish && Clash.bFinishHit;
	const float SeamX = bWinSide ? (bLeftWins ? Wd * (0.75f + 0.3f * FMath::Min(1.f, Clash.FinishAge)) : Wd * (0.25f - 0.3f * FMath::Min(1.f, Clash.FinishAge))) : Wd * (0.5f + (LRatio - 0.5f) * 0.7f);
	const int32 N = 12;
	TArray<FVector2D> Seam; Seam.SetNum(N + 1);
	for (int32 k = 0; k <= N; ++k)
	{
		const float J = ((k % 2) ? 1.f : -1.f) * (16.f + 12.f * FMath::Sin(Tn * 23.f + k * 1.7f)) * U;
		Seam[k] = FVector2D(SeamX + J, Ht * k / N);
	}
	TArray<FCanvasUVTri> Ts;
	// strong colour at the screen edges, light near the seam so the fighters stay clear
	const FLinearColor Ea = Al(Ca * 0.7f, 0.24f * K), Sa = Al(Ca, 0.04f * K), Ed = Al(Cd * 0.7f, 0.24f * K), Sd = Al(Cd, 0.04f * K);
	for (int32 k = 0; k < N; ++k)
	{
		const FVector2D L0(0.f, Seam[k].Y), L1(0.f, Seam[k + 1].Y), R0(Wd, Seam[k].Y), R1(Wd, Seam[k + 1].Y);
		Tri(Ts, L0, Seam[k], Seam[k + 1], Ea, Sa, Sa); Tri(Ts, L0, Seam[k + 1], L1, Ea, Sa, Ea);
		Tri(Ts, R0, Seam[k], Seam[k + 1], Ed, Sd, Sd); Tri(Ts, R0, Seam[k + 1], R1, Ed, Sd, Ed);
	}
	Flush(Ts);
	for (int32 k = 0; k < N; ++k) // the seam bolt: a coloured glow under a white core
	{
		const float Mid = FMath::Abs((Seam[k].Y + Seam[k + 1].Y) * 0.5f / Ht - 0.5f) < 0.24f ? 0.35f : 1.f; // fainter across the fighters
		FCanvasLineItem Gl(Seam[k], Seam[k + 1]); Gl.LineThickness = 10.f * U; Gl.SetColor(Al(FMath::Lerp(Ca, Cd, 0.5f), 0.45f * K * Mid)); C->DrawItem(Gl);
		FCanvasLineItem Co(Seam[k], Seam[k + 1]); Co.LineThickness = 3.f * U; Co.SetColor(Al(FLinearColor::White, 0.85f * K * Mid)); C->DrawItem(Co);
	}

	// 2) the fists' collision burst (alternating spikes of both colours, bigger on every landed punch)
	if (Clash.HitScreen.X >= 0.f)
	{
		const FVector2D P = Clash.HitScreen;
		const float R = (44.f + 60.f * Clash.Hit) * U * (1.f + 0.08f * FMath::Sin(Tn * 31.f)); // build 29: smaller, less white (glare)
		const int32 S = 14;
		for (int32 i = 0; i < S; ++i)
		{
			const float Aa = 2.f * PI * i / S + Tn * 1.5f;
			const float Len = R * ((i % 2) ? 1.f : 0.62f) * (0.85f + 0.15f * FMath::Sin(Tn * 17.f + i));
			const FVector2D Tip = P + FVector2D(FMath::Cos(Aa), FMath::Sin(Aa)) * Len;
			const FVector2D B0 = P + FVector2D(FMath::Cos(Aa - 0.2f), FMath::Sin(Aa - 0.2f)) * R * 0.2f, B1 = P + FVector2D(FMath::Cos(Aa + 0.2f), FMath::Sin(Aa + 0.2f)) * R * 0.2f;
			const FLinearColor Sc = (FMath::Cos(Aa) < 0.f) ? Ca : Cd; // left spikes in the left fighter's colour, right in the right one's
			Tri(Ts, B0, Tip, B1, Al(FMath::Lerp(Sc, FLinearColor::White, 0.5f), 0.7f * K), Al(Sc, 0.f), Al(FMath::Lerp(Sc, FLinearColor::White, 0.5f), 0.7f * K));
		}
		Flush(Ts);
		FunRing(C, P, R * 0.1f, R * 0.2f, Al(FLinearColor::White, 0.6f * K));
	}

	// 3) title, like the mockup: ULTIMATE STRIKE over POWER CLASH (Lilita, chunky)
	const float TY = Ht * 0.115f;
	const float Pop = 1.f + 0.25f * FMath::Max(0.f, 1.f - Clash.Age / 0.2f);
	{
		const float TW = 760.f * U, TH = 132.f * U, TS = 30.f * U, TX = Wd * 0.5f - TW * 0.5f, T0 = TY - 64.f * U;
		Quad(Ts, FVector2D(TX + TS, T0), FVector2D(TX + TW + TS, T0), FVector2D(TX + TW - TS, T0 + TH), FVector2D(TX - TS, T0 + TH),
			FLinearColor(0.10f, 0.02f, 0.16f, 0.8f * K), FLinearColor(0.10f, 0.02f, 0.16f, 0.8f * K), FLinearColor(0.02f, 0.f, 0.06f, 0.8f * K), FLinearColor(0.02f, 0.f, 0.06f, 0.8f * K));
		Quad(Ts, FVector2D(TX - TS, T0 + TH), FVector2D(TX + TW - TS, T0 + TH), FVector2D(TX + TW - TS - 4.f * U, T0 + TH + 7.f * U), FVector2D(TX - TS - 4.f * U, T0 + TH + 7.f * U),
			Al(Ca, K), Al(Cd, K), Al(Cd, K), Al(Ca, K));
		Flush(Ts);
	}
	FunTextL(C, TEXT("ULTIMATE STRIKE"), Wd * 0.5f, TY - 34.f * U, 30.f * U, Al(FLinearColor(1.f, 0.84f, 0.3f), K));
	FunTextL(C, TEXT("POWER CLASH"), Wd * 0.5f, TY + 20.f * U, 80.f * U * Pop, Al(FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.45f, 0.9f), FLinearColor::White, 0.3f + 0.3f * FMath::Sin(Tn * 9.f)), K));

	// 4) fighter banners: slanted plate, the ultimate-form avatar in a ring at the outer end, name + a comic punch counter
	auto Plate = [&](bool bLeft, const AZoogiBall* B, const FLinearColor& Cc, int32 Taps, float LastTap)
	{
		if (!B) return;
		const float Pu = FMath::Clamp(1.f - (Tn - LastTap) / 0.18f, 0.f, 1.f);
		const float PW = 430.f * U, PH = 96.f * U, Sk = 24.f * U, Y = Ht * 0.215f;
		const float X0 = bLeft ? Wd * 0.03f + 40.f * U : Wd * 0.83f - PW - 40.f * U;
		const float Sg = bLeft ? 1.f : -1.f;
		const FLinearColor OutC(0.03f, 0.03f, 0.09f, 0.88f * K), InC = Al(Cc * 0.6f, 0.88f * K); // dark at the avatar end, the fighter's colour toward the middle
		const FVector2D P0(X0 + Sk * Sg, Y), P1(X0 + PW + Sk * Sg, Y), P2(X0 + PW - Sk * Sg, Y + PH), P3(X0 - Sk * Sg, Y + PH);
		Quad(Ts, P0, P1, P2, P3, bLeft ? OutC : InC, bLeft ? InC : OutC, bLeft ? InC : OutC, bLeft ? OutC : InC);
		Quad(Ts, P3, P2, P2 + FVector2D(0.f, 8.f * U), P3 + FVector2D(0.f, 8.f * U), Al(Cc, K), Al(Cc, K), Al(Cc * 0.6f, K), Al(Cc * 0.6f, K)); // colour stripe
		Quad(Ts, P0, P1, P1 + FVector2D(0.f, 3.f * U), P0 + FVector2D(0.f, 3.f * U), Al(FLinearColor(1.f, 0.84f, 0.35f), K), Al(FLinearColor(1.f, 0.84f, 0.35f), K), Al(FLinearColor(1.f, 0.84f, 0.35f), 0.f), Al(FLinearColor(1.f, 0.84f, 0.35f), 0.f)); // gold top edge
		Flush(Ts);
		// avatar medallion
		const float AvR = 70.f * U * (1.f + 0.1f * Pu);
		const FVector2D AvC(bLeft ? X0 - 6.f * U : X0 + PW + 6.f * U, Y + PH * 0.5f);
		FunRing(C, AvC, AvR * 0.5f, AvR, FLinearColor(0.04f, 0.03f, 0.1f, 0.95f * K));
		if (UTexture2D* Av = UltAvatar(B))
			if (const FTexture* Rt = Av->GetResource())
			{
				const float S2 = AvR * 1.9f;
				FCanvasTileItem T(AvC - FVector2D(S2, S2) * 0.5f, Rt, FVector2D(S2, S2), Al(FLinearColor::White, K));
				T.BlendMode = SE_BLEND_Translucent;
				C->DrawItem(T);
			}
		FunRing(C, AvC, AvR, 7.f * U, Al(Cc, K));
		FunRing(C, AvC, AvR + 5.f * U, 3.f * U, Al(FLinearColor(1.f, 0.84f, 0.35f), K));
		if (Pu > 0.f) FunRing(C, AvC, AvR * (1.f + 0.5f * (1.f - Pu)), 4.f * U, Al(FLinearColor::White, 0.7f * Pu * K)); // punch ripple
		// name + punch counter
		const float TxIn = bLeft ? X0 + AvR + 18.f * U : X0 + PW - AvR - 18.f * U;
		FunTextL(C, B->Def.Name.ToUpper(), TxIn, Y + 30.f * U, 44.f * U, Al(FLinearColor::LerpUsingHSV(Cc, FLinearColor::White, 0.45f), K), bLeft ? 0 : 2);
		FunTextL(C, FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, B->bCPU ? TEXT("CPU") : TEXT("YOU")), TxIn, Y + 72.f * U, 18.f * U, Al(FLinearColor(0.82f, 0.85f, 0.95f), K), bLeft ? 0 : 2);
		const float CntX = bLeft ? X0 + PW - 14.f * U : X0 + 14.f * U;
		FunTextL(C, FString::Printf(TEXT("x%d"), Taps), CntX, Y + 40.f * U, 50.f * U * (1.f + 0.3f * Pu), Al(FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.82f, 0.25f), FLinearColor::White, 0.6f * Pu), K), bLeft ? 2 : 0);
		FunTextL(C, TEXT("PUNCHES"), CntX, Y + 78.f * U, 15.f * U, Al(FLinearColor(1.f, 0.82f, 0.35f), K), bLeft ? 2 : 0);
	};
	Plate(true, LB, Ca, bFl ? Clash.TapsD : Clash.TapsA, bFl ? Clash.TapD : Clash.TapA);
	Plate(false, RB, Cd, bFl ? Clash.TapsA : Clash.TapsD, bFl ? Clash.TapA : Clash.TapD);

	// 5) POWER meter: slanted energy bar, both colours pushing toward a sparking clash point, stripes racing into it
	const float BW = Wd * 0.5f, BX = (Wd - BW) * 0.5f, BY = Ht * 0.82f, BH = 36.f * U, Sk = BH * 0.55f;
	const float Rr = Clash.bFinish ? (bLeftWins ? 1.f : 0.f) : LRatio;
	const float SplitX = BX + BW * Rr;
	auto Bar = [&](float X0, float X1, float Y0, float Y1, const FLinearColor& C0, const FLinearColor& C1) // slanted segment, top edge shifted right
	{
		const float Sh = (Y1 - Y0) / BH * Sk;
		Quad(Ts, FVector2D(X0 + Sh, Y0), FVector2D(X1 + Sh, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1), C0, C1, C1, C0);
	};
	{
		const float F = 8.f * U;
		Quad(Ts, FVector2D(BX - F + Sk + F, BY - F), FVector2D(BX + BW + F + Sk + F, BY - F), FVector2D(BX + BW + F, BY + BH + F), FVector2D(BX - F, BY + BH + F),
			FLinearColor(0.02f, 0.01f, 0.05f, 0.9f * K), FLinearColor(0.02f, 0.01f, 0.05f, 0.9f * K), FLinearColor(0.02f, 0.01f, 0.05f, 0.9f * K), FLinearColor(0.02f, 0.01f, 0.05f, 0.9f * K));
		const FLinearColor Gd = Al(FLinearColor(1.f, 0.82f, 0.3f), K);
		Quad(Ts, FVector2D(BX - F + Sk + F, BY - F), FVector2D(BX + BW + F + Sk + F, BY - F), FVector2D(BX + BW + F + Sk + F - 1.f, BY - F + 3.f * U), FVector2D(BX - F + Sk + F - 1.f, BY - F + 3.f * U), Gd, Gd, Gd, Gd);
		Quad(Ts, FVector2D(BX - F, BY + BH + F - 3.f * U), FVector2D(BX + BW + F, BY + BH + F - 3.f * U), FVector2D(BX + BW + F, BY + BH + F), FVector2D(BX - F, BY + BH + F), Gd, Gd, Gd, Gd);
		Flush(Ts);
	}
	Bar(BX, SplitX, BY, BY + BH, Al(Ca * 0.55f, K), Al(FMath::Lerp(Ca, FLinearColor::White, 0.35f), K));
	Bar(SplitX, BX + BW, BY, BY + BH, Al(FMath::Lerp(Cd, FLinearColor::White, 0.35f), K), Al(Cd * 0.55f, K));
	Bar(BX, BX + BW, BY, BY + BH * 0.38f, FLinearColor(1.f, 1.f, 1.f, 0.2f * K), FLinearColor(1.f, 1.f, 1.f, 0.2f * K)); // gloss
	Flush(Ts);
	{	// energy stripes racing into the clash point from both ends
		const float Gap = 46.f * U, Wst = 12.f * U, Off = FMath::Fmod(Tn * 220.f * U, Gap);
		for (float X = BX + Off - Gap; X < SplitX - Wst; X += Gap) if (X > BX) Bar(X, X + Wst, BY, BY + BH, FLinearColor(1.f, 1.f, 1.f, 0.0f), FLinearColor(1.f, 1.f, 1.f, 0.28f * K));
		for (float X = BX + BW - Off + Gap; X > SplitX + Wst; X -= Gap) if (X < BX + BW) Bar(X - Wst, X, BY, BY + BH, FLinearColor(1.f, 1.f, 1.f, 0.28f * K), FLinearColor(1.f, 1.f, 1.f, 0.0f));
		Flush(Ts);
	}
	{	// the clash point: a sparking star in both colours + VS
		const FVector2D Mk(SplitX + Sk * 0.5f, BY + BH * 0.5f);
		const float R = (62.f + 28.f * Clash.Hit) * U * (1.f + 0.1f * FMath::Sin(Tn * 28.f));
		const int32 Sp = 10;
		for (int32 i = 0; i < Sp; ++i)
		{
			const float Aa = 2.f * PI * i / Sp + Tn * 3.f;
			const FVector2D Tip = Mk + FVector2D(FMath::Cos(Aa), FMath::Sin(Aa)) * R * ((i % 2) ? 1.f : 0.6f);
			const FVector2D B0 = Mk + FVector2D(FMath::Cos(Aa - 0.3f), FMath::Sin(Aa - 0.3f)) * R * 0.28f, B1 = Mk + FVector2D(FMath::Cos(Aa + 0.3f), FMath::Sin(Aa + 0.3f)) * R * 0.28f;
			const FLinearColor Sc = FMath::Cos(Aa) < 0.f ? Ca : Cd;
			Tri(Ts, B0, Tip, B1, Al(FLinearColor::White, K), Al(FMath::Lerp(Sc, FLinearColor(1.f, 0.85f, 0.3f), 0.3f), 0.9f * K), Al(FLinearColor::White, K));
		}
		Flush(Ts);
		FunRing(C, Mk, R * 0.17f, R * 0.34f, FLinearColor(0.08f, 0.02f, 0.14f, 0.95f * K));
		FunRing(C, Mk, R * 0.34f, 3.f * U, Al(FLinearColor(1.f, 0.84f, 0.35f), K));
		FunTextL(C, TEXT("VS"), Mk.X, Mk.Y, 22.f * U, Al(FLinearColor(1.f, 0.88f, 0.4f), K));
	}
	auto TapIcon = [&](const FVector2D& P, const FLinearColor& Cc, float LastTap, bool bHuman)
	{
		const float Pu = FMath::Clamp(1.f - (Tn - LastTap) / 0.16f, 0.f, 1.f);
		const float R = 42.f * U * (1.f + 0.2f * Pu);
		FunRing(C, P, R * 0.5f, R, FLinearColor(0.05f, 0.03f, 0.1f, 0.95f * K));
		FunRing(C, P, R * 0.4f, R * 0.8f, Al(FMath::Lerp(Cc * 0.8f, FLinearColor::White, 0.45f * Pu), 0.95f * K));
		FunRing(C, P + FVector2D(0.f, -R * 0.35f), R * 0.32f, R * 0.4f, FLinearColor(1.f, 1.f, 1.f, 0.18f * K)); // gloss
		FunRing(C, P, R, 6.f * U, Al(FLinearColor(1.f, 0.84f, 0.35f), K));
		if (Pu > 0.f) FunRing(C, P, R * (1.f + 0.7f * (1.f - Pu)), 4.f * U, Al(FLinearColor::White, 0.8f * Pu * K));
		FunTextL(C, bHuman ? TEXT("TAP!") : TEXT("CPU"), P.X, P.Y, 24.f * U, Al(FLinearColor::White, K));
	};
	TapIcon(FVector2D(BX - 72.f * U, BY + BH * 0.5f), Ca, bFl ? Clash.TapD : Clash.TapA, bFl ? Clash.bHumanDef : Clash.bHumanAtt);
	TapIcon(FVector2D(BX + BW + Sk + 72.f * U, BY + BH * 0.5f), Cd, bFl ? Clash.TapA : Clash.TapD, bFl ? Clash.bHumanAtt : Clash.bHumanDef);

	// 6) instruction line + time left
	if (!Clash.bFinish)
	{
		const bool bHuman = Clash.bHumanAtt || Clash.bHumanDef;
		// Oct 30 (Evan): READY... beat before taps count, a big TAP! on go, FINAL PUSH (double punches) in the last second
		const float Fa = Clash.Age - ClashReadySecs;
		if (Fa < 0.f) FunTextL(C, TEXT("READY..."), Wd * 0.5f, Ht * 0.6f, 96.f * U, Al(FLinearColor(1.f, 0.88f, 0.4f), K));
		else if (Fa < 0.5f) FunTextL(C, bHuman ? TEXT("TAP!") : TEXT("GO!"), Wd * 0.5f, Ht * 0.6f, 150.f * U * (1.f + 0.6f * FMath::Max(0.f, 1.f - Fa / 0.15f)), Al(FLinearColor::White, K * FMath::Clamp((0.5f - Fa) / 0.2f, 0.f, 1.f)));
		else if (Clash.Age > ClashFightSecs - 1.f) FunTextL(C, TEXT("FINAL PUSH!  x2"), Wd * 0.5f, Ht * 0.6f, 80.f * U * (1.f + 0.08f * FMath::Sin(Tn * 20.f)), Al(FLinearColor(1.f, 0.35f, 0.75f), K));
		FunTextL(C, Fa < 0.f ? (bHuman ? TEXT("GET READY TO TAP!") : TEXT("CPU POWER CLASH!")) : (bHuman ? TEXT("TAP FAST!  BEHIND? EVERY PUNCH HITS HARDER!") : TEXT("CPU POWER CLASH!")), Wd * 0.5f, Ht * 0.925f, 30.f * U * (1.f + 0.06f * FMath::Sin(Tn * 14.f)), Al(FLinearColor::White, K));
		const float Left = FMath::Clamp(1.f - FMath::Max(0.f, Fa) / (ClashFightSecs - ClashReadySecs), 0.f, 1.f);
		Bar(BX, BX + BW * Left, BY + BH + 14.f * U, BY + BH + 20.f * U, Al(FLinearColor(1.f, 0.85f, 0.3f), 0.9f * K), Al(FLinearColor(1.f, 0.6f, 0.2f), 0.9f * K));
		Flush(Ts);
	}
}

// ------------------------------------------------------------------ build 28: ultimate-form avatars (Evan's face art, Content/Data/MenuArt/ult_avi_*.png)

int32 UZoogiFunSys::AvatarIndex(const AZoogiBall* B)
{
	if (!B) return -1;
	const FString N = B->Def.Name.ToLower();
	if (N == TEXT("bolt")) return 3; // the electric one gets the lightning face
	switch (FormIndex(B)) { case 0: return 2; case 1: case 2: return 1; case 3: return 0; default: return -1; }
}

UTexture2D* UZoogiFunSys::UltAvatar(const AZoogiBall* B)
{
	const int32 I = AvatarIndex(B);
	if (I < 0) return nullptr;
	if (UltAvis.Num() != 4)
	{
		UltAvis.SetNum(4);
		static const TCHAR* Names[] = { TEXT("frost"), TEXT("fox"), TEXT("ember"), TEXT("lightning") };
		int32 Got = 0;
		for (int32 k = 0; k < 4; ++k)
		{
			const FString P = FPaths::ProjectContentDir() / TEXT("Data/MenuArt") / FString::Printf(TEXT("ult_avi_%s.png"), Names[k]);
			UltAvis[k] = FPaths::FileExists(P) ? FImageUtils::ImportFileAsTexture2D(P) : nullptr;
			if (UltAvis[k]) ++Got;
		}
		UE_LOG(LogZoogi, Log, TEXT("FUN ultimate avatars loaded: %d / 4"), Got);
	}
	return UltAvis[I];
}

bool UZoogiFunSys::IsTransformed(int32 Player) const
{
	if (IsClashFighter(Player)) return true;
	const AZoogiBall* B = Strike.Ball.Get();
	return Strike.bOn && B && B->PlayerIndex == Player;
}
