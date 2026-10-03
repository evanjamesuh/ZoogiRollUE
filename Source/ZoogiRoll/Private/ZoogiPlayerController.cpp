#include "ZoogiPlayerController.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "ZoogiArena.h"
#include "ZoogiHUD.h"
#include "ZoogiTargetMarker.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/GameViewportClient.h"

// ============================================================================
// Camera director tuning: every number for the per-stage cameras lives here.
// ============================================================================
namespace ZoogiCamTune
{
	struct FProfile
	{
		const TCHAR* Name;
		float Pitch;      // degrees above horizontal
		float Dist;       // boom length, cm
		float Fov;        // horizontal FOV
		float Dutch;      // roll, degrees
		float PosLag;     // pivot / look-at damping (1/s)
		float BlendLag;   // pitch / distance / FOV / grade damping (1/s): ~6 = 0.5 s blend
		float YawFreq;    // yaw spring angular frequency (rad/s)
		float YawZeta;    // yaw spring damping ratio (<1 overshoots)
		float Vignette;
		float Dof;        // 0 = sharp, 1 = light background blur
		float Saturation;
		float Contrast;
		FLinearColor Gain; // colour grade lift (warm / neutral / cool)
		float Blend;      // seconds to blend INTO this profile from whatever was before
	};

	enum EProfile { PDraw, PStandby, PMain, PBattle, PEnd, PFollow, PMenu, PTargetCU, PCount };

	static const FProfile Profiles[PCount] = {
		// Name       Pitch  Dist   FOV   Dutch PosLag Blend YawW  Zeta  Vign  DOF   Sat   Contr  Gain                                 BlendIn
		{ TEXT("Draw"),     7.f,  420.f, 102.f, 0.f, 5.f,  6.f,  5.f, 1.0f, 0.40f, 0.40f, 1.10f, 1.04f, FLinearColor(1.12f, 1.00f, 0.86f), 0.55f }, // (1) hero shot: low side, warm rim lift, wide
		{ TEXT("Standby"), 66.f, 3500.f, 78.f, 0.f, 2.f,  5.f,  3.f, 1.0f, 0.22f, 0.0f,  0.94f, 0.98f, FLinearColor(1.00f, 1.00f, 1.00f), 0.55f }, // (2) calm overview: high, far, neutral soft
		{ TEXT("Main"),    37.f, 1150.f, 84.f, 0.f, 9.f,  7.f,  9.f, 0.85f, 0.15f, 0.0f, 1.00f, 1.02f, FLinearColor(1.00f, 1.00f, 1.00f), 0.45f }, // (3) tactical OTS: clean, readable
		{ TEXT("Battle"),  24.f,  680.f, 72.f, 4.f, 10.f, 7.f,  7.5f, 0.55f, 0.70f, 1.0f, 1.20f, 1.12f, FLinearColor(1.05f, 0.99f, 0.96f), 0.50f }, // (4) dramatic lock-on
		{ TEXT("End"),     83.f, 4300.f, 74.f, 0.f, 1.5f, 1.6f, 3.f, 1.0f, 0.18f, 0.0f,  0.78f, 1.00f, FLinearColor(0.90f, 0.97f, 1.10f), 0.60f }, // (5) aerial resolution: top-down, cool, desaturated
		{ TEXT("Follow"),  55.f, 2400.f, 90.f, 0.f, 2.5f, 3.f,  3.f, 1.0f, 0.25f, 0.0f,  1.05f, 1.02f, FLinearColor(1.00f, 1.00f, 1.00f), 0.40f }, // shot follow (first part of End)
		{ TEXT("Menu"),    38.f, 3600.f, 90.f, 0.f, 2.f,  1.f,  2.f, 1.0f, 0.20f, 0.0f,  1.00f, 1.00f, FLinearColor(1.00f, 1.00f, 1.00f), 0.60f },
		{ TEXT("TargetCU"),  9.f,  640.f, 40.f, -5.f, 14.f, 9.f, 12.f, 0.9f, 0.80f, 1.0f, 1.25f, 1.16f, FLinearColor(1.10f, 0.96f, 0.92f), 0.28f }, // Battle beat 1 (long locks): tight on the target
	};

	// Draw hero shot: side angle, push-in and orbit sweep.
	constexpr float DrawSideYaw = 100.f;      // start this far around from the aim heading
	constexpr float DrawSweep = 40.f;         // orbit sweep over the stage
	constexpr float DrawPushIn = 0.22f;       // boom shortens by this fraction
	// Standby drift.
	constexpr float StandbyDrift = 5.f;       // deg/s orbit around the arena centre
	// Main.
	constexpr float MainLookBias = 0.45f;     // look point between shooter (0) and target (1)
	constexpr float MainCentreBias = 0.35f;   // no lock: look this far from the shooter toward the arena centre
	constexpr float WhipFreq = 16.f;          // target switch whip-pan spring
	constexpr float WhipZeta = 0.7f;
	constexpr float WhipTime = 0.35f;
	// Battle cinematic.
	constexpr float BattleLookBias = 0.72f;   // target big and centred, shooter as a shoulder silhouette
	constexpr float BattleLookBiasFar = 0.5f; // long-range locks: keep the shooter in frame
	constexpr float ShoulderOffset = 150.f;   // cm to the side of the shooter
	constexpr float BattleFarPitch = 10.f;    // extra pitch for long locks
	constexpr float BattleFarDist = 0.3f;     // extra boom for long locks
	constexpr float BattleShooterMaxDrop = 0.62f;
	constexpr float BattleTargetMaxRise = 0.62f;  // target top stays within 62% of the half-height above centre (below the top HUD)
	constexpr float BattleFovMax = 92.f;          // FOV may widen up to this to fit both // shooter's bottom stays within 55% of the half-height below centre (clear of the letterbox + HUD)
	constexpr float BattleFovFrame = 650.f;   // FOV narrows so ~6.5 m around the target fills the frame...
	constexpr float BattleFovMin = 52.f;      // ...but never tighter than this
	constexpr float SwingDip = 0.25f;         // boom shortens mid-swing
	constexpr float SwingPitchDip = 7.f;      // and drops lower mid-swing
	constexpr float LockFovPunch = 8.f;       // degrees of punch-in at the lock moment
	constexpr float LockFovPunchTime = 0.4f;
	constexpr float ShakeDeg = 1.1f;
	constexpr float ShakeDecay = 6.f;
	constexpr float SlowMoScale = 0.6f;       // only while everything is at rest (no effect on physics outcomes)
	constexpr float SlowMoTime = 0.25f;
	// Impact (first contact of a shot).
	constexpr float HitStopScale = 0.08f;
	constexpr float HitStopTime = 0.09f;      // real seconds at full strength
	constexpr float ImpactShakeDeg = 1.4f;
	constexpr float ImpactShakeDecay = 7.f;
	constexpr float ImpactFovPunch = 5.f;
	// Fire / follow.
	constexpr float FireFovKick = 12.f;       // punch-out on release
	constexpr float FireKickDecay = 3.5f;
	constexpr float FollowTime = 1.4f;        // End: follow the shot this long, then aerial
	constexpr float FollowYawRate = 0.5f;
	constexpr float FollowYawCap = 20.f;
	// Collision probe.
	constexpr float ProbeRadius = 18.f;
	constexpr float ProbeIn = 25.f;           // pull in fast when blocked
	constexpr float ProbeOut = 3.f;           // ease back out
	constexpr float NeonScale = 1.15f;        // bigger court -> longer overview booms
	constexpr float NeonEndScale = 0.62f;     // End aerial on the rectangular court (squared up, so it can sit closer)

	inline float Damp(float X, float Target, float Lambda, float Dt) { return X + (Target - X) * (1.f - FMath::Exp(-Lambda * Dt)); }
	inline FVector Damp(const FVector& X, const FVector& Target, float Lambda, float Dt) { return X + (Target - X) * (1.f - FMath::Exp(-Lambda * Dt)); }
	inline FLinearColor Damp(const FLinearColor& X, const FLinearColor& Target, float Lambda, float Dt) { return X + (Target - X) * (1.f - FMath::Exp(-Lambda * Dt)); }
	inline float SmoothStep(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	inline float EaseOut3(float T) { T = 1.f - FMath::Clamp(T, 0.f, 1.f); return 1.f - T * T * T; }
	// Phase blend curve: Main and Battle answer the player immediately (ease-out), the mood shots ease in and out.
	inline float BlendCurve(int32 Profile, float T) { return (Profile == PMain || Profile == PBattle || Profile == PTargetCU) ? EaseOut3(T) : SmoothStep(T); }
	constexpr float TargetCUYaw = 28.f;       // close-up looks at the target from this far off the shot line
	constexpr float WhipBackBlend = 0.32f;    // beat 2: whip from the target close-up back to the shooter
}

namespace
{
	FVector ActorCenter(const AActor* A)
	{
		if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->BallLocation();
		if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->OrbLocation();
		return A ? A->GetActorLocation() : FVector::ZeroVector;
	}
}

AZoogiPlayerController::AZoogiPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

AZoogiGameMode* AZoogiPlayerController::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

void AZoogiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(FRotator(-35, 0, 0), FVector(-2500, 0, 1500)));
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(90.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		Cam->GetCameraComponent()->PostProcessBlendWeight = 1.f;
		SetViewTarget(Cam);
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	HoverMarker = GetWorld()->SpawnActor<AZoogiTargetMarker>(AZoogiTargetMarker::StaticClass(), FTransform::Identity, P);
	LockMarker = GetWorld()->SpawnActor<AZoogiTargetMarker>(AZoogiTargetMarker::StaticClass(), FTransform::Identity, P);
	bPowerTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerTest"));
	bHumanTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHumanTest"));
}

FVector2D AZoogiPlayerController::GetCursor() const
{
	if (bHumanTest) return SimMouse;
	float MX = 0.f, MY = 0.f;
	GetMousePosition(MX, MY);
	return FVector2D(MX, MY);
}

// ------------------------------------------------------------------ scripted human

void AZoogiPlayerController::UpdateHumanTest(float Dt)
{
	bSimClick = bSimTab = bSimSpace = false;
	AZoogiGameMode* G = GM();
	if (!G || !G->IsHumanAiming()) { SimStep = 0; return; }
	if (G->Stage != SimStage) { SimStage = G->Stage; SimStep = 0; }
	SimClock += Dt;
	const float T = G->StageTime;
	auto Aim = [&](bool bWantBall)
	{
		// Move the virtual cursor onto the nearest orb (or rival Zoogi).
		for (AActor* A : G->TargetCandidates())
		{
			if ((Cast<AZoogiBall>(A) != nullptr) != bWantBall) continue;
			FVector2D S;
			if (ProjectWorldLocationToScreen(ActorCenter(A), S)) { SimMouse = S; return; }
		}
	};
	if (G->Stage == EZoogiStage::Standby)
	{
		if (SimStep == 0 && T > 0.45f) { Aim(false); SimStep = 1; }
		else if (SimStep == 1 && T > 0.75f) { Aim(false); bSimClick = true; SimStep = 2; }  // soft lock
	}
	else if (G->Stage == EZoogiStage::Main)
	{
		if (SimStep == 0 && T > 0.7f) { bSimTab = true; SimStep = 1; }                     // Tab cycles the lock
		else if (SimStep == 1 && T > 1.4f) { Aim(true); SimStep = 2; }
		else if (SimStep == 2 && T > 1.7f) { Aim(true); bSimClick = true; SimStep = 3; }   // switch lock to a rival
		else if (SimStep == 3 && T > 2.6f) { bSimSpace = true; SimStep = 4; }              // fire
	}
}

// ------------------------------------------------------------------ turn hooks

void AZoogiPlayerController::OnTurnStarted(AZoogiBall* Ball)
{
	CamTarget = Ball;
	bMenuCam = false;
	HoverTarget.Reset();
	if (Ball)
	{
		DrawBaseYaw = Ball->AimYaw;
		// Hard cut into the hero shot (C18).
		Yaw = DrawBaseYaw + ZoogiCamTune::DrawSideYaw;
		YawVel = 0.f;
		Pivot = Ball->BallLocation();
		Look = Pivot + FVector(0, 0, 25.f);
		Pitch = ZoogiCamTune::Profiles[ZoogiCamTune::PDraw].Pitch;
		Dist = ZoogiCamTune::Profiles[ZoogiCamTune::PDraw].Dist;
		Fov = ZoogiCamTune::Profiles[ZoogiCamTune::PDraw].Fov;
		ArmScale = 1.f;
	}
}

void AZoogiPlayerController::OnStageChanged(EZoogiStage Stage)
{
	if (Stage == EZoogiStage::Standby) StandbyYaw = Yaw;
	if (Stage == EZoogiStage::Battle)
	{
		SwingAge = 0.f;
		bWhip = false;
		LockPunchAge = 0.f;
		AZoogiGameMode* G = GM();
		if (G && G->AllAtRestPublic())
		{
			// Slow-mo dip on the lock moment: everything is at rest, so physics outcomes are unaffected.
			UGameplayStatics::SetGlobalTimeDilation(this, ZoogiCamTune::SlowMoScale);
			SlowMoUntil = GetWorld()->GetRealTimeSeconds() + ZoogiCamTune::SlowMoTime;
		}
	}
	if (Stage == EZoogiStage::End) EndSlowMo();
}

void AZoogiPlayerController::EndSlowMo()
{
	if (SlowMoUntil > 0.0)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		SlowMoUntil = 0.0;
	}
}

void AZoogiPlayerController::OnImpact(float Strength)
{
	using namespace ZoogiCamTune;
	if (bMenuCam) return;
	ImpactAge = 0.f;
	ImpactStrength = FMath::Clamp(Strength, 0.f, 1.f);
	// Hit-stop: a few frames of near-freeze (uniform time scale, so the physics outcome is the same).
	UGameplayStatics::SetGlobalTimeDilation(this, HitStopScale);
	SlowMoUntil = GetWorld()->GetRealTimeSeconds() + HitStopTime * (0.5f + 0.5f * ImpactStrength);
}

void AZoogiPlayerController::OnShotFired(AZoogiBall* Ball)
{
	CamTarget = Ball;
	FireKickAge = 0.f;
	EndSlowMo();
}

void AZoogiPlayerController::OnGameOver(AZoogiBall* Winner)
{
	bMenuCam = true;
	EndSlowMo();
}

void AZoogiPlayerController::OnMenu()
{
	bMenuCam = true;
	EndSlowMo();
}

void AZoogiPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (SlowMoUntil > 0.0 && GetWorld()->GetRealTimeSeconds() >= SlowMoUntil) EndSlowMo();
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	if (bHumanTest) UpdateHumanTest(Dt);
	HandleInput(Dt);
	UpdateCamera(Dt);
	UpdateMarkers();
}

// ------------------------------------------------------------------ input

AActor* AZoogiPlayerController::PickTarget(float MX, float MY) const
{
	AZoogiGameMode* G = GM();
	if (!G || !PlayerCameraManager) return nullptr;
	const FVector CamRight = PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* A : G->TargetCandidates())
	{
		const FVector C = ActorCenter(A);
		const float R = Cast<AZoogiOrb>(A) ? ZoogiConst::OrbRadius : ZoogiConst::ZoogiRadius;
		FVector2D S, SR;
		if (!ProjectWorldLocationToScreen(C, S) || !ProjectWorldLocationToScreen(C + CamRight * R, SR)) continue;
		const float ScreenR = FMath::Max((SR - S).Size() * 1.25f, 16.f);
		const float D = FVector2D::Distance(S, FVector2D(MX, MY));
		if (D > ScreenR) continue;
		const float Score = D / ScreenR;
		if (Score < BestScore) { BestScore = Score; Best = A; }
	}
	return Best;
}

void AZoogiPlayerController::HandleInput(float Dt)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	AZoogiHUD* Hud = Cast<AZoogiHUD>(GetHUD());
	const FVector2D Mouse = GetCursor();
	float RMX = 0.f, RMY = 0.f;
	GetMousePosition(RMX, RMY);

	// Buttons (menu, results, in-match corner buttons).
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && Hud)
	{
		FName Action; int32 Index = 0;
		if (Hud->HitTest(FVector2D(RMX, RMY), Action, Index))
		{
			G->MenuAction(Action, Index);
			return;
		}
	}

	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)
	{
		HoverTarget.Reset();
		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) G->StartMatch();
		if (WasInputKeyJustPressed(EKeys::Escape) && G->Phase == EZoogiPhase::GameOver) G->ReturnToMenu();
		return;
	}

	AZoogiBall* Active = G->GetActiveBall();
	const bool bHumanTurn = Active && !Active->bCPU;
	const bool bPickStage = G->IsHumanAiming() && (G->Stage == EZoogiStage::Standby || G->Stage == EZoogiStage::Main);

	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		if (bPickStage && G->TargetLock.IsValid()) G->ClearLock(); // Esc clears the lock first
		else G->ReturnToMenu();
		return;
	}
	if (WasInputKeyJustPressed(EKeys::R) && IsInputKeyDown(EKeys::LeftControl)) { G->StartMatch(); return; }

	// Power: E / Q (SPACE now fires the locked shot).
	if (bHumanTurn)
	{
		if (WasInputKeyJustPressed(EKeys::E) || WasInputKeyJustPressed(EKeys::Q)) G->TryPower();
		if (bHumanTest && bPowerTest && Active->Charges > 0 && G->CanUsePower() && !G->bPowerArmed)
		{
			const bool bArmed = ZoogiUtil::IsArmedPower(Active->Def.PowerKind);
			if ((bArmed && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Main && G->StageTime > 0.3f) || (!bArmed && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.4f)) G->TryPower();
		}
	}

	if (!bPickStage)
	{
		HoverTarget.Reset();
		return;
	}

	HoverTarget = PickTarget(Mouse.X, Mouse.Y);
	const bool bClick = WasInputKeyJustPressed(EKeys::LeftMouseButton) || bSimClick;
	const bool bStandby = G->Stage == EZoogiStage::Standby;
	if (bClick && HoverTarget.IsValid())
	{
		if (!bStandby && HoverTarget.Get() == G->TargetLock.Get() && !G->bSoftLock) G->CommitLock(); // click the locked target again = fire
		else G->LockOn(HoverTarget.Get(), bStandby);
	}
	if (WasInputKeyJustPressed(EKeys::Tab) || bSimTab) G->CycleLock(bStandby);
	if (WasInputKeyJustPressed(EKeys::RightMouseButton)) G->ClearLock();
	if (WasInputKeyJustPressed(EKeys::SpaceBar) || bSimSpace) G->CommitLock();
}

// ------------------------------------------------------------------ markers

void AZoogiPlayerController::UpdateMarkers()
{
	AZoogiGameMode* G = GM();
	if (!G || !HoverMarker || !LockMarker) return;
	const bool bInMatch = G->Phase == EZoogiPhase::Aiming;
	AZoogiBall* Active = G->GetActiveBall();
	const FLinearColor PC = Active ? ZoogiUtil::PlayerColor(Active->PlayerIndex) : FLinearColor::White;
	AActor* Lock = bInMatch ? G->TargetLock.Get() : nullptr;
	const EZoogiMarkerStyle LockStyle = !Lock ? EZoogiMarkerStyle::Hidden : (G->bSoftLock ? EZoogiMarkerStyle::SoftLock : EZoogiMarkerStyle::Lock);
	const FLinearColor LockColor = G->Stage == EZoogiStage::Battle ? FLinearColor(1.f, 0.35f, 0.2f) : FMath::Lerp(PC, FLinearColor(1.f, 0.4f, 0.25f), 0.35f);
	LockMarker->SetTarget(Lock, LockStyle, LockColor);
	AActor* Hover = bInMatch ? HoverTarget.Get() : nullptr;
	HoverMarker->SetTarget(Hover != Lock ? Hover : nullptr, EZoogiMarkerStyle::Hover, FLinearColor(0.85f, 0.95f, 1.f));
}

// ------------------------------------------------------------------ camera

FVector AZoogiPlayerController::ProbeCamera(const FVector& From, const FVector& To, float Dt)
{
	using namespace ZoogiCamTune;
	AZoogiGameMode* G = GM();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiCamProbe), false);
	if (G)
	{
		for (AZoogiBall* B : G->Balls) if (B) Q.AddIgnoredActor(B);
		for (AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
		for (AZoogiClone* C : G->Clones) if (C) Q.AddIgnoredActor(C);
	}
	if (HoverMarker) Q.AddIgnoredActor(HoverMarker);
	if (LockMarker) Q.AddIgnoredActor(LockMarker);
	FHitResult Hit;
	float Want = 1.f;
	const float Len = FVector::Dist(From, To);
	if (Len > 1.f && GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(ProbeRadius), Q) && !Hit.bStartPenetrating)
	{
		Want = FMath::Clamp(Hit.Time - 0.02f, 0.08f, 1.f);
	}
	ArmScale = Damp(ArmScale, Want, Want < ArmScale ? ProbeIn : ProbeOut, Dt);
	return From + (To - From) * ArmScale;
}

void AZoogiPlayerController::ApplyPostProcess(float Dt, int32 Profile, float FocusDist)
{
	using namespace ZoogiCamTune;
	if (!Cam) return;
	const FProfile& P = Profiles[Profile];
	const float L = P.BlendLag;
	PPVignette = Damp(PPVignette, P.Vignette, L, Dt);
	PPDof = Damp(PPDof, P.Dof, L, Dt);
	PPSat = Damp(PPSat, P.Saturation, L, Dt);
	PPContrast = Damp(PPContrast, P.Contrast, L, Dt);
	PPGain = Damp(PPGain, P.Gain, L, Dt);
	FPostProcessSettings& S = Cam->GetCameraComponent()->PostProcessSettings;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = PPVignette;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(PPSat, PPSat, PPSat, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(PPContrast, PPContrast, PPContrast, 1.f);
	S.bOverride_ColorGain = true;
	S.ColorGain = FVector4(PPGain.R, PPGain.G, PPGain.B, 1.f);
	S.bOverride_DepthOfFieldFocalDistance = true;
	S.DepthOfFieldFocalDistance = FMath::Max(FocusDist, 50.f);
	S.bOverride_DepthOfFieldFstop = true;
	S.DepthOfFieldFstop = FMath::Lerp(32.f, 2.2f, FMath::Clamp(PPDof, 0.f, 1.f)); // f/32 = effectively sharp
}

void AZoogiPlayerController::UpdateCamera(float Dt)
{
	using namespace ZoogiCamTune;
	AZoogiGameMode* G = GM();
	if (!Cam || !G) return;
	const float Scale = (G->Arena && G->Arena->IsNeon()) ? NeonScale : 1.f;

	if (bMenuCam || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)
	{
		AZoogiBall* W = CamTarget.Get();
		MenuOrbit += Dt * 6.f;
		Yaw = FRotator::NormalizeAxis(Yaw + FRotator::NormalizeAxis(MenuOrbit - Yaw) * (1.f - FMath::Exp(-2.f * Dt)));
		Pitch = Damp(Pitch, 38.f, 1.f, Dt);
		Dist = Damp(Dist, (G->Phase == EZoogiPhase::GameOver ? 2600.f : 3600.f) * Scale, 1.f, Dt);
		Fov = Damp(Fov, 90.f, 2.f, Dt);
		Dutch = Damp(Dutch, 0.f, 4.f, Dt);
		const FVector Target = (G->Phase == EZoogiPhase::GameOver && W) ? W->BallLocation() : FVector(0, 0, -150.f);
		Pivot = Damp(Pivot, Target, 2.f, Dt);
		Look = Damp(Look, Target, 4.f, Dt);
		const FRotator Heading(0.f, Yaw, 0.f);
		const float Pr = FMath::DegreesToRadians(Pitch);
		const FVector Loc = Pivot - Heading.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));
		Cam->SetActorLocation(Loc);
		Cam->SetActorRotation((Look - Loc).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(Fov);
		ApplyPostProcess(Dt, PMenu, FVector::Dist(Loc, Look));
		CurProfile = PMenu;
		return;
	}

	AZoogiBall* Shooter = G->GetActiveBall();
	const FVector SL = Shooter ? Shooter->BallLocation() : FVector::ZeroVector;
	AActor* Lock = (G->TargetLock.IsValid() && !G->bSoftLock) ? G->TargetLock.Get() : nullptr;
	const FVector TL = Lock ? ActorCenter(Lock) : FVector::ZeroVector;
	const float T = G->StageTime;

	// Lock changes in Main swing (switching = whip-pan).
	if (G->LockSerial != SeenLockSerial)
	{
		const bool bHadLock = SeenLockSerial >= 0 && SwingAge < 100.f;
		SeenLockSerial = G->LockSerial;
		if (Lock && G->Stage == EZoogiStage::Main) { bWhip = bHadLock; SwingAge = 0.f; }
	}
	SwingAge += Dt;
	LockPunchAge += Dt;
	FireKickAge += Dt;
	ImpactAge += Dt;

	int32 PIdx = PMain;
	float WantFov = -1.f;
	float WantYaw = Yaw;
	FVector WantPivot = SL, WantLook = SL;
	float PitchOff = 0.f, DistMul = 1.f;
	FVector ProbeFrom = SL + FVector(0, 0, 60.f);
	switch (G->Stage)
	{
	case EZoogiStage::Draw:
	{
		PIdx = PDraw;
		const float Dur = Shooter && Shooter->bCPU ? ZoogiStageTune::DrawCPU : ZoogiStageTune::DrawHuman;
		const float S = SmoothStep(T / Dur);
		WantYaw = DrawBaseYaw + DrawSideYaw - DrawSweep * S;
		DistMul = 1.f - DrawPushIn * S;
		WantPivot = SL;
		WantLook = SL + FVector(0, 0, 25.f);
		break;
	}
	case EZoogiStage::Standby:
	{
		PIdx = PStandby;
		StandbyYaw += StandbyDrift * Dt;
		WantYaw = StandbyYaw;
		WantPivot = FVector(0, 0, -50.f);
		WantLook = WantPivot;
		DistMul = Scale;
		ProbeFrom = FVector(0, 0, 200.f);
		break;
	}
	case EZoogiStage::Main:
	{
		PIdx = PMain;
		if (Lock)
		{
			const FVector D = TL - SL;
			WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			WantLook = FMath::Lerp(SL, TL, MainLookBias) + FVector(0, 0, 20.f);
		}
		else
		{
			// No lock: aim from the shooter toward the arena centre.
			const FVector2D ToC(-SL.X, -SL.Y);
			WantYaw = ToC.Size() > 150.f ? FMath::RadiansToDegrees(FMath::Atan2(ToC.Y, ToC.X)) : (Shooter ? Shooter->AimYaw : Yaw);
			WantLook = (ToC.Size() > 150.f ? FMath::Lerp(SL, FVector(0, 0, SL.Z), MainCentreBias) : SL + FRotator(0.f, WantYaw, 0.f).Vector() * 400.f) + FVector(0, 0, 20.f);
		}
		break;
	}
	case EZoogiStage::Battle:
	{
		// Long locks: two-beat camera. Beat 1 sits tight on the target (who's about to get hit), then whips
		// back to the shooter's shoulder for the release.
		const bool bTwoBeat = Lock && G->IsLongLock();
		if (bTwoBeat && T < ZoogiStageTune::TargetBeat)
		{
			PIdx = PTargetCU;
			FVector D = TL - SL; D.Z = 0.f; D = D.GetSafeNormal();
			WantYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + TargetCUYaw;
			WantPivot = TL;
			WantLook = TL + FVector(0, 0, 10.f);
			DistMul = 1.f - 0.12f * SmoothStep(T / ZoogiStageTune::TargetBeat); // slow push-in
			ProbeFrom = TL + FVector(0, 0, 60.f);
			break;
		}
		PIdx = PBattle;
		const float TB = bTwoBeat ? T - ZoogiStageTune::TargetBeat : T;
		const float S = FMath::Clamp(TB / ZoogiStageTune::BattleSwing, 0.f, 1.f);
		const float Mid = FMath::Sin(PI * S);
		DistMul = 1.f - SwingDip * Mid;
		PitchOff = -SwingPitchDip * Mid;
		FVector Dir2 = Lock ? (TL - SL) : FRotator(0.f, Shooter ? Shooter->AimYaw : Yaw, 0.f).Vector();
		Dir2.Z = 0.f;
		Dir2 = Dir2.GetSafeNormal();
		WantYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir2.Y, Dir2.X));
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir2);
		WantPivot = SL + Right * ShoulderOffset;
		// Long shots: pull the look point back toward the shooter so the shooter's shoulder stays in frame.
		const float LockDist = Lock ? FVector::Dist2D(SL, TL) : 0.f;
		const float Far = FMath::Clamp((LockDist - 700.f) / 1400.f, 0.f, 1.f);
		const float Bias = FMath::Lerp(BattleLookBias, BattleLookBiasFar, Far);
		// Long locks: rise and pull back a little so the shooter and a distant target both read.
		PitchOff += BattleFarPitch * Far;
		DistMul *= 1.f + BattleFarDist * Far;
		WantLook = Lock ? FMath::Lerp(SL, TL, Bias) + FVector(0, 0, 15.f) : SL + Dir2 * 700.f;
		if (Lock)
		{
			const float CamToTarget = FVector::Dist2D(SL, TL) + Profiles[PBattle].Dist;
			WantFov = FMath::Clamp(2.f * FMath::RadiansToDegrees(FMath::Atan2(BattleFovFrame, CamToTarget)), BattleFovMin, Profiles[PBattle].Fov);
			// Widen if the shooter (bottom) and the target (top) can't both fit inside the HUD-safe band.
			int32 VX = 16, VY = 9;
			GetViewportSize(VX, VY);
			const float Aspect = VY > 0 ? (float)VX / (float)VY : 16.f / 9.f;
			const FVector CL = Cam->GetActorLocation();
			const float SP = (SL - FVector(0, 0, ZoogiConst::ZoogiRadius) - CL).Rotation().Pitch;
			const float TP = (TL + FVector(0, 0, 2.f * ZoogiConst::ZoogiRadius) - CL).Rotation().Pitch;
			const float Span = TP - SP;
			if (Span > 0.f)
			{
				const float VNeed = Span / (BattleShooterMaxDrop + BattleTargetMaxRise);
				const float HNeed = 2.f * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VNeed)) * Aspect));
				WantFov = FMath::Max(WantFov, FMath::Min(HNeed, BattleFovMax));
			}
		}
		break;
	}
	case EZoogiStage::End:
	default:
	{
		const bool bGone = !Shooter || Shooter->bOut || Shooter->BallLocation().Z < -600.f;
		if (T < FollowTime && !bGone)
		{
			PIdx = PFollow;
			const FVector V = Shooter->GetBallVelocity();
			const FVector2D V2(V.X, V.Y);
			WantYaw = Yaw;
			if (V2.Size() > 50.f)
			{
				const float Want = FMath::RadiansToDegrees(FMath::Atan2(V2.Y, V2.X));
				const float Delta = FRotator::NormalizeAxis(Want - Yaw);
				if (FMath::Abs(Delta) < 170.f) WantYaw = Yaw + FMath::Clamp(Delta * (1.f - FMath::Exp(-FollowYawRate * Dt)), -FollowYawCap * Dt, FollowYawCap * Dt);
			}
			WantPivot = SL;
			WantLook = SL;
		}
		else
		{
			PIdx = PEnd;
			WantYaw = Yaw;
			WantPivot = FVector(0, 0, 0);
			WantLook = FVector(0, 0, 0);
			DistMul = Scale;
			if (G->Arena && G->Arena->IsNeon())
			{
				// Rectangular court: square up so the long side runs across the screen, and come in closer to fill it.
				WantYaw = FMath::Abs(FRotator::NormalizeAxis(Yaw - 90.f)) <= 90.f ? 90.f : -90.f;
				DistMul = NeonEndScale;
			}
			ProbeFrom = FVector(0, 0, 300.f);
		}
		break;
	}
	}

	const FProfile& P = Profiles[PIdx];
	// Profile change: snapshot the current pose and blend to the new profile over P.Blend seconds
	// (time-based ease, so every phase change lands in ~0.4-0.6 s regardless of the profile's tracking lags).
	if (PIdx != CurProfile)
	{
		BlendDur = (CurProfile == PTargetCU && PIdx == PBattle) ? WhipBackBlend : Profiles[PIdx].Blend;
		CurProfile = PIdx;
		ProfileAge = 0.f;
		BlendFrom.Yaw = Yaw; BlendFrom.Pitch = Pitch; BlendFrom.Dist = Dist; BlendFrom.Fov = Fov; BlendFrom.Dutch = Dutch; BlendFrom.Pivot = Pivot; BlendFrom.Look = Look;
	}
	ProfileAge += Dt;
	const float WantPitch = P.Pitch + PitchOff;
	const float WantDist = P.Dist * DistMul;
	const float WantFovF = WantFov > 0.f ? WantFov : P.Fov;
	const bool bBlending = ProfileAge < BlendDur;
	// Yaw: spring (underdamped in Battle = arc sweep with overshoot and settle; whip on switches).
	float W = P.YawFreq, Z = P.YawZeta;
	if (G->Stage == EZoogiStage::Main && bWhip && SwingAge < WhipTime) { W = WhipFreq; Z = WhipZeta; }
	if (bBlending && PIdx != PBattle)
	{
		// Battle keeps its spring arc swing; every other profile eases in on the clock.
		const float A = BlendCurve(PIdx, ProfileAge / BlendDur);
		Yaw = FRotator::NormalizeAxis(BlendFrom.Yaw + FRotator::NormalizeAxis(WantYaw - BlendFrom.Yaw) * A);
		YawVel = 0.f;
	}
	else if (PIdx == PFollow || PIdx == PEnd)
	{
		Yaw = FRotator::NormalizeAxis(WantYaw);
		YawVel = 0.f;
	}
	else
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dt / (1.f / 120.f)));
		const float H = Dt / Steps;
		for (int32 i = 0; i < Steps; ++i)
		{
			const float Delta = FRotator::NormalizeAxis(WantYaw - Yaw);
			YawVel += (W * W * Delta - 2.f * Z * W * YawVel) * H;
			Yaw = FRotator::NormalizeAxis(Yaw + YawVel * H);
		}
	}
	if (bBlending)
	{
		const float A = BlendCurve(PIdx, ProfileAge / BlendDur);
		Pitch = FMath::Lerp(BlendFrom.Pitch, WantPitch, A);
		Dist = FMath::Lerp(BlendFrom.Dist, WantDist, A);
		Dutch = FMath::Lerp(BlendFrom.Dutch, P.Dutch, A);
		Fov = FMath::Lerp(BlendFrom.Fov, WantFovF, A);
		Pivot = FMath::Lerp(BlendFrom.Pivot, WantPivot, A);
		Look = FMath::Lerp(BlendFrom.Look, WantLook, A);
	}
	else
	{
		Pitch = Damp(Pitch, WantPitch, P.BlendLag, Dt);
		Dist = Damp(Dist, WantDist, P.BlendLag, Dt);
		Dutch = Damp(Dutch, P.Dutch, P.BlendLag, Dt);
		Fov = Damp(Fov, WantFovF, P.BlendLag, Dt);
		Pivot = Damp(Pivot, WantPivot, P.PosLag, Dt);
		Look = Damp(Look, WantLook, P.PosLag, Dt);
	}

	const FRotator Heading(0.f, Yaw, 0.f);
	const float Pr = FMath::DegreesToRadians(Pitch);
	const FVector Desired = Pivot - Heading.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));
	const FVector Loc = ProbeCamera(ProbeFrom, Desired, Dt);

	FRotator Rot = (Look - Loc).Rotation();
	Rot.Roll = Dutch;
	// Battle framing guard: keep the shooter's silhouette above the letterbox / bottom HUD band by tilting
	// down just enough, without pushing the target out of the top of the frame.
	{
		float Want = 0.f;
		if (PIdx == PBattle && Shooter)
		{
			int32 VX = 16, VY = 9;
			GetViewportSize(VX, VY);
			const float Aspect = VY > 0 ? (float)VX / (float)VY : 16.f / 9.f;
			const float VHalf = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)) / Aspect));
			const float ShooterLow = (SL - FVector(0, 0, ZoogiConst::ZoogiRadius) - Loc).Rotation().Pitch;
			const float Rel = ShooterLow - Rot.Pitch; // negative = below the centre line
			const float Limit = -VHalf * BattleShooterMaxDrop;
			if (Rel < Limit)
			{
				Want = Rel - Limit; // negative: tilt down by this much
				if (Lock)
				{
					// Don't let the target's top leave the frame.
					const float TargetTop = (TL + FVector(0, 0, 2.f * ZoogiConst::ZoogiRadius) - Loc).Rotation().Pitch - Rot.Pitch;
					const float Room = VHalf * BattleTargetMaxRise - TargetTop; // stay below the top HUD band // how far we can tilt down before the target exits
					Want = FMath::Max(Want, -FMath::Max(Room, 0.f));
				}
			}
		}
		FrameFix = Damp(FrameFix, Want, 10.f, Dt);
		Rot.Pitch += FrameFix;
	}
	// Lock impulse shake (Battle).
	if (LockPunchAge < 2.f)
	{
		const float A = ShakeDeg * FMath::Exp(-ShakeDecay * LockPunchAge);
		Rot.Pitch += A * FMath::Sin(LockPunchAge * 47.f);
		Rot.Yaw += A * FMath::Sin(LockPunchAge * 31.f + 1.3f);
	}
	if (ImpactAge < 1.5f)
	{
		const float A = ImpactShakeDeg * ImpactStrength * FMath::Exp(-ImpactShakeDecay * ImpactAge);
		Rot.Pitch += A * FMath::Sin(ImpactAge * 53.f);
		Rot.Yaw += A * FMath::Sin(ImpactAge * 37.f + 0.7f);
	}
	float FinalFov = Fov;
	if (ImpactAge < 0.3f) FinalFov -= ImpactFovPunch * ImpactStrength * FMath::Sin(PI * ImpactAge / 0.3f);
	if (LockPunchAge < LockFovPunchTime) FinalFov -= LockFovPunch * FMath::Sin(PI * LockPunchAge / LockFovPunchTime);
	if (FireKickAge < 2.f) FinalFov += FireFovKick * FMath::Exp(-FireKickDecay * FireKickAge) * FMath::Min(1.f, FireKickAge * 12.f);
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation(Rot);
	Cam->GetCameraComponent()->SetFieldOfView(FMath::Clamp(FinalFov, 30.f, 120.f));
	ApplyPostProcess(Dt, PIdx, Lock ? FVector::Dist(Loc, TL) : FVector::Dist(Loc, Look));
}
