// Episode Engine presentation: callouts, glow, centre-frame panels, power escalation, slow-mo, freeze, Titan Meteor.
// RULE ZERO: nothing in this file writes gameplay state. The only simulation-facing calls are CinematicTimeDilation
// (restored to the exact saved value) and the presentation freeze (pause + snapshot / verify of every body).
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeWidget.h"
#include "ZoogiEpisodeStage.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// =====================================================================================================================
// camera nudge

bool UZoogiEpisodeCamMod::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
	const float Dt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.1f);
	KickAge += Dt;
	if (Kick <= 0.f) return false;
	const float Env = Kick * FMath::Exp(-KickAge * 6.f);
	if (Env < 0.003f) { Kick = 0.f; return false; }
	const FVector Fwd = InOutPOV.Rotation.Vector();
	const float Punch = 30.f * Env * FMath::Sin(FMath::Min(KickAge * 16.f, PI));
	InOutPOV.Location += Fwd * Punch + FVector(FMath::Sin(KickAge * 53.f + Seed), FMath::Cos(KickAge * 47.f + 2.f * Seed), FMath::Sin(KickAge * 61.f + 3.f * Seed)) * 7.f * Env;
	InOutPOV.Rotation.Roll += 0.9f * Env * FMath::Sin(KickAge * 38.f + Seed);
	InOutPOV.FOV = FMath::Max(20.f, InOutPOV.FOV - 3.f * Env);
	return false;
}

// =====================================================================================================================
// setup

void UZoogiEpisodeSubsystem::EnsurePresentation()
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController()) return;
	if (!Widget)
	{
		Widget = CreateWidget<UZoogiEpisodeWidget>(PC, UZoogiEpisodeWidget::StaticClass());
		if (Widget) Widget->AddToViewport(6);
	}
	if (!Stage)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transient;
		Stage = W->SpawnActor<AZoogiEpisodeStage>(AZoogiEpisodeStage::StaticClass(), FTransform::Identity, SP);
	}
	if (!CamMod && PC->PlayerCameraManager)
		CamMod = Cast<UZoogiEpisodeCamMod>(PC->PlayerCameraManager->AddNewCameraModifier(UZoogiEpisodeCamMod::StaticClass()));
}

int32 UZoogiEpisodeSubsystem::PresentCap() const
{
	switch (Mode)
	{
	case EZoogiEpisodeMode::Off: return 0;
	case EZoogiEpisodeMode::Competitive: return 2; // callouts + glow only (the meteor plays short, see TryStartMeteor)
	default: return 5;
	}
}

void UZoogiEpisodeSubsystem::Nudge(float Strength)
{
	if (CamMod) CamMod->Nudge(Strength);
}

void UZoogiEpisodeSubsystem::Shot(const FString& Name)
{
	if (ShotDir.IsEmpty()) return;
	int32& N = ShotCounts.FindOrAdd(Name);
	if (N >= 2) return;
	++N;
	const FString Path = FPaths::Combine(ShotDir, FString::Printf(TEXT("ep_a%02d_%s_%d.png"), Truth.ActionId, *Name, N));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE screenshot requested: %s"), *Path);
}

// =====================================================================================================================
// live recognition -> tiers

void UZoogiEpisodeSubsystem::UpdateLive()
{
	if (!bActive) return;
	LiveRec = Recognize(Truth, Tune);
	if (Mode == EZoogiEpisodeMode::Off || !Widget) return;
	int32 Tier = LiveRec.Tier;
	if (Mode == EZoogiEpisodeMode::Full && Tier < 4)
	{
		// Full: the same deterministic score, a slightly lower presentation threshold
		const float S = LiveRec.Score * Tune.FullScoreMul;
		Tier = S >= Tune.T3 ? 3 : (S >= Tune.T2 ? 2 : (S >= Tune.T1 ? 1 : 0));
		Tier = FMath::Max(Tier, LiveRec.Tier);
	}
	for (EZoogiEpisodeTag T : LiveRec.Tags)
		if (!Announced.Contains(T)) { Announced.Add(T); QueueCallout(T, Tier); }
	const int32 Cap = PresentCap();
	while (PresentedTier < FMath::Min(Tier, Cap))
	{
		++PresentedTier;
		PresentTier(PresentedTier, LiveRec);
	}
}

void UZoogiEpisodeSubsystem::PresentTier(int32 Tier, const FZoogiEpisodeRecognitionRecord& R)
{
	Plan.Tier = FMath::Max(Plan.Tier, Tier);
	switch (Tier)
	{
	case 1:
		Nudge(0.35f);
		break;
	case 2:
	{
		// glow pulse on the walls / pieces the shot really used (last few meaningful surface contacts)
		int32 N = 0;
		TSet<UPrimitiveComponent*> Done;
		for (int32 i = Truth.Contacts.Num() - 1; i >= 0 && N < 3; --i)
		{
			const FZoogiContactEvent& E = Truth.Contacts[i];
			if (!E.bMeaningful || !E.Surface.IsValid() || Done.Contains(E.Surface.Get())) continue;
			Done.Add(E.Surface.Get());
			if (Stage) Stage->GlowSurface(E.Surface.Get(), E.Location, E.Normal, FLinearColor(1.f, 0.85f, 0.4f));
			++N;
		}
		Plan.bGlow = N > 0;
		Nudge(0.5f);
		Shot(TEXT("tier2"));
		break;
	}
	case 3:
		if (!bMeteor && !bMeteorPending) { StartPanels(3, R); StartSlowMo(); }
		Nudge(0.6f);
		break;
	case 4:
	case 5:
	{
		const FLinearColor C = ZoogiEpisode::PowerColor(Truth.Awaken);
		Plan.bPowerEscalation = true;
		Plan.PowerColor = C;
		if (Widget) Widget->SetPower(true, C, Tier >= 5 ? FString::Printf(TEXT("%s  //  EPISODE MOMENT"), *ZoogiEpisode::PowerTitle(Truth.Awaken)) : ZoogiEpisode::PowerTitle(Truth.Awaken));
		PowerLeft = Tune.PanelSeconds + 0.6f;
		if (!bMeteor && !bMeteorPending) StartPanels(Tier, R);
		Nudge(0.8f);
		break;
	}
	default: break;
	}
}

// =====================================================================================================================
// callouts (recency: never the same variant / text twice in a row)

void UZoogiEpisodeSubsystem::QueueCallout(EZoogiEpisodeTag Tag, int32 Tier)
{
	static const TArray<FString> V[(int32)EZoogiEpisodeTag::Num] = {
		{ TEXT("BANK SHOT"), TEXT("OFF THE WALL"), TEXT("BANKED IN") },
		{ TEXT("DOUBLE BANK"), TEXT("TWO-RAIL SHOT"), TEXT("WALL TO WALL") },
		{ TEXT("ALLY BUMP"), TEXT("PARTNER ASSIST"), TEXT("TEAM BUMP") },
		{ TEXT("ORB RELAY"), TEXT("RELAY!"), TEXT("CHAIN RELAY") },
		{ TEXT("WALL REDIRECT"), TEXT("REDIRECTED"), TEXT("CAROM") },
		{ TEXT("EDGE SAVE"), TEXT("SAVED ON THE LINE"), TEXT("BOUNDARY SAVE") },
		{ TEXT("AIRBORNE"), TEXT("LIFT OFF"), TEXT("SKY HOP") },
		{ TEXT("POWER HIT"), TEXT("FULL POWER"), TEXT("POWER STRIKE") },
		{ TEXT("GUARD BREAK"), TEXT("SHELL CRACKED"), TEXT("GUARD SHATTERED") },
		{ TEXT("ORB OUT"), TEXT("ORB AWAY"), TEXT("OFF THE EDGE") },
		{ TEXT("RING OUT"), TEXT("OUT OF THE RING"), TEXT("RING OUT!") },
		{ TEXT("TERRAIN"), TEXT("STAGE DAMAGE"), TEXT("ARENA CRACKED") },
		{ TEXT("PARTNER FOLLOW-UP"), TEXT("FOLLOW-UP"), TEXT("TAG IN") } };
	if (Mode == EZoogiEpisodeMode::Off) return;
	const TArray<FString>& Opts = V[(int32)Tag];
	int32 Pick = Truth.ActionId % Opts.Num();
	for (int32 Try = 0; Try < Opts.Num(); ++Try)
	{
		const int32 P = (Pick + Try) % Opts.Num();
		if ((LastVariant.Contains(Tag) && LastVariant[Tag] == P) || Opts[P] == LastCalloutText) continue;
		Pick = P;
		break;
	}
	LastVariant.Add(Tag, Pick);
	FCallout C;
	C.Main = Opts[Pick];
	if (Tag == EZoogiEpisodeTag::PowerHit && Truth.bCrit) C.Main = TEXT("CRITICAL HIT");
	C.Tier = FMath::Max(1, Tier);
	switch (Tag)
	{
	case EZoogiEpisodeTag::AllyBump: case EZoogiEpisodeTag::OrbRelay: case EZoogiEpisodeTag::PartnerFollowUp: C.Color = FLinearColor(0.45f, 0.9f, 1.f); break;
	case EZoogiEpisodeTag::PowerHit: case EZoogiEpisodeTag::GuardBreak: C.Color = ZoogiEpisode::PowerColor(Truth.Awaken); break;
	case EZoogiEpisodeTag::OrbOut: case EZoogiEpisodeTag::RingOut: C.Color = FLinearColor(1.f, 0.42f, 0.3f); break;
	case EZoogiEpisodeTag::Terrain: C.Color = FLinearColor(0.92f, 0.72f, 0.45f); break;
	default: C.Color = FLinearColor(1.f, 0.86f, 0.36f); break;
	}
	C.Sub = C.Tier >= 2 ? FString::Printf(TEXT("TRICK SHOT  -  P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()) : FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper());
	CalloutQueue.Add(C);
	LastCalloutText = C.Main;
}

void UZoogiEpisodeSubsystem::PumpCallouts()
{
	if (!Widget || CalloutQueue.Num() == 0 || RealClock < NextCalloutAt || bMeteor) return;
	const FCallout C = CalloutQueue[0];
	CalloutQueue.RemoveAt(0);
	Widget->ShowCallout(C.Main, C.Sub, C.Color, C.Tier);
	NextCalloutAt = RealClock + Tune.CalloutGap;
	Plan.Callouts++;
	Plan.LastCallout = C.Main;
	Nudge(C.Tier >= 2 ? 0.45f : 0.3f);
	if (C.Tier >= 2) Shot(TEXT("callout_t2"));
}

// =====================================================================================================================
// centre-frame panel compositor (tier 3+)

void UZoogiEpisodeSubsystem::StartPanels(int32 Tier, const FZoogiEpisodeRecognitionRecord& R)
{
	if (!Widget || !Stage || Mode == EZoogiEpisodeMode::Off || Mode == EZoogiEpisodeMode::Competitive) return;
	const bool bFull = Mode == EZoogiEpisodeMode::Full;
	int32 Count = Tier >= 5 ? 4 : (Tier >= 4 ? (bFull ? 4 : 3) : (bFull ? 3 : 2));
	EZoogiPanelLayout L = Count == 2 ? EZoogiPanelLayout::CornerPair : (Count == 3 ? EZoogiPanelLayout::Trio : EZoogiPanelLayout::Quad);
	// recency: never the same layout twice in a row
	if (L == LastLayout)
	{
		if (L == EZoogiPanelLayout::CornerPair) L = EZoogiPanelLayout::SidePair;
		else if (L == EZoogiPanelLayout::SidePair) L = EZoogiPanelLayout::CornerPair;
		else if (L == EZoogiPanelLayout::Trio) L = EZoogiPanelLayout::Quad;
		else L = EZoogiPanelLayout::Trio;
	}
	else if (L == EZoogiPanelLayout::CornerPair && (Truth.ActionId % 2) == 1 && LastLayout != EZoogiPanelLayout::SidePair) L = EZoogiPanelLayout::SidePair;
	Count = L == EZoogiPanelLayout::Quad ? 4 : (L == EZoogiPanelLayout::Trio ? 3 : 2);
	LastLayout = L;
	Plan.Layout = L;
	Plan.Panels = Count;
	// subjects: shooter, the first body it hit, a wide shot, the power target / deepest chain body
	PanelSubject.Reset();
	PanelSubject.Add(Truth.Actor);
	int32 Victim = ShooterFirstContact;
	int32 Deep = -1, DeepD = -1;
	for (const FZoogiContactEvent& E : Truth.Contacts)
		if (E.bMeaningful && E.OtherBody >= 0 && E.OtherBody != Truth.Actor && E.Depth + 1 > DeepD) { DeepD = E.Depth + 1; Deep = E.OtherBody; }
	if (Victim < 0) Victim = Deep;
	PanelSubject.Add(Victim >= 0 ? Victim : -1);
	PanelSubject.Add(-1);
	PanelSubject.Add(Deep >= 0 && Deep != Victim ? Deep : (Truth.PowerHitTarget >= 0 ? Truth.PowerHitTarget : Truth.Actor));
	if (Count == 2) PanelSubject.SetNum(2);
	Stage->EnsureCaptures(Count, Tune.PanelRes);
	PanelCount = Count;
	PanelAge = 0.f;
	bPanels = true;
	UpdatePanelCameras(0.f);
	Stage->CaptureAll(Count);
	const FLinearColor Frame = Tier >= 4 ? ZoogiEpisode::PowerColor(Truth.Awaken) : FLinearColor(0.96f, 0.96f, 1.f);
	Widget->ShowPanels(L, Stage->GetTargets(Count), Frame, Tune.PanelSeconds * (bFull ? 1.3f : 1.f) + (Tier >= 4 ? 0.4f : 0.f));
	UE_LOG(LogZoogi, Log, TEXT("EPISODE panels action=%d tier=%d layout=%s panels=%d"), Truth.ActionId, Tier, ZoogiEpisode::LayoutName(L), Count);
}

void UZoogiEpisodeSubsystem::UpdatePanelCameras(float Dt)
{
	if (!Stage) return;
	AZoogiGameMode* G = GM();
	for (int32 i = 0; i < PanelCount && i < PanelSubject.Num(); ++i)
	{
		const AActor* A = BodyActor(PanelSubject[i]);
		const float Orbit = FMath::DegreesToRadians(35.f + 95.f * i + 14.f * PanelAge);
		if (PanelSubject[i] < 0 || !A)
		{
			// wide: high over the arena, looking at the shooter's area
			const AActor* S = BodyActor(Truth.Actor);
			const FVector Look = S ? S->GetActorLocation() * 0.5f : FVector::ZeroVector;
			const FVector Cam = Look + FVector(FMath::Cos(Orbit) * 1700.f, FMath::Sin(Orbit) * 1700.f, 1900.f);
			Stage->SetCaptureView(i, Cam, Look, 60.f);
			continue;
		}
		const FVector P = A->GetActorLocation();
		const float Dist = i == 0 ? 420.f : 480.f;
		const float H = i == 1 ? 70.f : 160.f; // the struck body: low, heroic angle
		const FVector Cam = P + FVector(FMath::Cos(Orbit) * Dist, FMath::Sin(Orbit) * Dist, H);
		Stage->SetCaptureView(i, Cam, P + FVector(0, 0, 30.f), 50.f);
	}
	(void)G; (void)Dt;
}

// =====================================================================================================================
// slow-mo (CinematicTimeDilation, restored exactly)

void UZoogiEpisodeSubsystem::StartSlowMo()
{
	UWorld* W = GetWorld();
	if (bSlowMo || bFrozen || !W || Mode == EZoogiEpisodeMode::Off || Mode == EZoogiEpisodeMode::Competitive) return;
	AWorldSettings* WS = W->GetWorldSettings();
	if (!WS) return;
	SavedCinematic = WS->CinematicTimeDilation;
	AppliedCinematic = SavedCinematic * Tune.SlowMoScale;
	WS->CinematicTimeDilation = AppliedCinematic;
	bSlowMo = true;
	SlowMoLeft = Tune.SlowMoSeconds * (Mode == EZoogiEpisodeMode::Full ? 1.3f : 1.f);
	Plan.bSlowMo = true;
	Plan.SlowMoScale = Tune.SlowMoScale;
}

void UZoogiEpisodeSubsystem::StopSlowMo()
{
	if (!bSlowMo) return;
	bSlowMo = false;
	UWorld* W = GetWorld();
	AWorldSettings* WS = W ? W->GetWorldSettings() : nullptr;
	if (!WS) return;
	const bool bUntouched = WS->CinematicTimeDilation == AppliedCinematic;
	WS->CinematicTimeDilation = SavedCinematic;
	const bool bRestored = WS->CinematicTimeDilation == SavedCinematic;
	Plan.bDilationRestored &= bRestored;
	UE_LOG(LogZoogi, Log, TEXT("EPISODE slowmo action=%d scale=%.2f restored=%d value=%.3f%s"), Truth.ActionId, Tune.SlowMoScale, bRestored ? 1 : 0, WS->CinematicTimeDilation,
		bUntouched ? TEXT("") : TEXT(" (another system changed it meanwhile)"));
}

// =====================================================================================================================
// epilogue: only what physics / Resolve confirmed

void UZoogiEpisodeSubsystem::ShowEpilogue()
{
	if (bEpilogueShown) return;
	bEpilogueShown = true;
	if (Mode == EZoogiEpisodeMode::Off || !Widget) return;
	if (Plan.Tier < 3 && !Plan.bMeteor) return;
	AZoogiGameMode* G = GM();
	FString Text, Sub;
	FLinearColor C = Plan.bPowerEscalation ? Plan.PowerColor : FLinearColor(1.f, 0.88f, 0.45f);
	TArray<FString> Names;
	for (int32 P : Truth.RingOuts) if (P != Truth.Actor && G && G->Balls.IsValidIndex(P) && G->Balls[P]) Names.Add(FString::Printf(TEXT("P%d %s"), P + 1, *G->Balls[P]->Def.Name.ToUpper()));
	if (Names.Num() > 0) { Text = TEXT("RING OUT"); Sub = FString::Join(Names, TEXT("  +  ")); C = FLinearColor(1.f, 0.42f, 0.3f); }
	else if (Truth.OrbOuts > 0) { Text = Truth.OrbOuts > 1 ? FString::Printf(TEXT("ORB OUT x%d"), Truth.OrbOuts) : TEXT("ORB OUT"); Sub = FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()); }
	else if (Plan.bMeteor && MeteorTarget.IsValid()) { Text = TEXT("STILL STANDING"); Sub = FString::Printf(TEXT("P%d %s stays in"), MeteorTarget->PlayerIndex + 1, *MeteorTarget->Def.Name.ToUpper()); }
	else { Text = LiveRec.Headline.IsEmpty() ? TEXT("NICE SHOT") : LiveRec.Headline; Sub = FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()); }
	Plan.Epilogue = Text;
	Widget->ShowBanner(Text, Sub, C, Mode == EZoogiEpisodeMode::Full ? 1.8f : 1.3f);
	Shot(TEXT("epilogue"));
}

// =====================================================================================================================
// presentation freeze: pause + snapshot; on release verify nothing moved (and restore if anything did)

void UZoogiEpisodeSubsystem::FreezeWorld()
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = GM();
	if (bFrozen || !W || !G) return;
	StopSlowMo();
	Snap.Reset();
	auto Add = [this](UPrimitiveComponent* C)
	{
		if (!C || !C->IsSimulatingPhysics()) return;
		FBodySnap S; S.Comp = C; S.Xf = C->GetComponentTransform(); S.Lin = C->GetPhysicsLinearVelocity(); S.Ang = C->GetPhysicsAngularVelocityInRadians();
		Snap.Add(S);
	};
	for (AZoogiBall* B : G->Balls) if (B) Add(B->Sphere);
	for (AZoogiOrb* O : G->Orbs) if (O) Add(O->Sphere);
	if (!W->IsPaused()) UGameplayStatics::SetGamePaused(W, true);
	bFrozen = true;
	Plan.Freezes++;
}

void UZoogiEpisodeSubsystem::UnfreezeWorld()
{
	UWorld* W = GetWorld();
	if (!bFrozen || !W) return;
	float DriftP = 0.f, DriftV = 0.f;
	for (const FBodySnap& S : Snap)
	{
		UPrimitiveComponent* C = S.Comp.Get();
		if (!C) continue;
		DriftP = FMath::Max(DriftP, (float)FVector::Dist(C->GetComponentLocation(), S.Xf.GetLocation()));
		DriftV = FMath::Max(DriftV, (float)FVector::Dist(C->GetPhysicsLinearVelocity(), S.Lin));
		DriftV = FMath::Max(DriftV, (float)FVector::Dist(C->GetPhysicsAngularVelocityInRadians(), S.Ang));
	}
	if (DriftP > 0.001f || DriftV > 0.001f)
	{
		// Something moved a body during the freeze: put every body back exactly where the simulation left it.
		for (const FBodySnap& S : Snap)
		{
			UPrimitiveComponent* C = S.Comp.Get();
			if (!C) continue;
			C->SetWorldTransform(S.Xf, false, nullptr, ETeleportType::TeleportPhysics);
			C->SetPhysicsLinearVelocity(S.Lin);
			C->SetPhysicsAngularVelocityInRadians(S.Ang);
		}
	}
	Plan.FreezeDriftCm = FMath::Max(Plan.FreezeDriftCm, DriftP);
	Plan.FreezeDriftVel = FMath::Max(Plan.FreezeDriftVel, DriftV);
	UZoogiAnimeSubsystem* Anime = W->GetSubsystem<UZoogiAnimeSubsystem>();
	if (!(Anime && Anime->bFrozen)) UGameplayStatics::SetGamePaused(W, false);
	bFrozen = false;
	UE_LOG(LogZoogi, Log, TEXT("EPISODE freeze released action=%d bodies=%d drift_cm=%.4f drift_vel=%.4f%s"), Truth.ActionId, Snap.Num(), DriftP, DriftV,
		(DriftP > 0.001f || DriftV > 0.001f) ? TEXT(" (restored to the snapshot)") : TEXT(""));
	Snap.Reset();
}

// =====================================================================================================================
// Titan Meteor (archetype): presentation-only rock in perception space, then back to the exact real result

void UZoogiEpisodeSubsystem::TryStartMeteor()
{
	if (!bMeteorPending || bMeteor) return;
	UWorld* W = GetWorld();
	if (!W || !Stage || !Widget) return;
	UZoogiAnimeSubsystem* Anime = W->GetSubsystem<UZoogiAnimeSubsystem>();
	if ((Anime && Anime->bFrozen) || W->IsPaused()) return; // an anime freeze-frame owns the moment: wait for it to release
	if (W->GetTimeSeconds() - MeteorHitGame > Tune.MeteorMaxDelay || !MeteorTarget.IsValid())
	{
		bMeteorPending = false;
		UE_LOG(LogZoogi, Log, TEXT("EPISODE meteor skipped action=%d (%.2f s after the hit)"), Truth.ActionId, W->GetTimeSeconds() - MeteorHitGame);
		return;
	}
	bMeteorPending = false;
	if (Widget->PanelsVisible()) Widget->CollapsePanels();
	FreezeWorld();
	if (!bFrozen) return;
	MeteorDur = Mode == EZoogiEpisodeMode::Competitive ? Tune.MeteorSecondsShort : (Mode == EZoogiEpisodeMode::Full ? Tune.MeteorSecondsFull : Tune.MeteorSeconds);
	MeteorT = 0.f;
	bMeteor = true;
	bMeteorImpactShot = false;
	const FLinearColor C = ZoogiEpisode::PowerColor(EZoogiArtifact::Titan);
	Stage->BeginMeteor(MeteorTarget->BallLocation(), MeteorShooter.IsValid() ? MeteorShooter->BallLocation() : MeteorTarget->BallLocation() - FVector(400, 0, 0), C);
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		PrevView = PC->GetViewTarget();
		if (Stage->GetCamera()) PC->SetViewTarget(Stage->GetCamera());
	}
	Widget->SetPower(true, C, TEXT("TITAN METEOR"));
	PowerLeft = 0.f;
	Widget->ShowCallout(TEXT("TITAN METEOR"), FString::Printf(TEXT("P%d %s  >>  P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper(), MeteorTarget->PlayerIndex + 1, *MeteorTarget->Def.Name.ToUpper()), C, 2);
	Plan.bMeteor = true;
	Plan.MeteorSeconds = MeteorDur;
	Plan.bPowerEscalation = true;
	Plan.PowerColor = C;
	Plan.Tier = FMath::Max(Plan.Tier, 4);
	PresentedTier = FMath::Max(PresentedTier, 4);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE meteor start action=%d target=P%d mode=%s seconds=%.2f"), Truth.ActionId, MeteorTarget->PlayerIndex + 1, ZoogiEpisode::ModeName(Mode), MeteorDur);
}

void UZoogiEpisodeSubsystem::TickMeteor(float Dt)
{
	if (!bMeteor || !Stage) return;
	MeteorT += Dt;
	const float T = FMath::Clamp(MeteorT / FMath::Max(0.1f, MeteorDur), 0.f, 1.f);
	if (Stage->UpdateMeteor(T))
	{
		Widget->Flash(0.85f, FLinearColor(1.f, 0.93f, 0.8f));
		Nudge(1.f);
	}
	Widget->SetDarken(0.18f * FMath::SmoothStep(0.f, 0.15f, T) * (1.f - FMath::SmoothStep(0.8f, 1.f, T)));
	if (T > 0.22f && T < 0.5f) Shot(TEXT("meteor_lift"));
	if (T > 0.56f && T < 0.66f) Shot(TEXT("meteor_drop"));
	if (T > 0.68f && !bMeteorImpactShot) { bMeteorImpactShot = true; Shot(TEXT("meteor_impact")); }
	if (MeteorT >= MeteorDur) EndMeteor();
}

void UZoogiEpisodeSubsystem::EndMeteor()
{
	if (!bMeteor) return;
	bMeteor = false;
	if (Stage) Stage->EndMeteor();
	UWorld* W = GetWorld();
	if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
		if (PrevView.IsValid()) PC->SetViewTarget(PrevView.Get());
	if (Widget) { Widget->SetDarken(0.f); }
	PowerLeft = 1.0f; // power frame lingers into the real result
	UnfreezeWorld();
	UE_LOG(LogZoogi, Log, TEXT("EPISODE meteor end action=%d (real physics resumes from the exact frozen state)"), Truth.ActionId);
}

// =====================================================================================================================

void UZoogiEpisodeSubsystem::TickPresentation(float Dt)
{
	if (!Widget) return;
	PumpCallouts();
	if (bSlowMo)
	{
		SlowMoLeft -= Dt;
		if (SlowMoLeft <= 0.f) StopSlowMo();
	}
	if (bPanels && Stage)
	{
		PanelAge += Dt;
		UpdatePanelCameras(Dt);
		Stage->CaptureAll(PanelCount);
		if (Widget->PanelsFullyIn()) Shot(PresentedTier >= 4 ? TEXT("tier4_panels") : TEXT("tier3_panels"));
		if (!Widget->PanelsVisible()) bPanels = false;
	}
	if (PowerLeft > 0.f && !bMeteor)
	{
		PowerLeft -= Dt;
		if (PowerLeft <= 0.f) Widget->SetPower(false, Plan.PowerColor, TEXT(""));
	}
	TryStartMeteor();
	if (bMeteor) TickMeteor(Dt);
	if (Stage) Stage->TickStage(Dt);
	Widget->TickEpisode(Dt);
}
