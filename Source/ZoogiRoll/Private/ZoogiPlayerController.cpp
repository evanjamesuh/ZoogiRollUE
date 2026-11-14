#include "ZoogiPlayerController.h"
#include "ZoogiWallCine.h" // b230
#include "ZoogiGattling.h"
#include "ZoogiGuide.h"
#include "ZoogiDemoReel.h" // b213
#include "ZoogiSkills.h"
#include "ZoogiRedirect.h"
#include "ZoogiStandForms.h"
#include "ZoogiFrozenFun.h"
#include "ZoogiActionFun.h"

#include "ZoogiPortraitCam.h"
#include "ZoogiCelebPip.h"

#include "ZoogiStageForming.h"

#include "ZoogiArena.h"

#include "ZoogiRoll.h"

#include "ZoogiGameMode.h"

#include "ZoogiDropIntro.h"

#include "ZoogiBall.h"

#include "ZoogiOrb.h"

#include "ZoogiClone.h"

#include "ZoogiArena.h"

#include "ZoogiHUD.h"

#include "ZoogiPhoneControls.h"

#include "ZoogiTargetMarker.h"

#include "ZoogiAnimeSubsystem.h"

#include "ZoogiAwakenWidget.h"

#include "ZoogiArtifactComponent.h"

#include "Framework/Application/SlateApplication.h"

#include "Widgets/SViewport.h"

#include "Camera/CameraActor.h"

#include "Camera/CameraComponent.h"

#include "Engine/World.h"

#include "InputCoreTypes.h"

#include "Kismet/GameplayStatics.h"

#include "Misc/CommandLine.h"

#include "Misc/Parse.h"

#include "Engine/GameViewportClient.h"

#include "ZoogiFramedLayout.h"

#include "ZoogiHaptics.h"

#include "ZoogiFeel.h"

#include "ZoogiCardSubsystem.h"

#include "ZoogiSpawnElevator.h"

#include "ZoogiAimArrow.h"

#include "UnrealClient.h"

#include "Misc/Paths.h"

#include "ZoogiAimView.h"

#include "ZoogiLaunchPadCam.h"

#include "ZoogiLaunchPad.h"

#include "ZoogiQualityCam.h"
#include "ZoogiOcclusionFade.h"

#include "ZoogiCamConfig.h"
#include "ZoogiP5Command.h"
#include "ZoogiClassicPad.h"
#include "ZoogiApplePad.h"
#include "ZoogiFront.h"
#include "ZoogiWolfPack.h"
#include "ZoogiSkillLab.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiArcCam.h"
#include "ZoogiSkyDiveCam.h"
#include "ZoogiBossAttack.h"
#include "ZoogiPhysics.h"
#include "Components/SphereComponent.h"
#include "ZoogiPauseOrbit.h"
#include "ZoogiShowtime.h"
#include "ZoogiEpisodeCameraDirector.h" // b187 CAMGUARD

void AZoogiPlayerController::RestoreMatchCam(float Blend) { AActor* MatchCam = Cam.Get(); if (MatchCam && GetViewTarget() != MatchCam) SetViewTargetWithBlend(MatchCam, Blend); } // b98b
static float GDragCamYaw = 0.f; // Nov 1 b53: camera yaw at the slingshot press (drag-anywhere aim)



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



	enum EProfile { PDraw, PStandby, PMain, PBattle, PEnd, PFollow, PMenu, PTargetCU, PTactical, PAnime, PCount };



	static FProfile Profiles[PCount] = { // Oct 27: not const - the Tactical row takes the Camera Configuration Manager's values each frame

		// Name       Pitch  Dist   FOV   Dutch PosLag Blend YawW  Zeta  Vign  DOF   Sat   Contr  Gain                                 BlendIn

		{ TEXT("Draw"),     7.f,  420.f, 102.f, 0.f, 5.f,  6.f,  5.f, 1.0f, 0.40f, 0.40f, 1.10f, 1.04f, FLinearColor(1.12f, 1.00f, 0.86f), 0.55f }, // (1) hero shot: low side, warm rim lift, wide

		{ TEXT("Standby"), 66.f, 3500.f, 78.f, 0.f, 2.f,  5.f,  3.f, 1.0f, 0.22f, 0.0f,  0.94f, 0.98f, FLinearColor(1.00f, 1.00f, 1.00f), 0.55f }, // (2) calm overview: high, far, neutral soft

		{ TEXT("Main"),    37.f, 1150.f, 84.f, 0.f, 9.f,  7.f,  9.f, 0.85f, 0.15f, 0.0f, 1.00f, 1.02f, FLinearColor(1.00f, 1.00f, 1.00f), 0.45f }, // (3) tactical OTS: clean, readable

		{ TEXT("Battle"),  24.f,  680.f, 72.f, 4.f, 10.f, 7.f,  7.5f, 0.55f, 0.70f, 1.0f, 1.20f, 1.12f, FLinearColor(1.05f, 0.99f, 0.96f), 0.50f }, // (4) dramatic lock-on

		{ TEXT("End"),     83.f, 4300.f, 74.f, 0.f, 1.5f, 1.6f, 3.f, 1.0f, 0.18f, 0.0f,  0.78f, 1.00f, FLinearColor(0.90f, 0.97f, 1.10f), 0.60f }, // (5) aerial resolution: top-down, cool, desaturated

		{ TEXT("Follow"),  55.f, 2400.f, 90.f, 0.f, 2.5f, 3.f,  3.f, 1.0f, 0.25f, 0.0f,  1.05f, 1.02f, FLinearColor(1.00f, 1.00f, 1.00f), 0.40f }, // shot follow (first part of End)

		{ TEXT("Menu"),    38.f, 3600.f, 90.f, 0.f, 2.f,  1.f,  2.f, 1.0f, 0.20f, 0.0f,  1.00f, 1.00f, FLinearColor(1.00f, 1.00f, 1.00f), 0.60f },

		{ TEXT("TargetCU"),  9.f,  640.f, 40.f, -5.f, 14.f, 9.f, 12.f, 0.9f, 0.80f, 1.0f, 1.25f, 1.16f, FLinearColor(1.10f, 0.96f, 0.92f), 0.28f }, // Battle beat 1 (long locks): tight on the target
		{ TEXT("Tactical"), 40.f, 2000.f, 78.f, 0.f, 2.0f, 4.f,  3.f, 1.0f, 0.18f, 0.0f,  1.00f, 1.00f, FLinearColor(1.00f, 1.00f, 1.00f), 0.50f }, // teaser-bible: soft high chase ~45-60 teaser-lower: match teaser f_018/f_024 lower+closer 40/2000/78/PosLag2 (was 55/2700/86/2.5; Oct4 2300/80/3)
		{ TEXT("Anime"),    12.f,  520.f, 50.f, -6.f, 12.f, 10.f, 10.f, 0.9f, 0.60f, 1.0f, 1.30f, 1.15f, FLinearColor(1.08f, 1.00f, 0.95f), 0.18f }, // anime placeholder close shot

	};

	// Tactical camera emphasis (fractions of the Tactical boom).

	constexpr float TacDrawZoom = 0.78f;      // Draw / Awaken: slight zoom on the active Zoogi

	constexpr float TacAimLook = 0.4f;        // Aim: look between the shooter (0) and the aim point (1)

	constexpr float TacPowerZoom = 0.86f;     // PowerUp: small push-in

	constexpr float TacRollTrack = 0.8f;      // Rolling: track the shooter this much (rest = arena centre; teaser-lower: 0.55->0.8, teaser follows the marble)

	constexpr float TacResolveDist = 1.18f;   // Resolve / End: auto-return to the whole-arena view

	// Oct 27 Phase 3: classic strategic zoom while the shot rolls (fractions of the Tactical boom / degrees / seconds)

	constexpr float StratZoomDist = 1.45f;

	constexpr float StratZoomCentre = 0.3f;     // pivot slides this far toward the arena centre

	constexpr float StratZoomPitch = 10.f;      // extra look-down

	constexpr float StratZoomIn = 0.5f;

	constexpr float TacShake = 0.4f;          // shake scale vs cinematic

	constexpr float TacFovPunch = 3.f;



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
	// Oct 29 (Evan, build 19): cinematic aim follows the drag arrow (ECine::Main). -ZoogiNoCineDrag = the old fixed view.
	inline bool CineDragOn() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCineDrag")); return b; }
	inline bool CineDragTest() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCineDragTest")); return b; }
	inline float CineDragTestPull(float T) { return T < 0.8f ? 0.f : T < 1.8f ? T - 0.8f : T < 2.6f ? 1.f : T < 3.6f ? 3.6f - T : 0.f; }

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

	// Oct 27 (iPhone): no stock UE virtual joysticks - the game draws and reads its own touch controls (they covered the

	// right side and swallowed JUMP / TRANSFORM taps). Config: DefaultInput.ini DefaultTouchInterface=None.

	ActivateTouchInterface(nullptr);

	ZoogiPortrait::SetModeOverride(-1); // portrait camera: auto-detect until SetPortraitCameraMode

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

	bUiTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUiTest"));

	bHumanTest = bUiTest || FParse::Param(FCommandLine::Get(), TEXT("ZoogiHumanTest"));

	bPinchTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPinchTest"));

	bCamStuckTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCamStuckTest"));

	bArrowTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiArrowTest"));



	// Mobile: phone ability / Awaken orb taps (Notify* or HitTest -> MenuAction).

	ZoogiPhoneControls::AbilityIndexDelegate().AddUObject(this, &AZoogiPlayerController::OnPhoneAbilityIndex);

	ZoogiPhoneControls::AwakenPressedDelegate().AddUObject(this, &AZoogiPlayerController::OnPhoneAwakenPressed);

}



void AZoogiPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)

{

	ZoogiPhoneControls::AbilityIndexDelegate().RemoveAll(this);

	ZoogiPhoneControls::AwakenPressedDelegate().RemoveAll(this);

	Super::EndPlay(EndPlayReason);

}



void AZoogiPlayerController::OnPhoneAbilityIndex(int32 Index0to3)

{

	if (AZoogiGameMode* G = GM()) G->MenuAction(FName(TEXT("PhoneAbility")), Index0to3);

}



void AZoogiPlayerController::OnPhoneAwakenPressed()

{

	if (AZoogiGameMode* G = GM()) G->MenuAction(FName(TEXT("PhoneAwaken")), 0);

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
	static const bool bP5CmdTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CmdTest"));
	if (bP5CmdTest && ZoogiP5Cmd::Showing()) { SimClock = 0.f; return; } // -ZoogiP5CmdTest: the scripted command picks run before the auto shot

	if (ZoogiCamTune::CineDragTest() && G->IsAimStep() && CineTestShot < 4) return; // -ZoogiCineDragTest: hold the shot until the scripted pull is captured

	const int32 Key = (int32)G->Stage * 10 + (G->Stage == EZoogiStage::Battle ? (int32)G->BattleStep : 0);

	if (Key != SimKey)

	{

		if (G->Stage == EZoogiStage::Draw) ++SimTurn;

		SimKey = Key;

		SimStep = 0;

	}

	SimClock += Dt;

	const float T = G->Stage == EZoogiStage::Battle ? G->StepTime : G->StageTime;

	auto Aim = [&](bool bWantBall)

	{

		// Move the virtual cursor onto the nearest orb (or rival Zoogi).

		for (AActor* A : G->TargetCandidates())

		{

			if ((Cast<AZoogiBall>(A) != nullptr) != bWantBall) continue;

			FVector2D S;

			if (ProjectWorldLocationToScreen(ActorCenter(A), S)) { SimMouse = S; return true; }

		}

		return false;

	};

	if (G->Stage == EZoogiStage::Awaken)

	{

		// Cycle through the HUD actions like clicking the buttons: Save x3 (fills the hand, third one discards),

		// Guard with saved card 1, then Infuse the drawn card.

		if (SimStep == 0 && T > 1.3f)

		{

			const int32 Mode = SimTurn % 5;

			if (Mode >= 1 && Mode <= 3) G->MenuAction(TEXT("Save"), -1);

			else if (Mode == 4) G->MenuAction(TEXT("Guard"), 1);

			else G->MenuAction(TEXT("Infuse"), -1);

			SimStep = 1;

		}

	}

	else if (G->IsAimStep())

	{

		const bool bFloorTurn = SimTurn % 2 == 0;

		if (SimStep == 0 && T > 0.5f)

		{

			if (bFloorTurn)

			{

				// Floor click: a point toward the arena centre (free aim, stops on the point).

				if (AZoogiBall* Me = G->GetActiveBall())

				{

					FVector L = Me->BallLocation();

					FVector Dir = FVector(-L.X, -L.Y, 0.f).GetSafeNormal();

					if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);

					FVector2D S;

					if (ProjectWorldLocationToScreen(L + Dir * 700.f - FVector(0, 0, ZoogiConst::ZoogiRadius), S)) SimMouse = S;

				}

			}

			else if (!Aim(true)) Aim(false);

			bSimClick = true;

			SimStep = 1;

		}

		else if (SimStep == 1 && T > 1.1f) { G->MenuAction(TEXT("AimNudge"), bFloorTurn ? -2 : 3); SimStep = 2; } // cut nudges

		else if (SimStep == 2 && T > 1.4f) { if (!bFloorTurn) G->MenuAction(TEXT("AimPower"), 3); SimStep = 3; }

		else if (SimStep == 3 && T > 2.0f) { bSimSpace = true; SimStep = 4; }                                       // confirm

	}

}



// ------------------------------------------------------------------ scripted UMG test (-ZoogiUiTest)



void AZoogiPlayerController::UiCheck(bool bOk, const FString& What)

{

	(bOk ? UiPass : UiFail)++;

	UE_LOG(LogZoogi, Log, TEXT("UITEST %s: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What);

}



void AZoogiPlayerController::UpdateUiTest(float Dt)

{

	bSimClick = bSimTab = bSimSpace = false;

	AZoogiGameMode* G = GM();

	if (!G) return;

	// The confirm check runs after the shot is away (P1 is no longer aiming by then).

	if (UiConfirmFrame && GFrameCounter > UiConfirmFrame + 10)

	{

		// CommitLock leaves the Aim step for the Power-up beat; the shot itself fires a moment later.

		UiCheck(!G->IsAimStep() || ShotsSeen > UiShotSnap, FString::Printf(TEXT("confirm via %s left the Aim step (BattleStep=%d, phase=%d)"), UiAct == TEXT("mouse") ? TEXT("drag release (mouse)") : TEXT("Space (key)"), (int32)G->BattleStep, (int32)G->Phase));

		UiConfirmFrame = 0;

	}

	if (!G->IsHumanAiming()) { SimStep = 0; return; }

	AZoogiHUD* Hud = Cast<AZoogiHUD>(GetHUD());

	UZoogiAwakenWidget* W = Hud ? Hud->GetAwakenWidget() : nullptr;

	if (!W) return;

	W->TickQueuedClick();

	const int32 Key = (int32)G->Stage * 10 + (G->Stage == EZoogiStage::Battle ? (int32)G->BattleStep : 0);

	if (Key != SimKey)

	{

		if (G->Stage == EZoogiStage::Draw) ++SimTurn;

		SimKey = Key;

		SimStep = 0;

	}

	const float T = G->Stage == EZoogiStage::Battle ? G->StepTime : G->StageTime;

	const bool bMouse = (SimTurn % 2) == 1;

	const TCHAR* Via = bMouse ? TEXT("mouse") : TEXT("key");



	auto ClickCtl = [&](FName A, int32 I) -> bool

	{

		FVector2D Abs, VP;

		if (!W->FindControl(A, I, Abs)) { UiCheck(false, FString::Printf(TEXT("UMG control %s %d is on screen"), *A.ToString(), I)); return false; }

		if (W->AbsToViewport(Abs, VP)) SimMouse = VP;

		UE_LOG(LogZoogi, Log, TEXT("UITEST mouse click on %s %d at viewport (%.0f, %.0f), desktop (%.0f, %.0f)"), *A.ToString(), I, SimMouse.X, SimMouse.Y, Abs.X, Abs.Y);

		W->QueueControlClick(A, I);

		return true;

	};

	auto HoverCtl = [&](FName A, int32 I)

	{

		FVector2D Abs;

		if (W->FindControl(A, I, Abs)) W->SynthHover(Abs);

	};

	auto PressKey = [&](const FKey& K)

	{

		if (!FSlateApplication::IsInitialized()) return;

		FKeyEvent E(K, FModifierKeysState(), 0, false, 0, 0);

		FSlateApplication::Get().ProcessKeyDownEvent(E);

		FSlateApplication::Get().ProcessKeyUpEvent(E);

		UE_LOG(LogZoogi, Log, TEXT("UITEST key %s"), *K.ToString());

	};

	auto BackgroundClick = [&](bool bAimBar, const FVector2D& Frac)

	{

		FVector2D Abs, VP;

		if (!W->PanelPoint(bAimBar, Frac, Abs)) { UiCheck(false, TEXT("panel on screen for the background click")); return false; }

		if (W->AbsToViewport(Abs, VP)) SimMouse = VP; // a leak would floor-aim right here

		UiSnapFrame = GFrameCounter;

		UE_LOG(LogZoogi, Log, TEXT("UITEST mouse click on %s background at viewport (%.0f, %.0f)"), bAimBar ? TEXT("aim bar") : TEXT("Awaken panel"), SimMouse.X, SimMouse.Y);

		W->QueueClick(Abs, bAimBar ? TEXT("aim bar background") : TEXT("Awaken panel background"));

		return true;

	};

	auto CardKey = [](int32 Card) { return Card == 0 ? EKeys::One : (Card == 1 ? EKeys::Two : EKeys::Three); };



	if (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone)

	{

		static const TCHAR* Seq[7] = { TEXT("Save"), TEXT("Save"), TEXT("Guard"), TEXT("Infuse"), TEXT("Save"), TEXT("Infuse"), TEXT("Guard") };

		const FName Act = Seq[(FMath::Max(SimTurn, 1) - 1) % 7];

		AZoogiBall* Me = G->GetActiveBall();

		const int32 HandN = Me && Me->Artifact ? Me->Artifact->Hand.Num() : 0;

		const bool bDrawn = Me && Me->Artifact && Me->Artifact->Drawn != EZoogiArtifact::None;

		if (SimStep == 0 && T > 1.0f)

		{

			UiWantPick = (Act == TEXT("Infuse") && bDrawn) ? 0 : (HandN > 0 ? 1 : 0);

			if (!bDrawn && UiWantPick == 0 && HandN > 0) UiWantPick = 1;

			UiPickClickFrame = GFrameCounter;

			if (bMouse) ClickCtl(TEXT("AwakenPick"), UiWantPick);

			else PressKey(CardKey(UiWantPick));

			SimStep = 1;

		}

		else if (SimStep == 1 && T > 1.3f)

		{

			// Mouse: the card button's OnClicked itself must have fired (the drawn card is pre-selected, so AwakenPick alone

			// can pass without any click landing).

			const bool bFired = !bMouse || W->FiredSince(UiPickClickFrame, TEXT("AwakenPick"), UiWantPick);

			UiCheck(G->AwakenPick == UiWantPick && bFired, FString::Printf(TEXT("turn %d: pick card %d via %s (AwakenPick=%d, OnClicked %s)"), SimTurn, UiWantPick + 1, Via, G->AwakenPick, bMouse ? (bFired ? TEXT("fired") : TEXT("did NOT fire")) : TEXT("n/a")));

			if (bMouse) G->TakeShot(FString::Printf(TEXT("umg_awaken_t%d"), SimTurn));

			UiSnapPick = G->AwakenPick;

			if (bMouse) BackgroundClick(false, FVector2D(0.006f, 0.5f)); // left margin, beside the first card

			SimStep = 2;

		}

		else if (SimStep == 2 && T > 1.55f)

		{

			if (bMouse)

			{

				UiCheck(LastRawLMBFrame < UiSnapFrame, TEXT("Awaken panel background press swallowed by Slate (no LMB reached the game)"));

				UiCheck(G->Stage == EZoogiStage::Awaken && !G->bAwakenDone && G->AwakenPick == UiSnapPick, TEXT("Awaken panel background press changed nothing"));

				if (!bUiControlDone)

				{

					// Control: a press on open viewport (top middle, no UI there) does reach the game.

					if (UGameViewportClient* VC = GetWorld()->GetGameViewport())

						if (TSharedPtr<SViewport> VW = VC->GetGameViewportWidget())

						{

							const FGeometry& Geo = VW->GetCachedGeometry();

							UiSnapFrame = GFrameCounter;

							W->QueueClick(Geo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.42f)), TEXT("other (SViewport)"));

							UE_LOG(LogZoogi, Log, TEXT("UITEST control click on open viewport"));

						}

				}

				HoverCtl(Act, G->AwakenPick < 0 ? 0 : (Act == TEXT("Save") ? 0 : G->AwakenPick));

			}

			SimStep = 3;

		}

		else if (SimStep == 3 && T > 1.85f)

		{

			if (bMouse && !bUiControlDone)

			{

				UiCheck(LastRawLMBFrame >= UiSnapFrame, TEXT("control: a press on open viewport does reach the game (the leak detector works)"));

				bUiControlDone = true;

			}

			if (bMouse) G->TakeShot(FString::Printf(TEXT("umg_awaken_hover_t%d"), SimTurn));

			SimStep = 4;

		}

		else if (SimStep == 4 && T > 2.0f)

		{

			UiAct = Act;

			if (bMouse) ClickCtl(Act, Act == TEXT("Save") ? 0 : G->AwakenPick);

			else PressKey(Act == TEXT("Save") ? EKeys::S : (Act == TEXT("Guard") ? EKeys::G : EKeys::I));

			SimStep = 5;

		}

		return;

	}

	if (G->Stage == EZoogiStage::Awaken && G->bAwakenDone && SimStep == 5)

	{

		UiCheck(G->AwakenChoiceText.StartsWith(UiAct.ToString().ToUpper()), FString::Printf(TEXT("turn %d: %s via %s -> \"%s\""), SimTurn, *UiAct.ToString(), Via, *G->AwakenChoiceText));

		SimStep = 6;

		return;

	}

	if (!G->IsAimStep()) return;



	if (SimStep == 0 && T > 0.6f)

	{

		// Oct 14: no aim bar any more (slingshot drag aim) - nothing to background-click

		SimStep = 1;

	}

	else if (SimStep == 1 && T > 0.9f)

	{

		if (bMouse)

		{

			UiCheck(!G->HasAim() && !G->bFreeAim, TEXT("no aim before the world click"));

			// Lock through the world pick (the same targeting code a real click uses).

			bool bFound = false;

			for (AActor* A : G->TargetCandidates())

			{

				FVector2D S;

				if (ProjectWorldLocationToScreen(ActorCenter(A), S) && !(Hud && Hud->IsOverUI(S))) { SimMouse = S; bFound = true; break; }

			}

			if (bFound) bSimClick = true;

		}

		else PressKey(EKeys::Tab);

		SimStep = 2;

	}

	else if (SimStep == 2 && T > 1.3f)

	{

		UiCheck(G->HasAim(), FString::Printf(TEXT("turn %d: lock via %s"), SimTurn, bMouse ? TEXT("world click") : TEXT("Tab (key)")));

		UiSnapCut = G->AimCutDeg;

		PressKey(EKeys::Right); // the << < > >> buttons are gone; nudge stays on the keys

		SimStep = 3;

	}

	else if (SimStep == 3 && T > 1.6f)

	{

		UiCheck(FMath::Abs(G->AimCutDeg - UiSnapCut) > 0.5f, FString::Printf(TEXT("nudge via %s: cut %.1f -> %.1f deg"), TEXT("Right key"), UiSnapCut, G->AimCutDeg));

		UiSnapPower = G->AimPowerStep;

		if (bMouse) G->HumanDrag(0.9f, false); // drag-back pull (the OS-level drag is covered by ui.ps1 drag)

		else PressKey(EKeys::Up);

		SimStep = 4;

	}

	else if (SimStep == 4 && T > 1.9f)

	{

		const int32 Want = bMouse ? 3 : FMath::Min(UiSnapPower + 1, 3);

		if (bMouse) UiCheck(G->HumanDragFrac > 0.85f, FString::Printf(TEXT("power via drag pull: %.0f%%"), G->HumanDragFrac * 100.f));

		else UiCheck(G->AimPowerStep == Want, FString::Printf(TEXT("power via Up key: step %d -> %d"), UiSnapPower, G->AimPowerStep));

		SimStep = 5;

	}

	else if (SimStep == 5 && T > 2.15f)

	{

		if (bMouse) G->TakeShot(FString::Printf(TEXT("umg_aim_t%d"), SimTurn));

		SimStep = 6;

	}

	else if (SimStep == 6 && T > 2.4f)

	{

		UiShotSnap = ShotsSeen;

		UiAct = bMouse ? TEXT("mouse") : TEXT("key");

		if (bMouse) G->HumanDrag(0.9f, true); // drag release launches

		else PressKey(EKeys::SpaceBar);

		UiConfirmFrame = GFrameCounter;

		SimStep = 7;

	}

}



// ------------------------------------------------------------------ turn hooks



void AZoogiPlayerController::OnTurnStarted(AZoogiBall* Ball)

{

	CamTarget = Ball;

	const bool bFromMenu = bMenuCam;

	bMenuCam = false;

	HoverTarget.Reset();

	AZoogiGameMode* G = GM();

	ArrowGesture.Cancel(); bArrowCamHold = false; // aim-arrow

	if (G) G->bArrowAim = false;

	if (Ball) TacYaw = Ball->AimYaw;

	if (Ball && G && !G->bCinematicCam && !bFromMenu)

	{

		DrawBaseYaw = Ball->AimYaw; // tactical: no hard cut, the camera glides to the next shooter

	}

	else if (Ball)

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

	if (Stage == EZoogiStage::Awaken) StandbyYaw = Yaw;

	if (Stage == EZoogiStage::End || Stage == EZoogiStage::Resolve) EndSlowMo();

}



void AZoogiPlayerController::OnBattleStep(EZoogiBattleStep Step)

{

	AZoogiGameMode* G = GM();

	if (!G) return;

	if (Step == EZoogiBattleStep::PowerUp)

	{

		SwingAge = 0.f;

		bWhip = false;

		LockPunchAge = 0.f;

		if (G->bCinematicCam && G->AllAtRestPublic() && ZoogiUtil::ReactionPauses())

		{

			// Slow-mo dip on the lock moment: everything is at rest, so physics outcomes are unaffected.

			// Oct 24 (Evan): lock slow-mo dip removed (physics runs naturally)

		}

	}

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

	AZoogiGameMode* G = GM();

	if (G && !G->bCinematicCam) { ImpactStrength *= TacShake; return; } // tactical: light shake, no hit-stop for normal hits

	if (!ZoogiUtil::ReactionPauses()) return; // Oct 15 (Evan): no hit-stop; the shake / FOV punch above still play

	// Hit-stop: a few frames of near-freeze (uniform time scale, so the physics outcome is the same).

	// Oct 24 (Evan): impact hit-stop removed (no time dilation)

}



void AZoogiPlayerController::OnImpactFeel(float Strength)

{

	// Oct 22 quality feel: shake + FOV punch for button camera modes; no free-cam; hit-stop is separate.

	if (bMenuCam) return;

	ImpactAge = 0.f;

	ImpactStrength = FMath::Clamp(Strength, 0.f, 1.f);

	AZoogiGameMode* G = GM();

	if (G && !G->bCinematicCam) ImpactStrength *= ZoogiCamTune::TacShake; // tactical: lighter, still readable on phones

}



void AZoogiPlayerController::OnBigBlast(float Strength)
{	// b171 (Evan: "make the screen shake everytime a big blast occurs"): no tactical 0.4 cut; -ZoogiNoBlastShake; Settings CAMERA SHAKE off = none
	if (bMenuCam) return;
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBlastShake"));
	if (bOff) return;
	int32 On = 1; if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("CameraShake"), On, GGameUserSettingsIni);
	if (!On) return;
	const float K = FMath::Clamp(Strength, 0.f, 2.f);
	if (ImpactAge < 0.2f && ImpactStrength >= K) return; // already shaking at least this hard
	ImpactAge = 0.f; ImpactStrength = K;
	UE_LOG(LogZoogi, Log, TEXT("BLAST SHAKE k=%.2f"), K);
}

void AZoogiPlayerController::ExtendSlowMoUntil(double RealTimeUntil)

{

	if (RealTimeUntil > SlowMoUntil) SlowMoUntil = RealTimeUntil;

}



void AZoogiPlayerController::OnPoweredHit(bool bCrit)

{

	using namespace ZoogiCamTune;

	if (bMenuCam) return;

	ImpactAge = 0.f;

	ImpactStrength = bCrit ? 1.f : 0.7f;

	if (!ZoogiUtil::ReactionPauses()) return; // Oct 15 (Evan): no hit-stop; shake / FOV punch still play

	// Oct 24 (Evan): powered/crit hit-stop removed (no time dilation)

}



void AZoogiPlayerController::OnAnimeStarted(const FZoogiCombatEvent& E)

{

	AnimeFocus = E.Focus;

	AnimeYaw = Yaw + (E.Event == TEXT("Infuse") || E.Event == TEXT("Guard") ? 60.f : 35.f);

	AnimeDistMul = (E.Event == TEXT("Guard") || E.Event == TEXT("GuardBlock")) ? 1.7f : 1.f; // the guard bubble is big: frame it from outside

	bAnimeCam = AnimeFocus.IsValid();
	if (bAnimeCam && ZoogiCelebPip::Enabled()) { ZoogiCelebPip::Start(GetWorld(), AnimeFocus.Get(), AnimeYaw, AnimeDistMul); bAnimeCam = false; } // b74 (Evan): the close-up plays in a small window, the game view stays

	EndSlowMo();

}



void AZoogiPlayerController::OnAnimeFinished()

{

	bAnimeCam = false;
	ZoogiCelebPip::Stop(); // b74

	AnimeFocus.Reset();

}



bool AZoogiPlayerController::FloorPoint(const FVector2D& Screen, FVector& Out) const

{

	FVector O, D;

	if (!DeprojectScreenPositionToWorld(Screen.X, Screen.Y, O, D) || D.Z > -0.02f) return false;

	AZoogiGameMode* G = GM();

	const AZoogiBall* Me = G ? G->GetActiveBall() : nullptr;

	const float Z = Me ? Me->BallLocation().Z - ZoogiConst::ZoogiRadius : 0.f;

	const float T = (Z - O.Z) / D.Z;

	if (T <= 0.f) return false;

	Out = O + D * T;

	return true;

}



void AZoogiPlayerController::OnShotFired(AZoogiBall* Ball)

{

	++ShotsSeen;

	CamTarget = Ball;

	FireKickAge = 0.f;

	EndSlowMo();

}



void AZoogiPlayerController::OnGameOver(AZoogiBall* Winner)

{

	if (bUiTest) UE_LOG(LogZoogi, Log, TEXT("UITEST SUMMARY: %d passed, %d failed"), UiPass, UiFail);

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

	if (PerfectFlashT > 0.f) PerfectFlashT = FMath::Max(0.f, PerfectFlashT - Dt);

	{
		// Oct 31 b41: Apple on-screen controller while a Battle Royale fight runs (ZoogiApplePad.h)
		const AZoogiGameMode* AG = GM();
		ZoogiApplePad::Want(AG && AG->IsRoyaleMatch() && AG->Phase != EZoogiPhase::GameOver && !AG->bRoyaleMarble && AG->Royale.Step == EZoogiRoyaleStep::Fight && !ZoogiFront::RoyalePauseOpen());
	}
	if (bUiTest) UpdateUiTest(Dt);

	else if (bHumanTest && !ZoogiGuide::Active() && !ZoogiDemoReel::Holding() && !ZoogiWallCine::Active()) UpdateHumanTest(Dt); // b172e: the trainer plays the guided turn; b213: not under the demo reel

	if (bPinchTest) UpdatePinchTest(GM(), Dt);

	if (bCamStuckTest) UpdateCamStuckTest(GM(), Dt);

	if (bArrowTest) UpdateArrowTest(GM(), Dt);

	// Battle Royale: twin-stick touch + chase camera replace the turn input / camera director (ZoogiRoyaleInput.cpp)
	if (AZoogiGameMode* RG = GM(); RG && RG->IsRoyaleMatch()) { RoyaleInput(Dt); RoyaleCamera(Dt); ZoogiOcclusionFade::Tick(this, RG, Dt); UpdateMarkers(); return; }
	HandleInput(Dt);

	UpdateCamera(Dt);

	ZoogiQualityCam::Tick(this, GM(), Dt);
	ZoogiOcclusionFade::Tick(this, GM(), Dt); // Evan Oct 23: see-through walls/objects between camera and active Zoogi

	UpdateMarkers();

}



// ------------------------------------------------------------------ aim zoom mode (Oct 14, Evan 10:28 / 10:30 PM)



void AZoogiPlayerController::LoadZoomTune()

{

	if (bZoomCfg) return;

	bZoomCfg = true;

	FAimZoomTune& T = ZoomTune;

	if (GConfig)

	{

		const TCHAR* S = TEXT("ZoogiRoll.AimZoom");

		GConfig->GetBool(S, TEXT("bAutoExitOnTap"), T.bAutoExitOnTap, GGameIni);

		auto F = [&](const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };

		F(TEXT("MaxZoom"), T.MaxZoom); F(TEXT("EnterZoom"), T.EnterZoom); F(TEXT("WheelStep"), T.WheelStep); F(TEXT("KeyStep"), T.KeyStep);

		F(TEXT("PinchPower"), T.PinchPower); F(TEXT("MinPinchPx"), T.MinPinchPx); F(TEXT("EaseIn"), T.EaseIn); F(TEXT("EaseOut"), T.EaseOut);

		F(TEXT("PanEase"), T.PanEase); F(TEXT("PitchWide"), T.PitchWide); F(TEXT("PitchClose"), T.PitchClose); F(TEXT("PitchMin"), T.PitchMin);

		F(TEXT("EdgeMargin"), T.EdgeMargin); F(TEXT("FollowRadius"), T.FollowRadius); F(TEXT("FollowPad"), T.FollowPad); F(TEXT("FollowLag"), T.FollowLag); F(TEXT("MinHeight"), T.MinHeight);

	}

	T.bEnabled = false; // Oct 22: camera buttons only - no manual zoom (the ZOOM button hides: CanShowZoom is false)

	T.MaxZoom = FMath::Clamp(T.MaxZoom, 1.f, 5.f);

	T.EnterZoom = FMath::Clamp(T.EnterZoom, 1.05f, T.MaxZoom);

	T.PitchMin = FMath::Clamp(T.PitchMin, 15.f, 80.f);

	T.PitchClose = FMath::Clamp(T.PitchClose, T.PitchMin, 85.f);

	T.PitchWide = FMath::Clamp(T.PitchWide, T.PitchMin, 85.f);

	UE_LOG(LogZoogi, Log, TEXT("AIMZOOM tune: enabled=%d max=%.2fx enter=%.2fx pitch %.0f->%.0f (min %.0f) edge %.0f follow %.0f+%.0f autoexit=%d"), T.bEnabled ? 1 : 0, T.MaxZoom, T.EnterZoom,

		T.PitchWide, T.PitchClose, T.PitchMin, T.EdgeMargin, T.FollowRadius, T.FollowPad, T.bAutoExitOnTap ? 1 : 0);

}



bool AZoogiPlayerController::ZoomStageLive(const AZoogiGameMode* G)

{

	// Oct 15 playtest ("ZOOM does nothing at game start"): the zoom is live in every human phase where aiming is possible -

	// the Standby / Awaken step (from the very first turn) and the battle aim step - instead of only arming in the Standby.

	// The Draw hero beat and the anime / cinematic shots are untouched (no zoom there).

	return G && G->IsHumanAiming() && ((G->Stage == EZoogiStage::Battle && G->IsAimStep()) || (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone));

}



// Default zoom anchor height (screen fraction from the top): landscape 0.45; portrait the safe band's centre (0.475 at the

// default 5% / 10% band), so key / button zoom closes in on the play focus, not under the top controls or the thumb zone.

static float ZoomAnchorY(const APlayerController* PC)

{

	const float A = ZoogiPortrait::ViewAspect(PC);

	if (!ZoogiPortrait::IsPortrait(A)) return 0.45f;

	float T = 0.f, B = 0.f; ZoogiPortrait::Band(PC, A, T, B);

	return 0.5f - 0.5f * (B - T);

}



bool AZoogiPlayerController::CanZoom() const

{

	const AZoogiGameMode* G = GM();

	return ZoomTune.bEnabled && ZoomStageLive(G);

}



void AZoogiPlayerController::SetZoomAnchor(const FVector2D& Screen)

{

	FVector P;

	if (FloorPoint(Screen, P)) ZoomAnchor = FVector(P.X, P.Y, CourtZ);

}



void AZoogiPlayerController::ZoomAbout(float NewWant)

{

	NewWant = FMath::Clamp(NewWant, 1.f, ZoomTune.MaxZoom);

	if (NewWant <= 1.0001f) { ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector; return; }

	// homothety about the anchor: the anchor keeps its screen spot while the view scales

	const FVector Eff = Pivot + ZoomOffWant;

	const FVector NewEff = ZoomAnchor + (Eff - ZoomAnchor) * (ZoomWant / NewWant);

	ZoomOffWant = NewEff - Pivot; ZoomOffWant.Z = 0.f;

	ZoomWant = NewWant;

}



void AZoogiPlayerController::PanBy(const FVector2D& From, const FVector2D& To)

{

	FVector A, B;

	if (!FloorPoint(From, A) || !FloorPoint(To, B)) return;

	FVector D = A - B; D.Z = 0.f; // grab-and-slide: the floor under the finger follows it

	ZoomOffWant += D;

	if (PanLogN++ % 20 == 0) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM pan by (%.0f, %.0f) cm -> view offset (%.0f, %.0f)"), D.X, D.Y, ZoomOffWant.X, ZoomOffWant.Y);

}



bool AZoogiPlayerController::CanShowZoom() const

{

	const AZoogiGameMode* G = GM();

	return ZoomTune.bEnabled && G && G->IsHumanAiming() && (G->Stage == EZoogiStage::Awaken || G->IsAimStep());

}



void AZoogiPlayerController::ToggleZoomMode(const TCHAR* Why)

{

	SetZoomMode(!IsZoomShownOn(), Why);

}



void AZoogiPlayerController::SetZoomMode(bool bOn, const TCHAR* Why)

{

	LoadZoomTune();

	AZoogiGameMode* G = GM();

	if (!bOn && bZoomArmed && !bZoomMode) { bZoomArmed = false; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM disarmed (%s)"), Why); return; }

	if (bOn == bZoomMode) return;

	if (bOn && (!CanZoom() || !G))

	{

		// Awaken / standby step: the button still works - it arms the mode, which turns on as the aim step starts

		if (CanShowZoom()) { bZoomArmed = true; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM armed (%s): on when the aim step starts"), Why); }

		else UE_LOG(LogZoogi, Log, TEXT("AIMZOOM toggle ignored (%s): not aiming"), Why);

		return;

	}

	bZoomArmed = false;

	bZoomMode = bOn;

	bPanDown = false; bPanMoved = false;

	if (bZoomMode)

	{

		// drag-aim pauses in the mode

		if (bDragArmed || bPendingTap) { bDragArmed = false; bPendingTap = false; DragFrac = 0.f; UiPull = 0.f; }

		if (G && G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f;

		AActor* Lk = G && G->TargetLock.IsValid() ? G->TargetLock.Get() : nullptr;

		if (Lk) ZoomAnchor = FVector(ActorCenter(Lk).X, ActorCenter(Lk).Y, CourtZ);

		else if (G && G->bFreeAim) ZoomAnchor = FVector(G->FreeAimPoint.X, G->FreeAimPoint.Y, CourtZ);

		else { int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY); SetZoomAnchor(FVector2D(VX * 0.5f, VY * ZoomAnchorY(this))); }

		ZoomAbout(FMath::Max(ZoomWant, ZoomTune.EnterZoom));

	}

	else { ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector; }

	UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode %s (%s): zoom -> %.2fx anchor (%.0f, %.0f)"), bZoomMode ? TEXT("ON") : TEXT("OFF"), Why, ZoomWant, ZoomAnchor.X, ZoomAnchor.Y);

}



bool AZoogiPlayerController::UpdateZoomInput(AZoogiGameMode* G, bool bAimStage, const FVector2D& Mouse, float Dt)

{

	// Oct 22 (Evan): the camera is controlled ONLY by its buttons (normal / tactical / cinema). The manual input paths are gone:

	// two-finger pinch, mouse wheel, = - and Z keys, one-finger / LMB / RMB / MMB pan and the zoom tap-to-lock. The aim view's

	// rotation with the arrow is not manual camera control and is unchanged. Nothing here owns the pointers any more.

	if (bZoomMode || bZoomArmed) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM off (camera buttons only)"));

	bZoomMode = false; bZoomArmed = false; ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector;

	bPinch = false; bPinchHold = false; bPanDown = false; bPanMoved = false;

	return false;

}



void AZoogiPlayerController::UpdateCamStuckTest(AZoogiGameMode* G, float Dt)

{

	// -ZoogiCamStuckTest (no OS input): on P1's first aim step, a tap far past the edge (Evan's (-1634,-2834) on Frozen Ring),

	// then a stuck high camera (boom x4, centre 60 m off, menu orbit flag, a foreign far-above view target) held for 1.2 s;

	// expects the tap clamped to the court, the watchdog to bring the battle view back and the aim reticle to stay drawn.

	if (!G) return;

	static float AwakenT = 0.f;

	if (G->IsHumanAiming() && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone)

	{

		AwakenT += Dt;

		if (AwakenT > 1.5f) { AwakenT = 0.f; UE_LOG(LogZoogi, Log, TEXT("CAMSTUCK auto-commit P1 Awaken")); G->MenuAction(TEXT("CardCommit"), 0); }

		return;

	}

	AwakenT = 0.f;

	if (!(G->IsHumanAiming() && G->IsAimStep() && G->Stage == EZoogiStage::Battle)) { CamStuckT = -1.f; return; }

	if (CamStuckT < 0.f) CamStuckT = 0.f;

	CamStuckT += Dt;

	if (CamStuckPhase >= 9)

	{

		if (CamStuckT > 1.2f && CamStuckPhase == 9) { CamStuckPhase = 10; if (!G->HasAim()) { int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY); FVector P; if (FloorPoint(FVector2D(VX * 0.5f, VY * 0.35f), P)) G->SetFreeAim(P); } G->HumanDrag(0.6f, true); }

		if (CamStuckT < 0.5f) CamStuckPhase = 9;

		return;

	}

	const AZoogiBall* Me = G->GetActiveBall();

	if (CamStuckPhase == 0 && CamStuckT > 1.5f)

	{

		CamStuckPhase = 1;

		G->SetFreeAim(FVector(-1634.f, -2834.f, Me ? Me->BallLocation().Z : 0.f));

		const bool bIn = G->bFreeAim && !(G->Arena ? G->Arena->IsOffCourt(FVector2D(G->FreeAimPoint.X, G->FreeAimPoint.Y)) : false);

		UE_LOG(LogZoogi, Log, TEXT("CAMSTUCK %s far tap: free aim (%.0f, %.0f) on court=%d"), bIn ? TEXT("PASS") : TEXT("FAIL"), G->FreeAimPoint.X, G->FreeAimPoint.Y, bIn ? 1 : 0);

		G->TakeShot(TEXT("camstuck_0_far_tap_clamped"));

	}

	else if (CamStuckPhase == 1 && CamStuckT > 3.5f)

	{

		CamStuckPhase = 2; CamStuckFixes0 = CamWatchFixes;

		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ACameraActor* Bogus = GetWorld()->SpawnActor<ACameraActor>(FVector(0.f, 2600.f, 7000.f), FRotator(-58.f, -90.f, 0.f), SP);

		if (Bogus) { Bogus->GetCameraComponent()->SetFieldOfView(80.f); SetViewTarget(Bogus); CamStuckBogus = Bogus; }

		UE_LOG(LogZoogi, Log, TEXT("CAMSTUCK breaking the camera: boom x4, centre 60 m off, menu orbit on, foreign high view target"));

	}

	if (CamStuckPhase == 2 || CamStuckPhase == 3)

	{

		if (CamStuckT < 4.7f) { Dist = FMath::Max(Dist, 4.f * 3000.f); Pivot = FVector(0.f, -6000.f, CourtZ); Look = Pivot; bMenuCam = CamStuckT < 3.6f; } // held stuck for 1.2 s

		if (CamStuckPhase == 2 && CamStuckT > 3.9f) { CamStuckPhase = 3; G->TakeShot(TEXT("camstuck_1_broken_high_view")); }

	}

	if (CamStuckPhase == 3 && CamStuckT > 7.0f)

	{

		CamStuckPhase = 4;

		const float D = Cam ? FVector::Dist(Cam->GetActorLocation(), Look) : 0.f;

		const bool bOk = GetViewTarget() == Cam && !bMenuCam && CamWatchFixes > CamStuckFixes0 && D < 6000.f && FVector2D(Pivot.X, Pivot.Y).Size() < 4000.f && G->bFreeAim;

		UE_LOG(LogZoogi, Log, TEXT("CAMSTUCK %s restored: view=%s menuCam=%d watchdog fixes=%d boom=%.0f centre (%.0f, %.0f) aim kept=%d"), bOk ? TEXT("PASS") : TEXT("FAIL"),

			*GetNameSafe(GetViewTarget()), bMenuCam ? 1 : 0, CamWatchFixes - CamStuckFixes0, D, Pivot.X, Pivot.Y, G->bFreeAim ? 1 : 0);

		G->TakeShot(TEXT("camstuck_2_restored"));

		if (CamStuckBogus.IsValid()) CamStuckBogus->Destroy();

	}

	if (CamStuckPhase == 4 && CamStuckT > 8.0f) { CamStuckPhase = 9; CamStuckT = 0.f; }

}



void AZoogiPlayerController::UpdatePinchTest(AZoogiGameMode* G, float Dt)

{

	// -ZoogiPinchTest: retired with the pinch zoom (Oct 22, camera buttons only)

	static bool bSaid = false;

	if (!bSaid) { bSaid = true; UE_LOG(LogZoogi, Log, TEXT("PINCHTEST SKIP: pinch zoom removed (camera buttons only, Oct 22)")); }

}



// ------------------------------------------------------------------ aim arrow (Oct 17)



bool AZoogiPlayerController::ArrowScreen(FVector2D OutS[7], FVector2D& OutBallS, float& OutBallRpx) const

{

	AZoogiGameMode* G = GM();

	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;

	float AYaw = 0.f;

	if (!B || !PlayerCameraManager || !ZoogiAimArrow::CurrentYaw(G, AYaw)) return false;

	FVector W[7];

	ZoogiAimArrow::Outline(B->BallLocation(), ZoogiConst::ZoogiRadius, AYaw, ZoogiAimArrow::Tune(), W);

	for (int32 i = 0; i < 7; ++i) if (!ProjectWorldLocationToScreen(W[i], OutS[i])) return false;

	const FVector CamRight = PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();

	FVector2D SR;

	if (!ProjectWorldLocationToScreen(B->BallLocation(), OutBallS) || !ProjectWorldLocationToScreen(B->BallLocation() + CamRight * ZoogiConst::ZoogiRadius, SR)) return false;

	OutBallRpx = (SR - OutBallS).Size();

	return true;

}



bool AZoogiPlayerController::ArrowFingerDir(const FVector2D& Screen, FVector2D& OutDir) const

{

	AZoogiGameMode* G = GM();

	AZoogiBall* B = G ? G->GetActiveBall() : nullptr;

	FVector P;

	if (!B || !FloorPoint(Screen, P)) return false;

	const FVector2D D = FVector2D(P) - FVector2D(B->BallLocation());

	if (D.Size() < ZoogiConst::ZoogiRadius * 0.6f) return false; // on the ball: the angle is unstable, hold

	OutDir = D.GetSafeNormal();

	return true;

}



void AZoogiPlayerController::UpdateArrowTest(AZoogiGameMode* G, float Dt)

{

	// -ZoogiArrowTest (with -ZoogiAuto -ZoogiShots=<dir>): drives P1's aim through real touch events (InputTouch, the same

	// path a phone uses) and logs ARROWTEST PASS / FAIL + screenshots. No OS input needed.

	if (!G) return;

	if (ArrowExitT >= 0.f) { ArrowExitT += Dt; if (ArrowExitT > 1.5f) { ArrowExitT = -2.f; FGenericPlatformMisc::RequestExit(false); } return; }

	if (ArrowExitT < -1.f) return;

	auto Check = [&](bool bOk, const FString& What) { (bOk ? ArrowPass : ArrowFail)++; UE_LOG(LogZoogi, Log, TEXT("ARROWTEST %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What); };

	auto Touch = [&](ETouchType::Type Ty, const FVector2D& P) { ArrowTouch = P; InputTouch(0, Ty, P, 1.f, FDateTime::Now(), 0); };

	auto CurYaw = [&]() { float Y = 0.f; ZoogiAimArrow::CurrentYaw(G, Y); return Y; };

	auto AngDiff = [](float A, float B) { return FMath::Abs(FRotator::NormalizeAxis(A - B)); };

	// PNG sequence of rotate + launch (15 fps), viewport + HUD canvas (the arrow is drawn on the canvas)

	if (bArrowSeq && !G->ShotDir.IsEmpty())

	{

		ArrowSeqT += Dt;

		if (ArrowSeqT >= 1.f / 15.f) { ArrowSeqT = 0.f; FScreenshotRequest::RequestScreenshot(FPaths::Combine(G->ShotDir, TEXT("seq"), FString::Printf(TEXT("f_%04d.png"), ArrowSeqN++)), true, false); }

	}

	// pass P1's Awaken / Standby (no OS input)

	if (G->IsHumanAiming() && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone)

	{

		ArrowAwakenT += Dt;

		if (ArrowAwakenT > 1.2f) { ArrowAwakenT = -3.f; UE_LOG(LogZoogi, Log, TEXT("ARROWTEST auto-commit P1 Awaken (cards=%d)"), G->CardsOn() ? 1 : 0); G->MenuAction(G->CardsOn() ? TEXT("CardCommit") : TEXT("Infuse"), G->CardsOn() ? 0 : -1); }

		return;

	}

	ArrowAwakenT = 0.f;

	AZoogiBall* Me = G->GetActiveBall();

	const bool bAim = G->IsHumanAiming() && G->IsAimStep() && Me;

	int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);

	const float UPx = FMath::Max(VY, 1) / 1080.f; // the slingshot's own px scale

	ArrowTestT += Dt;

	auto Next = [&](int32 P) { ArrowTestPhase = P; ArrowTestT = 0.f; };

	const bool bFirst = ArrowTestT <= Dt + 1e-6f; // first frame of a phase (frame-rate independent one-shots)

	const float TPx = FMath::Max(FMath::Min(VX, VY), 1) / 1080.f;

	// screen-space rotation: finger x = press x + (yaw - yaw at grab) / deg-per-px (ArrowDragFrom holds the grab point here)

	auto FingerAt = [&](float A) { return ArrowDragFrom + FVector2D((A - ArrowYaw0) / (ZoogiAimArrow::Tune().RotateDegPerPx / TPx), 0.f); };

	switch (ArrowTestPhase)

	{

	case 0: // aim start: arrow visible

		if (!bAim) { ArrowTestT = 0.f; break; }

		if (ArrowTestT > 1.5f)

		{

			Check(ZoogiAimArrow::IsShown(G, this), FString::Printf(TEXT("arrow visible at aim start (yaw %.1f, hasAim=%d, viewport %dx%d)"), CurYaw(), G->HasAim() ? 1 : 0, VX, VY));

			G->TakeShot(TEXT("arrow_1_aim_start"));

			ArrowCamYaw0 = Yaw; ArrowCamPivot0 = Pivot;

			Next(1);

		}

		break;

	case 1: // grab on the arrowhead

		if (ArrowTestT > 0.4f)

		{

			FVector2D S[7], BallS; float BallRpx = 0.f;

			if (!ArrowScreen(S, BallS, BallRpx)) { Check(false, TEXT("arrow projects on screen")); Next(90); break; }

			const ZoogiAimArrow::FTune& T = ZoogiAimArrow::Tune();

			ArrowHeadDist = ZoogiConst::ZoogiRadius + 2.f * ZoogiConst::ZoogiRadius * (T.LengthD - 0.5f * T.HeadLengthD); // middle of the head, on the axis

			ArrowYaw0 = CurYaw();

			bArrowSeq = true;

			ArrowDragFrom = (S[2] + S[3] + S[4]) / 3.f; // arrowhead centre

			Touch(ETouchType::Began, ArrowDragFrom);

			Next(2);

		}

		break;

	case 2: // rotate left 45 deg (screen left = yaw down for a camera behind the shooter)

	case 3: // rotate right to +45

	{

		if (bFirst && ArrowTestPhase == 2) { Check(ArrowGesture.bActive, TEXT("press on the arrowhead grabs the arrow")); }

		const float From = ArrowTestPhase == 2 ? ArrowYaw0 : ArrowYaw0 - 45.f, To = ArrowTestPhase == 2 ? ArrowYaw0 - 45.f : ArrowYaw0 + 45.f;

		const float Len = ArrowTestPhase == 2 ? 0.8f : 1.4f;

		const float A = FMath::Clamp(ArrowTestT / Len, 0.f, 1.f);

		Touch(ETouchType::Moved, FingerAt(FMath::Lerp(From, To, FMath::SmoothStep(0.f, 1.f, A)))); // every frame (final pose held), finger stays down

		if (ArrowTestT > Len + 1.0f) // hold: the camera keeps orbiting toward the aim; the yaw must not move

		{

			const float Got = CurYaw();

			Check(AngDiff(Got, To) < 1.5f, FString::Printf(TEXT("grab-and-drag rotated %s: yaw %.1f (expected %.1f)"), ArrowTestPhase == 2 ? TEXT("left") : TEXT("right"), Got, FRotator::NormalizeAxis(To)));

			Check(AngDiff(Yaw, Got) < 10.f, FString::Printf(TEXT("camera orbits with the aim (camera yaw %.1f, arrow %.1f, moved %.1f deg since aim start)"), FRotator::NormalizeAxis(Yaw), Got, FRotator::NormalizeAxis(Yaw - ArrowCamYaw0)));

			float FY = 0.f;

			Check(ZoogiAimArrow::FaceYaw(G, Me, FY) == ZoogiAimArrow::Tune().bFaceArrow && (!ZoogiAimArrow::Tune().bFaceArrow || AngDiff(FY, Got) < 0.5f), FString::Printf(TEXT("Zoogi faces the arrow (visual yaw target %.1f)"), FY));

			Check(G->Phase == EZoogiPhase::Aiming && G->IsAimStep() && !bDragArmed, TEXT("rotating never arms / fires the slingshot"));

			G->TakeShot(ArrowTestPhase == 2 ? TEXT("arrow_2_rotated_left") : TEXT("arrow_3_rotated_right"));

			Next(ArrowTestPhase + 1);

		}

		break;

	}

	case 4: // release the grab: aim kept, no launch

		if (bFirst) Touch(ETouchType::Ended, ArrowTouch);

		if (ArrowTestT > 0.3f && ArrowTestT - Dt <= 0.3f)

		{

			ArrowWantYaw = CurYaw();

			Check(!ArrowGesture.bActive && !bDragArmed, TEXT("release ends the grab (gesture state cleared)"));

			Check(G->Phase == EZoogiPhase::Aiming && G->IsAimStep() && G->HasAim() && G->bArrowAim && AngDiff(ArrowWantYaw, ArrowYaw0 + 45.f) < 1.5f, FString::Printf(TEXT("release after rotating keeps the aim (yaw %.1f), no launch"), ArrowWantYaw));

		}

		if (ArrowTestT > 1.5f) // camera settled; the aim must not have drifted

		{

			Check(AngDiff(CurYaw(), ArrowWantYaw) < 0.1f, FString::Printf(TEXT("no drift after release (yaw %.2f, at release %.2f)"), CurYaw(), ArrowWantYaw));

			ArrowWantYaw = CurYaw();

			// press elsewhere: beside the shooter, perpendicular to the shot, the same Fwd the slingshot uses

			float Fr = 0.f, Imp = 0.f;

			const FVector2D SDir = G->ComputeShotVelocity(Fr, Imp).GetSafeNormal();

			FVector2D BallS, AheadS;

			ProjectWorldLocationToScreen(Me->BallLocation(), BallS); ProjectWorldLocationToScreen(Me->BallLocation() + FVector(SDir * 200.f, 0.f), AheadS);

			ArrowDragFwd = (AheadS - BallS).GetSafeNormal();

			const FVector2D Perp(-ArrowDragFwd.Y, ArrowDragFwd.X);

			FVector2D S[7], BS; float BR = 0.f; ArrowScreen(S, BS, BR);

			const float APx = FMath::Max(FMath::Min(VX, VY), 1) / 1080.f;

			ArrowDragFrom = BallS + Perp * 300.f * APx - ArrowDragFwd * 60.f * APx;

			if (ZoogiAimArrow::HitTest(S, BS, BR, ArrowDragFrom, APx, ZoogiAimArrow::Tune())) ArrowDragFrom = BallS - Perp * 300.f * APx - ArrowDragFwd * 60.f * APx;

			Check(!ZoogiAimArrow::HitTest(S, BS, BR, ArrowDragFrom, APx, ZoogiAimArrow::Tune()), FString::Printf(TEXT("press elsewhere (%.0f, %.0f) is not an arrow grab"), ArrowDragFrom.X, ArrowDragFrom.Y));

			Touch(ETouchType::Began, ArrowDragFrom);

			ArrowWantPull = ZoogiAimArrow::SlingshotPull(130.f * UPx, UPx, MaxDragDistancePx, MaxDragForce);

			Next(5);

		}

		break;

	case 5: // drag back 130 px (1080p) along -Fwd: the unchanged slingshot

	{

		const float A = FMath::Clamp(ArrowTestT / 0.5f, 0.f, 1.f);

		Touch(ETouchType::Moved, ArrowDragFrom - ArrowDragFwd * 130.f * UPx * A);

		if (ArrowTestT > 0.7f)

		{

			Check(!ArrowGesture.bActive && bDragArmed && FMath::Abs(DragFrac - ArrowWantPull) < 0.02f && FMath::Abs(G->HumanDragFrac - ArrowWantPull) < 0.02f,

				FString::Printf(TEXT("drag elsewhere arms the normal slingshot: pull %.1f%% (expected %.1f%%, max %.0f px)"), DragFrac * 100.f, ArrowWantPull * 100.f, MaxDragDistancePx));

			Check(AngDiff(CurYaw(), ArrowWantYaw) < 0.5f, TEXT("drag elsewhere does not rotate the arrow"));

			Touch(ETouchType::Ended, ArrowDragFrom - ArrowDragFwd * 130.f * UPx);

			Next(6);

		}

		break;

	}

	case 6: // launch along the arrow, power from the pull

		if (G->Phase == EZoogiPhase::Rolling && Me)

		{

			const FVector V = Me->GetBallVelocity();

			const float VYaw = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));

			const float WantSpeed = ZoogiConst::LaunchSpeedForPull(FMath::Clamp(ArrowWantPull, 0.08f, 1.f));

			Check(AngDiff(VYaw, ArrowWantYaw) < 3.f, FString::Printf(TEXT("launch follows the arrow: velocity yaw %.1f (arrow %.1f)"), VYaw, ArrowWantYaw));

			UE_LOG(LogZoogi, Log, TEXT("ARROWTEST note shot speed %.0f cm/s (pull %.0f%% -> %.0f cm/s before card / combo multipliers)"), G->GetShotSpeed(), ArrowWantPull * 100.f, WantSpeed);

			Check(G->bComboMode || FMath::Abs(G->GetShotSpeed() / FMath::Max(WantSpeed, 1.f) - 1.f) < 0.25f, TEXT("launch speed from the drag pull (same curve)"));

			Next(7);

		}

		else if (ArrowTestT > 4.f) { Check(false, TEXT("drag release launched the shot")); Next(7); }

		break;

	case 7:

		if (ArrowTestT > 0.25f && ArrowTestT - Dt <= 0.25f) G->TakeShot(TEXT("arrow_4_mid_launch"));

		if (ArrowTestT > 0.9f) { bArrowSeq = false; Check(!ZoogiAimArrow::IsShown(G, this), TEXT("arrow hidden once the shot rolls")); Next(8); }

		break;

	case 8: // next human aim step: AIM ZOOM (crosshair / tap-to-lock) hides the arrow

		if (!bAim) { ArrowTestT = 0.f; break; }

		if (ArrowTestT > 1.2f)

		{

			Check(ZoogiAimArrow::IsShown(G, this), TEXT("arrow visible again at the next aim start"));

			SetZoomMode(true, TEXT("arrowtest")); // must be ignored: no manual zoom (camera buttons only)

			Check(!IsZoomMode(), TEXT("no AIM ZOOM mode (camera buttons only)"));

			Next(10);

		}

		break;

	case 9:

		if (ArrowTestT > 0.5f)

		{

			Check(IsZoomMode() && !ZoogiAimArrow::IsShown(G, this), TEXT("arrow hidden in AIM ZOOM (crosshair / tap-to-lock unchanged)"));

			G->TakeShot(TEXT("arrow_5_zoom_hidden"));

			SetZoomMode(false, TEXT("arrowtest"));

			Next(10);

		}

		break;

	case 10:

		if (ArrowTestT > 0.5f)

		{

			Check(ZoogiAimArrow::IsShown(G, this), TEXT("arrow back after leaving AIM ZOOM"));

			Next(90);

		}

		break;

	case 90:

		UE_LOG(LogZoogi, Log, TEXT("ARROWTEST SUMMARY %d passed, %d failed (%d sequence frames)"), ArrowPass, ArrowFail, ArrowSeqN);

		bArrowSeq = false;

		ArrowExitT = 0.f;

		Next(99);

		break;

	default: break;

	}

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

		const float ScreenR = FMath::Max((SR - S).Size() * SnapAimMul, 16.f); // #44 forgiving snap-to-aim

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

	// Oct 27 (Evan, Build 9 corrections): Frozen Ring human aim turns use the two sticks + tap-to-target (ZoogiFrozenInput.cpp);

	// the classic drag / tap-the-floor aim never runs there, so taps, camera moves and target picks cannot launch.

	if (!ZoogiSkillLab::Open() && G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn() && G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle && G->GetActiveBall() && !G->GetActiveBall()->bCPU) // b110: not over the lab

	{

		FrozenInput(G, Hud, Dt);

		HoverTarget.Reset();

		return;

	}

	const FVector2D Mouse = GetCursor();

	float RMX = 0.f, RMY = 0.f;

	GetMousePosition(RMX, RMY);



	// UMG controls (Awaken panel, aim bar) swallow their presses in Slate; as a second guard, a press the UI

	// took this frame / last frame, or any press with the cursor over a UMG panel, never reaches world picking.

	// Oct 14 root cause of "can't click": DefaultInput has bUseMouseForTouch=True, so in a packaged build the mouse

	// arrives as Touch1 and WasInputKeyJustPressed(LeftMouseButton) never fires - world picks never ran. Treat a

	// Touch1 / held-LMB press edge as the press too.

	float PreTX = 0.f, PreTY = 0.f; bool bPreTouch = false;

	GetInputTouchState(ETouchIndex::Touch1, PreTX, PreTY, bPreTouch);

	const bool bRawLMB = WasInputKeyJustPressed(EKeys::LeftMouseButton) || ((IsInputKeyDown(EKeys::LeftMouseButton) || bPreTouch) && !bPtrWasDown);

	if (bRawLMB) LastRawLMBFrame = GFrameCounter;

	const bool bOverUI = Hud && Hud->IsOverUI(Mouse);

	const bool bUIClick = Hud && Hud->UIConsumedClick();

	const bool bLMB = bRawLMB && !bOverUI && !bUIClick;

	if (bRawLMB && G->Phase == EZoogiPhase::Aiming) // Oct 14 urgent: one line per press, so "can't click" reports are diagnosable

		UE_LOG(LogZoogi, Log, TEXT("INPUT lmb at (%.0f, %.0f) raw (%.0f, %.0f) stage=%d step=%d humanAim=%d aimStep=%d panels=%d outsideArena=%d uiClick=%d -> world=%d"),

			Mouse.X, Mouse.Y, RMX, RMY, (int32)G->Stage, (int32)G->BattleStep, G->IsHumanAiming() ? 1 : 0, G->IsAimStep() ? 1 : 0,

			(Hud && Hud->AwakenWidgetOverPanels(Mouse)) ? 1 : 0, ZoogiFramed::IsOutsideArena(GetWorld(), Mouse) ? 1 : 0, bUIClick ? 1 : 0, bLMB ? 1 : 0);



	// Oct 14 drag aim: one pointer for mouse and touch (Touch1), with press / release edges

	if (!bDragCfgLoaded && GConfig)

	{

		bDragCfgLoaded = true;

		GConfig->GetFloat(TEXT("ZoogiRoll.Aim"), TEXT("MaxDragDistancePx"), MaxDragDistancePx, GGameIni);

		GConfig->GetFloat(TEXT("ZoogiRoll.Aim"), TEXT("MaxDragForce"), MaxDragForce, GGameIni);

		GConfig->GetFloat(TEXT("ZoogiRoll.Aim"), TEXT("MinDragPx"), MinDragPx, GGameIni);

		GConfig->GetFloat(TEXT("ZoogiRoll.Aim"), TEXT("CancelOnZoogiPx"), CancelOnZoogiPx, GGameIni);

		const TCHAR* HF = TEXT("ZoogiRoll.HudFeedback");

		GConfig->GetFloat(HF, TEXT("PullEaseRate"), PullEaseRate, GGameIni);

		GConfig->GetFloat(HF, TEXT("PerfectLo"), PerfectLo, GGameIni);

		GConfig->GetFloat(HF, TEXT("PerfectHi"), PerfectHi, GGameIni);

		GConfig->GetFloat(HF, TEXT("SnapAimMul"), SnapAimMul, GGameIni);

		MaxDragForce = FMath::Clamp(MaxDragForce, 0.1f, 1.f);

		PullEaseRate = FMath::Clamp(PullEaseRate, 1.f, 60.f);

		PerfectLo = FMath::Clamp(PerfectLo, 0.4f, 0.95f);

		PerfectHi = FMath::Clamp(FMath::Max(PerfectHi, PerfectLo + 0.02f), PerfectLo + 0.02f, 1.f);

		SnapAimMul = FMath::Clamp(SnapAimMul, 1.05f, 2.5f);

		CancelOnZoogiPx = FMath::Max(CancelOnZoogiPx, 1.f);

	}

	float TchX = 0.f, TchY = 0.f; bool bTouch = false;

	GetInputTouchState(ETouchIndex::Touch1, TchX, TchY, bTouch);

	const bool bPtrDown = IsInputKeyDown(EKeys::LeftMouseButton) || bTouch;

	// b122b (Evan: "I'm unable to tap any of the circles"): on a phone the frame the finger lifts has no touch position (Ptr fell back
	// to the mouse point) - a release uses the last spot the finger was down at, so pick-on-release lands where it was tapped
	const FVector2D PtrRaw = bTouch ? FVector2D(TchX, TchY) : Mouse;
	static FVector2D GPtrLastDown = FVector2D::ZeroVector;
	const bool bPtrNowDown = IsInputKeyDown(EKeys::LeftMouseButton) || bTouch;
	const FVector2D Ptr = (!bPtrNowDown && bPtrWasDown) ? GPtrLastDown : PtrRaw;
	if (bPtrNowDown) GPtrLastDown = PtrRaw;

	const bool bPtrPress = bPtrDown && !bPtrWasDown, bPtrRelease = !bPtrDown && bPtrWasDown;

	bPtrWasDown = bPtrDown;
	// b182: SHOWTIME owns the pointer while it plays; a press on its gold button starts it
	if (ZoogiGattling::Active()) { ZoogiGattling::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }
	if (ZoogiBossAttack::DuelActive()) { HoverTarget.Reset(); return; } // b190: the boss grab / duel owns the taps
	if (ZoogiSkills::RapidActive() || (ZoogiSkills::DashActive() && G->Phase == EZoogiPhase::Rolling)) { HoverTarget.Reset(); return; } // b224: only while rolling - aiming / launching stays normal // b197: GATTLING RAPID owns the taps (b223: the dash taps too)
	if (bPtrPress && ZoogiGattling::TryButton(G, Ptr)) { HoverTarget.Reset(); return; }
	// b63: a skill target pick owns the pointer (area circle / line / taps / FIRE / CANCEL)
	if (ZoogiSkillLab::Open()) { ZoogiSkillLab::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; } // b107
	// b120 (Evan: "button taps should never count as a target tap on field"): a press that belongs to a button never reaches a pick
	static bool bPickPressOk = true;
	const bool bBtnOwned = ZoogiClassicPad::PointerOwned() || ZoogiP5Cmd::InputBlocked() || (Hud && Hud->UIConsumedClick());
	if (bPtrPress) bPickPressOk = !bBtnOwned;
	const bool bPickPtr = bPickPressOk && !bBtnOwned;
	if (ZoogiSkills::Picking()) { if (bPickPtr || (!bPtrPress && !bPtrDown && !bPtrRelease)) ZoogiSkills::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }
	if (ZoogiShowtime::Picking()) { if (bPtrPress && bPickPtr) ZoogiShowtime::Tap(G, this, Ptr); HoverTarget.Reset(); return; } // b122: SHOWTIME - tap your target
	if (ZoogiRedirect::Picking()) { if (bPickPtr || (!bPtrPress && !bPtrDown && !bPtrRelease)) ZoogiRedirect::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; } // b66
	// b64 (Evan: "Starting form switches back into ball before launch once I touch the yellow"): the human's standing form turns
	// back into its ball the moment the aim is touched (not over a button)
	if (bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick())))
		if (AZoogiBall* Ab = G->GetActiveBall())
			if (!Ab->bCPU && G->Phase == EZoogiPhase::Aiming && ZoogiStandForms::IsStanding(Ab)) { UE_LOG(LogZoogi, Log, TEXT("STANDFORMS: P%d touched the aim -> ball"), Ab->PlayerIndex + 1); ZoogiStandForms::Release(Ab, true); }
	// Oct 24 (Evan 7:45 AM): the ability bubbles are gone - a double tap / double-click fires the one power the Zoogi menu
	// assigns this Zoogi (Def.PowerKind; Wolfgang = Wolf Pack clones). Human turn only, not over UI; TryPower keeps its
	// usual gates (charges, CanUsePower phase window). The second press still reaches aiming below like any press.
	bool bFunTap = false; // Oct 29 build 23: impact timing tap / parry / replay skip / CPU fast-forward
	if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(this)) // Nov 1 Wolf Pack 2.0: paint targets (tap / swipe) and parry taps
	{
		// b120: a paint drag only counts when its press began on the field (the held SUPERPOWER finger painted the orb under it)
		const bool bOverUiNow = Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick());
		static bool bPaintDragOk = false;
		if (bPtrPress) bPaintDragOk = !bOverUiNow;
		if (bPtrPress && !bOverUiNow) bFunTap = WP->OnTap(Ptr);
		else if (bPtrDown && bPaintDragOk && !bOverUiNow && WP->IsPainting()) WP->OnDrag(Ptr);
	}
	if (!bFunTap && bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick())))
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(this)) bFunTap = Fun->OnTap(Ptr);
	static const bool bUltOnRelease = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPickOnRelease"));
	if ((bUltOnRelease ? (bPtrRelease && !ZoogiPauseOrbit::WasDrag()) : bPtrPress) && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick()))) // b125: the bullet-time pick on the press again (b120: on release)
		if (UZoogiFunSys* FunR = UZoogiFunSys::Get(this)) if (FunR->UltAimOn()) FunR->UltAimRelease(Ptr);
	if (bPtrPress && !bFunTap)
	{
		static double LastTapTime = -10.0; static FVector2D LastTapPos = FVector2D::ZeroVector;
		const double Now = FPlatformTime::Seconds();
		const AZoogiBall* TapBall = G->GetActiveBall();
		const bool bTapUI = Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick());
		if (!bTapUI && Now - LastTapTime < 0.35 && FVector2D::Distance(Ptr, LastTapPos) < 90.f)
		{
			LastTapTime = -10.0;
			if (TapBall && !TapBall->bCPU && !G->bArtifactMode)
			{
				UE_LOG(LogZoogi, Log, TEXT("INPUT double tap -> power %d (charges %d, usable %d)"), (int32)TapBall->Def.PowerKind, TapBall->Charges, G->CanUsePower() ? 1 : 0);
				G->TryPower();
			}
		}
		else if (!bTapUI) { LastTapTime = Now; LastTapPos = Ptr; }
	}

	// Oct 14 aim zoom mode: while it is on (or two fingers are / were down) the pointers pan / pinch / tap-to-lock and

	// nothing below sees them, so drag-aim is paused and a pinch never arms or releases a drag

	// (in the Standby the zoom only looks / pans: the Standby keys - A, 1-7, Enter - and the UMG tray keep working below)

	if (UpdateZoomInput(G, ZoomStageLive(G), bTouch ? FVector2D(TchX, TchY) : Mouse, Dt) && (G->Stage != EZoogiStage::Awaken || WasInputKeyJustPressed(EKeys::Escape))) { HoverTarget.Reset(); return; }

	const bool bTouchPress = bTouch && bPtrPress && !bLMB && !(Hud && Hud->IsOverUI(Ptr)); // bLMB already covers the press



	// Buttons + phone orb HitTest (ability / Awaken). Raw press / touch so framed margins and

	// bUseMouseForTouch still reach PhoneAbility / PhoneAwaken (bLMB alone skips OverUI).

	if ((bRawLMB || bTouchPress) && Hud)

	{

		FName Action; int32 Index = 0;

		const FVector2D HitP = bTouch ? Ptr : FVector2D(RMX, RMY);

		if (Hud->HitTest(HitP, Action, Index))

		{

			G->MenuAction(Action, Index);

			return;

		}

	}



	if (G->Phase == EZoogiPhase::Forming)

	{

		HoverTarget.Reset();

		if (bLMB || WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Escape)) G->SkipStageForming();

		return;

	}

	if (G->Phase == EZoogiPhase::DropIntro)

	{

		HoverTarget.Reset();

		if (WasInputKeyJustPressed(EKeys::Escape)) { G->SkipDropIntro(); return; }

		if (G->DropIntro.Step == EZoogiDropStep::ShipHold)

		{

			// Hold only Ã¢â‚¬â€ no skip-to-Tac on click (Evan: hold cabin then hard cut). Space still allows skip whole intro.

			if (WasInputKeyJustPressed(EKeys::SpaceBar) && FZoogiDropIntro::AutoDrive()) G->SkipDropIntro();

			return;

		}

		if (G->IsDropPick())

		{

			if (bLMB || bTouchPress)

			{

				FVector O, D;

				const FVector2D Scr = bTouch ? Ptr : Mouse;

				if (DeprojectScreenPositionToWorld(Scr.X, Scr.Y, O, D) && D.Z < -0.02f)

				{

					const float Z = 0.f;

					const float T = (Z - O.Z) / D.Z;

					if (T > 0.f) G->SetDropPick(O + D * T);

				}

			}

			if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter)) G->ConfirmDrop();

		}

		return;

	}

	if (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)

	{

		HoverTarget.Reset();

		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) G->StartMatch();

		if (WasInputKeyJustPressed(EKeys::Escape) && G->Phase == EZoogiPhase::GameOver) G->ReturnToMenu();

		return;

	}



	// Anime placeholder playing: click / Space / Esc skips (after 0.3 s), nothing else.

	if (UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>())

	{

		if (Anime->IsBusy())

		{

			if (bLMB || WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Escape)) Anime->Skip();

			HoverTarget.Reset();

			return;

		}

	}



	AZoogiBall* Active = G->GetActiveBall();

	const bool bHumanTurn = Active && !Active->bCPU;

	const bool bPickStage = G->IsHumanAiming() && G->IsAimStep();

	const bool bAwaken = G->IsHumanAiming() && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone;



	if (WasInputKeyJustPressed(EKeys::Escape))

	{

		if (bPickStage && G->HasAim()) G->ClearLock(); // Esc cancels the aim first

		else G->ReturnToMenu();

		return;

	}

	if (WasInputKeyJustPressed(EKeys::R) && IsInputKeyDown(EKeys::LeftControl)) { G->StartMatch(); return; }

	// Oct 14 fantasy cards: a click mid-roll redirects a Precision homing shot (up to 2x) / fires the Recovery Roll dash

	if ((bLMB || bTouchPress) && G->CardsOn() && !bAwaken && !bDragArmed)

		if (UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this)) { FVector Pt; if (FloorPoint(Ptr, Pt) && CS->FantasyTap(G, Pt)) return; }

	if (WasInputKeyJustPressed(EKeys::C)) G->MenuAction(TEXT("CamStyle"), 0);



	// Classic powers (-ZoogiClassicPowers): E / Q.

	if (bHumanTurn && !G->bArtifactMode)

	{

		if (WasInputKeyJustPressed(EKeys::E) || WasInputKeyJustPressed(EKeys::Q)) G->TryPower();

		if (bHumanTest && bPowerTest && Active->Charges > 0 && G->CanUsePower() && !G->bPowerArmed)

		{

			const bool bArmed = ZoogiUtil::IsArmedPower(Active->Def.PowerKind);

			if ((bArmed && G->IsAimStep() && G->StepTime > 0.3f) || (!bArmed && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.4f)) G->TryPower();

		}

	}



	// Awaken keys: 1-3 pick the card, I / G / S = Infuse / Guard / Save.

	if (bAwaken && ZoogiP5Cmd::Showing()) {} // Oct 31: the P5 command menu owns 1-6 / Esc while it is up
	else if (bAwaken && G->CardsOn())

	{

		// v3 Standby: 1-5 toggle cards, Enter / Space commit, Backspace clear

		static const FKey CardKeys[7] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven }; // Oct 14: hand cap 7

		if (G->CardsAwakenViewOpen())

		{

			// Choose-your-Awaken tablets on show: 1-3 pick a tablet, 0 / X skips

			for (int32 i = 0; i < 3; ++i) if (WasInputKeyJustPressed(CardKeys[i])) G->MenuAction(TEXT("CardAwakenPick"), i);

			if (WasInputKeyJustPressed(EKeys::Zero) || WasInputKeyJustPressed(EKeys::X)) G->MenuAction(TEXT("CardAwakenSkip"), 0);

		}

		else for (int32 i = 0; i < 7; ++i) if (WasInputKeyJustPressed(CardKeys[i])) G->MenuAction(TEXT("CardToggle"), i);

		if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) G->MenuAction(TEXT("CardCommit"), 0);

		if (WasInputKeyJustPressed(EKeys::BackSpace)) G->MenuAction(TEXT("CardClear"), 0);

		if (WasInputKeyJustPressed(EKeys::A)) G->MenuAction(TEXT("CardAwaken"), 0);

	}

	else if (bAwaken)

	{

		if (WasInputKeyJustPressed(EKeys::One)) G->MenuAction(TEXT("AwakenPick"), 0);

		if (WasInputKeyJustPressed(EKeys::Two)) G->MenuAction(TEXT("AwakenPick"), 1);

		if (WasInputKeyJustPressed(EKeys::Three)) G->MenuAction(TEXT("AwakenPick"), 2);

		if (WasInputKeyJustPressed(EKeys::I)) G->MenuAction(TEXT("Infuse"), -1);

		if (WasInputKeyJustPressed(EKeys::G)) G->MenuAction(TEXT("Guard"), -1);

		if (WasInputKeyJustPressed(EKeys::S)) G->MenuAction(TEXT("Save"), -1);

	}



	// 2v2 Combo: Partner Follow-Up prompt (F / Enter accept, X / Backspace skip); Q switches to the partner Zoogi.

	if (G->bComboMode)

	{

		AZoogiBall* Partner = G->PartnerOf(Active);

		if (G->bFollowUpOffered && Partner && !Partner->bCPU)

		{

			if (WasInputKeyJustPressed(EKeys::F) || WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::SpaceBar)) G->MenuAction(TEXT("FollowUp"), 0);

			else if (WasInputKeyJustPressed(EKeys::X) || WasInputKeyJustPressed(EKeys::BackSpace)) G->MenuAction(TEXT("FollowSkip"), 0);

		}

		if (bHumanTurn && G->CanComboSelect() && WasInputKeyJustPressed(EKeys::Q)) G->MenuAction(TEXT("ComboSelect"), -1);

		if (bPickStage)

		{

			if (WasInputKeyJustPressed(EKeys::One)) G->MenuAction(TEXT("ComboForce"), 0);

			if (WasInputKeyJustPressed(EKeys::Two)) G->MenuAction(TEXT("ComboForce"), 1);

			if (WasInputKeyJustPressed(EKeys::Three)) G->MenuAction(TEXT("ComboForce"), 2);

			if (WasInputKeyJustPressed(EKeys::Up)) G->MenuAction(TEXT("ComboForce"), FMath::Min(2, (int32)G->ComboForce + 1));

			if (WasInputKeyJustPressed(EKeys::Down)) G->MenuAction(TEXT("ComboForce"), FMath::Max(0, (int32)G->ComboForce - 1));

		}

	}



	if (!bPickStage)

	{

		HoverTarget.Reset();

		if (bDragArmed) { bDragArmed = false; DragFrac = 0.f; UiPull = 0.f; }

		return;

	}



	const bool bPtrOverUI = Hud && Hud->IsOverUI(Ptr);

	HoverTarget = (bPtrOverUI || bDragArmed) ? nullptr : PickTarget(Ptr.X, Ptr.Y);

	const bool bClick = ((bLMB || bSimClick) && !bOverUI && !bUIClick) || bTouchPress;

	auto AimAt = [&](AActor* T, const FVector2D& At)

	{

		// b79 (Evan: "tapping the menu button or the sky should not count as a selected target and should not move the player"):
		// a tap above the horizon only picks a target drawn right under the finger
		{
			FVector Pf;
			if (!FloorPoint(At, Pf))
			{
				AActor* Tn = T ? T : PickTarget(At.X, At.Y);
				FVector2D Ts; int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);
				const float Px = FMath::Max(FMath::Min(VX, VY), 1) / 1080.f;
				if (!(Tn && ProjectWorldLocationToScreen(Tn->GetActorLocation(), Ts) && FVector2D::Distance(Ts, At) < 70.f * Px)) { UE_LOG(LogZoogi, Log, TEXT("INPUT sky tap ignored (no target, aim kept)")); return; }
				T = Tn;
			}
		}
		// #44 forgiving snap-to-aim: if the press missed a precise hit, retry PickTarget (SnapAimMul widens the disc)

		if (!T) T = PickTarget(At.X, At.Y);

		// Oct 31 b46 (Evan): tapping the target you already locked resets the launch and shows the command selection again
		if (T && ZoogiP5Cmd::On() && !ZoogiP5Cmd::Showing() && T == G->GetLockTarget() && !G->bSoftLock && G->LockTime > 0.3f)
		{ G->ClearLock(); ZoogiP5Cmd::Reopen(); UE_LOG(LogZoogi, Log, TEXT("INPUT target tapped again: launch reset, command menu back")); return; }
		if (T) { G->LockOn(T, false); UE_LOG(LogZoogi, Log, TEXT("INPUT aim click: target %s -> hasAim=%d"), *T->GetName(), G->HasAim() ? 1 : 0); return; } // tap an orb / a player

		FVector P = FVector::ZeroVector;

		const bool bFloor = FloorPoint(At, P);

		if (bFloor) G->SetFreeAim(P); // tap the floor: free (angled) aim at that point

		UE_LOG(LogZoogi, Log, TEXT("INPUT aim click: floor=%d at (%.0f, %.0f) -> hasAim=%d"), bFloor ? 1 : 0, P.X, P.Y, G->HasAim() ? 1 : 0);

	};

	// aim-arrow (Evan, Oct 17): a press that starts ON / near the arrow (generous area round the arrowhead and shaft) grabs it

	// and rotates the aim round the Zoogi (G->SetArrowAim: a free aim along the arrow, so the launch follows it); released

	// without moving it is the old tap (tap-to-aim at the press point). A press anywhere else runs the slingshot below unchanged.

	if (bPtrDown && G->IsAimStep()) ZoogiControls::SetAimHintDone(true); // Nov 1 b53: the first touch dismisses the slash hint
	const bool bArrowShown = ZoogiAimArrow::IsShown(G, this);

	if (!bArrowShown && ArrowGesture.bActive) ArrowGesture.Cancel();

	bool bArrowOwns = false;

	if (bClick && bArrowShown && bPtrDown && !bSimClick && !bDragArmed && !ArrowGesture.bActive)

	{

		FVector2D AS[7], BallS; float BallRpx = 0.f;

		int32 AVX = 1920, AVY = 1080; GetViewportSize(AVX, AVY);

		const float APx = FMath::Max(FMath::Min(AVX, AVY), 1) / 1080.f;

		if (ArrowScreen(AS, BallS, BallRpx) && ZoogiAimArrow::HitTest(AS, BallS, BallRpx, Ptr, APx, ZoogiAimArrow::Tune()))

		{

			AZoogiBall* Me = G->GetActiveBall();

			float Yaw0 = 0.f; ZoogiAimArrow::CurrentYaw(G, Yaw0);

			AActor* L = (G->TargetLock.IsValid() && !G->bSoftLock) ? G->TargetLock.Get() : nullptr;

			float ADist = ZoogiAimArrow::Tune().DefaultAimDistCm;

			if (G->bFreeAim) ADist = FVector::Dist2D(G->FreeAimPoint, Me->BallLocation());

			else if (G->TargetLock.IsValid()) ADist = FVector::Dist2D(ActorCenter(G->TargetLock.Get()), Me->BallLocation());

			if (!G->bArrowAim || !bArrowCamHold)

			{

				bArrowCamHasAim = L != nullptr || G->bFreeAim;

				ArrowCamAim = L ? ActorCenter(L) : (G->bFreeAim ? FVector(G->FreeAimPoint.X, G->FreeAimPoint.Y, Me->BallLocation().Z) : FVector::ZeroVector);

				bArrowCamHold = true;

			}

			ArrowGesture.Begin(Yaw0, Ptr, ADist);

			ArrowTapTarget = HoverTarget;

			bArrowOwns = true;

			UE_LOG(LogZoogi, Log, TEXT("INPUT arrow grab at (%.0f, %.0f): yaw %.1f, aim dist %.0f"), Ptr.X, Ptr.Y, Yaw0, ADist);

		}

	}

	if (ArrowGesture.bActive)

	{

		bArrowOwns = true;

		int32 AVX = 1920, AVY = 1080; GetViewportSize(AVX, AVY);

		const float APx = FMath::Max(FMath::Min(AVX, AVY), 1) / 1080.f;

		// build 27 (Evan): grab your Zoogi / the arrow and pull BACK = slingshot along the arrow right away (no aim needed first)
		if (bPtrDown && !bPtrRelease)
			if (AZoogiBall* Me = G->GetActiveBall())
			{
				FVector2D BS, AH;
				const FVector PullDir = FRotator(0.f, ArrowGesture.Yaw, 0.f).Vector();
				if (ProjectWorldLocationToScreen(Me->BallLocation(), BS) && ProjectWorldLocationToScreen(Me->BallLocation() + PullDir * 200.f, AH))
				{
					const FVector2D Fw = (AH - BS).GetSafeNormal();
					const FVector2D Dd = Ptr - ArrowGesture.Press;
					const float Back = -FVector2D::DotProduct(Dd, Fw);
					const float PullSide = FMath::Abs(FVector2D::CrossProduct(Fw, Dd));
					if (!Fw.IsNearlyZero() && Back > 34.f * APx && Back > 1.4f * PullSide)
					{
						const float PullYaw = ArrowGesture.bRotating ? ArrowGesture.Yaw : ArrowGesture.Yaw0;
						if (!G->HasAim() || ArrowGesture.bRotating) G->SetArrowAim(PullYaw, ArrowGesture.AimDist);
						const FVector2D P0 = ArrowGesture.Press;
						ArrowGesture.Cancel(); ArrowTapTarget.Reset();
						bDragArmed = true; bPendingTap = false; DragStart = P0; DragNow = Ptr; DragFrac = 0.f; UiPull = 0.f; bDragWouldCancel = true; bDragFrame0 = false;
						UE_LOG(LogZoogi, Log, TEXT("INPUT arrow pull-back -> slingshot along yaw %.1f (no prior aim needed)"), PullYaw);
					}
				}
			}
		// Oct 17 fix: screen-space horizontal travel only (the camera orbits with the aim; a world floor point fed back)
		if (ArrowGesture.bActive && bPtrDown && !bPtrRelease && ArrowGesture.Move(Ptr, ZoogiAimArrow::Tune().RotateStartPx * APx, ZoogiAimArrow::Tune().RotateDegPerPx / APx)) G->SetArrowAim(ArrowGesture.Yaw, ArrowGesture.AimDist);

		if (ArrowGesture.bActive && (bPtrRelease || !bPtrDown))
		{
			// release: stop at once, clear everything so the next press elsewhere arms the slingshot this frame

			const float EndYaw = ArrowGesture.Yaw;

			const FVector2D PressAt = ArrowGesture.Press;

			TWeakObjectPtr<AActor> TapT = ArrowTapTarget;

			if (ArrowGesture.End()) { UE_LOG(LogZoogi, Log, TEXT("INPUT arrow tap (no rotate): old tap-to-aim")); AimAt(TapT.Get(), PressAt); }

			else UE_LOG(LogZoogi, Log, TEXT("INPUT arrow release: yaw %.1f (aim kept, no launch)"), EndYaw);

			ArrowTapTarget.Reset();

		}

		HoverTarget.Reset();

	}

	// Oct 28 (Evan): the drag-to-aim arrow + drag-back slingshot is back in classic, next to the AUTO / CONTROL / DRIVE / LAUNCH

	// presets of the aim bar (Oct 27 evening's tap-only aim bar branch removed). Dragging the arrow sideways turns it (off-centre cuts).

	if (bClick && !bArrowOwns)

	{

		if (G->HasAim() && bPtrDown && !bSimClick)

		{

			// already aimed: this press is a drag (keeps the aim, the camera stays put) unless it is released without

			// moving, in which case it is a tap and re-aims at the press point on release

			bDragArmed = true; bPendingTap = true; TapPos = Ptr; TapTarget = HoverTarget;

			DragStart = DragNow = Ptr; DragFrac = 0.f; UiPull = 0.f; bDragWouldCancel = true; bDragFrame0 = false;

		}

		else if (bPtrDown && !bSimClick && G->GetActiveBall() && !G->IsOnLaunchPad(G->GetActiveBall()) && G->EnsureArrowAim(TEXT("press")))
		{
			// build 28 (Evan): pull back right away, no aim needed - a press on open floor arms the slingshot along the arrow;
			// released without moving it is still the old tap-to-aim at the press point
			// b143d (Evan: "drag back without needing to touch the arrow first"): a press over an orb / rival arms it too (tap = aim at it)
			bDragArmed = true; bPendingTap = true; TapPos = Ptr; TapTarget = HoverTarget;
			DragStart = DragNow = Ptr; DragFrac = 0.f; UiPull = 0.f; bDragWouldCancel = true; bDragFrame0 = false;
			UE_LOG(LogZoogi, Log, TEXT("INPUT press with no aim: slingshot armed along the arrow (tap = aim at the press point)"));
		}
		else
		{
			AimAt(HoverTarget.Get(), Ptr);

			// Oct 22 tablet fix: on the launch pad the floor deprojection can miss (the pad is far above the court / the Zoogi

			// may be off camera), leaving no aim and so no drag. Aim at the opening landing point and arm the slingshot anyway.

			if (!G->HasAim() && bPtrDown && !bSimClick && G->GetActiveBall() && !G->IsOnLaunchPad(G->GetActiveBall())) G->EnsureArrowAim(TEXT("press")); // build 27: a press that found nothing to aim at still arms the slingshot along the arrow
			if (!G->HasAim()) if (AZoogiBall* PadMe = G->GetActiveBall()) if (G->IsOnLaunchPad(PadMe) && bPtrDown && !bSimClick) { G->SetFreeAim(G->GetPadLandingPoint(PadMe->PlayerIndex)); UE_LOG(LogZoogi, Log, TEXT("INPUT pad press at (%.0f, %.0f): no floor aim, default pad aim -> hasAim=%d"), Ptr.X, Ptr.Y, G->HasAim() ? 1 : 0); }

			if (G->HasAim() && bPtrDown && !bSimClick) { bDragArmed = true; bPendingTap = false; DragStart = DragNow = Ptr; DragFrac = 0.f; UiPull = 0.f; bDragWouldCancel = true; bDragFrame0 = false; } // scripted clicks never start a drag

		}

	}

	// slingshot: hold + drag BACK (away from the shot direction) = force, release = launch; back near zero / onto the Zoogi = cancel

	if (bDragArmed)

	{

		AZoogiBall* Me = G->GetActiveBall();

		int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);

		const float UPx = FMath::Max(VY, 1) / 1080.f;

		if (!bDragFrame0) // Oct 17: project once at press time; the camera may still be orbiting toward the aim

		{

			FVector2D BallS0 = FVector2D::ZeroVector, AheadS = FVector2D::ZeroVector;

			float Fr = 0.f, Imp = 0.f;

			const FVector2D SDir = G->ComputeShotVelocity(Fr, Imp).GetSafeNormal();

			bDragProj0 = Me && !SDir.IsNearlyZero() && ProjectWorldLocationToScreen(Me->BallLocation(), BallS0) && ProjectWorldLocationToScreen(Me->BallLocation() + FVector(SDir * 200.f, 0.f), AheadS);

			DragBallS0 = BallS0;

			DragFwd0 = bDragProj0 ? (AheadS - BallS0).GetSafeNormal() : FVector2D(0.f, -1.f);
			GDragCamYaw = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation().Yaw : 0.f; // b53: drag-anywhere aim maps the finger with the camera at the press

			bDragFrame0 = true;

		}

		const bool bProj = bDragProj0;

		const FVector2D BallS = DragBallS0, Fwd = DragFwd0;

		if (bPtrDown) DragNow = Ptr;

		if (bPendingTap && FVector2D::Distance(DragNow, DragStart) > MinDragPx * UPx) bPendingTap = false;

		const FVector2D DragDelta = DragNow - DragStart;

		// Nov 1 b53 (Evan: "can launch without needing to touch the arrow first"): in the clean aim view the drag itself aims -
		// touch anywhere, pull back the opposite way you want to roll (slingshot), the pull length is the power. A hard lock on a
		// rival / orb (tapped first) keeps the old pull along the locked line.
		float BackPxClean = -1.f;
		if (ZoogiControls::CleanAim() && Me && !G->IsOnLaunchPad(Me) && !(G->TargetLock.IsValid() && !G->bSoftLock) && !bPendingTap && DragDelta.Size() > MinDragPx * UPx)
		{
			const FVector CF = FRotator(0.f, GDragCamYaw, 0.f).Vector(), CRt = FRotator(0.f, GDragCamYaw + 90.f, 0.f).Vector();
			const FVector WorldDrag = CRt * DragDelta.X - CF * DragDelta.Y; // screen right / down laid on the floor
			const FVector AimDir = (-WorldDrag).GetSafeNormal2D();
			if (!AimDir.IsNearlyZero())
			{
				G->SetArrowAim(AimDir.Rotation().Yaw, ZoogiAimArrow::Tune().DefaultAimDistCm);
				BackPxClean = DragDelta.Size();
			}
		}
		const float BackPx = BackPxClean >= 0.f ? BackPxClean : FMath::Max(0.f, -FVector2D::DotProduct(DragDelta, Fwd));

		// Item 21: lateral flick vs frozen shot axis (screen perp of Fwd). Pure back-pull -> ~0.

		const FVector2D Perp(-Fwd.Y, Fwd.X);

		const float SidePx = BackPxClean >= 0.f ? 0.f : FVector2D::DotProduct(DragDelta, Perp);

		const float SideFrac = FMath::Clamp(SidePx / FMath::Max(MaxDragDistancePx * UPx, 1.f), -1.f, 1.f);

		DragFrac = ZoogiAimArrow::SlingshotPull(BackPx, UPx, MaxDragDistancePx, MaxDragForce); // unchanged formula (shared with ZoogiRoll.Aim.ArrowRotate) ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â launch uses this

		bDragWouldCancel = BackPx < MinDragPx * UPx || (bProj && FVector2D::Distance(DragNow, BallS) < CancelOnZoogiPx * UPx);

		// #6 smoother power ramp: UI/haptics ease toward the live pull (cancel snaps display down fast)

		{

			const float Want = bDragWouldCancel ? 0.f : DragFrac;

			const float Rate = bDragWouldCancel ? PullEaseRate * 2.2f : PullEaseRate;

			UiPull = FMath::FInterpTo(UiPull, Want, Dt, Rate);

		}

		if (bDragWouldCancel) { if (G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f; G->HumanFlickSide = 0.f; }

		else G->HumanDrag(DragFrac, false, SideFrac);

		// Oct 22: pull-back rumble (device pulses + HUD jitter) ramps with the pull while held; stops on release

		// Oct 22: pull rumble (UiPull eased) + Sonic-style mesh spin while pulled; stop on release/cancel.

		{

			AZoogiBall* ActiveBall = G->GetActiveBall();

			const FVector ShotDir = ActiveBall ? FRotator(0.f, ActiveBall->AimYaw, 0.f).Vector() : FVector::ForwardVector;

			if (bPtrDown && !bPtrRelease && !bDragWouldCancel && !bPendingTap)

			{

				ZoogiHaptics::AimTick(UiPull, Dt, GetWorld());

				ZoogiFeel::TickAimSpin(ActiveBall, DragFrac, ShotDir);

			}

			else

			{

				ZoogiHaptics::AimStop(GetWorld());

				ZoogiFeel::StopAimSpin(ActiveBall);

			}

		}

		if (bPtrRelease || !bPtrDown)

		{

			bDragArmed = false; bDragFrame0 = false;

			ZoogiHaptics::AimStop(GetWorld());

			if (AZoogiBall* ActiveBall = G->GetActiveBall()) ZoogiFeel::StopAimSpin(ActiveBall);

			if (bPendingTap)

			{

				bPendingTap = false; DragFrac = 0.f; UiPull = 0.f;

				if (G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f;

				G->HumanFlickSide = 0.f;

				AimAt(TapTarget.Get(), TapPos);

				return;

			}

			UE_LOG(LogZoogi, Log, TEXT("INPUT drag release: back %.0f px of %.0f -> pull %.0f%% side %.2f%s"), BackPx, MaxDragDistancePx * UPx, DragFrac * 100.f, SideFrac, bDragWouldCancel ? TEXT(" (cancel: aim kept)") : TEXT(""));

			if (bDragWouldCancel) { G->HumanDragCancel(); UiPull = 0.f; }

			else

			{

				// #22 Perfect release: sweet-spot band around SlingshotPull ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â juice only (launch still uses DragFrac)

				if (DragFrac >= PerfectLo && DragFrac <= PerfectHi)

				{

					PerfectFlashT = 0.95f;

					if (AZoogiBall* MePop = G->GetActiveBall())

						G->CardPopup(TEXT("PERFECT!"), MePop->BallLocation() + FVector(0.f, 0.f, 140.f), FLinearColor(1.f, 0.92f, 0.28f));

					UE_LOG(LogZoogi, Log, TEXT("INPUT perfect release: pull %.0f%% (band %.0f-%.0f)"), DragFrac * 100.f, PerfectLo * 100.f, PerfectHi * 100.f);

				}

				G->HumanDrag(DragFrac, true, SideFrac);

				UiPull = 0.f;

			}

			DragFrac = 0.f;

		}

	}

	// Oct 14: Tab is the lock-cycle key, but Slate also uses it for focus navigation; once a HUD button took

	// keyboard focus, Left / Right / Up / Down went to UI navigation instead of the aim. Keep keyboard focus on the

	// game viewport while the human aims (not while paused, so the pause / settings menus still navigate).

	if (FSlateApplication::IsInitialized() && !UGameplayStatics::IsGamePaused(this))

	{

		TSharedPtr<SWidget> Foc = FSlateApplication::Get().GetKeyboardFocusedWidget();

		if (Foc.IsValid() && Foc->GetTypeAsString() == TEXT("SButton")) FSlateApplication::Get().SetAllUserFocusToGameViewport();

	}

	// Nudges: Left / Right (or A / D) = 2 deg cut, Shift = 10 deg. Up / Down = power step.

	const int32 NudgeMul = IsInputKeyDown(EKeys::LeftShift) ? 5 : 1;

	if (WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A)) G->NudgeAim(-NudgeMul); // + = clockwise seen from above (turns right on screen)

	if (WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D)) G->NudgeAim(NudgeMul);

	if (WasInputKeyJustPressed(EKeys::Up) && !G->bComboMode) G->AimPowerStep = FMath::Clamp(G->AimPowerStep + 1, 0, 3);

	if (WasInputKeyJustPressed(EKeys::Down) && !G->bComboMode) G->AimPowerStep = FMath::Clamp(G->AimPowerStep - 1, 0, 3);

	if (WasInputKeyJustPressed(EKeys::Tab) || bSimTab) G->CycleLock(false);

	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !bOverUI) G->ClearLock();

	if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter) || bSimSpace) G->CommitLock();

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



bool AZoogiPlayerController::IsOffPlayArea(const AZoogiArena* Arena, const FVector2D& P)

{

	if (Arena) return Arena->IsOffCourt(P);

	return AZoogiArena::ActiveKnockoffR > 0.f && P.Size() > AZoogiArena::ActiveKnockoffR;

}



float AZoogiPlayerController::PlayRadius(const AZoogiArena* Arena)

{

	return Arena ? Arena->GetKnockoffRadius() : AZoogiArena::ActiveKnockoffR;

}



bool AZoogiPlayerController::IsLeavingPlay(const FVector& L, bool bOut, float CourtZ, const AZoogiArena* Arena)

{

	if (bOut) return true;

	if (L.Z < CourtZ - 1.5f * ZoogiConst::ZoogiRadius) return true; // dropped below the floor it was rolling on

	return IsOffPlayArea(Arena, FVector2D(L.X, L.Y));               // off this arena's court

}



FVector AZoogiPlayerController::TacticalRollPivot(const FVector& SL, bool bOut, float CourtZ, const AZoogiArena* Arena)

{

	using namespace ZoogiCamTune;

	const FVector C0(0.f, 0.f, CourtZ);

	if (IsLeavingPlay(SL, bOut, CourtZ, Arena)) return C0;

	FVector P = FMath::Lerp(C0, SL, TacRollTrack);

	if (!(Arena && Arena->IsNeonRect())) // a rect court's pivot is already inside its rails

	{

		const float MaxR = PlayRadius(Arena) * 0.8f;

		FVector2D P2(P.X, P.Y);

		if (MaxR > 0.f && P2.Size() > MaxR) { P2 = P2.GetSafeNormal() * MaxR; P.X = P2.X; P.Y = P2.Y; }

	}

	P.Z = FMath::Max(P.Z, CourtZ);

	return P;

}



void AZoogiPlayerController::TacticalBoom(const AZoogiGameMode* G, float& OutPitch, float& OutDist, float& OutFov, float& OutPosLag)

{

	using namespace ZoogiCamTune;

	const float Scale = (G && G->Arena && G->Arena->IsNeon()) ? NeonScale : (G && G->Arena ? G->Arena->CamScale : 1.f);

	OutPitch = Profiles[PTactical].Pitch;

	OutDist = Profiles[PTactical].Dist * 1.05f * Scale; // the Rolling DistMul

	OutFov = Profiles[PTactical].Fov;

	OutPosLag = Profiles[PTactical].PosLag;

}



void AZoogiPlayerController::FormingSweepPose(float Pg, float Scale, float& OutYaw, float& OutPitch, float& OutDist, float& OutFov, FVector& OutPivot)

{

	const float E = Pg * Pg * (3.f - 2.f * Pg);

	OutYaw = FMath::Lerp(215.f, 270.f, E);

	OutPitch = FMath::Lerp(30.f, 52.f, E);

	OutDist = FMath::Lerp(4300.f, 3300.f, E) * Scale;

	OutFov = 70.f;

	OutPivot = FVector(0.f, 0.f, -60.f);

}



float AZoogiPlayerController::ArenaCamScale(const AZoogiGameMode* G)

{

	return (G && G->Arena && G->Arena->IsNeon()) ? ZoogiCamTune::NeonScale : (G && G->Arena ? G->Arena->CamScale : 1.f);

}



float AZoogiPlayerController::ElevatorPushWeight(float WinT)

{

	auto Smooth = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); };

	if (WinT <= 0.f || WinT >= ElevPushOutEnd) return 0.f;

	return FMath::Min(Smooth(WinT / ElevPushIn), Smooth((ElevPushOutEnd - WinT) / ElevPushOut));

}

void AZoogiPlayerController::ElevatorRiseBlend(float WinT, float Scale,
	const FVector& SweepPivot, float SweepPitch, float SweepDist, float SweepFov,
	const FVector& FitPivot, float FitPitch, float FitDist,
	FVector& OutPivot, float& OutPitch, float& OutDist, float& OutFov)
{
	using namespace ZoogiCamTune;
	auto Smooth = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); };
	const float W = ElevatorPushWeight(WinT);
	const float TacT = ElevPushOutEnd > 0.f ? Smooth(FMath::Clamp(WinT / ElevPushOutEnd, 0.f, 1.f)) : 1.f;
	// Land in the same Tac soft chase the player keeps (teaser-lower: pitch 40 / boom 2000 / FOV 78). Pivot = pad centroid so the
	// rise reads marbles below center, then Aiming/Tac PosLag takes over without a wide-sweep snap.
	const float TacPitch = Profiles[PTactical].Pitch;
	const float TacDist = Profiles[PTactical].Dist * Scale;
	const float TacFov = Profiles[PTactical].Fov;
	OutPivot = FMath::Lerp(SweepPivot, FitPivot, TacT);
	OutPitch = FMath::Lerp(SweepPitch, TacPitch, TacT);
	OutDist = FMath::Lerp(SweepDist, TacDist, TacT);
	OutFov = FMath::Lerp(SweepFov, TacFov, TacT);
	// Brief pad-readable flair on top (FitPitch ~42, not ornate hero 38).
	OutPivot = FMath::Lerp(OutPivot, FitPivot, W);
	OutPitch = FMath::Lerp(OutPitch, FitPitch, W);
	OutDist = FMath::Lerp(OutDist, FitDist, W);
}

FVector AZoogiPlayerController::BoomLocation(const FVector& P, float InYaw, float InPitch, float InDist)

{

	const float Pr = FMath::DegreesToRadians(InPitch);

	return P - FRotator(0.f, InYaw, 0.f).Vector() * InDist * FMath::Cos(Pr) + FVector(0.f, 0.f, InDist * FMath::Sin(Pr));

}



void AZoogiPlayerController::ElevatorPushFit(const TArray<FVector>& PadFloors, float InYaw, float InFov, float BaseDist, FVector& OutPivot, float& OutPitch, float& OutDist,

	float Aspect, float Top, float Bottom)

{

	FVector C = FVector::ZeroVector;

	for (const FVector& P : PadFloors) C += P;

	C /= FMath::Max(1, PadFloors.Num());

	OutPivot = FVector(C.X, C.Y, C.Z + 40.f); // pad centroid at lift-top height
	OutPitch = 42.f;                           // teaser-bible soft chase (~45-60); was hero 38 â€” light flair, Tac is 55
	// keep a margin inside the frame; portrait: the same margin as the court fit (the narrow width is the only limit there, so

	// the landscape 0.82 left the pads hardly bigger than the court-wide sweep end)

	const float Margin = Aspect < 1.f ? ZoogiPortrait::CourtMargin() : 0.82f;

	float TanH, TanV;

	if (Aspect < 1.f) { ZoogiPortrait::TanHalf(InFov, Aspect, TanH, TanV); TanH *= Margin; TanV *= Margin; }

	else { TanH = FMath::Tan(FMath::DegreesToRadians(InFov * 0.5f)) * Margin; TanV = TanH / FMath::Max(Aspect, 0.1f); } // landscape: unchanged

	const float VTop = TanV * (1.f - 2.f * Top), VBot = TanV * (1.f - 2.f * Bottom); // portrait: inside the HUD-safe band

	auto Fits = [&](float D)

	{

		const FVector Loc = BoomLocation(OutPivot, InYaw, OutPitch, D);

		const FRotator Rot = ZoogiPortrait::BandRotation(Loc, OutPivot, InFov, Aspect, Top, Bottom);

		for (const FVector& P : PadFloors)

			for (int32 k = 0; k < 10; ++k)

			{

				// pad rim (r ~150) at the floor and the Zoogi on top of the lift (~z +130)

				const float A = k * 72.f;

				const FVector Off = k < 5 ? FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 150.f, FMath::Sin(FMath::DegreesToRadians(A)) * 150.f, 0.f) : FVector(0.f, 0.f, 130.f);

				const FVector L = Rot.UnrotateVector(P + Off - Loc);

				if (L.X < 50.f || FMath::Abs(L.Y) / L.X > TanH || L.Z / L.X > VTop || -L.Z / L.X > VBot) return false;

			}

		return true;

	};

	float Lo = FMath::Min(900.f, BaseDist), Hi = BaseDist;

	if (!Fits(Hi)) { OutDist = BaseDist; return; }

	for (int32 i = 0; i < 18; ++i) { const float M = 0.5f * (Lo + Hi); if (Fits(M)) Hi = M; else Lo = M; }

	OutDist = Hi;

}



void AZoogiPlayerController::SetPortraitCameraMode(bool bPortrait)

{

	const int32 M = bPortrait ? 1 : 0;

	if (ZoogiPortrait::GetModeOverride() == M) return;

	ZoogiPortrait::SetModeOverride(M);

	UE_LOG(LogZoogi, Log, TEXT("CAM portrait camera mode -> %s (hook; view aspect %.3f)"), bPortrait ? TEXT("PORTRAIT") : TEXT("LANDSCAPE"), ZoogiPortrait::ViewAspect(this));

}



void AZoogiPlayerController::ClearPortraitCameraMode()

{

	if (ZoogiPortrait::GetModeOverride() == -1) return;

	ZoogiPortrait::SetModeOverride(-1);

	UE_LOG(LogZoogi, Log, TEXT("CAM portrait camera mode -> AUTO (view aspect %.3f)"), ZoogiPortrait::ViewAspect(this));

}



bool AZoogiPlayerController::IsPortraitCameraMode() const

{

	return ZoogiPortrait::IsPortrait(ZoogiPortrait::ViewAspect(this));

}



float AZoogiPlayerController::PortraitAimDist(const AZoogiGameMode* G, float Aspect, float Top, float Bottom, const FVector& Shooter, const FVector& LookPivot,

	float InYaw, float InPitch, float LandscapeFov, float LandscapeDist, float& OutHFov)

{

	float K = 1.f;

	ZoogiPortrait::AdaptFov(LandscapeFov, Aspect, OutHFov, K);

	// never closer than the landscape boom; wide enough to show AimWidth x the play radius across the look point

	float TanH, TanV0; ZoogiPortrait::TanHalf(OutHFov, Aspect, TanH, TanV0);

	const float DW = ZoogiPortrait::AimWidth() * PlayRadius(G ? G->Arena : nullptr) / FMath::Max(2.f * TanH * ZoogiPortrait::CourtMargin(), 0.05f);

	const float MinD = FMath::Max(LandscapeDist, DW);

	const float MaxD = FMath::Max(MinD, LandscapeDist * ZoogiPortrait::BoomMax());

	// and the shooter's silhouette inside the HUD-safe band (it sits low in the frame: the arena runs up the screen)

	const float R = ZoogiConst::ZoogiRadius;

	const FVector Right = FRotator(0.f, InYaw, 0.f).RotateVector(FVector(0.f, R, 0.f));

	const FVector Pts[] = { Shooter + FVector(0, 0, R), Shooter - FVector(0, 0, R), Shooter + Right, Shooter - Right };

	return ZoogiPortrait::FitPointsDist(Pts, LookPivot, InYaw, InPitch, OutHFov, Aspect, Top, Bottom, ZoogiPortrait::ShooterMargin(), MinD, MaxD);

}



void AZoogiPlayerController::PortraitChaseBoom(const AZoogiGameMode* G, float Aspect, float LandscapeFov, float& OutElev, float& OutBoom, float& OutAhead, float& OutHFov)

{

	float K = 1.f;

	ZoogiPortrait::AdaptFov(LandscapeFov, Aspect, OutHFov, K);

	float TanH0, TanV; ZoogiPortrait::TanHalf(OutHFov, Aspect, TanH0, TanV);

	const float Theta = ZoogiPortrait::ChasePitch();                  // look pitch below horizontal

	const float YS = 2.f * ZoogiPortrait::ChaseScreenY() - 1.f;       // shooter this far below the optical axis (normalised)

	OutElev = Theta + FMath::RadiansToDegrees(FMath::Atan(YS * TanV)); // boom elevation that puts the shooter there

	OutBoom = ZoogiPortrait::ChaseBoom() * ArenaCamScale(G);

	// look point: where the view axis meets the shooter's floor, ahead of the shooter along the aim yaw

	const float E = FMath::DegreesToRadians(OutElev), T = FMath::DegreesToRadians(FMath::Max(Theta, 3.f));

	OutAhead = OutBoom * FMath::Sin(E) / FMath::Tan(T) - OutBoom * FMath::Cos(E);

}



void AZoogiPlayerController::PortraitChasePose(const AZoogiGameMode* G, const APlayerController* PC, const FVector& Shooter, float InYaw, FVector& OutLoc, FRotator& OutRot, float& OutHFov)

{

	using namespace ZoogiCamTune;

	float Elev, B, Ahead;

	PortraitChaseBoom(G, ZoogiPortrait::ViewAspect(PC), Profiles[PTactical].Fov, Elev, B, Ahead, OutHFov);

	OutLoc = BoomLocation(Shooter, InYaw, Elev, B);

	const FVector Look = Shooter + FRotator(0.f, InYaw, 0.f).Vector() * Ahead;

	OutRot = (Look - OutLoc).Rotation();

}



float AZoogiPlayerController::PortraitCourtRadius(const AZoogiGameMode* G)

{

	const float R = FMath::Max(PlayRadius(G ? G->Arena : nullptr), AZoogiArena::OuterOutR);

	return R > 0.f ? R + ZoogiConst::ZoogiRadius : 0.f;

}



float AZoogiPlayerController::PortraitBoom(const AZoogiGameMode* G, float Aspect, float Top, float Bottom, float InYaw, float InPitch, float LandscapeFov, float InDist,

	bool bCourt, const FVector& Centre, float& OutHFov)

{

	float K = 1.f;

	ZoogiPortrait::AdaptFov(LandscapeFov, Aspect, OutHFov, K);

	if (!ZoogiPortrait::IsPortrait(Aspect)) return InDist;

	const float R = bCourt ? PortraitCourtRadius(G) : 0.f;

	if (R <= 0.f) return InDist * K;

	// overview: the court fills the width (never closer than the landscape boom)

	return FMath::Max(InDist, ZoogiPortrait::FitCircleDist(Centre, R, InYaw, InPitch, OutHFov, Aspect, Top, Bottom, 100.f, 40000.f));

}



void AZoogiPlayerController::PortraitOverviewPose(const AZoogiGameMode* G, const APlayerController* PC, float InYaw, float CourtZ, bool bLegacy, FVector& OutLoc, FRotator& OutRot, float& OutFov)

{

	using namespace ZoogiCamTune;

	// same numbers as UpdateCamera's tactical resolve / end branch at rest

	const float InPitch = Profiles[PTactical].Pitch, LFov = Profiles[PTactical].Fov;

	float D = Profiles[PTactical].Dist * TacResolveDist * ArenaCamScale(G);

	const FVector C0(0.f, 0.f, CourtZ);

	const float A = ZoogiPortrait::ViewAspect(PC);

	const bool bPortraitView = !bLegacy && ZoogiPortrait::IsPortrait(A);

	if (AZoogiArena::OuterOutR > 0.f && !bPortraitView)

	{

		float V = 1.3f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiR2ViewMul="), V);

		D *= FMath::Clamp(V, 1.f, 2.f);

	}

	if (bLegacy)

	{

		OutFov = LFov;

		OutLoc = BoomLocation(C0, InYaw, InPitch, D);

		OutRot = (C0 - OutLoc).Rotation();

		return;

	}

	float T = 0.f, B = 0.f;

	ZoogiPortrait::Band(PC, A, T, B);

	D = PortraitBoom(G, A, T, B, InYaw, InPitch, LFov, D, true, C0, OutFov);

	OutLoc = BoomLocation(C0, InYaw, InPitch, D);

	OutRot = ZoogiPortrait::BandRotation(OutLoc, C0, OutFov, A, T, B);

}



bool AZoogiPlayerController::ElevatorPushAllowed(const AZoogiGameMode* G)

{

	const TCHAR* Cmd = FCommandLine::Get();

	if (FParse::Param(Cmd, TEXT("ZoogiNoElevatorPush"))) return false;

	if (!G || G->bFormShots || G->FormPass != 0) return false;

	if (FParse::Param(Cmd, TEXT("ZoogiElevatorPush"))) return true;

	if (!AZoogiSpawnElevator::IsEnabled()) return false; // also covers -ZoogiAuto / battle tests / -ZoogiNoForm / automation

	FString V;

	static const TCHAR* Params[] = { TEXT("ZoogiShotTest"), TEXT("ZoogiPowerTest"), TEXT("ZoogiTitanTest"), TEXT("ZoogiCamStuckTest"), TEXT("ZoogiUiTest"),

		TEXT("ZoogiHumanTest"), TEXT("ZoogiPlaytest"), TEXT("ZoogiFrozenPlayTest"), TEXT("ZoogiArenaPlayTest"), TEXT("ZoogiClimaxTest"), TEXT("ZoogiPinchTest"), TEXT("ZoogiRockTest") };

	for (const TCHAR* P : Params) if (FParse::Param(Cmd, P)) return false;

	static const TCHAR* Values[] = { TEXT("ZoogiMatchTest="), TEXT("ZoogiCardTest="), TEXT("ZoogiEpisodeTest="), TEXT("ZoogiSaveTest=") };

	for (const TCHAR* P : Values) if (FParse::Value(Cmd, P, V)) return false;

	return true;

}



void AZoogiPlayerController::UpdatePadView(AZoogiGameMode* G, AZoogiBall* Shooter, float Dt)

{

	// Launch pad camera (Evan Oct 22): on a pad (opening launch / after the glove crane) -> the aim view from behind the pad;

	// launched -> keeps that heading until the Zoogi lands in the arena -> PadBlend (0.5 s) into the normal camera.

	// Evan Oct 23: cam stay behind character when launching from aerial platform. PadView CVar defaults to 0 (SolvePose

	// soft-off), but PadPhase must still track pad -> flight -> land so bPadAim / AimViewYaw keep behind-Zoogi through

	// the launch beat. Without this, fire clears Ball->bOnLaunchPad, UpdatePadView zeros PadPhase, AimView drops, and

	// cam snaps to soft Tac mid-air. SolvePose remains gated on ZoogiLaunchPadCam::Enabled() elsewhere.

	if (!G || !Shooter || G->bCinematicCam)

	{

		if (PadPhase) UE_LOG(LogZoogi, Log, TEXT("CAM pad view OFF (%s)"), !G || !Shooter ? TEXT("no shooter") : TEXT("cinematic camera button"));

		PadPhase = 0; PadBlendT = G && G->bCinematicCam ? -1.f : PadBlendT;

		return;

	}

	FVector PadLoc = FVector::ZeroVector, Into = FVector::ZeroVector; FString Src;

	const bool bOn = G->Phase == EZoogiPhase::Aiming && ZoogiLaunchPadCam::Query(this, G, Shooter, PadLoc, Into, Src);

	if (bOn)

	{

		if (PadPhase != 1) UE_LOG(LogZoogi, Log, TEXT("CAM pad view ON for P%d (%s): pad (%.0f, %.0f, %.0f), into the arena yaw %.1f, aim yaw %.1f"),

			Shooter->PlayerIndex + 1, *Src, PadLoc.X, PadLoc.Y, PadLoc.Z, Into.Rotation().Yaw, Shooter->AimYaw);

		PadPhase = 1; PadSource = Src; PadLaunchYaw = Shooter->AimYaw; PadFlightT = 0.f; PadBlendT = -1.f;

		return;

	}

	if (PadPhase == 1) { PadPhase = 2; PadFlightT = 0.f; UE_LOG(LogZoogi, Log, TEXT("CAM pad view: P%d left the pad (heading %.1f)"), Shooter->PlayerIndex + 1, PadLaunchYaw); }

	if (PadPhase == 2)

	{

		PadFlightT += Dt;

		const FVector SL = Shooter->BallLocation();

		const bool bIn = !IsOffPlayArea(G->Arena, FVector2D(SL.X, SL.Y));

		const bool bLanded = bIn && PadFlightT > 0.1f && FMath::Abs(Shooter->GetBallVelocity().Z) < 60.f && SL.Z < ZoogiLaunch::PadHeight * 0.5f;

		if (Shooter->bOut || bLanded || PadFlightT > ZoogiLaunchPadCam::MaxFlight())

		{

			UE_LOG(LogZoogi, Log, TEXT("CAM pad view: %s after %.2f s -> blend %.2f s into the normal camera"), Shooter->bOut ? TEXT("out") : bLanded ? TEXT("landed") : TEXT("flight timeout"), PadFlightT, ZoogiLaunchPadCam::BlendTime());

			PadEnd = FString::Printf(TEXT("%s after %.2f s"), Shooter->bOut ? TEXT("out") : bLanded ? TEXT("landed") : TEXT("flight timeout"), PadFlightT);

			PadPhase = 0; PadBlendT = 0.f;

		}

	}

}



void AZoogiPlayerController::UpdateCamera(float Dt)

{

	{
		// Oct 27 (Dev Mode CAMERA tab): classic / 2v2 tactical camera numbers from the Camera Configuration Manager
		const FZoogiCamSettings& CS = ZoogiCamConfig::Get(ZoogiCamConfig::ModeName(this));

		ZoogiCamTune::FProfile& TP = ZoogiCamTune::Profiles[ZoogiCamTune::PTactical];

		TP.Pitch = CS.TacPitch; TP.Dist = CS.TacDist; TP.Fov = CS.TacFov; TP.PosLag = CS.TacLag;

	}


	using namespace ZoogiCamTune;

	AZoogiGameMode* G = GM();

	if (!Cam || !G) return;

	const float Scale = (G->Arena && G->Arena->IsNeon()) ? NeonScale : (G->Arena ? G->Arena->CamScale : 1.f); // Oct 14: Frozen Ring 2 1.39 (frames r2300), A 1.3 courts 1.15

	// Oct 17 portrait (9:16 .. 9:20): capped vertical FOV, width-keeping boom, HUD-safe band (ZoogiPortraitCam; no-op on landscape)

	const float PAspect = ZoogiPortrait::ViewAspect(this);

	const bool bPortrait = ZoogiPortrait::IsPortrait(PAspect);

	float PTop = 0.f, PBot = 0.f;

	ZoogiPortrait::Band(this, PAspect, PTop, PBot);



	if (G->Phase == EZoogiPhase::Forming)

	{

		// Stage forming: a slow sweep in to the art orientation (screen up = -Y) while the stage assembles.

		const float Pg = (G->Arena && G->Arena->Forming) ? G->Arena->Forming->GetProgress() : 1.f;

		FormingSweepPose(Pg, Scale, Yaw, Pitch, Dist, Fov, Pivot);

		float HFov = Fov;

		Dist = PortraitBoom(G, PAspect, PTop, PBot, Yaw, Pitch, Fov, Dist, true, Pivot, HFov);

		Dutch = 0.f;
		// Spawn-elevator rise over the intro's last 1.25 s (same window the GameMode starts the elevators in), timed off the
		// forming progress: brief pad flair then Tac soft chase by ElevPushOutEnd (teaser-bible; was ornate push-in -> sweep).
		const float FormLen = (G->Arena && G->Arena->Forming) ? G->Arena->Forming->GetLength() * G->FormTimeScale : 0.f;

		const float WinT = ElevPushWindow - (1.f - Pg) * FormLen;

		if (G->FormCamMode == 0 && FormLen > 0.f && WinT > 0.f && ElevatorPushAllowed(G))

		{

			if (!bElevPushFit)

			{

				TArray<FVector> Pads;

				for (const AZoogiBall* B : G->Balls) if (B && !B->bOut) Pads.Add(B->BallLocation() - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius));

				float FYaw, FPitch, FDist, FFov; FVector FPivot;

				FormingSweepPose(1.f, Scale, FYaw, FPitch, FDist, FFov, FPivot);

				float FHFov = FFov;

				FDist = PortraitBoom(G, PAspect, PTop, PBot, FYaw, FPitch, FFov, FDist, true, FPivot, FHFov);

				ElevPushPivot = FPivot; ElevPushPitch = FPitch; ElevPushDist = FDist;

				if (Pads.Num()) ElevatorPushFit(Pads, FYaw, FHFov, FDist, ElevPushPivot, ElevPushPitch, ElevPushDist, PAspect, PTop, PBot);

				bElevPushFit = true;

				UE_LOG(LogZoogi, Log, TEXT("CAM elevator push-in: %d pads, pivot %s pitch %.1f dist %.0f (sweep end %.0f)"), Pads.Num(), *ElevPushPivot.ToString(), ElevPushPitch, ElevPushDist, FDist);

			}
			// Teaser-bible: brief pad flair, then Tac soft chase (not ease-back to wide sweep).
			ElevatorRiseBlend(WinT, Scale, Pivot, Pitch, Dist, Fov, ElevPushPivot, ElevPushPitch, ElevPushDist, Pivot, Pitch, Dist, Fov);
			HFov = Fov; // keep Tac FOV; do not re-PortraitBoom (would re-expand Dist to court-wide and undo the land)
		}

		else if (WinT <= 0.f) bElevPushFit = false;

		if (G->FormCamMode == 1)

		{

			// Pillar close-up (forming shots): low view across the floor at the 22.5 deg Resonance pillar and its neighbour.

			Yaw = 22.5f + 25.f; Pitch = 10.f; Dist = 1250.f; Fov = 60.f;

			Pivot = FVector(560.f, 360.f, 120.f);

			Dist = PortraitBoom(G, PAspect, PTop, PBot, Yaw, Pitch, Fov, Dist, false, Pivot, HFov);

		}

		Look = Pivot;

		const FRotator Heading(0.f, Yaw, 0.f);

		const float Pr = FMath::DegreesToRadians(Pitch);

		const FVector Loc = Pivot - Heading.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));

		Cam->SetActorLocation(Loc);

		Cam->SetActorRotation(ZoogiPortrait::BandRotation(Loc, Look, HFov, PAspect, PTop, PBot));

		Cam->GetCameraComponent()->SetFieldOfView(HFov);

		ApplyPostProcess(Dt, PMenu, FVector::Dist(Loc, Look));

		CurProfile = PMenu;

		return;

	}

	

	if (G->Phase == EZoogiPhase::DropIntro)

	{

		// Live DROP INTRO cameras (Evan Oct 23 t772u): empty-cabin intro video -> FADE to Tac bird's-eye.

		// Music/SFX untouched. Snap Tac under fade (bHardCutPending); no bounce-off-ship.

		float HFov = 70.f;

		if (G->DropIntro.Step == EZoogiDropStep::ShipHold || G->DropIntro.Step == EZoogiDropStep::Idle)

		{

			const FTransform SX = ZoogiDropIntro::ShipWorldXform(G->Arena);

			const float Bob = FMath::Sin(G->DropIntro.StepAge * 2.1f) * 10.f;

			const float Sway = FMath::Sin(G->DropIntro.StepAge * 1.35f) * 5.f;

			const FVector ShipLoc = SX.GetLocation() + FVector(Sway, 0.f, Bob);

			// t772u: tight INSIDE-cabin hero (TransformPosition so 2.35x scale matches Parts + seats).
			// 258d271 used TransformVectorNoScale and sat inside Hull_Canopy (frosted white).
			// Canopy/Nose/SpotHousing already skipped -- Deck_Floor + Glass_Panorama + gold trim only.
			// Cam at rear of cabin looking toward panoramic glass / empty seat row (NO Zoogi seated).
			const FTransform ShipX(SX.GetRotation(), ShipLoc, SX.GetScale3D());
			const FVector CamLoc = ShipX.TransformPosition(FVector(0.f, 240.f, 112.f));
			Look = ShipX.TransformPosition(FVector(0.f, -170.f, 92.f));
			Pivot = Look;

			Yaw = (Look - CamLoc).Rotation().Yaw;

			Pitch = (Look - CamLoc).Rotation().Pitch;

			Dist = FVector::Dist(CamLoc, Look);

			Fov = 72.f;

			HFov = Fov;

			Cam->SetActorLocation(CamLoc);

			Cam->SetActorRotation((Look - CamLoc).Rotation());

			Cam->GetCameraComponent()->SetFieldOfView(HFov);

			ApplyPostProcess(Dt, PMenu, Dist);

			CurProfile = PMenu;

			return;

		}

		// TacPick / Falling: straight-down bird's-eye (Tac variant for this drop phase only).

		float TY, TP, TD, TF; FVector TPivot;

		const float ScaleDI = (G->Arena && G->Arena->IsNeon()) ? NeonScale : (G->Arena ? G->Arena->CamScale : 1.f);

		ZoogiDropIntro::TacBirdEyePose(ScaleDI, TY, TP, TD, TF, TPivot);

		if (G->DropIntro.bHasPick) TPivot = FVector(G->DropIntro.PickPoint.X, G->DropIntro.PickPoint.Y, ZoogiConst::ZoogiRadius);

		else if (G->DropIntro.Step == EZoogiDropStep::Falling && G->Balls.IsValidIndex(G->DropIntro.ActiveDrop) && G->Balls[G->DropIntro.ActiveDrop])

		{

			const FVector BL = G->Balls[G->DropIntro.ActiveDrop]->BallLocation();

			const float ZFollow = FMath::Clamp(BL.Z, ZoogiConst::ZoogiRadius, ZoogiConst::ZoogiRadius + 420.f);

			TPivot = FVector(BL.X, BL.Y, ZoogiConst::ZoogiRadius + (ZFollow - ZoogiConst::ZoogiRadius) * 0.22f);

		}

		if (G->DropIntro.bHardCutPending)

		{

			Yaw = TY; Pitch = TP; Dist = TD; Fov = 88.f; Pivot = TPivot; Look = TPivot; // FOV punch on smash cut

			G->DropIntro.bHardCutPending = false;

			UE_LOG(LogZoogi, Log, TEXT("CAM DROP INTRO: fade->Tac bird's-eye pitch %.1f dist %.0f"), Pitch, Dist);

		}

		else

		{

			Yaw = TY;

			Pitch = Damp(Pitch, TP, 8.f, Dt);

			Dist = Damp(Dist, TD, 6.f, Dt);

			Fov = Damp(Fov, TF, 6.f, Dt);

			Pivot = Damp(Pivot, TPivot, 5.f, Dt);

			Look = Damp(Look, TPivot, 8.f, Dt);

		}

		HFov = Fov;

		const FRotator Heading(0.f, Yaw, 0.f);

		const float Pr = FMath::DegreesToRadians(Pitch);

		const FVector Loc = Pivot - Heading.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));

		Cam->SetActorLocation(Loc);

		Cam->SetActorRotation((Look - Loc).Rotation());

		Cam->GetCameraComponent()->SetFieldOfView(HFov);

		ApplyPostProcess(Dt, PTactical, FVector::Dist(Loc, Look));

		CurProfile = PTactical;

		return;

	}

	// Oct 24 (Evan 1:23 PM): camera button cycles Normal <-> Tac; Tac is the straight-down bird's-eye (same pose as the drop intro).
	if (G->bBirdEyeCam && !G->bCinematicCam && !bMenuCam && G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::Forming && G->Phase != EZoogiPhase::DropIntro)
	{
		float TY, TP, TD, TF; FVector TPivot;
		const float ScaleBE = (G->Arena && G->Arena->IsNeon()) ? NeonScale : (G->Arena ? G->Arena->CamScale : 1.f);
		ZoogiDropIntro::TacBirdEyePose(ScaleBE, TY, TP, TD, TF, TPivot);
		if (AZoogiBall* AB = G->GetActiveBall()) { const FVector BL = AB->BallLocation(); if (BL.Z > -200.f) TPivot = FVector(BL.X, BL.Y, ZoogiConst::ZoogiRadius); }
		Yaw = TY;
		Pitch = Damp(Pitch, TP, 8.f, Dt);
		Dist = Damp(Dist, TD, 6.f, Dt);
		Fov = Damp(Fov, TF, 6.f, Dt);
		Pivot = Damp(Pivot, TPivot, 4.f, Dt);
		Look = Damp(Look, TPivot, 6.f, Dt);
		const FRotator HeadingBE(0.f, Yaw, 0.f);
		const float PrBE = FMath::DegreesToRadians(Pitch);
		const FVector LocBE = Pivot - HeadingBE.Vector() * Dist * FMath::Cos(PrBE) + FVector(0, 0, Dist * FMath::Sin(PrBE));
		Cam->SetActorLocation(LocBE);
		Cam->SetActorRotation((Look - LocBE).Rotation());
		Cam->GetCameraComponent()->SetFieldOfView(Fov);
		ApplyPostProcess(Dt, PTactical, FVector::Dist(LocBE, Look));
		CurProfile = PTactical;
		return;
	}
	if (bMenuCam && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling))

	{

		// Oct 15 watchdog: the menu orbit (whole map from far above) never survives into play

		bMenuCam = false; ++CamWatchFixes;

		UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH menu orbit cam left on during play (phase %d) -> battle follow view"), (int32)G->Phase);

	}

	if (bMenuCam || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)

	{

		AZoogiBall* W = CamTarget.Get();

		MenuOrbit += Dt * 6.f;

		Yaw = FRotator::NormalizeAxis(Yaw + FRotator::NormalizeAxis(MenuOrbit - Yaw) * (1.f - FMath::Exp(-2.f * Dt)));

		Pitch = Damp(Pitch, 38.f, 1.f, Dt);

		float MenuHFov = 90.f;

		const float MenuDist = PortraitBoom(G, PAspect, PTop, PBot, Yaw, 38.f, 90.f, (G->Phase == EZoogiPhase::GameOver ? 2600.f : 3600.f) * Scale,

			G->Phase != EZoogiPhase::GameOver, FVector(0, 0, -150.f), MenuHFov);

		Dist = Damp(Dist, MenuDist, 1.f, Dt);

		Fov = Damp(Fov, 90.f, 2.f, Dt);

		Dutch = Damp(Dutch, 0.f, 4.f, Dt);

		const FVector Target = (G->Phase == EZoogiPhase::GameOver && W) ? W->BallLocation() : FVector(0, 0, -150.f);

		Pivot = Damp(Pivot, Target, 2.f, Dt);

		Look = Damp(Look, Target, 4.f, Dt);

		const FRotator Heading(0.f, Yaw, 0.f);

		const float Pr = FMath::DegreesToRadians(Pitch);

		const FVector Loc = Pivot - Heading.Vector() * Dist * FMath::Cos(Pr) + FVector(0, 0, Dist * FMath::Sin(Pr));

		float MenuFov = Fov, MenuK = 1.f;

		ZoogiPortrait::AdaptFov(Fov, PAspect, MenuFov, MenuK);

		Cam->SetActorLocation(Loc);

		Cam->SetActorRotation(ZoogiPortrait::BandRotation(Loc, Look, MenuFov, PAspect, PTop, PBot));

		Cam->GetCameraComponent()->SetFieldOfView(MenuFov);

		ApplyPostProcess(Dt, PMenu, FVector::Dist(Loc, Look));

		CurProfile = PMenu;

		return;

	}



	AZoogiBall* Shooter = G->GetActiveBall();

	const FVector SL = Shooter ? Shooter->BallLocation() : FVector::ZeroVector;

	AActor* Lock = (G->TargetLock.IsValid() && !G->bSoftLock) ? G->TargetLock.Get() : nullptr;

	const FVector TL = Lock ? ActorCenter(Lock) : FVector::ZeroVector;

	// Stage -> camera: cinematic keeps the Oct 3 profiles (Draw hero, Awaken overview, Aim OTS, PowerUp battle swing,

	// Rolling follow then aerial, Resolve / End aerial); tactical uses one profile with small emphasis moves.

	enum class ECine { Draw, Standby, Main, Battle, End };

	ECine Cine = ECine::End;

	float T = G->StageTime;

	switch (G->Stage)

	{

	case EZoogiStage::Draw: Cine = ECine::Draw; break;

	case EZoogiStage::Awaken: Cine = ECine::Standby; break;

	case EZoogiStage::Battle:

		T = G->StepTime;

		Cine = G->BattleStep == EZoogiBattleStep::Aim ? ECine::Main : G->BattleStep == EZoogiBattleStep::PowerUp ? ECine::Battle : ECine::End;

		break;

	default: Cine = ECine::End; T = 99.f; break;

	}

	UZoogiAnimeSubsystem* Anime = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();

	const AZoogiBall* FocusBall = Cast<AZoogiBall>(AnimeFocus.Get());

	const bool bRadial = !(G->Arena && G->Arena->IsNeonRect());

	const bool bFocusGone = FocusBall && IsLeavingPlay(FocusBall->BallLocation(), FocusBall->bOut, CourtZ, G->Arena);

	const bool bAnimeShot = bAnimeCam && Anime && Anime->IsBusy() && AnimeFocus.IsValid() && !bFocusGone && !ZoogiRedirect::Picking(); // b66: a redirect pick is always the top view

	// Court height only from a shooter still in play (a falling Zoogi must not drag the camera pivots down with it).

	// launch pad camera (Oct 22): a Zoogi on a pad outside the court (or flying in from it) is not leaving play

	UpdatePadView(G, Shooter, Dt);

	const bool bPadView = PadPhase > 0 && Shooter && !Shooter->bOut;

	// pad-as-gameplay: Zoogi on a pad is outside the court but still in play (same as legacy bPadView)

	const bool bOnLaunchPad = Shooter && Shooter->IsOnLaunchPad();

	// b144c (Evan: "Camera gets stuck at high elevations"): CourtZ follows the turn Zoogi only while it is ON a floor (never up into the
	// air - the fall back read as "below the floor" and froze CourtZ at the top of the flight); a grounded Zoogi on the court resets it
	const bool bShooterOnFloor = Shooter && !Shooter->bOut && Shooter->Sphere && SL.Z > -200.f && ZoogiPhysics::IsGrounded(Shooter->Sphere, ZoogiConst::ZoogiRadius);
	if (bShooterOnFloor && !bPadView && !bOnLaunchPad && SL.Z < CourtZ - 1.5f * ZoogiConst::ZoogiRadius && !IsOffPlayArea(G->Arena, FVector2D(SL.X, SL.Y)))
	{
		UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH court height %.0f was stuck over P%d on the floor at %.0f -> reset"), CourtZ, Shooter->PlayerIndex + 1, SL.Z);
		CourtZ = SL.Z; ++CamWatchFixes;
	}
	const bool bShooterLeaving = !Shooter || (!bPadView && !bOnLaunchPad && IsLeavingPlay(SL, Shooter->bOut, CourtZ, G->Arena));

	if (!bShooterLeaving && SL.Z > -200.f && (bShooterOnFloor || bPadView || bOnLaunchPad || SL.Z < CourtZ)) CourtZ = SL.Z; // b144c

	if (CineDragTest() && !G->bCinematicCam && G->Phase != EZoogiPhase::Menu) G->bCinematicCam = true; // -ZoogiCineDragTest
	const bool bTac = !G->bCinematicCam;



	// Lock changes in Main swing (switching = whip-pan).

	if (G->LockSerial != SeenLockSerial)

	{

		const bool bHadLock = SeenLockSerial >= 0 && SwingAge < 100.f;

		SeenLockSerial = G->LockSerial;

		if (Lock && G->IsAimStep()) { bWhip = bHadLock; SwingAge = 0.f; }

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

	bool bCourtView = false; FVector CourtCentre = FVector::ZeroVector; // portrait: whole-court views fit the court / Ring 2 (PortraitBoom)

	bool bPortraitAim = false; FVector LegacyPivot = FVector::ZeroVector; // portrait aim / rest framing (tactical branch)

	bool bPortraitChase = false; float ChaseElev = 0.f, ChaseBoomD = 0.f; // portrait AIM chase view (tablet reference)

	// Aim view (Evan Oct 17): normal aiming (no pinpoint lock) on a landscape view, tactical camera; the portrait view keeps its own

	// the launch pad view is the aim view from the pad (any lock, not only the aim step; the camera buttons still override)

	// pad-clear-aim (Evan Oct 22 6:57 PM): SharedPlatform hovers outside the arena. PadView=0 soft top-down looks at the floor /

	// leaves Zoogi off-screen. While IsOnLaunchPad, use the normal AIM view (behind Zoogi along AimYaw, aim boom/pitch/ScreenY) ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â

	// NOT the 7398f61 SolvePose special 2/3 framing. Buttons-only still; 811d899 ring-clamp bypass stays via bOnLaunchPad.

	const bool bPadClearAim = bOnLaunchPad && !bAnimeShot && !G->bCinematicCam && !bShooterLeaving;

	const bool bPadAim = (bPadView || bPadClearAim) && !bAnimeShot && !G->bCinematicCam && !bShooterLeaving && ZoogiFramed::ViewAspect(this) >= 1.f;

	// portrait: same clear-aim rule -- portrait aim chase behind the pad along AimYaw (zoogi.Cam.PadPortrait)

	const bool bPadPortrait = (bPadView || bPadClearAim) && !bAnimeShot && !G->bCinematicCam && !bShooterLeaving && bPortrait && ZoogiPortrait::AimFraming() && ZoogiLaunchPadCam::PortraitOn();

	// combo-clear-aim (Evan Oct 23): ComboStartMatch spawns on the arena floor (GComboSpawns), never SharedPlatform,

	// so bPadClearAim never arms. Battle Aim then kept soft top-down whenever TargetLock is set (CPU / HumanTest /

	// tap-target), while 1v1 pad AIM bypasses Lock via bPadAim. Mirror that for combo YOUR TURN / DRAG TO AIM.

	// Harden: bPadClearAim does NOT check ZoogiAimView::Enabled(); b50c74b gated combo on Enabled and failed the

	// PASS/FAIL matrix (1v1 pad PASS / combo FAIL) when AimView is soft-off. Banner-aligned Battle Aim predicate

	// (same as DRAG TO AIM / YOUR TURN) instead of IsAimStep alone, still Lock-bypassing via bAimViewWanted.

	// FIX3 (Evan Oct 23): drop !bShooterLeaving + ViewAspect - Meadow GComboSpawns sit near hub pillars and

	// IsLeavingPlay / aspect flaked soft-Tac on DRAG TO AIM while Volcanic PASS'd under the same Lock-bypass.

	// Battle Aim banner (YOUR TURN / DRAG TO AIM) alone arms behind-Zoogi; pad clear-aim keeps its own leaving gate.

	const bool bComboClearAim = G->bComboMode && G->Phase == EZoogiPhase::Aiming

		&& G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Aim

		&& !bAnimeShot && !G->bCinematicCam && Shooter;

	// b119b (Evan: "fix horizontal camera" - landscape training, the Zoogi off the bottom edge): a target lock (guided / tapped target)
	// dropped the human's aim step to the soft tactical view, its pivot pulled halfway to the target - the shooter fell under the AUTO /
	// ARC bar. The human's landscape aim step now always gets the behind-the-Zoogi aim view (heading = the aim arrow, which points at
	// the locked target anyway). -ZoogiAimLockTac = the old lock -> tactical view.
	static const bool bAimLockTac = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAimLockTac"));
	const bool bHumanLandAim = !bAimLockTac && !bAnimeShot && !G->bCinematicCam && Shooter && !Shooter->bCPU && !bShooterLeaving && ZoogiAimView::Enabled()
		&& ZoogiFramed::ViewAspect(this) >= 1.f && G->IsAimStep() && !G->bRoyaleMode;
	const bool bAimViewWanted = bPadAim || bComboClearAim || bHumanLandAim || (!bAnimeShot && !G->bCinematicCam && Shooter && !bShooterLeaving && !Lock && ZoogiAimView::Enabled()

		&& ZoogiFramed::ViewAspect(this) >= 1.f && (G->IsAimStep() || (bAimViewForce && G->Phase == EZoogiPhase::Aiming)));

	if (bAimViewWanted && !bAimViewActive)

		UE_LOG(LogZoogi, Log, TEXT("CAM aim view ON for P%d: heading %.1f (pitch %.1f [0 = auto, horizon at %.2f], boom %.0f, screen y %.2f, yaw follow %.2f s)"), Shooter->PlayerIndex + 1, Shooter->AimYaw, ZoogiAimView::Pitch(), ZoogiAimView::HorizonY(), ZoogiAimView::Boom(), ZoogiAimView::ScreenY(), ZoogiAimView::YawTime());

	AimViewAge = bAimViewWanted ? (bAimViewActive ? AimViewAge + Dt : 0.f) : 0.f;

	bAimViewActive = bAimViewWanted;

	if (bAimViewActive) AimViewYaw = FRotator::NormalizeAxis(PadPhase == 2 ? PadLaunchYaw : Shooter->AimYaw); // (in flight from a pad: the launch heading)

	// orbit with the aim arrow (Evan 2:46 PM: "the camera should rotate with it")

	if (bAnimeShot)

	{

		PIdx = PAnime;

		const FVector F = ActorCenter(AnimeFocus.Get());

		WantYaw = AnimeYaw;

		WantPivot = F;

		WantLook = F + FVector(0, 0, 15.f);

		DistMul = AnimeDistMul * (1.f - 0.18f * FMath::Clamp(Anime->Age / FMath::Max(Anime->Length, 0.1f), 0.f, 1.f));

		ProbeFrom = F + FVector(0, 0, 60.f);

	}

	else if (bTac)

	{

		PIdx = PTactical;

		const FVector C0(0.f, 0.f, CourtZ);

		FVector AimP = C0;

		bool bHasAim = false;

		if (Lock) { AimP = TL; bHasAim = true; }

		else if (G->bFreeAim) { AimP = FVector(G->FreeAimPoint.X, G->FreeAimPoint.Y, SL.Z); bHasAim = true; }

		// aim-arrow camera hold removed at the game-camera-aim merge (Evan, Oct 17): the camera turns with the arrow (aim view orbits

		// behind the Zoogi along it); the arrow grab no longer freezes the tactical framing. The Zoogi still faces the arrow.

		const bool bSettled = G->Stage == EZoogiStage::Resolve || G->Stage == EZoogiStage::End;

		if (G->Phase == EZoogiPhase::Aiming && !bSettled)

		{

			FVector D = (bHasAim ? AimP : C0) - SL;

			D.Z = 0.f;

			if (D.Size() > 150.f) TacYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));

		}

		WantYaw = TacYaw;

		DistMul = Scale;

		if (G->Stage == EZoogiStage::Draw || G->Stage == EZoogiStage::Awaken)

		{

			WantPivot = FVector(SL.X, SL.Y, CourtZ);

			// Awaken: the card panel covers the lower part of the screen; slide the pivot toward the camera so the Zoogi sits above it.

			if (G->Stage == EZoogiStage::Awaken && Shooter && !Shooter->bCPU) WantPivot -= FRotator(0.f, TacYaw, 0.f).Vector() * 230.f;

			DistMul = TacDrawZoom * Scale;

		}

		else if (G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Aim)

		{

			WantPivot = FMath::Lerp(SL, AimP, TacAimLook);

			// Human aiming: the aim control bar covers the bottom of the screen; slide the view so the shooter sits above it.

			if (Shooter && !Shooter->bCPU) { WantPivot -= FRotator(0.f, TacYaw, 0.f).Vector() * 150.f; DistMul *= 1.12f; }

		}

		else if (G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::PowerUp)

		{

			WantPivot = FMath::Lerp(SL, AimP, 0.3f);

			DistMul = TacPowerZoom * Scale;

		}

		else if (G->Phase == EZoogiPhase::Rolling)

		{

			// Ring-out safe: clamped to the court, the arena centre once the shooter leaves play (was: followed it down to -600).

			WantPivot = TacticalRollPivot(SL, !Shooter || Shooter->bOut, CourtZ, G->Arena);

			DistMul = 1.05f * Scale;

			// Oct 27 Phase 3 (Evan's spec, build 9): STRATEGIC ZOOM on every classic launch - the boom pulls out and up over

			// ~0.5 s so the whole play (shooter, targets, edges) reads while the shot rolls; Resolve then frames the arena as before.

			if (!G->bComboMode && !G->bRoyaleMode)

			{

				const float Z = SmoothStep(G->PhaseTime / StratZoomIn);

				DistMul = FMath::Lerp(1.05f, StratZoomDist, Z) * Scale;

				WantPivot = FMath::Lerp(WantPivot, FVector(0.f, 0.f, CourtZ), StratZoomCentre * Z);

				PitchOff += StratZoomPitch * Z;

			}

		}

		else

		{

			WantPivot = C0;

			DistMul = TacResolveDist * Scale;

			bCourtView = true; CourtCentre = C0;

			// Oct 15 Frozen Ring 2: the resolve / end view frames the whole outer ring (r2300) inside the HUD frame's

			// viewport (~0.65 of the screen width): 1.39 alone clipped its left / right / front edges. -ZoogiR2ViewMul=

			if (AZoogiArena::OuterOutR > 0.f && !bPortrait) // portrait: PortraitBoom fits Ring 2 itself

			{

				static const float R2ViewMul = [] { float V = 1.3f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiR2ViewMul="), V); return FMath::Clamp(V, 1.f, 2.f); }();

				DistMul *= R2ViewMul;

			}

			if (G->Arena && G->Arena->IsNeonRect()) WantYaw = TacYaw = (FMath::Abs(FRotator::NormalizeAxis(Yaw - 90.f)) <= 90.f ? 90.f : -90.f);

		}

		// Oct 17 portrait aim / rest (Evan's tablet build): from behind the shooter on its pad, looking up the arena along the

		// screen's long axis; the safe band replaces the landscape aim-bar / card-panel slides (PortraitAimDist fits the shooter)

		const bool bRestStep = G->Stage == EZoogiStage::Draw || G->Stage == EZoogiStage::Awaken;

		const bool bAimStep = G->Stage == EZoogiStage::Battle && (G->BattleStep == EZoogiBattleStep::Aim || G->BattleStep == EZoogiBattleStep::PowerUp);

		if (bPortrait && ZoogiPortrait::AimFraming() && Shooter && !bShooterLeaving && (bRestStep || bAimStep || bPadPortrait))

		{

			LegacyPivot = WantPivot;

			const FVector Tgt = (bHasAim && !bRestStep) ? AimP : C0;

			WantPivot = FMath::Lerp(SL, FVector(Tgt.X, Tgt.Y, SL.Z), bRestStep ? ZoogiPortrait::RestAhead() : ZoogiPortrait::AimAhead());

			WantPivot.Z = FMath::Max(SL.Z, CourtZ);

			bPortraitAim = true;

			if (bPadPortrait || (G->BattleStep == EZoogiBattleStep::Aim && G->Stage == EZoogiStage::Battle))

			{

				if (bPadPortrait) WantYaw = TacYaw = FRotator::NormalizeAxis(PadPhase == 2 ? PadLaunchYaw : Shooter->AimYaw);

				// AIM: low chase view from just behind the Zoogi, looking along the aim line (straight up the screen)

				float Ahead = 0.f, HF = 0.f;

				PortraitChaseBoom(G, PAspect, Profiles[PTactical].Fov, ChaseElev, ChaseBoomD, Ahead, HF);

				WantPivot = SL;

				bPortraitChase = true;

				WantLook = SL + FRotator(0.f, TacYaw, 0.f).Vector() * Ahead;

				WantLook.Z = SL.Z;

			}

		}

		if (!bPortraitChase) WantLook = WantPivot;

		ProbeFrom = WantPivot + FVector(0, 0, 150.f);

		// portrait aim / rest: the longer, lower boom behind the pad passes the court's pillars; probe from high over the look

		// point (as the aim zoom does) so a pillar between the shooter and the camera doesn't pull the view in onto the pad

		if (bPortraitAim) ProbeFrom = WantPivot + FVector(0, 0, ZoogiPortrait::AimProbeHeight());

		// chase: the boom is short and low; probe from just over the shooter, so a pillar between it and the camera pulls the

		// camera in along the boom (it never goes through one)

		if (bPortraitChase) ProbeFrom = SL + FVector(0, 0, 60.f);

		if (bAimViewActive)

		{

			// behind and above the Zoogi along the aim arrow, pitched AimPitch down, the Zoogi at AimScreenY: pivot on the Zoogi,

			// boom at the elevation that puts it there, look point ahead along the (smoothed) camera heading

			const FProfile& TP = Profiles[PTactical];

			WantFov = ZoogiAimView::Fov(TP.Fov);

			// b67 (Evan: on a wide phone the player sat at the bottom edge): the vertical half-FOV honouring the axis constraint - on screens wider
			// than 16:9 the engine keeps the VERTICAL FOV, so tan/aspect was ~2x too small and the shooter landed far below AimScreenY
			float TanHu = 0.f, TanV = 0.f; ZoogiPortrait::TanHalf(WantFov, FMath::Max(ZoogiFramed::ViewAspect(this), 0.1f), TanHu, TanV);

			float Elev, Ahead;

			// on a launch pad: zoogi.Cam.PadBoom / PadPitch / PadScreenY (0 = the aim view's own; the normal aim view never changes)

			const float AVBoom = bPadAim ? ZoogiLaunchPadCam::Boom(ZoogiAimView::Boom()) : ZoogiAimView::Boom();

			AimViewPitchUsed = bPadAim ? ZoogiLaunchPadCam::Pitch(ZoogiAimView::ViewPitch(TanV)) : ZoogiAimView::ViewPitch(TanV);

			ZoogiAimView::ChaseBoom(AVBoom, AimViewPitchUsed, bPadAim ? ZoogiLaunchPadCam::ScreenY(ZoogiAimView::ScreenY()) : ZoogiAimView::ScreenY(), TanV, Elev, Ahead);

			AimViewElevUsed = Elev;

			WantYaw = TacYaw = AimViewYaw;

			AimViewAhead = Ahead;

			WantPivot = SL;

			WantLook = SL + FRotator(0.f, AimViewYaw, 0.f).Vector() * Ahead;

			DistMul = AVBoom / FMath::Max(TP.Dist, 1.f);

			PitchOff = Elev - TP.Pitch;

			// FIX3: probe from higher over the Zoogi (portrait AimProbeHeight band) so Meadow hub pillars between

			// the boom and the subject don't collapse the chase into a soft-Tac-looking pull-up.

			ProbeFrom = SL + FVector(0, 0, FMath::Max(60.f, ZoogiPortrait::AimProbeHeight() * 0.5f));

		}

	}

	else switch (Cine)

	{

	case ECine::Draw:

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

	case ECine::Standby:

	{

		PIdx = PStandby;

		StandbyYaw += StandbyDrift * Dt;

		WantYaw = StandbyYaw;

		WantPivot = FVector(0, 0, -50.f);

		WantLook = WantPivot;

		DistMul = Scale;

		ProbeFrom = FVector(0, 0, 200.f);

		bCourtView = true; CourtCentre = WantPivot;

		break;

	}

	case ECine::Main:

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
		// Oct 29 (Evan, build 19): cinematic aim follows the drag arrow like CAM mode - the camera sits behind the shooter along the
		// arrow at the same Main rear distance; pulling back pushes it in toward the pre-launch Battle shoulder pose (boom, pitch,
		// FOV, shoulder offset) and it eases back out as the pull returns to zero. -ZoogiNoCineDrag restores the fixed view.
		if (CineDragOn() && Shooter && !Shooter->bCPU && !Shooter->bOut && G->IsHumanAiming() && G->IsAimStep())
		{
			float Raw = G->HumanDragFrac > 0.f ? FMath::Clamp(G->HumanDragFrac / 0.9f, 0.f, 1.f) : 0.f;
			if (CineDragTest())
			{
				Raw = CineDragTestPull(T);
				if (T > 1.f && T < 2.6f) Shooter->AimYaw = FRotator::NormalizeAxis(Shooter->AimYaw + 14.f * Dt); // the arrow swings; the camera follows
				static const float ShotT[4] = { 0.7f, 1.3f, 2.4f, 3.9f };
				static const TCHAR* ShotN[4] = { TEXT("rest"), TEXT("half"), TEXT("full"), TEXT("back") };
				if (CineTestShot < 4 && T >= ShotT[CineTestShot] && !G->ShotDir.IsEmpty())
				{
					FScreenshotRequest::RequestScreenshot(FPaths::Combine(G->ShotDir, FString::Printf(TEXT("cine_%d_%s.png"), CineTestShot, ShotN[CineTestShot])), true, false);
					UE_LOG(LogZoogi, Log, TEXT("CINEDRAG shot %s: pull %.2f k %.2f dist %.0f fov %.1f"), ShotN[CineTestShot], Raw, CineDragK, Dist, Fov);
					++CineTestShot;
				}
			}
			CineDragK = Damp(CineDragK, Raw, 10.f, Dt);
			const float K = SmoothStep(CineDragK);
			WantYaw = FRotator::NormalizeAxis(Shooter->AimYaw);
			const FVector Dir = FRotator(0.f, WantYaw, 0.f).Vector();
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
			WantPivot = SL + Right * (ShoulderOffset * K);
			WantLook = SL + Dir * FMath::Lerp(400.f, 700.f * BattleLookBias, K) + FVector(0, 0, FMath::Lerp(20.f, 15.f, K));
			DistMul = FMath::Lerp(1.f, Profiles[PBattle].Dist / FMath::Max(1.f, Profiles[PMain].Dist), K);
			PitchOff = (Profiles[PBattle].Pitch - Profiles[PMain].Pitch) * K;
			WantFov = FMath::Lerp(Profiles[PMain].Fov, Profiles[PBattle].Fov, K);
			ProbeFrom = SL + FVector(0, 0, 60.f);
		}
		else CineDragK = 0.f;


		break;

	}

	case ECine::Battle:

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

			const float Aspect = ZoogiFramed::ViewAspect(this); // framed HUD: the arena window's real view-rect aspect (full viewport otherwise)

			const FVector CL = Cam->GetActorLocation();

			const float SP = (SL - FVector(0, 0, ZoogiConst::ZoogiRadius) - CL).Rotation().Pitch;

			const float TP = (TL + FVector(0, 0, 2.f * ZoogiConst::ZoogiRadius) - CL).Rotation().Pitch;

			const float Span = TP - SP;

			if (Span > 0.f && !bPortrait) // portrait: the capped vertical FOV is already far taller than the landscape view

			{

				const float VNeed = Span / (BattleShooterMaxDrop + BattleTargetMaxRise);

				const float HNeed = 2.f * FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VNeed)) * Aspect));

				WantFov = FMath::Max(WantFov, FMath::Min(HNeed, BattleFovMax));

			}

		}

		break;

	}

	case ECine::End:

	default:

	{

		const bool bGone = bShooterLeaving; // ring-out safe: stop following as soon as the shooter leaves play

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

			bCourtView = true; CourtCentre = FVector(0, 0, CourtZ);

			if (G->Arena && G->Arena->IsNeonRect())

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



	// Oct 15 (Evan stuck cam): during play the view centre / look point stay over the arena (a far aim point or a lock on

	// something flying away can't drag the follow view out over the scenery)

	// pad-clamp2-bypass (Evan Oct 22 playtest): SharedPlatform sits outside knockoffR+250. This Clamp2 had no IsOnLaunchPad

	// skip, so it yanked WantPivot/WantLook onto the ring and defeated pad-clear-aim (74ce370) -> soft top-down floor view.

	// Skip while on pad (same rule as 811d899 ZoomOff bypass / CAMWATCH). Keep normal AIM; do not revive 7398f61 SolvePose.

	if (G->Phase == EZoogiPhase::Aiming && !bAnimeShot && bRadial && !bOnLaunchPad)

	{

		const float RMax = (G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) + 250.f;

		auto Clamp2 = [RMax](FVector& V) { const FVector2D V2(V.X, V.Y); if (V2.Size() > RMax) { const FVector2D C = V2.GetSafeNormal() * RMax; V.X = C.X; V.Y = C.Y; } };

		Clamp2(WantPivot); Clamp2(WantLook);

	}
	{	// b175 (Evan: "If losing grip gravitate the camera over to view"): while a Zoogi hangs off the edge / loses its grip the view
		// drifts over to it (no cut, no zoom), then eases back. -ZoogiNoHangDrift = off.
		static const bool bNoDrift = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHangDrift"));
		static float HangK = 0.f; static FVector HangAt = FVector::ZeroVector;
		const ZoogiEdgeHang::FView& Hv = ZoogiEdgeHang::View();
		const bool bHang = !bNoDrift && Hv.bOn && Hv.Phase <= 3 && !Hv.At.IsNearlyZero();
		if (bHang) HangAt = Hv.At;
		HangK = FMath::FInterpTo(HangK, bHang ? 1.f : 0.f, Dt, bHang ? 3.5f : 1.6f);
		if (HangK > 0.01f && !HangAt.IsNearlyZero())
		{
			const float E = HangK * HangK * (3.f - 2.f * HangK);
			WantPivot = FMath::Lerp(WantPivot, HangAt, 0.65f * E);
			WantLook = FMath::Lerp(WantLook, HangAt, 0.85f * E);
		}
	}

	// ---- Oct 29 evening (Evan, build 20): ACTION CAMERAS while the shot rolls - side tracking, low chase, a side angle on the
	// first impact, then following whatever got hit hardest, a wide low side angle over the stands while it settles and a slow
	// orbit; hard cuts between shots. CINE + CAM modes (not the TAC bird's-eye, not royale). -ZoogiNoActionCam = off.
	bool bActCut = false;
	{
		static const bool bActOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoActionCam"));
		static const bool bActShots = FParse::Param(FCommandLine::Get(), TEXT("ZoogiActionCamShots"));
		const bool bLive = bActOn && G->Phase == EZoogiPhase::Rolling && !bAnimeShot && !G->bBirdEyeCam && !G->IsRoyaleMatch() && Shooter && PadPhase != 2 && G->PhaseTime > 0.55f;
		// b62 (Evan: "Implement smart cam"): instead of hard cuts between fixed shots, one continuous camera that frames everything that
		// matters - the shooter plus whatever is moving fast after a hit - zooming out to fit them, turning with the roll, never cutting.
		// The camera probe keeps it in front of scenery. -ZoogiNoSmartCam = the Oct 29 action cuts.
		static const bool bSmartOn = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoSmartCam"));
		if (!bLive) { ActShot = -1; ActYawInit = false; }
		else if (bSmartOn)
		{
			auto Gone = [&](const AActor* A) -> bool
			{
				if (!A) return true;
				if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->bOut || AZoogiArena::IsRingOut(B->BallLocation());
				if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->bOut || AZoogiArena::IsRingOut(O->OrbLocation());
				return false;
			};
			// b131 (Evan: the camera flew up into the sky, the arena tiny in a corner): only bodies over the court (not flung high / far
			// away) are framed, and the framing centre stays on the court. -ZoogiSmartCamAll = the old framing.
			static const bool bFrameAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSmartCamAll"));
			const FVector ArC = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const float ArR = G->Arena ? G->Arena->GetKnockoffRadius() : 2000.f;
			auto OnCourt = [&](const AActor* A) { if (bFrameAll) return true; const FVector P = ActorCenter(A); return FVector::Dist2D(P, ArC) < ArR * 1.1f && P.Z < CourtZ + 450.f; };
			TArray<TPair<float, AActor*>> Movers;
			for (AZoogiBall* B : G->Balls) if (B && B != Shooter && !Gone(B) && OnCourt(B)) { const float S = B->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, B }); }
			for (AZoogiOrb* O : G->Orbs) if (O && !Gone(O) && OnCourt(O)) { const float S = O->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, O }); }
			Movers.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B) { return A.Key > B.Key; });
			FVector Sum = FVector::ZeroVector; float Wt = 0.f;
			TArray<FVector, TInlineAllocator<6>> Pts;
			const bool bShooterIn = !Gone(Shooter);
			// b180 (Evan: "camera needs to follow player better and especially towards the edges"): the shooter weighs more, the
			// framing centre slides onto the shooter near the rim, catches up faster and leads further. -ZoogiOldFollowCam = b179
			static const bool bOldFollow = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldFollowCam"));
			if (bShooterIn) { const FVector P0 = ActorCenter(Shooter); const float Ws = bOldFollow ? 2.f : 3.5f; Sum += P0 * Ws; Wt += Ws; Pts.Add(P0); }
			for (int32 k = 0; k < FMath::Min(3, Movers.Num()); ++k) { const FVector Pk = ActorCenter(Movers[k].Value); Sum += Pk; Wt += 1.f; Pts.Add(Pk); }
			FVector C = Wt > 0.f ? Sum / Wt : FVector(0.f, 0.f, CourtZ);
			if (!bFrameAll)
			{	// b131: on the court, at court height (a shooter flying off the map no longer drags the camera up and away)
				FVector2D C2(C.X - ArC.X, C.Y - ArC.Y);
				if (!bOldFollow && bShooterIn)
				{	// b180: toward the rim the shooter takes over the framing (so the edge it is heading for stays in view)
					const FVector CamSp0 = ActorCenter(Shooter);
					const float CamEdgeK = FMath::Clamp((FVector::Dist2D(CamSp0, ArC) / ArR - 0.55f) / 0.4f, 0.f, 1.f);
					C2 = FMath::Lerp(C2, FVector2D(CamSp0.X - ArC.X, CamSp0.Y - ArC.Y), 0.75f * CamEdgeK);
				}
				const float CamMaxC = bOldFollow ? ArR : ArR * 1.12f;
				if (C2.Size() > CamMaxC) C2 = C2.GetSafeNormal() * CamMaxC;
				C = FVector(ArC.X + C2.X, ArC.Y + C2.Y, FMath::Clamp((float)C.Z, CourtZ - 100.f, CourtZ + 300.f));
			}
			float Rad = 300.f;
			for (const FVector& Pp : Pts) Rad = FMath::Max(Rad, FVector::Dist2D(Pp, C) + 220.f);
			if (!bFrameAll) Rad = FMath::Min(Rad, ArR * 1.2f);
			// b122d (Evan: "if it's mid-roll, target select from that angle - don't switch to what it looks like at turn-start aiming"): a
			// mid-roll pick (REDIRECT, a COMBO cast, the superpower pick) keeps the roll camera as it is; -ZoogiRedirectWide = the b66 zoom-out
			static const bool bRedirWide = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectWide"));
			if (bRedirWide && ZoogiRedirect::Picking()) Rad = FMath::Max(Rad, ZoogiRedirect::Range(G) + 150.f); // b66: the whole range ring + its targets in view
			// heading: along the shooter's roll while it moves, else hold
			FVector Vd = bShooterIn ? Shooter->GetVelocity() : FVector::ZeroVector; Vd.Z = 0.f;
			if (!ActYawInit) { ActWideYaw = Yaw; ActSmartC = C; ActSmartR = Rad; ActYawInit = true; }
			// b122d: a mid-roll pick holds the roll camera exactly where it was (no drift, no zoom)
			const UZoogiFunSys* FunPk = UZoogiFunSys::Get(this);
			const bool bMidRollPick = ZoogiSkills::Picking() || ZoogiRedirect::Picking() || (FunPk && FunPk->UltAimOn());
			if (!bMidRollPick)
			{
				if (Vd.Size() > 160.f) ActWideYaw = FRotator::NormalizeAxis(ActWideYaw + FRotator::NormalizeAxis(Vd.Rotation().Yaw - ActWideYaw) * (1.f - FMath::Exp(-1.2f * Dt)));
				// b180: catch up faster, faster still when the shooter is running away from the frame
				const float CamCatchUp = bOldFollow ? 3.5f : FMath::Lerp(5.5f, 9.f, FMath::Clamp((FVector::Dist2D(ActSmartC, C) - 250.f) / 700.f, 0.f, 1.f));
				ActSmartC = FMath::VInterpTo(ActSmartC, C, Dt, CamCatchUp);
				ActSmartR = FMath::FInterpTo(ActSmartR, Rad, Dt, Rad > ActSmartR ? 4.f : 1.2f); // zoom out fast, back in slowly
			}
			const float FD = Profiles[PFollow].Dist, FPit = Profiles[PFollow].Pitch;
			const float Fit = ActSmartR / FMath::Tan(FMath::DegreesToRadians(84.f * 0.5f)) * 1.35f;
			const float CDist = FMath::Clamp(Fit, FD * 0.7f, FD * 1.3f);
			const float CPitch = FMath::Clamp(FPit + (ActSmartR - 600.f) * 0.006f, FPit - 4.f, FPit + 10.f);
			PIdx = PFollow;
			WantYaw = ActWideYaw;
			const float CamLeadK = bOldFollow ? 140.f : FMath::Clamp(Vd.Size() * 0.18f, 140.f, 360.f); // b180: lead with the speed
			WantPivot = ActSmartC + (Vd.Size() > 160.f ? Vd.GetSafeNormal() * CamLeadK : FVector::ZeroVector); WantLook = WantPivot;
			PitchOff = CPitch - FPit;
			DistMul = CDist / FMath::Max(1.f, FD);
			WantFov = 84.f;
			bCourtView = false;
			ProbeFrom = WantPivot + FVector(0, 0, 80.f);
			static double LastLog = 0.0;
			if (FPlatformTime::Seconds() - LastLog > 2.0) { LastLog = FPlatformTime::Seconds(); UE_LOG(LogZoogi, Verbose, TEXT("SMARTCAM %d bodies, r %.0f, dist %.0f"), Pts.Num(), ActSmartR, CDist); }
		}
		else
		{
			const float Now = G->PhaseTime;
			auto Speed2 = [](const AActor* A) { return A ? (float)A->GetVelocity().Size2D() : 0.f; };
			auto Gone = [&](const AActor* A) -> bool
			{
				if (!A) return true;
				if (const AZoogiBall* B = Cast<AZoogiBall>(A)) return B->bOut || AZoogiArena::IsRingOut(B->BallLocation());
				if (const AZoogiOrb* O = Cast<AZoogiOrb>(A)) return O->bOut || AZoogiArena::IsRingOut(O->OrbLocation());
				return false;
			};
			// Oct 29 night (Evan, build 23): every action shot stays out at the after-launch overhead distance (Follow profile ~2400 cm,
			// high pitch) and only the ANGLE changes - no low chase / impact close-ups.
			static const TCHAR* ShotNames[] = { TEXT("overhead"), TEXT("high_side"), TEXT("impact_34"), TEXT("follow_hit"), TEXT("top_down"), TEXT("orbit") };
			auto Pick = [&](int32 S, AActor* F, float Len)
			{
				ActShot = S; ActFocus = F; ActT0 = Now; ActLen = Len; bActCut = ActCuts > 0; ActYawInit = false; ++ActCuts;
				UE_LOG(LogZoogi, Log, TEXT("ACTIONCAM cut %d -> %s (%s) at %.2f s"), ActCuts, ShotNames[S], F ? *F->GetName() : TEXT("-"), Now);
				if (bActShots && ActCuts <= 30) { ActShotAt = Now + 0.32f; ActShotName = FString::Printf(TEXT("act_%02d_%s"), ActCuts, ShotNames[S]); }
			};
			const int32 Ser = ZoogiShow::ImpactSerial();
			if (ActShot < 0) { ActSeenImpact = Ser; ActCuts = 0; ActYawInit = false; Pick(0, Shooter, 1.6f); }
			else if (Ser != ActSeenImpact && ActShot != 2) { ActSeenImpact = Ser; ActImpactAt = ZoogiShow::ImpactAt(); ActImpactDir = (ActImpactAt - Shooter->BallLocation()).GetSafeNormal2D(); Pick(2, ZoogiShow::ImpactOther(), 1.05f); }
			else if (Now - ActT0 > ActLen || ((ActShot == 0 || ActShot == 1 || ActShot == 3) && Gone(ActFocus.Get())))
			{
				if (ActShot == 2)
				{
					AActor* Best = nullptr; float Bv = 260.f;
					for (AZoogiBall* B : G->Balls) if (B && B != Shooter && !Gone(B) && Speed2(B) > Bv) { Bv = Speed2(B); Best = B; }
					for (AZoogiOrb* O : G->Orbs) if (O && !Gone(O) && Speed2(O) > Bv) { Bv = Speed2(O); Best = O; }
					if (Best) Pick(3, Best, 1.7f); else Pick(4, Shooter, 2.2f);
				}
				else if (Gone(Shooter) || Speed2(Shooter) < 160.f) Pick(ActShot == 4 ? 5 : 4, Shooter, ActShot == 4 ? 3.f : 2.2f);
				else Pick(ActShot == 0 ? 1 : (ActShot == 1 ? 4 : 0), Shooter, ActShot == 0 ? 1.4f : 1.6f);
			}
			if (ActShotAt > 0.f && Now >= ActShotAt && !G->ShotDir.IsEmpty())
			{
				ActShotAt = -1.f;
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(G->ShotDir, ActShotName + TEXT(".png")), true, false);
			}
			AActor* F = ActFocus.Get();
			if (!F || Gone(F)) F = Shooter;
			const FVector FP = ActShot == 2 ? ActImpactAt : ActorCenter(F);
			FVector V = F ? F->GetVelocity() : FVector::ZeroVector; V.Z = 0.f;
			FVector D = V.Size() > 60.f ? V.GetSafeNormal() : FVector(-FP.X, -FP.Y, 0.f).GetSafeNormal();
			if (ActShot == 2 && !ActImpactDir.IsNearlyZero()) D = ActImpactDir;
			if (D.IsNearlyZero()) D = FVector::ForwardVector;
			const float Y0 = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			const FVector C0(0.f, 0.f, CourtZ);
			const float KRc = G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius;
			auto PickSide = [&](float Base, float Off, const FVector& At, float Dist)
			{
				// the side whose sight line keeps clear of the centre hologram spire, without the camera going far out over the void
				auto Score = [&](float S)
				{
					const FVector2D Cam(At - FRotator(0.f, Base + S * Off, 0.f).Vector() * Dist), Tg(At);
					const FVector2D Seg = Tg - Cam;
					const float T = FMath::Clamp(FVector2D::DotProduct(-Cam, Seg) / FMath::Max(1.f, Seg.SizeSquared()), 0.f, 1.f);
					const float Clear = FMath::Min((Cam + Seg * T).Size(), 650.f);
					return Clear - FMath::Max(0.f, Cam.Size() - (KRc + 350.f)) * 1.5f;
				};
				return Score(1.f) >= Score(-1.f) ? 1.f : -1.f;
			};
			const float FD = Profiles[PFollow].Dist, FPit = Profiles[PFollow].Pitch; // the after-launch overhead (2400 cm, 55 deg)
			const float FH = FD * FMath::Cos(FMath::DegreesToRadians(FPit)); // ground distance for the side pick
			float CYaw = Y0, CPitch = FPit, CDist = FD, CFov = 84.f;
			FVector CPiv = FP, CLook = FP;
			const float U = FMath::Clamp((Now - ActT0) / FMath::Max(ActLen, 0.1f), 0.f, 1.f);
			switch (ActShot)
			{
			case 0: // the overhead follow Evan likes: behind the roll, high
				if (!ActYawInit) ActWideYaw = ActCuts <= 1 ? Yaw : Y0;
				ActWideYaw = FRotator::NormalizeAxis(ActWideYaw + FRotator::NormalizeAxis(Y0 - ActWideYaw) * (1.f - FMath::Exp(-1.5f * Dt)));
				CYaw = ActWideYaw; CPitch = FPit; CDist = FD; CFov = 86.f; CPiv = FP + D * 120.f; CLook = CPiv;
				break;
			case 1: case 3: // high side angle at the same distance (on the shooter / on whatever got hit hardest)
				if (!ActYawInit) ActSideSign = PickSide(Y0, 90.f, FP, FH);
				CYaw = Y0 + (90.f - 12.f * U) * ActSideSign; CPitch = FPit - 7.f; CDist = FD * 0.96f; CFov = 82.f;
				CPiv = FP + D * 160.f; CLook = CPiv;
				break;
			case 2: // first impact: high three-quarter angle on the hit, same distance
				if (!ActYawInit) ActSideSign = PickSide(Y0, 50.f, FP, FH);
				CYaw = Y0 + (50.f + 10.f * U) * ActSideSign; CPitch = FPit - 3.f; CDist = FD * (0.97f - 0.04f * U); CFov = 80.f;
				break;
			case 4: // near top-down, slowly turning, between the action and the centre
			{
				CPiv = FMath::Lerp(FP, C0, 0.35f); CLook = CPiv;
				if (!ActYawInit) ActWideYaw = Yaw;
				CYaw = ActWideYaw + 10.f * (Now - ActT0); CPitch = 72.f; CDist = FD * 1.08f; CFov = 84.f;
				break;
			}
			default: // wide orbit of the whole court
				if (!ActYawInit) ActWideYaw = Yaw;
				CPiv = C0; CLook = C0; CYaw = ActWideYaw + 12.f * (Now - ActT0); CDist = FMath::Max(FD * 1.15f, 2700.f); CPitch = 50.f; CFov = 80.f;
				break;
			}
			if (!ActYawInit || bActCut) { ActYawS = FRotator::NormalizeAxis(CYaw); ActYawInit = true; }
			else ActYawS = FRotator::NormalizeAxis(ActYawS + FRotator::NormalizeAxis(CYaw - ActYawS) * (1.f - FMath::Exp(-5.f * Dt)));
			PIdx = PFollow;
			WantYaw = ActYawS;
			WantPivot = CPiv; WantLook = CLook;
			PitchOff = CPitch - Profiles[PFollow].Pitch;
			DistMul = CDist / FMath::Max(1.f, Profiles[PFollow].Dist);
			WantFov = CFov;
			bCourtView = false;
			ProbeFrom = CPiv + FVector(0, 0, 80.f);
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
	if (bActCut) ProfileAge = 1000.f; // action camera hard cut: no profile blend

	const float LegacyWantPitch = P.Pitch + PitchOff;

	float WantPitch = bPortraitChase ? ChaseElev : bPortraitAim ? ZoogiPortrait::AimPitch() : LegacyWantPitch;
	// b66 (Evan): "the angle to sell this game is 90 to 65 degree top view to view the trails of destruction and smashing hits" -
	// a rolling classic shot is always seen from 65-88 deg above (-ZoogiOldRollCam = the old pitches; -ZoogiRollPitch=deg)
	{
		static const bool bOldRoll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRollCam"));
		static float RollPitch = 72.f; static bool bRp = false; if (!bRp) { bRp = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRollPitch="), RollPitch); }
		if (!bOldRoll && G && G->Phase == EZoogiPhase::Rolling && !G->bRoyaleMode && !bPortraitAim) WantPitch = FMath::Clamp(RollPitch < 65.f ? RollPitch : FMath::Max(WantPitch, RollPitch), 55.f, 88.f); // b171: -ZoogiRollPitch below 65 = a fixed lower tilt
	}

	const float WantFovF = WantFov > 0.f ? WantFov : P.Fov;

	float WantHFov = WantFovF;

	float WantDist = PortraitBoom(G, PAspect, PTop, PBot, WantYaw, WantPitch, WantFovF, P.Dist * DistMul, bCourtView, CourtCentre, WantHFov);

	// (rest steps too: at least the tactical aim boom, so Draw / Awaken also look up the arena instead of the landscape close-up)

	if (bPortraitChase) { WantDist = ChaseBoomD; float K0 = 1.f; ZoogiPortrait::AdaptFov(WantFovF, PAspect, WantHFov, K0); }

	else if (bPortraitAim) WantDist = PortraitAimDist(G, PAspect, PTop, PBot, SL, WantPivot, WantYaw, WantPitch, WantFovF, FMath::Max(P.Dist * DistMul, Profiles[PTactical].Dist * Scale), WantHFov);

	PortDbgLegacyPivot = bPortraitAim ? LegacyPivot : WantPivot; PortDbgLegacyPitch = LegacyWantPitch; PortDbgLegacyDist = P.Dist * DistMul; PortDbgAim = bPortraitAim;

	const bool bBlending = ProfileAge < BlendDur;

	// Yaw: spring (underdamped in Battle = arc sweep with overshoot and settle; whip on switches).

	float W = P.YawFreq, Z = P.YawZeta;

	if (G->IsAimStep() && bWhip && SwingAge < WhipTime) { W = WhipFreq; Z = WhipZeta; }

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

	else if (bAimViewActive)

	{

		// aim view: critically damped yaw follow (~AimYawTime to settle, shortest path, no overshoot / wobble); eased in from

		// the tactical spring over the first 0.5 s of the view so the entry isn't a whip

		const float Entry = FMath::Clamp(AimViewAge / 0.5f, 0.f, 1.f);

		const float Wf = FMath::Lerp(W, 4.7f / ZoogiAimView::YawTime(), Entry * Entry * (3.f - 2.f * Entry));

		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Dt / (1.f / 240.f)));

		const float H = Dt / Steps;

		for (int32 i = 0; i < Steps; ++i)

		{

			const float Delta = FRotator::NormalizeAxis(WantYaw - Yaw);

			YawVel += (Wf * Wf * Delta - 2.f * Wf * YawVel) * H;

			Yaw = FRotator::NormalizeAxis(Yaw + YawVel * H);

		}

		if (FMath::Abs(FRotator::NormalizeAxis(WantYaw - Yaw)) < 0.02f && FMath::Abs(YawVel) < 0.5f) { Yaw = FRotator::NormalizeAxis(WantYaw); YawVel = 0.f; }

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
	if (ActShot >= 0)
	{
		if (bActCut) { Pitch = WantPitch; Dist = WantDist; Fov = WantFovF; Pivot = WantPivot; Look = WantLook; Yaw = FRotator::NormalizeAxis(WantYaw); YawVel = 0.f; }
		else { Pivot = Damp(Pivot, WantPivot, 9.f, Dt); Look = Damp(Look, WantLook, 9.f, Dt); Dist = Damp(Dist, WantDist, 5.f, Dt); Pitch = Damp(Pitch, WantPitch, 5.f, Dt); Fov = Damp(Fov, WantFovF, 5.f, Dt); }
	}



	// aim view: the look point rides the smoothed heading exactly (so the Zoogi and its arrow stay centred while the camera

	// orbits), blended in over the view's first 0.5 s

	if (bAimViewActive)

	{

		const FVector AimLook = FVector(Pivot.X, Pivot.Y, Pivot.Z) + FRotator(0.f, Yaw, 0.f).Vector() * AimViewAhead;

		const float A = FMath::Clamp(AimViewAge / 0.5f, 0.f, 1.f);

		Look = FMath::Lerp(Look, AimLook, A * A * (3.f - 2.f * A));

	}

	// Ring-out safety: pivots and the look point never sink below the court.

	Pivot.Z = FMath::Max(Pivot.Z, CourtZ - 100.f);

	Look.Z = FMath::Max(Look.Z, CourtZ - 100.f);

	ProbeFrom.Z = FMath::Max(ProbeFrom.Z, CourtZ + 60.f);

	// Oct 14 aim zoom mode: boom shortened by the zoom, view centre shifted by zoom-about-anchor + pan (inside the ring,

	// shooter softly kept in frame), pitch tilting toward a lower battle angle as it closes in; eased in and out

	FVector ZPivot = Pivot, ZLook = Look; float ZDist = Dist, ZPitch = Pitch;

	{

		LoadZoomTune();

		const FAimZoomTune& ZT = ZoomTune;

		const bool bAimZoom = ZT.bEnabled && !bAnimeShot && ZoomStageLive(G);

		if (!bAimZoom) { if (bZoomMode) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode OFF (camera left the aim step)")); bZoomMode = false; ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector; }

		if (bZoomArmed && !G->IsHumanAiming()) { bZoomArmed = false; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM disarmed (turn over)")); }

		if (bZoomArmed && bAimZoom) SetZoomMode(true, TEXT("armed in the Awaken step"));

		const float Ease = ZoomWant >= ZoomCur ? ZT.EaseIn : ZT.EaseOut;

		ZoomCur = Damp(ZoomCur, ZoomWant, Ease, Dt);

		if (FMath::Abs(ZoomCur - ZoomWant) < 0.002f) ZoomCur = ZoomWant;

		if (ZoomOffWant.SizeSquared() > 1.f)

		{

			// limits on the wanted view centre: inside the knockout ring, shooter within reach of the frame

			FVector2D E(Pivot.X + ZoomOffWant.X, Pivot.Y + ZoomOffWant.Y);

			if (Shooter && !bShooterLeaving)

			{

				const FVector2D S2(SL.X, SL.Y);

				const float R = ZT.FollowRadius / FMath::Max(ZoomWant, 1.f) + ZT.FollowPad;

				const FVector2D DS = E - S2;

				if (DS.Size() > R) E = FMath::Lerp(E, S2 + DS.GetSafeNormal() * R, 1.f - FMath::Exp(-ZT.FollowLag * Dt)); // soft follow

			}

			const float RMax = FMath::Max(200.f, (G->Arena ? G->Arena->GetKnockoffRadius() : 2000.f) - ZT.EdgeMargin);

			// 811d899 / pad-as-gameplay: a Zoogi on a launch pad sits outside the ring - never pull the view centre into the court

			// (even when zoogi.Cam.PadView is soft-off and the normal gameplay camera is used).

			const bool bPadOutsideRing = bPadView || Shooter->IsOnLaunchPad();

			if (!bPadOutsideRing && E.Size() > RMax) E = E.GetSafeNormal() * RMax;

			ZoomOffWant = FVector(E.X - Pivot.X, E.Y - Pivot.Y, 0.f);

		}

		ZoomOff = Damp(ZoomOff, ZoomOffWant, bZoomMode ? ZT.PanEase : ZT.EaseOut, Dt);

		if (ZoomCur <= 1.0005f && ZoomWant <= 1.f && ZoomOffWant.IsNearlyZero() && ZoomOff.Size() < 1.f) { ZoomCur = 1.f; ZoomOff = FVector::ZeroVector; }

		const float A = ZT.MaxZoom > 1.001f ? FMath::Clamp((ZoomCur - 1.f) / (ZT.MaxZoom - 1.f), 0.f, 1.f) : 0.f;   // 0 wide .. 1 close

		const float M = FMath::Clamp((ZoomCur - 1.f) / FMath::Max(ZT.EnterZoom - 1.f, 0.05f), 0.f, 1.f);            // mode blend

		ZPitch = FMath::Max(ZT.PitchMin, FMath::Lerp(Pitch, FMath::Lerp(ZT.PitchWide, ZT.PitchClose, A), M));

		ZPivot = Pivot + ZoomOff; ZLook = Look + ZoomOff; ZDist = Dist / ZoomCur;

		// stay above the pillars / props: the camera never drops below MinHeight over the court while zoomed

		if (ZoomCur > 1.001f && ZDist > 1.f && ZDist * FMath::Sin(FMath::DegreesToRadians(ZPitch)) < ZT.MinHeight)

			ZPitch = FMath::Min(FMath::Max(Pitch, ZPitch), FMath::RadiansToDegrees(FMath::Asin(FMath::Min(ZT.MinHeight / ZDist, 0.98f))));

		// zoomed: probe the boom from high above the view centre, so pillars / props between the shooter and the aim point

		// don't pull the camera in against them (it stays >= MinHeight, over the props)

		if (ZoomCur > 1.001f || !ZoomOff.IsNearlyZero()) ProbeFrom = ZPivot + FVector(0, 0, FMath::Max(150.f, ZT.MinHeight * 0.85f));

	}

	const FRotator Heading(0.f, Yaw, 0.f);

	const float Pr = FMath::DegreesToRadians(ZPitch);

	const FVector Desired = ZPivot - Heading.Vector() * ZDist * FMath::Cos(Pr) + FVector(0, 0, ZDist * FMath::Sin(Pr));

	FVector Loc = ProbeCamera(ProbeFrom, Desired, Dt);

	Loc.Z = FMath::Max(Loc.Z, CourtZ); // and the camera never drops below the court

	// Oct 15 camera watchdog: in play, if the boom is left far longer than this profile wants (or the view centre far

	// from where it should be) for a moment - a special cam / zoom / blend that never came back - snap the follow state

	// back to the battle view (zoom off) instead of staying stuck high over the map

	if (G->Phase == EZoogiPhase::Aiming && !bAnimeShot && !bBlending && !bPadView && !bOnLaunchPad && !bAimViewActive) // pad / on-pad / AimView: no far-from-centre restore

	{

		float ExpD = WantDist / FMath::Max(ZoomCur, 1.f);

		if (bAimViewActive) ExpD = FMath::Max(ExpD, FVector::Dist(Desired, ZLook)); // the low aim view looks far ahead of its short boom

		const float GotD = FVector::Dist(Loc, ZLook);

		const bool bFar = GotD > ExpD * 1.6f + 600.f || FVector::Dist2D(Pivot, WantPivot) > 3000.f || Loc.Z - CourtZ > ExpD * 1.5f + 800.f;

		CamBadT = bFar ? CamBadT + Dt : 0.f;

		if (CamBadT > 1.0f)

		{

			UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH camera stuck off the battle view (boom %.0f vs %.0f, height %.0f, centre off %.0f cm) -> restored"), GotD, ExpD, Loc.Z - CourtZ, FVector::Dist2D(Pivot, WantPivot));

			Dist = WantDist; Pitch = WantPitch; Pivot = WantPivot; Look = WantLook; Fov = WantFovF;

			if (bZoomMode) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode OFF (camera watchdog)"));

			bZoomMode = false; bZoomArmed = false; ZoomWant = ZoomCur = 1.f; ZoomOffWant = ZoomOff = FVector::ZeroVector; ArmScale = 1.f;

			CamBadT = 0.f; ++CamWatchFixes;

			const FRotator H2(0.f, Yaw, 0.f); const float Pr2 = FMath::DegreesToRadians(Pitch);

			ZPivot = Pivot; ZLook = Look;

			Loc = Pivot - H2.Vector() * Dist * FMath::Cos(Pr2) + FVector(0, 0, Dist * FMath::Sin(Pr2));

			Loc.Z = FMath::Max(Loc.Z, CourtZ);

		}

	}

	else CamBadT = 0.f;

	// and the view target: on the human's aim step the match camera must be the view (a special cam that never handed back)

	if (G->Phase == EZoogiPhase::Aiming && G->IsHumanAiming() && G->IsAimStep() && !bAnimeShot && !(Anime && Anime->IsBusy()) && GetViewTarget() != Cam && !ZoogiGattling::Active()) // b195: never over the SHOWTIME camera

	{

		ViewBadT += Dt;

		if (ViewBadT > 1.5f)

		{

			UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH view target was %s on the aim step -> match camera restored"), *GetNameSafe(GetViewTarget()));

			SetViewTarget(Cam); ViewBadT = 0.f; ++CamWatchFixes;

		}

	}

	else ViewBadT = 0.f;
	{	// b98b (Evan: "doesn't return to normal gameplay after the duel clip until my turn again"): the CPUs' aim steps and the 30 s after
		// a duel get the same watchdog, unless a real special camera is up
		const UZoogiFunSys* FunV = UZoogiFunSys::Get(this);
		const UZoogiWolfPackSys* WpV = UZoogiWolfPackSys::Get(this);
		const bool bSpecialV = ZoogiSkillLab::Open() || (FunV && FunV->SpecialCamOn()) || (WpV && WpV->PackActive()) || bAnimeShot || (Anime && Anime->IsBusy()) || ZoogiSkills::EffectBusy() || ZoogiRedirect::Picking();
		const bool bAfterDuel = FunV && FunV->LastDuelEndAt > 0.0 && FPlatformTime::Seconds() - FunV->LastDuelEndAt < 30.0;
		const bool bCpuAim = G->IsAimStep() && !G->IsHumanAiming();
		if ((bAfterDuel || bCpuAim) && G->Phase == EZoogiPhase::Aiming && !G->bCinematicCam && !bSpecialV && Cam && GetViewTarget() != Cam)
		{
			PostDuelBadT += Dt;
			if (PostDuelBadT > 0.6f) { UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH view target was %s (cpu aim %d, after duel %d) -> match camera restored"), *GetNameSafe(GetViewTarget()), bCpuAim ? 1 : 0, bAfterDuel ? 1 : 0); SetViewTargetWithBlend(Cam, 0.3f); PostDuelBadT = 0.f; ++CamWatchFixes; }
		}
		else PostDuelBadT = 0.f;
		// b102 (Evan: "the cam still gets stuck on the last camera view and doesn't track the turn player"): many special cameras
		// hand back to the view that was live when they began; nested, that view is another special cam that has finished - frozen.
		// Any phase of a classic match: a view target other than the match camera that has not moved for 2 s, with no special-camera
		// owner active, goes back to the match camera (which tracks the turn player).
		static TWeakObjectPtr<AActor> StuckVT; static FTransform StuckXf; static float StuckT = 0.f;
		AActor* VTs = GetViewTarget();
		const bool bPlayPh = G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling || G->Phase == EZoogiPhase::Resolving;
		const bool bOwner = bSpecialV || ZoogiEdgeHang::Active() || ZoogiArcCam::Active() || ZoogiSkyDiveCam::Active() || ZoogiBossAttack::CamActive() || G->bCinematicCam || G->bRoyaleMode || G->bComboMode;
		if (Cam && VTs && VTs != Cam && bPlayPh && !bOwner)
		{
			if (VTs != StuckVT.Get()) { StuckVT = VTs; StuckXf = VTs->GetActorTransform(); StuckT = 0.f; }
			else if (VTs->GetActorTransform().Equals(StuckXf, 1.f)) { StuckT += Dt; if (StuckT > 2.f) { UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH frozen view %s for 2 s (P%d's %s) -> match camera"), *GetNameSafe(VTs), G->ActivePlayer + 1, G->Phase == EZoogiPhase::Aiming ? TEXT("aim") : G->Phase == EZoogiPhase::Rolling ? TEXT("roll") : TEXT("resolve")); SetViewTargetWithBlend(Cam, 0.35f, VTBlend_Cubic); StuckT = 0.f; StuckVT.Reset(); ++CamWatchFixes; } }
			else { StuckXf = VTs->GetActorTransform(); StuckT = 0.f; }
		}
		else { StuckT = 0.f; StuckVT.Reset(); }
		// b187 (Evan: "camera still gets stuck, even after game exit - fix it so it never can happen no matter what shot it came from"):
		// one guard over every special camera, whoever spawned it. (1) A match start always begins on the match camera, with any
		// episode shot stopped. (2) A view that is no camera at all (nothing / a pawn / a ball / a destroyed or hidden actor) goes
		// back at once. (3) No camera may hold the view off the match camera for long, owner or not: 6 s on an aim, 10 s otherwise
		// (SHOWTIME and the skill lab excepted - they end on their own and have their own safety).
		static EZoogiPhase LastPhG = EZoogiPhase::Menu; static float OffCamT = 0.f;
		const bool bMatchStart = (LastPhG == EZoogiPhase::Menu || LastPhG == EZoogiPhase::GameOver) && G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::GameOver;
		LastPhG = G->Phase;
		AActor* VTg = GetViewTarget();
		if (Cam && bMatchStart)
		{
			if (UZoogiEpisodeCameraDirector* Cd = UZoogiEpisodeCameraDirector::Get(this)) Cd->StopAllShots(true);
			if (VTg != Cam) { UE_LOG(LogZoogi, Warning, TEXT("CAMGUARD match start: view was %s -> match camera"), *GetNameSafe(VTg)); SetViewTarget(Cam); ++CamWatchFixes; }
			OffCamT = 0.f;
		}
		else if (Cam && bPlayPh && !ZoogiGattling::Active() && !ZoogiSkillLab::Open())
		{
			VTg = GetViewTarget();
			const bool bJunk = !IsValid(VTg) || VTg == this || VTg == GetPawn() || Cast<AZoogiBall>(VTg) != nullptr || (VTg->IsHidden() && !Cast<ACameraActor>(VTg));
			OffCamT = VTg != Cam ? OffCamT + Dt : 0.f;
			const float Cap = bJunk ? 0.3f : (G->Phase == EZoogiPhase::Aiming ? 6.f : 10.f);
			if (OffCamT > Cap)
			{
				UE_LOG(LogZoogi, Warning, TEXT("CAMGUARD view %s held the screen %.1f s (%s, phase %d) -> match camera"), *GetNameSafe(VTg), OffCamT, bJunk ? TEXT("not a camera") : TEXT("too long"), (int32)G->Phase);
				if (UZoogiEpisodeCameraDirector* Cd = UZoogiEpisodeCameraDirector::Get(this)) if (Cd->IsMainViewTaken()) Cd->StopAllShots(true);
				SetViewTargetWithBlend(Cam, bJunk ? 0.f : 0.35f); OffCamT = 0.f; ++CamWatchFixes;
			}
		}
		else OffCamT = 0.f;
	}



	PortDbgLook2 = ZLook; PortDbgFov = Fov; PortDbgWantDist = WantDist; PortDbgArm = ArmScale;

	float ViewHFov = Fov, ViewK = 1.f;

	ZoogiPortrait::AdaptFov(Fov, PAspect, ViewHFov, ViewK);

	// portrait: look point at the safe band's centre (the aim chase view places the shooter by its own screen height instead)

	FRotator Rot = bPortraitChase ? (ZLook - Loc).Rotation() : ZoogiPortrait::BandRotation(Loc, ZLook, ViewHFov, PAspect, PTop, PBot);

	Rot.Roll = Dutch;

	AimViewLogT = bAimViewActive ? AimViewLogT + Dt : 0.f;

	if (bAimViewActive && (AimViewLogT >= 1.f || AimViewAge <= Dt))

	{

		AimViewLogT = 0.f;

		UE_LOG(LogZoogi, Log, TEXT("CAM aim view live: cam %s pitch %.1f yaw %.1f fov %.1f | want view pitch %.1f boom elev %.1f (got %.1f) boom %.0f (got %.0f, arm %.2f) | height over the Zoogi %.0f | aim yaw %.1f | dbg pivot %s sl %s desired %s zdist %.0f yaw %.1f"),

			*Loc.ToString(), -Rot.Pitch, Rot.Yaw, Fov, AimViewPitchUsed, AimViewElevUsed, ZPitch, ZoogiAimView::Boom(), FVector::Dist(Loc, SL), ArmScale, Loc.Z - SL.Z, AimViewYaw, *ZPivot.ToString(), *SL.ToString(), *Desired.ToString(), ZDist, Yaw);

	}

	// Battle framing guard: keep the shooter's silhouette above the letterbox / bottom HUD band by tilting

	// down just enough, without pushing the target out of the top of the frame.

	{

		float Want = 0.f;

		if (PIdx == PBattle && Shooter)

		{

			const float Aspect = ZoogiFramed::ViewAspect(this); // framed HUD: the arena window's real view-rect aspect (full viewport otherwise)

			float TanHH = 0.f, TanVH = 0.f;

			if (bPortrait) ZoogiPortrait::TanHalf(ViewHFov, Aspect, TanHH, TanVH);

			else TanVH = FMath::Tan(FMath::DegreesToRadians(ViewHFov * 0.5f)) / Aspect; // landscape: unchanged

			const float VHalf = FMath::RadiansToDegrees(FMath::Atan(TanVH));

			const float ShooterLow = (SL - FVector(0, 0, ZoogiConst::ZoogiRadius) - Loc).Rotation().Pitch;

			const float Rel = ShooterLow - Rot.Pitch; // negative = below the centre line

			// portrait: the HUD-safe band's edges (top SafeTop / bottom SafeBottom of the screen)

			const float Limit = bPortrait ? -FMath::RadiansToDegrees(FMath::Atan((1.f - 2.f * PBot) * TanVH)) : -VHalf * BattleShooterMaxDrop;

			const float TopLimit = bPortrait ? FMath::RadiansToDegrees(FMath::Atan((1.f - 2.f * PTop) * TanVH)) : VHalf * BattleTargetMaxRise;

			if (Rel < Limit)

			{

				Want = Rel - Limit; // negative: tilt down by this much

				if (Lock)

				{

					// Don't let the target's top leave the frame.

					const float TargetTop = (TL + FVector(0, 0, 2.f * ZoogiConst::ZoogiRadius) - Loc).Rotation().Pitch - Rot.Pitch;

					const float Room = TopLimit - TargetTop; // stay below the top HUD band // how far we can tilt down before the target exits

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

		const float A = ShakeDeg * (bTac ? TacShake : 1.f) * FMath::Exp(-ShakeDecay * LockPunchAge);

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

	if (LockPunchAge < LockFovPunchTime) FinalFov -= (bTac ? TacFovPunch : LockFovPunch) * FMath::Sin(PI * LockPunchAge / LockFovPunchTime);

	if (FireKickAge < 2.f) FinalFov += FireFovKick * (bTac ? 0.3f : 1.f) * FMath::Exp(-FireKickDecay * FireKickAge) * FMath::Min(1.f, FireKickAge * 12.f);

	// pad-as-gameplay (Evan Oct 22 5:50 PM): soft-disable the 7398f61 every-frame SolvePose. On the pad the Loc/Rot above

	// (normal/action soft top-down follow, buttons only) stand; AimYaw still drives aim via the normal aim path. PadBlend

	// below still eases out if a legacy pad-view flight was already mid-air. zoogi.Cam.PadView defaults to 0.

	bPadPoseValid = false;

	// launch pad camera: after the landing, blend from the last pad-view pose into the normal camera

	if (PadBlendT >= 0.f)

	{

		const float BDur = ZoogiLaunchPadCam::BlendTime();

		PadBlendT += Dt;

		if (G->bCinematicCam || PadBlendT >= BDur) { PadBlendT = -1.f; UE_LOG(LogZoogi, Log, TEXT("CAM pad view: blend done")); }

		else

		{

			const float A0 = FMath::Clamp(PadBlendT / BDur, 0.f, 1.f), A = A0 * A0 * (3.f - 2.f * A0);

			Loc = FMath::Lerp(PadFromLoc, Loc, A);

			Rot = FQuat::Slerp(PadFromRot.Quaternion(), Rot.Quaternion(), A).Rotator();

			FinalFov = FMath::Lerp(PadFromFov, FinalFov, A);

		}

	}

	if (PadPhase > 0) { PadFromLoc = Loc; PadFromRot = Rot; PadFromFov = FinalFov; }
	ZoogiPauseOrbit::Ensure(); ZoogiPauseOrbit::Apply(GetWorld(), Loc, Rot, Dt); // b120: a pause orbit eases back into the match camera

	Cam->SetActorLocation(Loc);

	Cam->SetActorRotation(Rot);

	if (bPortrait) { float K2 = 1.f; ZoogiPortrait::AdaptFov(FinalFov, PAspect, FinalFov, K2); }

	Cam->GetCameraComponent()->SetFieldOfView(FMath::Clamp(FinalFov, 30.f, 120.f));

	ApplyPostProcess(Dt, PIdx, Lock ? FVector::Dist(Loc, TL) : FVector::Dist(Loc, Look));

}



UTextureRenderTarget2D* AZoogiPlayerController::GetGameplayPipTexture()

{

	return ZoogiQualityCam::GetGameplayPipTexture(this);

}



UTextureRenderTarget2D* AZoogiPlayerController::GetReplayPipTexture()

{

	return ZoogiQualityCam::GetReplayPipTexture(this);

}



void AZoogiPlayerController::SetGameplayPipActive(bool bActive)

{

	ZoogiQualityCam::SetGameplayPipActive(this, bActive);

}



bool AZoogiPlayerController::ForceEdgeSlowMoForTest(AZoogiBall* Falling)

{

	return ZoogiQualityCam::ForceEdgeSlowMo(this, Falling);

}



bool AZoogiPlayerController::ForceBestShotForTest(AZoogiBall* Focus, FName Reason)

{

	return ZoogiQualityCam::ForceBestShot(this, Focus, Reason.IsNone() ? FName(TEXT("ShotTest")) : Reason);

}





