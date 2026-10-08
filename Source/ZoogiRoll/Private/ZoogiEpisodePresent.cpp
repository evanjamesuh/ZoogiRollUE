// Episode Engine core: tier presentation planning -> cues for the presentation owners, recency, presentation hold.
// This file renders nothing except the minimal fallback text overlay (callouts / epilogue) used while nobody is bound
// to OnCallout / OnEpilogue. Panels (Gameplay & UI), camera shots + time scale (Game Camera) and power / Titan Meteor
// visuals (Animations & Powers) plug in through the cue delegates on UZoogiEpisodeSubsystem.
// RULE ZERO: nothing in this file writes gameplay state. The only simulation-facing call is the presentation hold
// (pause + snapshot / verify / restore of every body).
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeCalloutWidget.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "Components/SphereComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	const TCHAR* CueKindName(EZoogiEpisodeCueKind K)
	{
		switch (K)
		{
		case EZoogiEpisodeCueKind::Tier: return TEXT("Tier");
		case EZoogiEpisodeCueKind::Callout: return TEXT("Callout");
		case EZoogiEpisodeCueKind::Panels: return TEXT("Panels");
		case EZoogiEpisodeCueKind::Camera: return TEXT("Camera");
		case EZoogiEpisodeCueKind::Power: return TEXT("Power");
		case EZoogiEpisodeCueKind::Archetype: return TEXT("Archetype");
		case EZoogiEpisodeCueKind::Epilogue: return TEXT("Epilogue");
		default: return TEXT("?");
		}
	}
}

// =====================================================================================================================
// setup / helpers

void UZoogiEpisodeSubsystem::EnsureFallback()
{
	BindDirector();
	if (Fallback || (OnCallout.IsBound() && OnEpilogue.IsBound())) return;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController()) return;
	Fallback = CreateWidget<UZoogiEpisodeCalloutWidget>(PC, UZoogiEpisodeCalloutWidget::StaticClass());
	if (Fallback) Fallback->AddToViewport(6);
}

int32 UZoogiEpisodeSubsystem::PresentCap() const
{
	switch (Mode)
	{
	case EZoogiEpisodeMode::Off: return 0;
	case EZoogiEpisodeMode::Competitive: return 2; // callouts + glow only (the archetype plays short, see TryStartArchetype)
	default: return 5;
	}
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

void UZoogiEpisodeSubsystem::ShotLater(const FString& Name, float Delay)
{
	if (!ShotDir.IsEmpty()) LaterShots.Add(TPair<FString, double>(Name, RealClock + Delay));
}

FZoogiEpisodeCue UZoogiEpisodeSubsystem::MakeCue(EZoogiEpisodeCueKind Kind, int32 Tier) const
{
	FZoogiEpisodeCue C;
	C.Kind = Kind;
	C.ActionId = Truth.ActionId;
	C.Tier = Tier;
	C.Mode = Mode;
	C.Archetype = LiveRec.Archetype;
	C.Tags = LiveRec.Tags;
	C.Power = Truth.Awaken;
	C.PowerColor = ZoogiEpisode::PowerColor(Truth.Awaken);
	C.Actor = BodyActor(Truth.Actor);
	// target: the shooter's first zoogi / orb contact; secondary: the deepest body in the causal chain
	int32 Deep = -1, DeepD = -1;
	for (const FZoogiContactEvent& E : Truth.Contacts)
		if (E.bMeaningful && E.OtherBody >= 0 && E.OtherBody != Truth.Actor && E.Depth + 1 > DeepD) { DeepD = E.Depth + 1; Deep = E.OtherBody; }
	const int32 Tgt = ShooterFirstContact >= 0 ? ShooterFirstContact : Deep;
	C.Target = BodyActor(Tgt);
	C.Secondary = BodyActor(Deep >= 0 && Deep != Tgt ? Deep : (Truth.PowerHitTarget >= 0 ? Truth.PowerHitTarget : -1));
	C.ImpactPoint = Truth.bPowerHit ? Truth.PowerHitLocation : (C.Target.IsValid() ? C.Target->GetActorLocation() : FVector::ZeroVector);
	return C;
}

void UZoogiEpisodeSubsystem::SendCue(const FZoogiEpisodeCueEvent& Event, FZoogiEpisodeCue& Cue)
{
	Plan.Cues++;
	if (Cue.Shots.Num() == 0 && Mode != EZoogiEpisodeMode::Off) PickShots(Cue);
	NoteShots(Cue);
	const bool bBound = Event.IsBound();
	if (bBound) { Plan.CuesHandled++; Event.Broadcast(Cue); }
	UE_LOG(LogZoogi, Log, TEXT("EPISODE cue action=%d kind=%s tier=%d receivers=%d%s%s"), Cue.ActionId, CueKindName(Cue.Kind), Cue.Tier, bBound ? 1 : 0,
		Cue.Text.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" text=\"%s\""), *Cue.Text),
		Cue.Kind == EZoogiEpisodeCueKind::Panels ? *FString::Printf(TEXT(" layout=%s panels=%d seconds=%.2f"), ZoogiEpisode::LayoutName(Cue.Layout), Cue.PanelCount, Cue.Seconds)
		: (Cue.Kind == EZoogiEpisodeCueKind::Archetype ? *FString::Printf(TEXT(" archetype=%s hold=%d seconds=%.2f"), ZoogiEpisode::ArchetypeName(Cue.Archetype), Cue.HoldToken, Cue.Seconds)
		: (Cue.Kind == EZoogiEpisodeCueKind::Camera ? *FString::Printf(TEXT(" slowmo=%.2fx%.2fs"), Cue.SlowMoScale, Cue.SlowMoSeconds) : TEXT(""))));
	if (Cue.Shots.Num() > 0)
	{
		FString S;
		for (const FZoogiEpisodeShot& Sh : Cue.Shots)
			S += FString::Printf(TEXT("%s%s%s%s%s"), S.IsEmpty() ? TEXT("") : TEXT(","), *Sh.ShotId.ToString(), Sh.Character.IsEmpty() ? TEXT("") : TEXT("@"), *Sh.Character,
				Sh.AssetKey.IsNone() ? TEXT("") : *FString::Printf(TEXT("{%s}"), *Sh.AssetKey.ToString()));
		UE_LOG(LogZoogi, Log, TEXT("EPISODE shots action=%d kind=%s tier=%d shots=%s"), Cue.ActionId, CueKindName(Cue.Kind), Cue.Tier, *S);
	}
}

// =====================================================================================================================
// live recognition -> tiers

void UZoogiEpisodeSubsystem::UpdateLive()
{
	if (!bActive) return;
	LiveRec = Recognize(Truth, Tune);
	if (Mode == EZoogiEpisodeMode::Off) return;
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
	const int32 Before = PresentedTier;
	while (PresentedTier < FMath::Min(Tier, Cap))
	{
		++PresentedTier;
		PresentTier(PresentedTier, LiveRec);
	}
	if (PresentedTier > Before) FallbackTier(PresentedTier);
}

void UZoogiEpisodeSubsystem::PresentTier(int32 Tier, const FZoogiEpisodeRecognitionRecord& R)
{
	Plan.Tier = FMath::Max(Plan.Tier, Tier);
	FZoogiEpisodeCue C = MakeCue(EZoogiEpisodeCueKind::Tier, Tier);
	C.Text = R.Headline;
	if (Tier == 2)
	{
		// glow targets: the walls / pieces the shot really used (last few meaningful surface contacts)
		TSet<UPrimitiveComponent*> Done;
		for (int32 i = Truth.Contacts.Num() - 1; i >= 0 && C.Surfaces.Num() < 3; --i)
		{
			const FZoogiContactEvent& E = Truth.Contacts[i];
			if (!E.bMeaningful || !E.Surface.IsValid() || Done.Contains(E.Surface.Get())) continue;
			Done.Add(E.Surface.Get());
			C.Surfaces.Add(E.Surface);
			C.SurfacePoints.Add(E.Location);
			C.SurfaceNormals.Add(E.Normal);
		}
		Plan.bGlow = C.Surfaces.Num() > 0;
		Shot(TEXT("tier2"));
	}
	SendCue(OnTier, C);
	LastTierShots = C.Shots;
	const bool bArchetypeOwns = bArchetypePending || bFrozen || Plan.bMeteor;
	if (Tier == 3 && !bArchetypeOwns) RequestPanels(3);
	if (Tier >= 4)
	{
		if (!bPowerSent && !bArchetypeOwns)
		{
			bPowerSent = true;
			FZoogiEpisodeCue P = MakeCue(EZoogiEpisodeCueKind::Power, Tier);
			P.Text = Tier >= 5 ? FString::Printf(TEXT("%s  //  EPISODE MOMENT"), *ZoogiEpisode::PowerTitle(Truth.Awaken)) : ZoogiEpisode::PowerTitle(Truth.Awaken);
			P.Color = P.PowerColor;
			P.Seconds = Tune.PanelSeconds + 0.6f;
			Plan.bPowerEscalation = true;
			Plan.PowerColor = P.PowerColor;
			SendCue(OnPower, P);
		}
		if (!bArchetypeOwns) RequestPanels(Tier);
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
		{ TEXT("PARTNER FOLLOW-UP"), TEXT("FOLLOW-UP"), TEXT("TAG IN") },
		{ TEXT("NEAR MISS!"), TEXT("SO CLOSE!"), TEXT("WHISKER AWAY") },
		{ TEXT("BOING!"), TEXT("PINBALL!"), TEXT("RICOCHET!") },
		{ TEXT("ORB ALMOST OUT!"), TEXT("ON THE LINE!"), TEXT("TEETERING!") } };
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
	FZoogiEpisodeCue C = MakeCue(EZoogiEpisodeCueKind::Callout, FMath::Max(1, Tier));
	C.Tag = Tag;
	C.Text = Opts[Pick];
	if (Tag == EZoogiEpisodeTag::PowerHit && Truth.bCrit) C.Text = TEXT("CRITICAL HIT");
	switch (Tag)
	{
	case EZoogiEpisodeTag::AllyBump: case EZoogiEpisodeTag::OrbRelay: case EZoogiEpisodeTag::PartnerFollowUp: C.Color = FLinearColor(0.45f, 0.9f, 1.f); break;
	case EZoogiEpisodeTag::PowerHit: case EZoogiEpisodeTag::GuardBreak: C.Color = ZoogiEpisode::PowerColor(Truth.Awaken); break;
	case EZoogiEpisodeTag::OrbOut: case EZoogiEpisodeTag::RingOut: C.Color = FLinearColor(1.f, 0.42f, 0.3f); break;
	case EZoogiEpisodeTag::Terrain: C.Color = FLinearColor(0.92f, 0.72f, 0.45f); break;
	case EZoogiEpisodeTag::NearMiss: case EZoogiEpisodeTag::FunnyBounce: case EZoogiEpisodeTag::OrbAlmostOut: C.Color = FLinearColor(0.75f, 1.f, 0.55f); break;
	default: C.Color = FLinearColor(1.f, 0.86f, 0.36f); break;
	}
	C.SubText = C.Tier >= 2 ? FString::Printf(TEXT("TRICK SHOT  -  P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()) : FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper());
	CalloutQueue.Add(C);
	LastCalloutText = C.Text;
}

void UZoogiEpisodeSubsystem::PumpCallouts()
{
	if (CalloutQueue.Num() == 0 || RealClock < NextCalloutAt || bFrozen) return;
	FZoogiEpisodeCue C = CalloutQueue[0];
	CalloutQueue.RemoveAt(0);
	SendCue(OnCallout, C);
	if (!OnCallout.IsBound() && Fallback) Fallback->ShowCallout(C.Text, C.SubText, C.Color, C.Tier);
	NextCalloutAt = RealClock + Tune.CalloutGap;
	Plan.Callouts++;
	Plan.LastCallout = C.Text;
	if (C.Tier >= 2) Shot(TEXT("callout_t2"));
}

// =====================================================================================================================
// tier 3+: panel compositor + camera requests (recency: never the same layout twice in a row)

void UZoogiEpisodeSubsystem::RequestPanels(int32 Tier)
{
	if (Mode == EZoogiEpisodeMode::Off || Mode == EZoogiEpisodeMode::Competitive) return;
	const bool bFull = Mode == EZoogiEpisodeMode::Full;
	int32 Count = Tier >= 5 ? 4 : (Tier >= 4 ? (bFull ? 4 : 3) : (bFull ? 3 : 2));
	EZoogiPanelLayout L = Count == 2 ? EZoogiPanelLayout::CornerPair : (Count == 3 ? EZoogiPanelLayout::Trio : EZoogiPanelLayout::Quad);
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

	FZoogiEpisodeCue P = MakeCue(EZoogiEpisodeCueKind::Panels, Tier);
	P.Layout = L;
	P.PanelCount = Count;
	P.Seconds = Tune.PanelSeconds * (bFull ? 1.3f : 1.f) + (Tier >= 4 ? 0.4f : 0.f);
	P.Color = Tier >= 4 ? P.PowerColor : FLinearColor(0.96f, 0.96f, 1.f); // panel frame colour
	P.Text = LiveRec.Headline;
	SendCue(OnPanels, P);

	if (!Plan.bSlowMo)
	{
		// one camera / time-scale request per action (the camera director eases in / out and restores exactly)
		FZoogiEpisodeCue Cam = MakeCue(EZoogiEpisodeCueKind::Camera, Tier);
		Cam.bSlowMo = true;
		Cam.SlowMoScale = Tune.SlowMoScale;
		Cam.SlowMoSeconds = Tune.SlowMoSeconds * (bFull ? 1.3f : 1.f);
		Cam.Seconds = P.Seconds;
		Plan.bSlowMo = true;
		Plan.SlowMoScale = Cam.SlowMoScale;
		SendCue(OnCamera, Cam);
	}
	Shot(Tier >= 4 ? TEXT("tier4_request") : TEXT("tier3_request"));
}

// =====================================================================================================================
// epilogue: only what physics / Resolve confirmed

void UZoogiEpisodeSubsystem::ShowEpilogue()
{
	if (bEpilogueShown) return;
	bEpilogueShown = true;
	if (Mode == EZoogiEpisodeMode::Off) return;
	if (Plan.Tier < 3 && !Plan.bMeteor) return;
	AZoogiGameMode* G = GM();
	FString Text, Sub;
	FLinearColor C = Plan.bPowerEscalation ? Plan.PowerColor : FLinearColor(1.f, 0.88f, 0.45f);
	TArray<FString> Names;
	for (int32 P : Truth.RingOuts) if (P != Truth.Actor && G && G->Balls.IsValidIndex(P) && G->Balls[P]) Names.Add(FString::Printf(TEXT("P%d %s"), P + 1, *G->Balls[P]->Def.Name.ToUpper()));
	if (Names.Num() > 0) { Text = TEXT("RING OUT"); Sub = FString::Join(Names, TEXT("  +  ")); C = FLinearColor(1.f, 0.42f, 0.3f); }
	else if (Truth.OrbOuts > 0) { Text = Truth.OrbOuts > 1 ? FString::Printf(TEXT("ORB OUT x%d"), Truth.OrbOuts) : TEXT("ORB OUT"); Sub = FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()); }
	else if (Plan.bMeteor && ArchetypeTarget.IsValid()) { Text = TEXT("STILL STANDING"); Sub = FString::Printf(TEXT("P%d %s stays in"), ArchetypeTarget->PlayerIndex + 1, *ArchetypeTarget->Def.Name.ToUpper()); }
	else { Text = LiveRec.Headline.IsEmpty() ? TEXT("NICE SHOT") : LiveRec.Headline; Sub = FString::Printf(TEXT("P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper()); }
	Plan.Epilogue = Text;
	FZoogiEpisodeCue E = MakeCue(EZoogiEpisodeCueKind::Epilogue, Plan.Tier);
	E.Text = Text;
	E.SubText = Sub;
	E.Color = C;
	E.Seconds = Mode == EZoogiEpisodeMode::Full ? 1.8f : 1.3f;
	SendCue(OnEpilogue, E);
	if (!OnEpilogue.IsBound() && Fallback) Fallback->ShowBanner(Text, Sub, C, E.Seconds);
	Shot(TEXT("epilogue"));
}

// =====================================================================================================================
// presentation hold: pause + snapshot; on release verify nothing moved (and restore if anything did)

int32 UZoogiEpisodeSubsystem::BeginHold(float MaxSeconds, const TCHAR* Reason)
{
	if (bFrozen || Mode == EZoogiEpisodeMode::Off) return 0;
	FreezeWorld();
	if (!bFrozen) return 0;
	HoldToken = NextHoldToken++;
	HoldLeft = FMath::Clamp(MaxSeconds, 0.05f, 6.f);
	HoldReason = Reason ? Reason : TEXT("");
	UE_LOG(LogZoogi, Log, TEXT("EPISODE hold begin action=%d token=%d seconds=%.2f reason=%s bodies=%d"), Truth.ActionId, HoldToken, HoldLeft, *HoldReason, Snap.Num());
	return HoldToken;
}

void UZoogiEpisodeSubsystem::ReleaseHold(int32 Token)
{
	if (bFrozen && Token != 0 && Token == HoldToken) UnfreezeWorld();
}

void UZoogiEpisodeSubsystem::FreezeWorld()
{
	UWorld* W = GetWorld();
	AZoogiGameMode* G = GM();
	if (bFrozen || !W || !G) return;
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
		// Something moved a body during the hold: put every body back exactly where the simulation left it.
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
	UE_LOG(LogZoogi, Log, TEXT("EPISODE freeze released action=%d token=%d reason=%s bodies=%d drift_cm=%.4f drift_vel=%.4f%s"), Truth.ActionId, HoldToken, *HoldReason, Snap.Num(), DriftP, DriftV,
		(DriftP > 0.001f || DriftV > 0.001f) ? TEXT(" (restored to the snapshot)") : TEXT(""));
	HoldToken = 0;
	HoldLeft = 0.f;
	Snap.Reset();
}

// =====================================================================================================================
// Titan Meteor archetype: hold the real world, ask Animations & Powers to play the meteor, then resume exactly

void UZoogiEpisodeSubsystem::TryStartArchetype()
{
	if (!bArchetypePending) return;
	UWorld* W = GetWorld();
	if (!W) return;
	UZoogiAnimeSubsystem* Anime = W->GetSubsystem<UZoogiAnimeSubsystem>();
	if ((Anime && Anime->bFrozen) || W->IsPaused()) return; // an anime freeze-frame owns the moment: wait for it to release
	// RULE ZERO: the game's own hit-stop / slow-mo (AZoogiPlayerController) changes global time dilation and ends on a
	// real-time clock that keeps running while paused. Holding inside that window would shorten the dilated stretch and
	// change physics integration, so wait until it has ended; then the dilation timeline is exactly the Off-mode one.
	if (const AWorldSettings* WS = W->GetWorldSettings())
		if (WS->TimeDilation != 1.f && W->GetTimeSeconds() - ArchetypeHitGame <= Tune.MeteorMaxDelay) return;
	bArchetypePending = false;
	if (W->GetTimeSeconds() - ArchetypeHitGame > Tune.MeteorMaxDelay || !ArchetypeTarget.IsValid())
	{
		UE_LOG(LogZoogi, Log, TEXT("EPISODE archetype skipped action=%d (%.2f s after the hit)"), Truth.ActionId, W->GetTimeSeconds() - ArchetypeHitGame);
		return;
	}
	const float Dur = Mode == EZoogiEpisodeMode::Competitive ? Tune.MeteorSecondsShort : (Mode == EZoogiEpisodeMode::Full ? Tune.MeteorSecondsFull : Tune.MeteorSeconds);
	const int32 Token = BeginHold(Dur, TEXT("TitanMeteor"));
	if (Token == 0) return;
	FZoogiEpisodeCue C = MakeCue(EZoogiEpisodeCueKind::Archetype, 4);
	C.Archetype = EZoogiEpisodeArchetype::TitanMeteor;
	C.Actor = ArchetypeShooter;
	C.Target = ArchetypeTarget;
	C.ImpactPoint = ArchetypeTarget->BallLocation();          // AZoogiTitanMeteorFX::Play(Start, Impact = this, Seconds)
	C.Power = EZoogiArtifact::Titan;
	C.PowerColor = ZoogiEpisode::PowerColor(EZoogiArtifact::Titan);
	C.Color = C.PowerColor;
	C.Seconds = Dur;
	C.HoldToken = Token;
	C.Text = TEXT("TITAN METEOR");
	C.SubText = FString::Printf(TEXT("P%d %s  >>  P%d %s"), Truth.Actor + 1, *Truth.ActorName.ToUpper(), ArchetypeTarget->PlayerIndex + 1, *ArchetypeTarget->Def.Name.ToUpper());
	Plan.bMeteor = true;
	Plan.MeteorSeconds = Dur;
	Plan.bPowerEscalation = true;
	Plan.PowerColor = C.PowerColor;
	Plan.Tier = FMath::Max(Plan.Tier, 4);
	PresentedTier = FMath::Max(PresentedTier, 4);
	bPowerSent = true;
	SendCue(OnArchetype, C);
	if (!OnArchetype.IsBound() && Fallback) Fallback->ShowCallout(C.Text, C.SubText, C.Color, 2);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE meteor start action=%d target=P%d mode=%s seconds=%.2f receivers=%d"), Truth.ActionId, ArchetypeTarget->PlayerIndex + 1, ZoogiEpisode::ModeName(Mode), Dur, OnArchetype.IsBound() ? 1 : 0);
}

// =====================================================================================================================

void UZoogiEpisodeSubsystem::TickPresentation(float Dt)
{
	PumpCallouts();
	for (int32 i = 0; i < LaterShots.Num(); ++i)
		if (RealClock >= LaterShots[i].Value) { Shot(LaterShots[i].Key); LaterShots.RemoveAt(i); break; } // one screenshot per frame
	TryStartArchetype();
	TickAirSlow(Dt);
	if (bFrozen)
	{
		// the world is paused, so the camera manager does not update by itself: drive it (view target / modifiers of
		// whichever receiver took the view)
		if (UWorld* W = GetWorld())
			if (APlayerController* PC = W->GetFirstPlayerController())
				if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(Dt);
		const float Total = Plan.MeteorSeconds > 0.f ? Plan.MeteorSeconds : 1.f;
		if (HoldLeft < Total * 0.5f) Shot(TEXT("hold_mid"));
		HoldLeft -= Dt;
		if (HoldLeft <= 0.f) UnfreezeWorld();
	}
	if (Fallback) Fallback->TickEpisode(Dt);
}
