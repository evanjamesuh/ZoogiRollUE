#include "ZoogiEpisodeCameraDirector.h"
#include "ZoogiEpisodeTypes.h"
#include "ZoogiRoll.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiShotTest.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "ZoogiFX.h"
#include "UObject/UObjectIterator.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Curves/CurveFloat.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ZoogiCamDirPrivate
{
	TAutoConsoleVariable<int32> CVarAllowTimeDilation(TEXT("zoogi.Shot.AllowTimeDilation"), 0,
		TEXT("Core rule: physics is never dilated, so EaseTimeScale refuses unless this is 1 (debug only)."));
	TAutoConsoleVariable<int32> CVarShotRTFormat(TEXT("zoogi.Shot.RTFormat"), 0,
		TEXT("Render target format for director-created shot targets: 0 = RGBA8 sRGB (default), 1 = RGBA8 linear, 2 = RGBA16F."));

	FVector CamDirSubjectCenter(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		if (!A) return FVector::ZeroVector;
		if (const USceneComponent* R = A->GetRootComponent()) return R->Bounds.Origin;
		return A->GetActorLocation();
	}

	FVector CamDirDampV(const FVector& X, const FVector& T, float Lambda, float Dt) { return X + (T - X) * (1.f - FMath::Exp(-Lambda * Dt)); }

	FMinimalViewInfo CamDirBlendViews(const FMinimalViewInfo& A, const FMinimalViewInfo& B, float W)
	{
		FMinimalViewInfo R = B;
		R.Location = FMath::Lerp(A.Location, B.Location, W);
		R.Rotation = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), W).Rotator();
		R.FOV = FMath::Lerp(A.FOV, B.FOV, W);
		return R;
	}

	void CamDirSlowLookGrade(FPostProcessSettings& PP, float W)
	{
		if (W <= 0.001f) return;
		const float V = PP.bOverride_VignetteIntensity ? PP.VignetteIntensity : 0.4f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = V + 0.15f * W;
		const FVector4 Sat = PP.bOverride_ColorSaturation ? PP.ColorSaturation : FVector4(1, 1, 1, 1);
		const float M = FMath::Lerp(1.f, 0.9f, W);
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(Sat.X * M, Sat.Y * M, Sat.Z * M, Sat.W);
	}

	UCameraComponent* CamDirCameraComponentOf(AActor* A)
	{
		if (!A) return nullptr;
		if (ACameraActor* C = Cast<ACameraActor>(A)) return C->GetCameraComponent();
		return A->FindComponentByClass<UCameraComponent>();
	}
}
using namespace ZoogiCamDirPrivate;

// ============================================================================ lifecycle

UZoogiEpisodeCameraDirector* UZoogiEpisodeCameraDirector::Get(const UObject* WorldContext)
{
	UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return W ? W->GetSubsystem<UZoogiEpisodeCameraDirector>() : nullptr;
}

bool UZoogiEpisodeCameraDirector::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UZoogiEpisodeCameraDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Library = NewObject<UZoogiShotLibrary>(this, TEXT("ZoogiDefaultShotLibrary"));
	Library->FillDefaults();
	TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UZoogiEpisodeCameraDirector::HandleWorldBeginTearDown);
}

void UZoogiEpisodeCameraDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiShotTest")))
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		InWorld.SpawnActor<AZoogiShotTestDriver>(AZoogiShotTestDriver::StaticClass(), FTransform::Identity, P);
	}
}

void UZoogiEpisodeCameraDirector::Deinitialize()
{
	HandleWorldBeginTearDown(GetWorld());
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	Super::Deinitialize();
}

void UZoogiEpisodeCameraDirector::HandleWorldBeginTearDown(UWorld* InWorld)
{
	if (bTornDown || !InWorld || InWorld != GetWorld()) return;
	bTornDown = true;
	if (Shots.Num() > 0 || TS.bActive)
	{
		UE_LOG(LogZoogi, Log, TEXT("CAMDIR world ending: stopping %d shot(s)%s"), Shots.Num(), TS.bActive ? TEXT(" and the time-scale ease") : TEXT(""));
	}
	while (Shots.Num() > 0) FinishShot(Shots.Num() - 1, EZoogiShotEndReason::WorldEnding);
	if (TS.bActive) RestoreTimeScaleExact(TEXT("world ending"));
	OnWorldTearDownRestored.Broadcast();
	if (ShotCam) { ShotCam->Destroy(); ShotCam = nullptr; }
	if (CaptureRig) { CaptureRig->Destroy(); CaptureRig = nullptr; }
	FreeCaptures.Reset();
}

void UZoogiEpisodeCameraDirector::SetShotLibrary(UZoogiShotLibrary* InLibrary)
{
	if (InLibrary) Library = InLibrary;
}

// ============================================================================ shots

FZoogiShotHandle UZoogiEpisodeCameraDirector::PlayShotType(EZoogiShotType Type, AActor* Focus, AActor* Secondary, EZoogiShotOutput Output, const FZoogiShotPlayParams& Params)
{
	const FZoogiShotSpec* Spec = Library ? Library->FindByType(Type) : nullptr;
	return PlayShot(Spec ? *Spec : UZoogiShotLibrary::MakeDefault(Type), Focus, Secondary, Output, Params);
}

FZoogiShotHandle UZoogiEpisodeCameraDirector::PlayShot(const FZoogiShotSpec& Spec, AActor* Focus, AActor* Secondary, EZoogiShotOutput Output, const FZoogiShotPlayParams& Params)
{
	FZoogiShotHandle H;
	UWorld* W = GetWorld();
	if (!W || bTornDown || !IsValid(Focus))
	{
		UE_LOG(LogZoogi, Warning, TEXT("CAMDIR PlayShot %s refused (%s)"), *Spec.Name.ToString(), !IsValid(Focus) ? TEXT("no focus actor") : TEXT("world ending"));
		return H;
	}
	FZoogiActiveShot S;
	S.Id = NextShotId++;
	S.Spec = Spec;
	S.Output = Output;
	S.Focus = Focus;
	S.Secondary = Secondary;
	S.Duration = FMath::Max(0.1f, Params.DurationOverride > 0.f ? Params.DurationOverride : Spec.Duration);
	S.LastFocus = CamDirSubjectCenter(Focus);
	S.bEventFromCaller = Params.bHasEventLocation;
	// Default event point: the arena centre (floor centre at the origin) at the focus' height.
	S.EventLoc = Params.bHasEventLocation ? Params.EventLocation : FVector(0.f, 0.f, S.LastFocus.Z);
	{
		const FVector Fl(S.LastFocus.X, S.LastFocus.Y, 0.f);
		FVector N = Params.bHasEventNormal ? FVector(Params.EventNormal.X, Params.EventNormal.Y, 0.f) : FVector(-S.EventLoc.X, -S.EventLoc.Y, 0.f);
		if (!N.Normalize()) { N = Fl - FVector(S.EventLoc.X, S.EventLoc.Y, 0.f); if (!N.Normalize()) N = FVector(1, 0, 0); }
		// the open side of the surface is the side the focus is on (or, failing that, the arena centre)
		const FVector ToF = Fl - FVector(S.EventLoc.X, S.EventLoc.Y, 0.f);
		if (ToF.SizeSquared() > 100.f ? (FVector::DotProduct(N, ToF) < 0.f) : (FVector::DotProduct(N, -FVector(S.EventLoc.X, S.EventLoc.Y, 0.f)) < 0.f)) N = -N;
		S.EventNormal = N;
		S.bHasSecondEvent = Params.bHasSecondEventLocation;
		S.SecondEventLoc = Params.bHasSecondEventLocation ? Params.SecondEventLocation : S.EventLoc;
		FVector D = Params.bHasDirection ? FVector(Params.Direction.X, Params.Direction.Y, 0.f) : Fl;
		if (!D.Normalize()) D = FVector(1, 0, 0);
		S.LineDir = D;
		S.Accent = Params.bHasAccentColor ? Params.AccentColor : Spec.DefaultAccent;
	}
	H.Id = S.Id;

	if (Output == EZoogiShotOutput::RenderTarget)
	{
		UTextureRenderTarget2D* RT = Params.RenderTarget;
		if (!RT)
		{
			const FIntPoint Size(FMath::Clamp(Params.RenderTargetSize.X, 64, 4096), FMath::Clamp(Params.RenderTargetSize.Y, 64, 4096));
			RT = NewObject<UTextureRenderTarget2D>(this);
			const int32 Fmt = CVarShotRTFormat.GetValueOnGameThread();
			RT->RenderTargetFormat = Fmt == 1 ? RTF_RGBA8 : (Fmt == 2 ? RTF_RGBA16f : RTF_RGBA8_SRGB);
			RT->ClearColor = FLinearColor::Black;
			RT->InitAutoFormat(Size.X, Size.Y);
			RT->UpdateResourceImmediate(true);
		}
		S.RenderTarget = RT;
		S.Capture = AcquireCapture();
		if (S.Capture) S.Capture->TextureTarget = RT;
		TargetById.Add(S.Id, RT);
	}
	else
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(W, Params.PlayerIndex);
		if (!PC || !PC->PlayerCameraManager)
		{
			UE_LOG(LogZoogi, Warning, TEXT("CAMDIR PlayShot %s: no player controller %d for MainView"), *Spec.Name.ToString(), Params.PlayerIndex);
			return FZoogiShotHandle();
		}
		// Replacing an active MainView shot: the new one blends in from the old one's last frame; the original
		// gameplay snapshot (taken before the first takeover) is kept so restoration still targets the real gameplay camera.
		const int32 OldIdx = Shots.IndexOfByPredicate([this](const FZoogiActiveShot& O) { return O.Id == MainViewShotId; });
		if (OldIdx != INDEX_NONE)
		{
			S.bBlendFromFrozen = true;
			S.BlendFrom = Shots[OldIdx].LastOutputView;
			const int32 OldId = MainViewShotId;
			MainViewShotId = S.Id; // hand over before finishing so the view is not restored in between
			const FZoogiActiveShot Old = Shots[OldIdx];
			Shots.RemoveAt(OldIdx);
			UE_LOG(LogZoogi, Log, TEXT("CAMDIR shot %d (%s) replaced by shot %d (%s)"), OldId, *Old.Spec.Name.ToString(), S.Id, *Spec.Name.ToString());
			OnShotFinished.Broadcast(FZoogiShotHandle::Make(OldId), EZoogiShotEndReason::Replaced);
		}
		else
		{
			TakeMainView(Params.PlayerIndex);
			if (!MainViewSnapshot.bValid) return FZoogiShotHandle();
			MainViewShotId = S.Id;
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("CAMDIR play shot %d %s (%s) on %s%s%s for %.2f s"), S.Id, *Spec.Name.ToString(), ZoogiShot::OutputName(Output), *GetNameSafe(Focus),
		Secondary ? TEXT(" + ") : TEXT(""), Secondary ? *GetNameSafe(Secondary) : TEXT(""), S.Duration);
	Shots.Add(S);
	// Evaluate once now so a capture / view is valid on the very first rendered frame.
	bool bFinished = false;
	EZoogiShotEndReason Reason = EZoogiShotEndReason::Completed;
	TickShot(Shots.Last(), 0.f, bFinished, Reason);
	if (bFinished) FinishShot(Shots.Num() - 1, Reason);
	return H;
}

void UZoogiEpisodeCameraDirector::StopShot(FZoogiShotHandle Shot, bool bImmediate)
{
	const int32 Idx = Shots.IndexOfByPredicate([&](const FZoogiActiveShot& S) { return S.Id == Shot.Id; });
	if (Idx == INDEX_NONE) return;
	FZoogiActiveShot& S = Shots[Idx];
	if (bImmediate || S.Output == EZoogiShotOutput::RenderTarget || S.Spec.EaseOut <= 0.f)
	{
		FinishShot(Idx, EZoogiShotEndReason::Stopped);
		return;
	}
	if (TS.bActive && TimeScaleOwnerShotId == S.Id) StopTimeScale(false); // eases out with the shot; FinishShot snaps any rest
	if (!S.bStopping)
	{
		// Ease out from whatever weight the shot has right now.
		const float In = (S.Spec.EaseIn > 0.f && S.Age < S.Spec.EaseIn) ? CurveOrEase(nullptr, S.Spec.Ease, S.Age / S.Spec.EaseIn) : 1.f;
		const float Rem = S.Duration - S.Age;
		const float Out = (S.Spec.EaseOut > 0.f && Rem < S.Spec.EaseOut) ? CurveOrEase(nullptr, S.Spec.Ease, Rem / S.Spec.EaseOut) : 1.f;
		S.bStopping = true;
		S.StopAge = 0.f;
		S.StopWeight = FMath::Min(In, Out);
	}
}

void UZoogiEpisodeCameraDirector::StopAllShots(bool bImmediate)
{
	if (bImmediate)
	{
		while (Shots.Num() > 0) FinishShot(Shots.Num() - 1, EZoogiShotEndReason::Stopped);
		return;
	}
	TArray<int32> Ids;
	for (const FZoogiActiveShot& S : Shots) Ids.Add(S.Id);
	for (int32 Id : Ids) StopShot(FZoogiShotHandle::Make(Id), false);
}

bool UZoogiEpisodeCameraDirector::IsShotActive(FZoogiShotHandle Shot) const
{
	return Shots.ContainsByPredicate([&](const FZoogiActiveShot& S) { return S.Id == Shot.Id; });
}

UTextureRenderTarget2D* UZoogiEpisodeCameraDirector::GetShotRenderTarget(FZoogiShotHandle Shot) const
{
	const TWeakObjectPtr<UTextureRenderTarget2D>* RT = TargetById.Find(Shot.Id);
	return RT ? RT->Get() : nullptr;
}

int32 UZoogiEpisodeCameraDirector::NumActiveCaptures() const
{
	int32 N = 0;
	for (const FZoogiActiveShot& S : Shots) if (S.Capture) ++N;
	return N;
}

void UZoogiEpisodeCameraDirector::FinishShot(int32 Index, EZoogiShotEndReason Reason)
{
	if (!Shots.IsValidIndex(Index)) return;
	const FZoogiActiveShot S = Shots[Index];
	Shots.RemoveAt(Index);
	if (S.Capture)
	{
		S.Capture->TextureTarget = nullptr; // the target keeps its last frame for the compositor
		ReleaseCapture(S.Capture);
	}
	if (S.RenderTarget)
	{
		RecentTargets.Remove(S.RenderTarget);
		RecentTargets.Add(S.RenderTarget);
		while (RecentTargets.Num() > 8) RecentTargets.RemoveAt(0);
	}
	if (S.Id == MainViewShotId)
	{
		MainViewShotId = 0;
		RestoreMainView(Reason);
	}
	StatsById.Add(S.Id, S.Stats);
	if (StatsById.Num() > 64) { TArray<int32> K; StatsById.GetKeys(K); K.Sort(); for (int32 i = 0; i < K.Num() - 64; ++i) StatsById.Remove(K[i]); }
	if (TS.bActive && TimeScaleOwnerShotId == S.Id)
	{
		// The shot's slow-mo is timed to be over before the shot ends; any early end puts the prior value back now.
		RestoreTimeScaleExact(TEXT("owning shot ended"));
	}
	static const TCHAR* ReasonNames[] = { TEXT("completed"), TEXT("stopped"), TEXT("replaced"), TEXT("focus lost"), TEXT("world ending") };
	UE_LOG(LogZoogi, Log, TEXT("CAMDIR shot %d %s (%s) ended: %s after %.2f s"), S.Id, *S.Spec.Name.ToString(), ZoogiShot::OutputName(S.Output), ReasonNames[FMath::Clamp((int32)Reason, 0, 4)], S.Age);
	OnShotFinished.Broadcast(FZoogiShotHandle::Make(S.Id), Reason);
}

// ============================================================================ main view take / restore

void UZoogiEpisodeCameraDirector::TakeMainView(int32 PlayerIndex)
{
	UWorld* W = GetWorld();
	APlayerController* PC = UGameplayStatics::GetPlayerController(W, PlayerIndex);
	if (!PC || !PC->PlayerCameraManager) return;
	if (!ShotCam)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShotCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P);
		if (!ShotCam) return;
		ShotCam->SetActorTickEnabled(false);
	}
	if (PC->GetViewTarget() != ShotCam)
	{
		MainViewSnapshot = CaptureGameplaySnapshot(PlayerIndex);
	}
	if (!MainViewSnapshot.bValid) return;
	UCameraComponent* SC = ShotCam->GetCameraComponent();
	SC->bConstrainAspectRatio = MainViewSnapshot.bCameraConstrainAspect;
	SC->AspectRatio = MainViewSnapshot.CameraAspectRatio;
	SC->SetFieldOfView(MainViewSnapshot.POV.FOV);
	SC->PostProcessSettings = MainViewSnapshot.CameraPP;
	SC->PostProcessBlendWeight = MainViewSnapshot.CameraPPBlendWeight;
	ShotCam->SetActorLocationAndRotation(MainViewSnapshot.POV.Location, MainViewSnapshot.POV.Rotation);
	// No engine blend: the director eases itself, so the PlayerCameraManager never holds blend state of ours.
	PC->SetViewTarget(ShotCam);
	UE_LOG(LogZoogi, Log, TEXT("CAMDIR main view taken from %s (pov %s / %s / fov %.2f)"), *GetNameSafe(MainViewSnapshot.ViewTarget.Get()),
		*MainViewSnapshot.POV.Location.ToString(), *MainViewSnapshot.POV.Rotation.ToString(), MainViewSnapshot.POV.FOV);
}

void UZoogiEpisodeCameraDirector::RestoreMainView(EZoogiShotEndReason Reason)
{
	APlayerController* PC = MainViewSnapshot.PC.Get();
	if (IsValid(PC) && PC->PlayerCameraManager)
	{
		AActor* Cur = PC->GetViewTarget();
		AActor* Orig = MainViewSnapshot.ViewTarget.Get();
		if (Cur == ShotCam || Cur == nullptr)
		{
			if (IsValid(Orig))
			{
				PC->SetViewTarget(Orig);
			}
			else
			{
				// The gameplay camera itself is gone (level ending / destroyed): hand the view back to the controller's own default.
				AActor* Fallback = PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : static_cast<AActor*>(PC);
				PC->SetViewTarget(Fallback);
				UE_LOG(LogZoogi, Warning, TEXT("CAMDIR restore: original view target is gone, falling back to %s"), *GetNameSafe(Fallback));
			}
		}
		else if (Cur != Orig)
		{
			UE_LOG(LogZoogi, Warning, TEXT("CAMDIR restore: view target was changed by someone else during the shot (%s); leaving it"), *GetNameSafe(Cur));
		}
		UE_LOG(LogZoogi, Log, TEXT("CAMDIR main view restored to %s"), *GetNameSafe(PC->GetViewTarget()));
	}
	if (ShotCam) ShotCam->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	MainViewSnapshot = FZoogiCameraSnapshot();
}

bool UZoogiEpisodeCameraDirector::GetLiveGameplayView(float Dt, FMinimalViewInfo& OutView) const
{
	AActor* Orig = MainViewSnapshot.ViewTarget.Get();
	if (!IsValid(Orig)) return false;
	Orig->CalcCamera(Dt, OutView);
	return true;
}

// ============================================================================ scene captures

USceneCaptureComponent2D* UZoogiEpisodeCameraDirector::AcquireCapture()
{
	UWorld* W = GetWorld();
	if (!CaptureRig)
	{
		FActorSpawnParameters P;
		P.ObjectFlags |= RF_Transient;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CaptureRig = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
		if (!CaptureRig) return nullptr;
		USceneComponent* Root = NewObject<USceneComponent>(CaptureRig, TEXT("ShotCaptureRoot"));
		Root->SetMobility(EComponentMobility::Movable);
		CaptureRig->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	USceneCaptureComponent2D* C = nullptr;
	while (FreeCaptures.Num() > 0 && !C) C = FreeCaptures.Pop();
	if (!C)
	{
		C = NewObject<USceneCaptureComponent2D>(CaptureRig);
		C->SetMobility(EComponentMobility::Movable);
		C->bCaptureEveryFrame = false;      // the director requests each frame itself (also while the game is paused)
		C->bCaptureOnMovement = false;
		C->bAlwaysPersistRenderingState = true; // temporal AA / exposure history between frames
		C->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		C->PostProcessBlendWeight = 1.f;
		C->ShowFlags.SetMotionBlur(false);
		C->RegisterComponent();
	}
	return C;
}

void UZoogiEpisodeCameraDirector::ReleaseCapture(USceneCaptureComponent2D* C)
{
	if (!C) return;
	C->TextureTarget = nullptr;
	FreeCaptures.AddUnique(C);
}

// ============================================================================ evaluation

float UZoogiEpisodeCameraDirector::CurveOrEase(UCurveFloat* Curve, EZoogiShotEase Ease, float T) const
{
	T = FMath::Clamp(T, 0.f, 1.f);
	return Curve ? FMath::Clamp(Curve->GetFloatValue(T), 0.f, 1.f) : ZoogiShot::ApplyEase(Ease, T);
}

FPostProcessSettings UZoogiEpisodeCameraDirector::BuildShotPostProcess(const FZoogiShotSpec& Spec, float FocusDistance, float Weight) const
{
	FPostProcessSettings PP;
	if (Spec.bInheritGameplayPostProcess)
	{
		AActor* VT = MainViewSnapshot.ViewTarget.Get();
		if (!VT)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0)) VT = PC->GetViewTarget();
			if (VT == ShotCam) VT = nullptr;
		}
		if (UCameraComponent* C = CamDirCameraComponentOf(VT)) PP = C->PostProcessSettings;
	}
	if (Spec.VignetteAdd != 0.f)
	{
		const float Base = PP.bOverride_VignetteIntensity ? PP.VignetteIntensity : 0.4f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = FMath::Max(0.f, Base + Spec.VignetteAdd * Weight);
	}
	if (Spec.SaturationMul != 1.f)
	{
		const FVector4 Base = PP.bOverride_ColorSaturation ? PP.ColorSaturation : FVector4(1, 1, 1, 1);
		const float M = FMath::Lerp(1.f, Spec.SaturationMul, Weight);
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(Base.X * M, Base.Y * M, Base.Z * M, Base.W);
	}
	if (Spec.ContrastMul != 1.f)
	{
		const FVector4 Base = PP.bOverride_ColorContrast ? PP.ColorContrast : FVector4(1, 1, 1, 1);
		const float M = FMath::Lerp(1.f, Spec.ContrastMul, Weight);
		PP.bOverride_ColorContrast = true;
		PP.ColorContrast = FVector4(Base.X * M, Base.Y * M, Base.Z * M, Base.W);
	}
	if (PP.bOverride_DepthOfFieldFocalDistance)
	{
		// Keep the gameplay aperture (sharp by default) but focus on the shot's subject.
		PP.DepthOfFieldFocalDistance = FMath::Lerp(PP.DepthOfFieldFocalDistance, FMath::Max(FocusDistance, 50.f), Weight);
	}
	return PP;
}

// Trick-shot pivot: lerp(focus | focus..secondary, event, EventBias), then toward the second event, then along the line.
static FVector CamDirShotPivot(const FZoogiActiveShot& S, const FZoogiShotSpec& Spec, const FVector& FC, const FVector& SC, bool bSec)
{
	FVector P = FMath::Lerp(bSec && Spec.SecondaryBias > 0.f ? FMath::Lerp(FC, SC, Spec.SecondaryBias) : FC, S.EventLoc, Spec.EventBias);
	if (Spec.SecondEventBias > 0.f && S.bHasSecondEvent) P = FMath::Lerp(P, S.SecondEventLoc, Spec.SecondEventBias);
	if (Spec.DirectionLead != 0.f) P += S.LineDir * Spec.DirectionLead;
	return P + Spec.PivotOffset;
}

// Power-colour accent (tint + bloom lift), weighted like the rest of the grade.
static void CamDirApplyAccent(FPostProcessSettings& PP, const FZoogiActiveShot& S, float Weight)
{
	const float A = S.Spec.AccentStrength * Weight;
	if (A <= 0.f) return;
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = FMath::Lerp(FLinearColor::White, S.Accent, A);
	const float BaseBloom = PP.bOverride_BloomIntensity ? PP.BloomIntensity : 0.675f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = BaseBloom * (1.f + 0.8f * A);
}

void UZoogiEpisodeCameraDirector::FillTrickParams(EZoogiShotType Type, const FZoogiGameplayTruthRecord& Truth, FZoogiShotPlayParams& P)
{
	TArray<const FZoogiContactEvent*, TInlineAllocator<8>> Walls;
	const FZoogiContactEvent* Ally = nullptr;
	const FZoogiContactEvent* OrbHit = nullptr;
	for (const FZoogiContactEvent& E : Truth.Contacts)
	{
		if (!E.bMeaningful) continue;
		if (E.Body == Truth.Actor && (E.Kind == EZoogiContactKind::Wall || E.Kind == EZoogiContactKind::Bumper)) Walls.Add(&E);
		if (!Ally && E.Kind == EZoogiContactKind::Ally) Ally = &E;
		if (!OrbHit && E.Kind == EZoogiContactKind::Orb && E.Body < 100) OrbHit = &E;
	}
	auto NextFrom = [&Truth](const FZoogiContactEvent* After, int32 Body) -> const FZoogiContactEvent*
	{
		for (const FZoogiContactEvent& E : Truth.Contacts)
			if (E.Body == Body && E.Time > After->Time && FVector::Dist2D(E.Location, After->Location) > 30.f) return &E;
		return nullptr;
	};
	auto SetWall = [&P](const FZoogiContactEvent* W)
	{
		P.bHasEventLocation = true; P.EventLocation = W->Location;
		if (!W->Normal.IsNearlyZero()) { P.bHasEventNormal = true; P.EventNormal = W->Normal; }
	};
	switch (Type)
	{
	case EZoogiShotType::TrickBank:
	case EZoogiShotType::TrickPowerBank:
		if (Walls.Num() > 0) SetWall(Walls[0]);
		if (Type == EZoogiShotType::TrickPowerBank && Truth.Awaken != EZoogiArtifact::None) { P.bHasAccentColor = true; P.AccentColor = ZoogiEpisode::PowerColor(Truth.Awaken); }
		break;
	case EZoogiShotType::TrickDoubleBank:
		if (Walls.Num() > 0) SetWall(Walls[0]);
		if (Walls.Num() > 1) { P.bHasSecondEventLocation = true; P.SecondEventLocation = Walls[1]->Location; }
		break;
	case EZoogiShotType::TrickWallRedirect:
		if (Walls.Num() > 0)
		{
			const FZoogiContactEvent* W = Walls.Last();
			SetWall(W);
			if (const FZoogiContactEvent* N = NextFrom(W, Truth.Actor)) { P.bHasDirection = true; P.Direction = N->Location - W->Location; }
			else if (!W->Normal.IsNearlyZero() && !Truth.LaunchVelocity.IsNearlyZero())
			{
				const FVector Nn = W->Normal.GetSafeNormal();
				P.bHasDirection = true; P.Direction = Truth.LaunchVelocity - 2.f * FVector::DotProduct(Truth.LaunchVelocity, Nn) * Nn;
			}
		}
		break;
	case EZoogiShotType::TrickAllyBump:
		if (Ally) { P.bHasEventLocation = true; P.EventLocation = Ally->Location; }
		break;
	case EZoogiShotType::TrickOrbRelay:
		if (OrbHit && OrbHit->OtherBody >= 100)
		{
			if (const FZoogiContactEvent* R = NextFrom(OrbHit, OrbHit->OtherBody))
			{
				P.bHasEventLocation = true; P.EventLocation = R->Location;
				P.bHasDirection = true; P.Direction = R->Location - OrbHit->Location;
			}
		}
		break;
	case EZoogiShotType::TrickRingOutLine:
		if (!Truth.LaunchVelocity.IsNearlyZero() && Truth.Contacts.Num() == 0) { P.bHasDirection = true; P.Direction = Truth.LaunchVelocity; }
		break;
	default: break;
	}
}

bool UZoogiEpisodeCameraDirector::EvaluateShotView(FZoogiActiveShot& S, float Dt, FMinimalViewInfo& OutView)
{
	AActor* F = S.Focus.Get();
	if (!IsValid(F) || F->IsActorBeingDestroyed()) return false;
	const FZoogiShotSpec& Spec = S.Spec;
	const FVector FC = CamDirSubjectCenter(F);
	AActor* Sec = S.Secondary.Get();
	const bool bSec = IsValid(Sec) && !Sec->IsActorBeingDestroyed();
	const FVector SC = bSec ? CamDirSubjectCenter(Sec) : FC;

	// Velocity from the observed motion (works for physics bodies and for moved actors alike).
	const FVector Vel = Dt > 1e-4f ? (FC - S.LastFocus) / Dt : FVector::ZeroVector;
	const float VelYawNow = FMath::RadiansToDegrees(FMath::Atan2(Vel.Y, Vel.X));
	const bool bMoving = FVector2D(Vel.X, Vel.Y).Size() > 30.f;

	if (!S.bInit)
	{
		float GameplayYaw = 0.f;
		if (MainViewSnapshot.bValid && S.Output == EZoogiShotOutput::MainView) GameplayYaw = MainViewSnapshot.POV.Rotation.Yaw;
		else if (MainViewSnapshot.bValid) GameplayYaw = MainViewSnapshot.POV.Rotation.Yaw;
		else if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			if (PC->PlayerCameraManager) GameplayYaw = PC->PlayerCameraManager->GetCameraCacheView().Rotation.Yaw;
		}
		switch (Spec.Heading)
		{
		case EZoogiShotHeading::FocusToSecondary:
			S.BaseYaw = (bSec && FVector::Dist2D(FC, SC) > 1.f) ? FMath::RadiansToDegrees(FMath::Atan2(SC.Y - FC.Y, SC.X - FC.X)) : GameplayYaw;
			break;
		case EZoogiShotHeading::FocusVelocity:
			S.BaseYaw = GameplayYaw;
			{
				const FVector V0 = F->GetVelocity();
				if (FVector2D(V0.X, V0.Y).Size() > 30.f) S.BaseYaw = FMath::RadiansToDegrees(FMath::Atan2(V0.Y, V0.X));
				else S.bVelSnapPending = true; // kinematically moved actors report no velocity: take the first observed motion
			}
			break;
		case EZoogiShotHeading::World: S.BaseYaw = 0.f; break;
		case EZoogiShotHeading::FocusFacing:
		{
			S.BaseYaw = GameplayYaw; // camera on the gameplay camera's side (the face is assumed to look at the player)
			FVector Face = F->GetActorForwardVector();
			if (const AZoogiBall* B = Cast<AZoogiBall>(F)) { if (B->Visual) Face = B->Visual->GetForwardVector(); }
			if (FMath::Abs(Face.Z) < 0.85f && FVector2D(Face.X, Face.Y).Size() > 0.2f)
			{
				// Camera = pivot - Dir * dist, so a heading opposite to the face puts the camera in front of it.
				S.BaseYaw = FMath::RadiansToDegrees(FMath::Atan2(Face.Y, Face.X)) + 180.f;
			}
			break;
		}
		case EZoogiShotHeading::FocusToEvent:
			S.BaseYaw = FVector::Dist2D(FC, S.EventLoc) > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(S.EventLoc.Y - FC.Y, S.EventLoc.X - FC.X)) : GameplayYaw;
			break;
		case EZoogiShotHeading::EventNormal:
			// camera = pivot - Dir * dist: Dir = -normal puts the camera out on the normal, looking back at the contact
			S.BaseYaw = FMath::RadiansToDegrees(FMath::Atan2(-S.EventNormal.Y, -S.EventNormal.X));
			break;
		case EZoogiShotHeading::ExitDirection:
			S.BaseYaw = FMath::RadiansToDegrees(FMath::Atan2(S.LineDir.Y, S.LineDir.X));
			break;
		case EZoogiShotHeading::EventChord:
			if (S.bHasSecondEvent && FVector::Dist2D(S.EventLoc, S.SecondEventLoc) > 1.f)
			{
				const FVector C = (S.SecondEventLoc - S.EventLoc).GetSafeNormal2D();
				FVector Perp(-C.Y, C.X, 0.f);
				const FVector Mid = (S.EventLoc + S.SecondEventLoc + FC) / 3.f;
				// camera on the arena-centre side: Dir must point away from the centre
				if (FVector::DotProduct(Perp, FVector(Mid.X, Mid.Y, 0.f)) < 0.f) Perp = -Perp;
				S.BaseYaw = FMath::RadiansToDegrees(FMath::Atan2(Perp.Y, Perp.X));
			}
			else S.BaseYaw = FVector::Dist2D(FC, S.EventLoc) > 1.f ? FMath::RadiansToDegrees(FMath::Atan2(S.EventLoc.Y - FC.Y, S.EventLoc.X - FC.X)) : GameplayYaw;
			break;
		case EZoogiShotHeading::GameplayCamera:
		default: S.BaseYaw = GameplayYaw; break;
		}
		S.VelYaw = S.BaseYaw;
		S.Pivot = CamDirShotPivot(S, Spec, FC, SC, bSec);
		S.bInit = true;
	}
	if (S.bVelSnapPending && (bMoving || S.Age > 0.3f))
	{
		S.bVelSnapPending = false;
		if (bMoving) S.VelYaw = VelYawNow;
	}
	else if (Spec.Heading == EZoogiShotHeading::FocusVelocity && bMoving)
	{
		S.VelYaw = FRotator::NormalizeAxis(S.VelYaw + FRotator::NormalizeAxis(VelYawNow - S.VelYaw) * (1.f - FMath::Exp(-3.f * Dt)));
	}
	S.LastFocus = FC;

	const float T = S.MotionClock / FMath::Max(S.Duration, 0.01f);
	const float M = CurveOrEase(Spec.MotionCurve, Spec.MoveEase, T);
	const FVector WantPivot = CamDirShotPivot(S, Spec, FC, SC, bSec);
	S.Pivot = (Spec.TrackingLag > 0.f && Dt > 0.f) ? CamDirDampV(S.Pivot, WantPivot, Spec.TrackingLag, Dt) : WantPivot;

	const float Heading = Spec.Heading == EZoogiShotHeading::FocusVelocity ? S.VelYaw : S.BaseYaw;
	const float Yaw = Heading + Spec.YawOffset + Spec.OrbitDegrees * M;
	const float Pitch = FMath::Lerp(Spec.Pitch, Spec.PitchEnd, M);
	float Dist = FMath::Lerp(Spec.Distance, Spec.DistanceEnd, M);
	if (Spec.bFrameBoth && bSec) Dist = FMath::Max(Dist, FVector::Dist(FC, SC) * 0.55f + Spec.SubjectRadius * 2.f);
	if (Spec.bFrameEvent) Dist = FMath::Max(Dist, FVector::Dist(FC, S.EventLoc) * 0.55f + Spec.SubjectRadius * 2.f);
	if (Spec.bFrameSecondEvent && S.bHasSecondEvent) Dist = FMath::Max(Dist, FVector::Dist(S.EventLoc, S.SecondEventLoc) * 0.6f + Spec.SubjectRadius * 2.f);
	const float Pr = FMath::DegreesToRadians(Pitch);
	const FVector Dir = FRotator(0.f, Yaw, 0.f).Vector();
	FVector Loc = S.Pivot - Dir * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));
	const FVector Look = S.Pivot + Spec.LookOffset;

	// ---- readability: static occluders (sweep subject -> camera, pull in or lift) and particle / glow bursts.
	{
		auto Place = [&](float DPitch, float DistMul, float DYaw)
		{
			const float P2 = FMath::DegreesToRadians(FMath::Clamp(Pitch + DPitch, -30.f, 88.f));
			const FVector D2 = FRotator(0.f, Yaw + DYaw, 0.f).Vector();
			const float R2 = Dist * DistMul;
			return S.Pivot - D2 * R2 * FMath::Cos(P2) + FVector(0, 0, R2 * FMath::Sin(P2));
		};
		auto Occlude = [&](const FVector& In, bool& bBlocked)
		{
			bBlocked = false;
			if (!Spec.bAvoidOcclusion) return In;
			FVector OutP = ResolveOcclusion(Look, In, F, bSec ? Sec : nullptr, bBlocked);
			if (bBlocked && FVector::Dist(OutP, Look) < 0.4f * FVector::Dist(In, Look))
			{
				// A deep pull-in reads badly: try going over the occluder first.
				for (float Lift : { 20.f, 40.f })
				{
					bool bB2 = false;
					const FVector Up = ResolveOcclusion(Look, Place(Lift, 1.f, 0.f), F, bSec ? Sec : nullptr, bB2);
					if (!bB2) { OutP = Up; break; }
				}
			}
			return OutP;
		};
		TArray<FBox> FX;
		if (Spec.bAvoidParticles) GatherAvoidVolumes(Look, Dist + 1500.f, FX);
		auto InsideFX = [&](const FVector& P)
		{
			for (const FBox& B : FX) if (B.ExpandBy(Spec.ParticleMargin).IsInsideOrOn(P)) return true;
			return false;
		};
		bool bBlocked = false;
		const FVector Base = Occlude(Loc, bBlocked);
		if (bBlocked) { S.Stats.OccludedFrames++; S.Stats.MaxPullInCm = FMath::Max(S.Stats.MaxPullInCm, FVector::Dist(Base, Loc)); }
		FVector Want = Base;
		for (const FBox& B : FX) S.Stats.MaxVolumeExtentCm = FMath::Max(S.Stats.MaxVolumeExtentCm, (float)B.GetExtent().GetMax());
		if (FX.Num() > 0 && InsideFX(Base))
		{
			S.Stats.RawInsideFXFrames++;
			// Lift first (keeps the readable high angle), then pull in, swing, back off.
			static const FVector Cands[] = {
				FVector(15.f, 1.f, 0.f), FVector(0.f, 0.75f, 0.f), FVector(30.f, 1.f, 0.f), FVector(0.f, 1.f, 25.f), FVector(0.f, 1.f, -25.f),
				FVector(15.f, 0.6f, 0.f), FVector(45.f, 1.f, 0.f), FVector(0.f, 1.f, 50.f), FVector(0.f, 1.f, -50.f), FVector(20.f, 1.4f, 0.f), FVector(0.f, 1.7f, 0.f),
				// The subject is inside a big burst: go wide and high until the camera is clear of it.
				FVector(30.f, 2.2f, 0.f), FVector(40.f, 3.0f, 0.f), FVector(55.f, 3.8f, 0.f), FVector(65.f, 4.6f, 0.f) };
			for (const FVector& Cd : Cands)
			{
				bool bB = false;
				const FVector P = Occlude(Place(Cd.X, Cd.Y, Cd.Z), bB);
				if (!InsideFX(P)) { Want = P; break; }
			}
		}
		const FVector WantOffset = Want - Base;
		const FVector Eased = (Dt > 0.f) ? CamDirDampV(S.AvoidOffset, WantOffset, 6.f, Dt) : WantOffset;
		// Never ease INTO a burst: snap out when the eased position would be inside one.
		S.AvoidOffset = (FX.Num() > 0 && InsideFX(Base + Eased)) ? WantOffset : Eased;
		Loc = Base + S.AvoidOffset;
		S.Stats.MaxAvoidOffsetCm = FMath::Max(S.Stats.MaxAvoidOffsetCm, S.AvoidOffset.Size());
		S.Stats.Frames++;
		if (FX.Num() > 0 && InsideFX(Loc)) S.Stats.FinalInsideFXFrames++;
		if (Spec.bAvoidOcclusion)
		{
			bool bStill = false;
			ResolveOcclusion(Look, Loc, F, bSec ? Sec : nullptr, bStill);
			if (bStill) S.Stats.FinalOccludedFrames++;
		}
	}

	FRotator Rot = (Look - Loc).Rotation();
	Rot.Roll = Spec.Roll;
	float Fov = FMath::Lerp(Spec.FOV, Spec.FOVEnd, M);
	const bool bFrameSec = Spec.bFrameBoth && bSec;
	const bool bFrameSecond = Spec.bFrameSecondEvent && S.bHasSecondEvent;
	if (bFrameSec || Spec.bFrameEvent || bFrameSecond)
	{
		const float Aspect = 16.f / 9.f;
		float NeedHalf = 0.f;
		TArray<FVector, TInlineAllocator<4>> Pts;
		Pts.Add(FC);
		if (bFrameSec) Pts.Add(SC);
		if (Spec.bFrameEvent) Pts.Add(S.EventLoc);
		if (bFrameSecond) Pts.Add(S.SecondEventLoc);
		for (const FVector& P : Pts)
		{
			const FVector L = Rot.UnrotateVector(P - Loc);
			if (L.X < 10.f) continue;
			const float H = FMath::Atan2(FMath::Abs(L.Y), L.X);
			const float V = FMath::Atan(FMath::Tan(FMath::Atan2(FMath::Abs(L.Z), L.X)) * Aspect);
			NeedHalf = FMath::Max(NeedHalf, FMath::Max(H, V) + FMath::Atan2(Spec.SubjectRadius, L.X));
		}
		Fov = FMath::Clamp(FMath::Max(Fov, FMath::RadiansToDegrees(2.f * NeedHalf) * Spec.FramePadding), 5.f, Spec.MaxFOV);
	}
	// Deterministic motion character (no randomness: the same shot always plays the same).
	const float Tm = S.MotionClock;
	if (Spec.RollWobbleDeg != 0.f) Rot.Roll += Spec.RollWobbleDeg * FMath::Sin(UE_TWO_PI * Spec.RollWobbleHz * Tm);
	if (Spec.ShakeDeg != 0.f)
	{
		const float A = Spec.ShakeDeg * (Spec.ShakeDecay > 0.f ? FMath::Exp(-Spec.ShakeDecay * Tm) : 1.f);
		const float SW = UE_TWO_PI * Spec.ShakeHz;
		Rot.Pitch += A * (0.7f * FMath::Sin(SW * Tm + 0.3f) + 0.3f * FMath::Sin(1.73f * SW * Tm + 1.1f));
		Rot.Yaw += A * (0.7f * FMath::Sin(1.21f * SW * Tm + 2.0f) + 0.3f * FMath::Sin(2.37f * SW * Tm + 0.5f));
	}
	OutView = FMinimalViewInfo();
	OutView.Location = Loc;
	OutView.Rotation = Rot;
	OutView.FOV = Fov;
	OutView.AspectRatio = 16.f / 9.f;
	OutView.bConstrainAspectRatio = false;
	return true;
}

float UZoogiEpisodeCameraDirector::SlowLookWeight(const FZoogiShotSpec& Spec, float Age, float Duration) const
{
	if (!Spec.bSlowMoEntry) return 0.f;
	// Same window shape the old time-scale entry had, fitted inside the shot.
	const float Avail = FMath::Max(0.f, Duration - Spec.SlowMoDelay - 0.1f);
	const float In = FMath::Min(Spec.SlowMoEaseIn, Avail * 0.3f);
	const float Out = FMath::Min(Spec.SlowMoEaseOut, Avail * 0.4f);
	const float Hold = FMath::Clamp(Spec.SlowMoHold, 0.f, FMath::Max(0.f, Avail - In - Out));
	const float T = Age - Spec.SlowMoDelay;
	if (T <= 0.f) return 0.f;
	if (T < In) return ZoogiShot::ApplyEase(EZoogiShotEase::SmoothStep, T / FMath::Max(In, 1e-3f));
	if (T < In + Hold) return 1.f;
	if (T < In + Hold + Out) return 1.f - ZoogiShot::ApplyEase(EZoogiShotEase::SmoothStep, (T - In - Hold) / FMath::Max(Out, 1e-3f));
	return 0.f;
}

void UZoogiEpisodeCameraDirector::GatherAvoidVolumes(const FVector& Around, float Radius, TArray<FBox>& Out) const
{
	UWorld* W = GetWorld();
	if (!W) return;
	if (AvoidCacheFrame == GFrameCounter && FVector::DistSquared(AvoidCacheAround, Around) < 1.f) { Out.Append(AvoidCache); return; }
	TArray<FBox> Found;
	auto Consider = [&](const FBox& B)
	{
		if (!B.IsValid) return;
		const FVector Ext = B.GetExtent();
		// Effects with huge fixed bounds (sky, ambient snow) cannot be avoided meaningfully; ignore them.
		if (Ext.GetMax() > 1500.f || Ext.GetMax() < 5.f) return;
		if (B.ComputeSquaredDistanceToPoint(Around) > Radius * Radius) return;
		Found.Add(B);
	};
	for (TObjectIterator<UFXSystemComponent> It; It; ++It)
	{
		UFXSystemComponent* C = *It;
		if (!C || C->GetWorld() != W || !C->IsRegistered() || !C->IsActive() || !C->IsVisible() || C->IsBeingDestroyed()) continue;
		if (const AActor* O = C->GetOwner()) { if (O->IsHidden()) continue; }
		Consider(C->Bounds.GetBox());
	}
	for (TActorIterator<AZoogiFX> It(W); It; ++It)
	{
		if (It->IsHidden() || It->IsActorBeingDestroyed()) continue;
		Consider(It->GetComponentsBoundingBox(true));
	}
	AvoidCacheFrame = GFrameCounter;
	AvoidCacheAround = Around;
	AvoidCache = Found;
	Out.Append(Found);
}

FVector UZoogiEpisodeCameraDirector::ResolveOcclusion(const FVector& Look, const FVector& Loc, AActor* F, AActor* Sec, bool& bOutBlocked) const
{
	bOutBlocked = false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiShotProbe), false);
	if (F) Q.AddIgnoredActor(F);
	if (Sec) Q.AddIgnoredActor(Sec);
	if (ShotCam) Q.AddIgnoredActor(ShotCam);
	for (int32 Try = 0; Try < 6; ++Try)
	{
		FHitResult Hit;
		if (!GetWorld()->SweepSingleByChannel(Hit, Look, Loc, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q)) return Loc;
		const UPrimitiveComponent* HC = Hit.GetComponent();
		if (Hit.bStartPenetrating || (HC && HC->IsSimulatingPhysics()))
		{
			// Moving bodies (Zoogis, orbs, rubble) never push the shot camera around; only static set pieces do.
			if (AActor* HA = Hit.GetActor()) { Q.AddIgnoredActor(HA); continue; }
			return Loc;
		}
		bOutBlocked = true;
		return Look + (Loc - Look) * FMath::Clamp(Hit.Time - 0.02f, 0.1f, 1.f);
	}
	return Loc;
}

void UZoogiEpisodeCameraDirector::TickShot(FZoogiActiveShot& S, float Dt, bool& bOutFinished, EZoogiShotEndReason& OutReason)
{
	bOutFinished = false;
	S.Age += Dt;
	// Presentation-only slow-mo look: the shot's own motion slows; world time is never touched.
	const float SlowW = SlowLookWeight(S.Spec, S.Age, S.Duration);
	S.MotionRate = FMath::Lerp(1.f, FMath::Clamp(S.Spec.SlowMoTarget, 0.05f, 1.f), SlowW);
	S.MotionClock += Dt * S.MotionRate;
	if (SlowW > 0.01f) { S.Stats.SlowLookFrames++; S.Stats.MinMotionRate = FMath::Min(S.Stats.MinMotionRate, S.MotionRate); }
	FMinimalViewInfo ShotView;
	if (!EvaluateShotView(S, Dt, ShotView))
	{
		bOutFinished = true;
		OutReason = EZoogiShotEndReason::FocusLost;
		return;
	}
	S.LastShotView = ShotView;
	const float FocusDist = FVector::Dist(ShotView.Location, S.LastFocus);

	if (S.Output == EZoogiShotOutput::RenderTarget)
	{
		if (S.Capture && S.RenderTarget)
		{
			S.Capture->TextureTarget = S.RenderTarget;
			S.Capture->SetWorldLocationAndRotation(ShotView.Location, ShotView.Rotation);
			S.Capture->FOVAngle = ShotView.FOV;
			S.Capture->PostProcessSettings = BuildShotPostProcess(S.Spec, FocusDist, 1.f);
			CamDirApplyAccent(S.Capture->PostProcessSettings, S, 1.f);
			CamDirSlowLookGrade(S.Capture->PostProcessSettings, SlowW);
			S.Capture->PostProcessBlendWeight = 1.f;
			S.Capture->CaptureSceneDeferred();
		}
		S.LastOutputView = ShotView;
		if (S.Age >= S.Duration) { bOutFinished = true; OutReason = EZoogiShotEndReason::Completed; }
		return;
	}

	// MainView: weight 0 = live gameplay view, 1 = shot.
	float W = 1.f;
	if (S.Spec.EaseIn > 0.f && S.Age < S.Spec.EaseIn) W = FMath::Min(W, CurveOrEase(nullptr, S.Spec.Ease, S.Age / S.Spec.EaseIn));
	const float Rem = S.Duration - S.Age;
	if (S.Spec.EaseOut > 0.f && Rem < S.Spec.EaseOut) W = FMath::Min(W, CurveOrEase(nullptr, S.Spec.Ease, Rem / S.Spec.EaseOut));
	if (Rem <= 0.f) W = 0.f;
	bool bStopDone = false;
	if (S.bStopping)
	{
		S.StopAge += Dt;
		const float A = S.Spec.EaseOut > 0.f ? S.StopAge / S.Spec.EaseOut : 1.f;
		W = FMath::Min(W, S.StopWeight * (1.f - CurveOrEase(nullptr, S.Spec.Ease, A)));
		bStopDone = A >= 1.f;
	}
	FMinimalViewInfo Live;
	const bool bLive = GetLiveGameplayView(Dt, Live);
	if (!bLive)
	{
		bOutFinished = true;
		OutReason = EZoogiShotEndReason::Stopped; // the gameplay camera vanished; RestoreMainView falls back safely
		return;
	}
	const FMinimalViewInfo& Base = (S.bBlendFromFrozen && S.Age < S.Spec.EaseIn) ? S.BlendFrom : Live;
	const FMinimalViewInfo Out = CamDirBlendViews(Base, ShotView, W);
	S.LastOutputView = Out;
	if (ShotCam)
	{
		UCameraComponent* C = ShotCam->GetCameraComponent();
		ShotCam->SetActorLocationAndRotation(Out.Location, Out.Rotation);
		C->SetFieldOfView(Out.FOV);
		C->PostProcessSettings = BuildShotPostProcess(S.Spec, FocusDist, W);
		CamDirApplyAccent(C->PostProcessSettings, S, W);
		CamDirSlowLookGrade(C->PostProcessSettings, SlowW * W);
		C->PostProcessBlendWeight = Live.PostProcessBlendWeight;
	}
	if (bStopDone) { bOutFinished = true; OutReason = EZoogiShotEndReason::Stopped; }
	else if (S.Age >= S.Duration) { bOutFinished = true; OutReason = EZoogiShotEndReason::Completed; }
}

void UZoogiEpisodeCameraDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* W = GetWorld();
	if (!W || bTornDown) return;
	// Presentation runs on real time: slow motion never stretches a shot, a pause never freezes one.
	const float RealDt = FMath::Clamp(W->DeltaRealTimeSeconds, 0.f, 0.1f);
	TickTimeScale(RealDt);
	for (int32 i = 0; i < Shots.Num();)
	{
		bool bFinished = false;
		EZoogiShotEndReason Reason = EZoogiShotEndReason::Completed;
		const int32 Id = Shots[i].Id;
		TickShot(Shots[i], RealDt, bFinished, Reason);
		if (bFinished)
		{
			const int32 Idx = Shots.IndexOfByPredicate([Id](const FZoogiActiveShot& S) { return S.Id == Id; });
			FinishShot(Idx, Reason);
			continue; // array shifted
		}
		++i;
	}
	// Safety net: our camera must never stay on the main view without an active MainView shot.
	if (MainViewShotId == 0 && ShotCam)
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->GetViewTarget() == ShotCam)
			{
				UE_LOG(LogZoogi, Warning, TEXT("CAMDIR safety net: shot camera still on %s's view with no active shot, restoring"), *GetNameSafe(PC));
				if (MainViewSnapshot.bValid) RestoreMainView(EZoogiShotEndReason::Stopped);
				else PC->SetViewTarget(PC->GetPawn() ? static_cast<AActor*>(PC->GetPawn()) : static_cast<AActor*>(PC));
			}
		}
	}
}

// ============================================================================ time scale

FString UZoogiEpisodeCameraDirector::FindAwakePhysicsBody() const
{
	UWorld* W = GetWorld();
	if (!W) return FString();
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		FString Found;
		It->ForEachComponent<UPrimitiveComponent>(false, [&](UPrimitiveComponent* C)
		{
			if (Found.IsEmpty() && C->IsRegistered() && C->IsSimulatingPhysics() && C->RigidBodyIsAwake())
			{
				Found = FString::Printf(TEXT("%s.%s"), *It->GetName(), *C->GetName());
			}
		});
		if (!Found.IsEmpty()) return Found;
	}
	return FString();
}

bool UZoogiEpisodeCameraDirector::EaseTimeScale(const FZoogiTimeScaleSpec& Spec)
{
	UWorld* W = GetWorld();
	AWorldSettings* WS = W ? W->GetWorldSettings() : nullptr;
	if (!WS || bTornDown) return false;
	if (CVarAllowTimeDilation.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogZoogi, Log, TEXT("CAMDIR time-scale ease refused: physics is never dilated (core rule); use the Episode pause-hold or a shot's presentation-only slow-mo look"));
		return false;
	}
	if (Policy == EZoogiTimeScalePolicy::Disabled)
	{
		UE_LOG(LogZoogi, Log, TEXT("CAMDIR time-scale ease refused: policy Disabled (opt in with SetTimeScalePolicy)"));
		return false;
	}
	if (Policy == EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest)
	{
		const FString Awake = FindAwakePhysicsBody();
		if (!Awake.IsEmpty())
		{
			UE_LOG(LogZoogi, Log, TEXT("CAMDIR time-scale ease refused: simulated body awake (%s); slowing it could change the physics outcome"), *Awake);
			return false;
		}
	}
	if (!TS.bActive) TS.Prior = WS->CinematicTimeDilation; // exact original, kept across overlapping eases
	TimeScaleOwnerShotId = PendingTimeScaleOwner; // a direct caller takes the ease over from any shot
	TS.bActive = true;
	TS.Spec = Spec;
	TS.From = WS->CinematicTimeDilation;
	TS.LastSet = TS.From;
	TS.Age = 0.f;
	TS.bOut = false;
	UE_LOG(LogZoogi, Log, TEXT("CAMDIR time-scale ease %.3f -> %.3f (in %.2f, hold %.2f, out %.2f), prior %.6f"), TS.From, Spec.Target, Spec.EaseIn, Spec.Hold, Spec.EaseOut, TS.Prior);
	return true;
}

void UZoogiEpisodeCameraDirector::StopTimeScale(bool bImmediate)
{
	if (!TS.bActive) return;
	if (bImmediate || TS.Spec.EaseOut <= 0.f) { RestoreTimeScaleExact(TEXT("stopped")); return; }
	if (!TS.bOut)
	{
		TS.bOut = true;
		TS.OutAge = 0.f;
		TS.OutFrom = TS.LastSet;
	}
}

void UZoogiEpisodeCameraDirector::SetCinematicDilation(float Value)
{
	if (AWorldSettings* WS = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr)
	{
		TS.LastSet = WS->SetCinematicTimeDilation(Value);
	}
}

void UZoogiEpisodeCameraDirector::RestoreTimeScaleExact(const TCHAR* Why)
{
	AWorldSettings* WS = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr;
	if (WS)
	{
		// Direct assignment: bypasses Min/MaxCinematicTimeDilation clamping so the prior value comes back bit-exact.
		WS->CinematicTimeDilation = TS.Prior;
	}
	UE_LOG(LogZoogi, Log, TEXT("CAMDIR time-scale restored to %.6f (%s)"), TS.Prior, Why);
	TimeScaleOwnerShotId = 0;
	TS.bActive = false;
	TS.bOut = false;
}

void UZoogiEpisodeCameraDirector::TickTimeScale(float RealDt)
{
	if (!TS.bActive) return;
	AWorldSettings* WS = GetWorld()->GetWorldSettings();
	if (!WS) return;
	if (!FMath::IsNearlyEqual(WS->CinematicTimeDilation, TS.LastSet, 1e-6f))
	{
		UE_LOG(LogZoogi, Warning, TEXT("CAMDIR time-scale: cinematic dilation changed externally (%.4f, expected %.4f); restoring prior and yielding"), WS->CinematicTimeDilation, TS.LastSet);
		RestoreTimeScaleExact(TEXT("external change"));
		return;
	}
	if (Policy == EZoogiTimeScalePolicy::OnlyWhenPhysicsAtRest)
	{
		const FString Awake = FindAwakePhysicsBody();
		if (!Awake.IsEmpty())
		{
			UE_LOG(LogZoogi, Warning, TEXT("CAMDIR time-scale aborted: %s woke up"), *Awake);
			RestoreTimeScaleExact(TEXT("physics woke"));
			return;
		}
	}
	const FZoogiTimeScaleSpec& Sp = TS.Spec;
	TS.Age += RealDt;
	if (!TS.bOut)
	{
		if (TS.Age < Sp.EaseIn)
		{
			SetCinematicDilation(FMath::Lerp(TS.From, Sp.Target, CurveOrEase(Sp.Curve, Sp.Ease, TS.Age / Sp.EaseIn)));
			return;
		}
		if (Sp.Hold < 0.f || TS.Age < Sp.EaseIn + Sp.Hold)
		{
			SetCinematicDilation(Sp.Target);
			return;
		}
		TS.bOut = true;
		TS.OutAge = 0.f;
		TS.OutFrom = TS.LastSet;
	}
	TS.OutAge += RealDt;
	if (Sp.EaseOut <= 0.f || TS.OutAge >= Sp.EaseOut)
	{
		RestoreTimeScaleExact(TEXT("ease finished"));
		return;
	}
	SetCinematicDilation(FMath::Lerp(TS.OutFrom, TS.Prior, CurveOrEase(Sp.Curve, Sp.Ease, TS.OutAge / Sp.EaseOut)));
}

// ============================================================================ verification

FZoogiCameraSnapshot UZoogiEpisodeCameraDirector::CaptureGameplaySnapshot(int32 PlayerIndex) const
{
	FZoogiCameraSnapshot S;
	UWorld* W = GetWorld();
	APlayerController* PC = W ? UGameplayStatics::GetPlayerController(W, PlayerIndex) : nullptr;
	if (!PC || !PC->PlayerCameraManager) return S;
	APlayerCameraManager* PCM = PC->PlayerCameraManager;
	S.bValid = true;
	S.PC = PC;
	S.ViewTarget = PC->GetViewTarget();
	S.POV = PCM->GetCameraCacheView();
	S.bAutoManageActiveCameraTarget = PC->bAutoManageActiveCameraTarget;
	S.PCMDefaultFOV = PCM->DefaultFOV;
	S.PCMLockedFOV = PCM->GetLockedFOV();
	S.PCMCameraStyle = PCM->CameraStyle;
	S.bPCMOrthographic = PCM->bIsOrthographic;
	S.bPCMEnableFading = PCM->bEnableFading;
	S.PCMFadeAmount = PCM->FadeAmount;
	S.PCMViewPitchMin = PCM->ViewPitchMin;
	S.PCMViewPitchMax = PCM->ViewPitchMax;
	if (UCameraComponent* C = CamDirCameraComponentOf(S.ViewTarget.Get()))
	{
		S.bHasCameraComponent = true;
		S.CameraActorTransform = S.ViewTarget->GetActorTransform();
		S.CameraComponentTransform = C->GetComponentTransform();
		S.CameraFOV = C->FieldOfView;
		S.bCameraConstrainAspect = C->bConstrainAspectRatio;
		S.CameraAspectRatio = C->AspectRatio;
		S.CameraPPBlendWeight = C->PostProcessBlendWeight;
		S.CameraPP = C->PostProcessSettings;
	}
	if (AWorldSettings* WS = W->GetWorldSettings())
	{
		S.TimeDilation = WS->TimeDilation;
		S.CinematicTimeDilation = WS->CinematicTimeDilation;
	}
	return S;
}

bool UZoogiEpisodeCameraDirector::CompareSnapshots(const FZoogiCameraSnapshot& A, const FZoogiCameraSnapshot& B, const FZoogiCameraTolerance& Tol, FString& OutReport)
{
	TArray<FString> Bad;
	if (!A.bValid || !B.bValid) Bad.Add(TEXT("snapshot invalid"));
	if (A.ViewTarget.Get() != B.ViewTarget.Get()) Bad.Add(FString::Printf(TEXT("viewtarget %s != %s"), *GetNameSafe(B.ViewTarget.Get()), *GetNameSafe(A.ViewTarget.Get())));
	const float DLoc = FVector::Dist(A.POV.Location, B.POV.Location);
	const float DRot = FMath::RadiansToDegrees(A.POV.Rotation.Quaternion().AngularDistance(B.POV.Rotation.Quaternion()));
	const float DFov = FMath::Abs(A.POV.FOV - B.POV.FOV);
	if (DLoc > Tol.LocationCm) Bad.Add(FString::Printf(TEXT("pov loc %.4f cm"), DLoc));
	if (DRot > Tol.RotationDeg) Bad.Add(FString::Printf(TEXT("pov rot %.4f deg"), DRot));
	if (DFov > Tol.FOVDeg) Bad.Add(FString::Printf(TEXT("pov fov %.4f deg"), DFov));
	if (A.bHasCameraComponent != B.bHasCameraComponent) Bad.Add(TEXT("camera component presence"));
	if (A.bHasCameraComponent && B.bHasCameraComponent)
	{
		if (!A.CameraActorTransform.Equals(B.CameraActorTransform, Tol.LocationCm)) Bad.Add(TEXT("camera actor transform"));
		if (FMath::Abs(A.CameraFOV - B.CameraFOV) > Tol.FOVDeg) Bad.Add(FString::Printf(TEXT("camera fov %.3f -> %.3f"), A.CameraFOV, B.CameraFOV));
		if (A.bCameraConstrainAspect != B.bCameraConstrainAspect || FMath::Abs(A.CameraAspectRatio - B.CameraAspectRatio) > Tol.Scalar) Bad.Add(TEXT("camera aspect"));
		if (FMath::Abs(A.CameraPPBlendWeight - B.CameraPPBlendWeight) > Tol.Scalar) Bad.Add(TEXT("camera pp blend weight"));
		const FPostProcessSettings& P = A.CameraPP;
		const FPostProcessSettings& Q = B.CameraPP;
		auto V4 = [&](const FVector4& X, const FVector4& Y) { return FMath::Abs(X.X - Y.X) <= Tol.Scalar && FMath::Abs(X.Y - Y.Y) <= Tol.Scalar && FMath::Abs(X.Z - Y.Z) <= Tol.Scalar && FMath::Abs(X.W - Y.W) <= Tol.Scalar; };
		if (P.bOverride_VignetteIntensity != Q.bOverride_VignetteIntensity || FMath::Abs(P.VignetteIntensity - Q.VignetteIntensity) > Tol.Scalar) Bad.Add(TEXT("pp vignette"));
		if (P.bOverride_ColorSaturation != Q.bOverride_ColorSaturation || !V4(P.ColorSaturation, Q.ColorSaturation)) Bad.Add(TEXT("pp saturation"));
		if (P.bOverride_ColorContrast != Q.bOverride_ColorContrast || !V4(P.ColorContrast, Q.ColorContrast)) Bad.Add(TEXT("pp contrast"));
		if (P.bOverride_ColorGain != Q.bOverride_ColorGain || !V4(P.ColorGain, Q.ColorGain)) Bad.Add(TEXT("pp gain"));
		if (P.bOverride_DepthOfFieldFstop != Q.bOverride_DepthOfFieldFstop || FMath::Abs(P.DepthOfFieldFstop - Q.DepthOfFieldFstop) > Tol.Scalar) Bad.Add(TEXT("pp dof fstop"));
		if (P.bOverride_DepthOfFieldFocalDistance != Q.bOverride_DepthOfFieldFocalDistance || FMath::Abs(P.DepthOfFieldFocalDistance - Q.DepthOfFieldFocalDistance) > Tol.LocationCm) Bad.Add(TEXT("pp dof focal"));
		if (P.bOverride_BloomIntensity != Q.bOverride_BloomIntensity || FMath::Abs(P.BloomIntensity - Q.BloomIntensity) > Tol.Scalar) Bad.Add(TEXT("pp bloom"));
		if (P.bOverride_AutoExposureBias != Q.bOverride_AutoExposureBias || FMath::Abs(P.AutoExposureBias - Q.AutoExposureBias) > Tol.Scalar) Bad.Add(TEXT("pp exposure bias"));
		if (P.WeightedBlendables.Array.Num() != Q.WeightedBlendables.Array.Num()) Bad.Add(TEXT("pp blendables"));
	}
	if (A.PCMDefaultFOV != B.PCMDefaultFOV || A.PCMLockedFOV != B.PCMLockedFOV || A.PCMCameraStyle != B.PCMCameraStyle || A.bPCMOrthographic != B.bPCMOrthographic) Bad.Add(TEXT("camera manager settings"));
	if (A.bPCMEnableFading != B.bPCMEnableFading || FMath::Abs(A.PCMFadeAmount - B.PCMFadeAmount) > Tol.Scalar) Bad.Add(TEXT("camera fade"));
	if (A.PCMViewPitchMin != B.PCMViewPitchMin || A.PCMViewPitchMax != B.PCMViewPitchMax) Bad.Add(TEXT("view pitch limits"));
	if (A.bAutoManageActiveCameraTarget != B.bAutoManageActiveCameraTarget) Bad.Add(TEXT("auto manage camera target"));
	if (A.TimeDilation != B.TimeDilation) Bad.Add(FString::Printf(TEXT("time dilation %.6f -> %.6f"), A.TimeDilation, B.TimeDilation));
	if (A.CinematicTimeDilation != B.CinematicTimeDilation) Bad.Add(FString::Printf(TEXT("cinematic dilation %.6f -> %.6f"), A.CinematicTimeDilation, B.CinematicTimeDilation));
	OutReport = FString::Printf(TEXT("vt=%s dLoc=%.5fcm dRot=%.5fdeg dFov=%.5f"), *GetNameSafe(B.ViewTarget.Get()), DLoc, DRot, DFov);
	if (Bad.Num() > 0) OutReport += TEXT(" MISMATCH: ") + FString::Join(Bad, TEXT(", "));
	return Bad.Num() == 0;
}

bool UZoogiEpisodeCameraDirector::GetShotAvoidStats(FZoogiShotHandle Shot, FZoogiShotAvoidStats& Out) const
{
	if (const FZoogiActiveShot* A = Shots.FindByPredicate([&](const FZoogiActiveShot& S) { return S.Id == Shot.Id; })) { Out = A->Stats; return true; }
	if (const FZoogiShotAvoidStats* F = StatsById.Find(Shot.Id)) { Out = *F; return true; }
	return false;
}
