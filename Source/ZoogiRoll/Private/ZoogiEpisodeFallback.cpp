// Episode core: tier 1-3 FALLBACK presentation, used only while no receiver is bound to the matching cue delegate.
//  tier 1  reaction  : callout + small insert panel (Game Camera director close-up into a render target)
//  tier 2  trick shot: callout + contact glow + brief panel on the subject
//  tier 3  airborne  : presentation hold (true pause + snapshot verify) while the Game Camera director flies a
//                      tracking shot around the anchored Zoogi in the main view, then exact hand-back + resume.
// Physics is never time-dilated by the episode (Rule Zero); the hold waits until the game's own hit-stop has ended.
#include "ZoogiEpisodeSubsystem.h"
#include "ZoogiEpisodeCalloutWidget.h"
#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiShotLibrary.h"
#include "ZoogiFX.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiRoll.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"

void UZoogiEpisodeSubsystem::BindDirector()
{
	if (bDirectorBound) return;
	UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(this);
	if (!Dir) return;
	bDirectorBound = true;
	Dir->OnShotFinished.AddWeakLambda(this, [this](FZoogiShotHandle Shot, EZoogiShotEndReason Reason)
	{
		if (Shot.Id == 0 || Shot.Id != AirShotId) return;
		AirShotId = 0;
		UWorld* W = GetWorld();
		APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
		const bool bView = !PC || !AirPrevView.IsValid() || PC->GetViewTarget() == AirPrevView.Get();
		const AWorldSettings* WS = W ? W->GetWorldSettings() : nullptr;
		const bool bCine = !WS || WS->CinematicTimeDilation == StartCinematicDilation;
		Plan.bCameraRestored &= bView && bCine;
		UE_LOG(LogZoogi, Log, TEXT("EPISODE airshot camera returned action=%d view_target_restored=%d cine_dilation_restored=%d reason=%d"), Truth.ActionId, bView ? 1 : 0, bCine ? 1 : 0, (int32)Reason);
	});
}

bool UZoogiEpisodeSubsystem::FallbackPanel(int32 Slot, AActor* Focus, AActor* Secondary, uint8 ShotType, float Seconds, const FString& Caption, const FLinearColor& Frame, bool bSmall)
{
	UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(this);
	if (!Dir || !Fallback || !Focus) return false;
	FZoogiShotPlayParams P;
	P.RenderTargetSize = bSmall ? FIntPoint(480, 270) : FIntPoint(640, 360);
	P.DurationOverride = Seconds + 0.25f;
	const FZoogiShotHandle H = Dir->PlayShotType((EZoogiShotType)ShotType, Focus, Secondary, EZoogiShotOutput::RenderTarget, P);
	UTextureRenderTarget2D* RT = H.IsValid() ? Dir->GetShotRenderTarget(H) : nullptr;
	if (!RT) return false;
	Fallback->ShowPanel(Slot, RT, Seconds, Caption, Frame, bSmall);
	Plan.Panels++;
	UE_LOG(LogZoogi, Log, TEXT("EPISODE fallback panel action=%d slot=%d shot=%s focus=%s secondary=%s seconds=%.2f caption=\"%s\""), Truth.ActionId, Slot,
		ZoogiShot::TypeName((EZoogiShotType)ShotType), *Focus->GetName(), Secondary ? *Secondary->GetName() : TEXT("-"), Seconds, *Caption);
	return true;
}

void UZoogiEpisodeSubsystem::FallbackGlow(int32 MaxN)
{
	UWorld* W = GetWorld();
	if (!W) return;
	int32 N = 0;
	for (int32 i = Truth.Contacts.Num() - 1; i >= 0 && N < MaxN; --i)
	{
		const FZoogiContactEvent& E = Truth.Contacts[i];
		if (!E.bMeaningful) continue;
		// presentation-only glow ring (no collision) where the real contact happened
		AZoogiFX::Spawn(W, E.Location + FVector(0, 0, 6.f), FLinearColor(1.f, 0.82f, 0.32f), 110.f, 0.7f, true, 5.f);
		AZoogiFX::Spawn(W, E.Location, FLinearColor(1.f, 0.9f, 0.55f), 45.f, 0.45f, false, 4.f);
		++N;
	}
	if (N > 0) Plan.bGlow = true;
}

void UZoogiEpisodeSubsystem::FallbackTier(int32 Tier)
{
	if (Mode == EZoogiEpisodeMode::Off || bFrozen || bArchetypePending || Plan.bMeteor) return;
	const bool bCompetitive = Mode == EZoogiEpisodeMode::Competitive;
	AActor* Subject = LastTierShots.Num() > 0 && LastTierShots[0].SubjectActor.IsValid() ? LastTierShots[0].SubjectActor.Get() : BodyActor(Truth.Actor);
	AActor* Shooter = BodyActor(Truth.Actor);
	const FString Head = LiveRec.Headline;
	if (Tier >= 2 && !OnTier.IsBound()) FallbackGlow(Tier >= 3 ? 3 : 2);
	if (bCompetitive) return;                                         // Competitive: callouts + glow only
	if (Tier == 1 && !OnTier.IsBound())
	{
		FallbackPanel(2, Subject, Subject != Shooter ? Shooter : nullptr, (uint8)EZoogiShotType::CloseUp, Tune.InsertSeconds, Head, FLinearColor(0.75f, 1.f, 0.55f), true);
		Shot(TEXT("tier1_insert"));
	}
	else if (Tier == 2 && !OnTier.IsBound())
	{
		FallbackPanel(0, Subject, Subject != Shooter ? Shooter : nullptr, (uint8)EZoogiShotType::LowAngleHero, Tune.TrickPanelSeconds, Head.IsEmpty() ? FString(TEXT("TRICK SHOT")) : Head, FLinearColor(1.f, 0.86f, 0.36f), false);
		Shot(TEXT("tier2_trick"));
	}
	else if (Tier >= 3 && !OnPanels.IsBound() && !Plan.bAirShot)
	{
		AActor* Target = BodyActor(SubjectBody(EZoogiShotSubject::Target));
		FallbackPanel(0, Shooter, Target, (uint8)EZoogiShotType::LowAngleHero, Tune.PanelSeconds, Head, FLinearColor(0.96f, 0.96f, 1.f), false);
		if (Target) FallbackPanel(1, Target, Shooter, (uint8)EZoogiShotType::TopDownImpact, Tune.PanelSeconds, FString::Printf(TEXT("TIER %d"), Tier), FLinearColor(0.96f, 0.96f, 1.f), false);
		Shot(TEXT("tier3_panels"));
	}
}

void UZoogiEpisodeSubsystem::StartAirShot(int32 Body, float Gap)
{
	UWorld* W = GetWorld();
	if (!W || Mode == EZoogiEpisodeMode::Off || bFrozen || bArchetypePending || Plan.bMeteor) return;
	AActor* A = BodyActor(Body);
	if (!A) return;
	if (!Announced.Contains(EZoogiEpisodeTag::Airborne)) { Announced.Add(EZoogiEpisodeTag::Airborne); QueueCallout(EZoogiEpisodeTag::Airborne, 3); }
	Plan.Tier = FMath::Max(Plan.Tier, 3);
	PresentedTier = FMath::Max(PresentedTier, 3);                     // this IS the tier 3 moment (no second tier-3 request at touchdown)
	if (Mode == EZoogiEpisodeMode::Competitive) return;
	FZoogiEpisodeCue Cam = MakeCue(EZoogiEpisodeCueKind::Camera, 3);
	Cam.Tag = EZoogiEpisodeTag::Airborne;
	Cam.Target = A;
	Cam.Seconds = Tune.AirShotSeconds;
	SendCue(OnCamera, Cam);
	if (OnCamera.IsBound()) return;                                    // a real camera receiver owns it
	// fallback: freeze the real world (hold = true pause + snapshot verify) and fly the director's tracking camera
	// around the anchored Zoogi; physics is never dilated (Rule Zero). Started from TickAirSlow once the game's own
	// real-time hit-stop has ended, exactly like the Titan Meteor hold.
	bAirSlow = true;
	AirBody = Body;
	AirSlowLeft = 0.6f;                                                // max wait for hit-stop to end, else skip the hold
	UE_LOG(LogZoogi, Log, TEXT("EPISODE airshot armed action=%d body=%d gap=%.0f"), Truth.ActionId, Body, Gap);
}

void UZoogiEpisodeSubsystem::TickAirSlow(float Dt)
{
	UWorld* W = GetWorld();
	if (!W) return;
	if (AirHoldToken != 0)
	{
		if (bFrozen && HoldToken == AirHoldToken) { if (HoldLeft < Tune.AirShotSeconds * 0.55f) Shot(TEXT("tier3_air")); return; }
		EndAirSlow(TEXT("hold released"));
		return;
	}
	if (!bAirSlow) return;
	AirSlowLeft -= Dt;
	UZoogiAnimeSubsystem* Anime = W->GetSubsystem<UZoogiAnimeSubsystem>();
	const AWorldSettings* WS = W->GetWorldSettings();
	if ((Anime && Anime->bFrozen) || W->IsPaused() || bFrozen || (WS && WS->TimeDilation != 1.f))
	{
		if (AirSlowLeft <= 0.f) { bAirSlow = false; UE_LOG(LogZoogi, Log, TEXT("EPISODE airshot skipped action=%d (hit-stop / freeze did not end)"), Truth.ActionId); }
		return;
	}
	bAirSlow = false;
	AActor* A = BodyActor(AirBody);
	if (!A || !bActive) return;
	AirHoldToken = BeginHold(Tune.AirShotSeconds, TEXT("Airborne"));
	if (AirHoldToken == 0) return;
	Plan.bAirShot = true;
	if (UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(this))
	{
		APlayerController* PC = W->GetFirstPlayerController();
		AirPrevView = PC ? PC->GetViewTarget() : nullptr;
		AActor* Sec = AirBody != Truth.Actor ? BodyActor(Truth.Actor) : BodyActor(SubjectBody(EZoogiShotSubject::Target));
		FZoogiShotPlayParams P;
		P.DurationOverride = FMath::Max(0.3f, Tune.AirShotSeconds - 0.15f); // camera is back before the world resumes
		AirShotId = Dir->PlayShotType(EZoogiShotType::Tracking, A, Sec, EZoogiShotOutput::MainView, P).Id;
	}
	if (AirShotId == 0) Plan.bCameraRestored = true;
	UE_LOG(LogZoogi, Log, TEXT("EPISODE airshot begin action=%d body=%d hold_token=%d shot_id=%d seconds=%.2f"), Truth.ActionId, AirBody, AirHoldToken, AirShotId, Tune.AirShotSeconds);
}

void UZoogiEpisodeSubsystem::EndAirSlow(const TCHAR* Why)
{
	bAirSlow = false;
	if (AirHoldToken == 0) return;
	const int32 Tok = AirHoldToken;
	AirHoldToken = 0;
	if (AirShotId != 0)
		if (UZoogiEpisodeCameraDirector* Dir = UZoogiEpisodeCameraDirector::Get(this))
		{
			FZoogiShotHandle H; H.Id = AirShotId;
			if (Dir->IsShotActive(H)) Dir->StopShot(H, true);       // OnShotFinished checks the hand-back
		}
	if (bFrozen && HoldToken == Tok) ReleaseHold(Tok);
	UE_LOG(LogZoogi, Log, TEXT("EPISODE airshot end action=%d why=%s camera_restored=%d"), Truth.ActionId, Why, Plan.bCameraRestored ? 1 : 0);
}
