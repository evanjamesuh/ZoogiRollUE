#include "ZoogiPlayerController.h"
#include "ZoogiStageForming.h"
#include "ZoogiArena.h"
#include "ZoogiRoll.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiClone.h"
#include "ZoogiArena.h"
#include "ZoogiHUD.h"
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
#include "ZoogiCardSubsystem.h"
#include "ZoogiSpawnElevator.h"

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
		{ TEXT("Tactical"), 55.f, 2300.f, 80.f, 0.f, 3.f, 4.f,  3.f, 1.0f, 0.18f, 0.0f,  1.00f, 1.00f, FLinearColor(1.00f, 1.00f, 1.00f), 0.50f }, // default (Oct 4): readable high view, small emphasis
		{ TEXT("Anime"),    12.f,  520.f, 50.f, -6.f, 12.f, 10.f, 10.f, 0.9f, 0.60f, 1.0f, 1.30f, 1.15f, FLinearColor(1.08f, 1.00f, 0.95f), 0.18f }, // anime placeholder close shot
	};
	// Tactical camera emphasis (fractions of the Tactical boom).
	constexpr float TacDrawZoom = 0.78f;      // Draw / Awaken: slight zoom on the active Zoogi
	constexpr float TacAimLook = 0.4f;        // Aim: look between the shooter (0) and the aim point (1)
	constexpr float TacPowerZoom = 0.86f;     // PowerUp: small push-in
	constexpr float TacRollTrack = 0.55f;     // Rolling: track the shooter this much (rest = arena centre)
	constexpr float TacResolveDist = 1.18f;   // Resolve / End: auto-return to the whole-arena view
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
	bUiTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiUiTest"));
	bHumanTest = bUiTest || FParse::Param(FCommandLine::Get(), TEXT("ZoogiHumanTest"));
	bPinchTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPinchTest"));
	bCamStuckTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCamStuckTest"));
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
			UGameplayStatics::SetGlobalTimeDilation(this, ZoogiCamTune::SlowMoScale);
			SlowMoUntil = GetWorld()->GetRealTimeSeconds() + ZoogiCamTune::SlowMoTime;
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
	UGameplayStatics::SetGlobalTimeDilation(this, HitStopScale);
	SlowMoUntil = GetWorld()->GetRealTimeSeconds() + HitStopTime * (0.5f + 0.5f * ImpactStrength);
}

void AZoogiPlayerController::OnPoweredHit(bool bCrit)
{
	using namespace ZoogiCamTune;
	if (bMenuCam) return;
	ImpactAge = 0.f;
	ImpactStrength = bCrit ? 1.f : 0.7f;
	if (!ZoogiUtil::ReactionPauses()) return; // Oct 15 (Evan): no hit-stop; shake / FOV punch still play
	UGameplayStatics::SetGlobalTimeDilation(this, HitStopScale);
	SlowMoUntil = GetWorld()->GetRealTimeSeconds() + HitStopTime * (bCrit ? 1.6f : 1.f);
}

void AZoogiPlayerController::OnAnimeStarted(const FZoogiCombatEvent& E)
{
	AnimeFocus = E.Focus;
	AnimeYaw = Yaw + (E.Event == TEXT("Infuse") || E.Event == TEXT("Guard") ? 60.f : 35.f);
	AnimeDistMul = (E.Event == TEXT("Guard") || E.Event == TEXT("GuardBlock")) ? 1.7f : 1.f; // the guard bubble is big: frame it from outside
	bAnimeCam = AnimeFocus.IsValid();
	EndSlowMo();
}

void AZoogiPlayerController::OnAnimeFinished()
{
	bAnimeCam = false;
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
	if (bUiTest) UpdateUiTest(Dt);
	else if (bHumanTest) UpdateHumanTest(Dt);
	if (bPinchTest) UpdatePinchTest(GM(), Dt);
	if (bCamStuckTest) UpdateCamStuckTest(GM(), Dt);
	HandleInput(Dt);
	UpdateCamera(Dt);
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
		GConfig->GetBool(S, TEXT("bEnabled"), T.bEnabled, GGameIni);
		GConfig->GetBool(S, TEXT("bAutoExitOnTap"), T.bAutoExitOnTap, GGameIni);
		auto F = [&](const TCHAR* K, float& V) { GConfig->GetFloat(S, K, V, GGameIni); };
		F(TEXT("MaxZoom"), T.MaxZoom); F(TEXT("EnterZoom"), T.EnterZoom); F(TEXT("WheelStep"), T.WheelStep); F(TEXT("KeyStep"), T.KeyStep);
		F(TEXT("PinchPower"), T.PinchPower); F(TEXT("MinPinchPx"), T.MinPinchPx); F(TEXT("EaseIn"), T.EaseIn); F(TEXT("EaseOut"), T.EaseOut);
		F(TEXT("PanEase"), T.PanEase); F(TEXT("PitchWide"), T.PitchWide); F(TEXT("PitchClose"), T.PitchClose); F(TEXT("PitchMin"), T.PitchMin);
		F(TEXT("EdgeMargin"), T.EdgeMargin); F(TEXT("FollowRadius"), T.FollowRadius); F(TEXT("FollowPad"), T.FollowPad); F(TEXT("FollowLag"), T.FollowLag); F(TEXT("MinHeight"), T.MinHeight);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoAimZoom"))) T.bEnabled = false;
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
		if (bDragArmed || bPendingTap) { bDragArmed = false; bPendingTap = false; DragFrac = 0.f; }
		if (G && G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f;
		AActor* Lk = G && G->TargetLock.IsValid() ? G->TargetLock.Get() : nullptr;
		if (Lk) ZoomAnchor = FVector(ActorCenter(Lk).X, ActorCenter(Lk).Y, CourtZ);
		else if (G && G->bFreeAim) ZoomAnchor = FVector(G->FreeAimPoint.X, G->FreeAimPoint.Y, CourtZ);
		else { int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY); SetZoomAnchor(FVector2D(VX * 0.5f, VY * 0.45f)); }
		ZoomAbout(FMath::Max(ZoomWant, ZoomTune.EnterZoom));
	}
	else { ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector; }
	UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode %s (%s): zoom -> %.2fx anchor (%.0f, %.0f)"), bZoomMode ? TEXT("ON") : TEXT("OFF"), Why, ZoomWant, ZoomAnchor.X, ZoomAnchor.Y);
}

bool AZoogiPlayerController::UpdateZoomInput(AZoogiGameMode* G, bool bAimStage, const FVector2D& Mouse, float Dt)
{
	LoadZoomTune();
	const FAimZoomTune& T = ZoomTune;
	AZoogiHUD* Hud = Cast<AZoogiHUD>(GetHUD());
	float AX = 0.f, AY = 0.f, BX = 0.f, BY = 0.f; bool bA = false, bB = false;
	GetInputTouchState(ETouchIndex::Touch1, AX, AY, bA);
	GetInputTouchState(ETouchIndex::Touch2, BX, BY, bB);
	if (!(T.bEnabled && bAimStage))
	{
		if (bZoomMode) { bZoomMode = false; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode OFF (aim step over)")); }
		ZoomWant = 1.f; ZoomOffWant = FVector::ZeroVector;
		bPinch = false; bPanDown = false;
		if (bPinchHold && !bA && !bB) bPinchHold = false;
		return bPinchHold;
	}
	if (WasInputKeyJustPressed(EKeys::Z)) ToggleZoomMode(TEXT("Z key"));
	if (bZoomMode && WasInputKeyJustPressed(EKeys::Escape)) { ToggleZoomMode(TEXT("Esc")); return true; }
	// two fingers: pinch zoom (enters the mode; never a drag)
	if (bA && bB)
	{
		const FVector2D A(AX, AY), B(BX, BY);
		const float D = FMath::Max(FVector2D::Distance(A, B), 1.f);
		if (!bPinch)
		{
			bPinch = true; bPinchHold = true; bPanDown = false;
			if (!bZoomMode) { bZoomMode = true; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM mode ON (two fingers)")); }
			if (bDragArmed || bPendingTap) { bDragArmed = false; bPendingTap = false; DragFrac = 0.f; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM second finger: drag cancelled (no launch)")); }
			if (G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f;
			PinchD0 = D; PinchZ0 = ZoomWant;
			SetZoomAnchor((A + B) * 0.5f);
			UE_LOG(LogZoogi, Log, TEXT("AIMZOOM pinch start at (%.0f, %.0f) span %.0f px zoom %.2fx anchor (%.0f, %.0f)"), (A.X + B.X) * 0.5f, (A.Y + B.Y) * 0.5f, D, ZoomWant, ZoomAnchor.X, ZoomAnchor.Y);
		}
		if (FMath::Abs(D - PinchD0) >= T.MinPinchPx)
		{
			const float Old = ZoomWant;
			ZoomAbout(PinchZ0 * FMath::Pow(D / PinchD0, T.PinchPower));
			if (!FMath::IsNearlyEqual(Old, ZoomWant, 0.05f) && ZoomLogN++ < 80) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM pinch -> %.2fx (max %.2fx)"), ZoomWant, T.MaxZoom);
		}
		return true;
	}
	if (bPinch) { bPinch = false; UE_LOG(LogZoogi, Log, TEXT("AIMZOOM pinch end: zoom %.2fx"), ZoomWant); }
	if (bPinchHold) { if (!bA && !bB) bPinchHold = false; return true; } // no drag / tap until every finger lifts
	// mouse wheel (up enters the mode) / = - keys
	int32 Steps = 0;
	if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) ++Steps;
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) --Steps;
	if (Steps == 0) { const float Ax = GetInputAnalogKeyState(EKeys::MouseWheelAxis); if (FMath::Abs(Ax) > 0.01f) Steps = Ax > 0.f ? 1 : -1; }
	int32 KSteps = 0;
	if (WasInputKeyJustPressed(EKeys::Equals) || WasInputKeyJustPressed(EKeys::Add)) ++KSteps;
	if (WasInputKeyJustPressed(EKeys::Hyphen) || WasInputKeyJustPressed(EKeys::Subtract)) --KSteps;
	if ((Steps > 0 || KSteps > 0) && !bZoomMode) { SetZoomMode(true, Steps > 0 ? TEXT("wheel") : TEXT("key")); Steps = FMath::Max(0, Steps - 1); KSteps = FMath::Max(0, KSteps - 1); }
	if (bZoomMode && (Steps != 0 || KSteps != 0))
	{
		if (Steps != 0) SetZoomAnchor(Mouse);
		else { int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY); SetZoomAnchor(FVector2D(VX * 0.5f, VY * 0.45f)); }
		const float Old = ZoomWant;
		ZoomAbout(ZoomWant * FMath::Pow(1.f + T.WheelStep, (float)Steps) * FMath::Pow(1.f + T.KeyStep, (float)KSteps));
		if (ZoomLogN++ < 80) UE_LOG(LogZoogi, Log, TEXT("AIMZOOM %s %.2fx -> %.2fx (max %.2fx) toward (%.0f, %.0f)"), Steps ? TEXT("wheel") : TEXT("key"), Old, ZoomWant, T.MaxZoom, ZoomAnchor.X, ZoomAnchor.Y);
	}
	if (!bZoomMode) return false;
	// one finger / LMB slide or RMB / MMB drag = pan; a still LMB / finger tap = lock the aim there
	const bool bL = bA || IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bR = IsInputKeyDown(EKeys::RightMouseButton) || IsInputKeyDown(EKeys::MiddleMouseButton);
	const FVector2D P = bA ? FVector2D(AX, AY) : Mouse;
	const bool bDown = bL || bR;
	int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);
	const float UPx = FMath::Max(VY, 1) / 1080.f;
	if (bDown && !bPanDown)
	{
		const bool bUI = Hud && Hud->IsOverUI(P);
		bPanDown = true; bPanMoved = false; bPanTapOk = bL && !bUI && G->Stage == EZoogiStage::Battle; // Standby zoom: look / pan only, a tap never locks an aim
		PanStart = PanLast = bUI ? FVector2D(-1.f, -1.f) : P; // UI presses (the ZOOM button, columns) never pan
	}
	else if (bDown && bPanDown)
	{
		if (PanStart.X >= 0.f)
		{
			if (!bPanMoved && FVector2D::Distance(P, PanStart) > MinDragPx * UPx) bPanMoved = true;
			if (bPanMoved) { PanBy(PanLast, P); PanLast = P; }
		}
	}
	else if (!bDown && bPanDown)
	{
		bPanDown = false;
		if (!bPanMoved && bPanTapOk && PanStart.X >= 0.f)
		{
			AActor* Tgt = PickTarget(PanStart.X, PanStart.Y);
			FVector Fp = FVector::ZeroVector;
			bool bAimed = false;
			if (Tgt) { G->LockOn(Tgt, false); bAimed = true; }
			else if (FloorPoint(PanStart, Fp)) { G->SetFreeAim(Fp); bAimed = true; }
			UE_LOG(LogZoogi, Log, TEXT("AIMZOOM tap at (%.0f, %.0f): %s -> hasAim=%d"), PanStart.X, PanStart.Y, Tgt ? *Tgt->GetName() : (bAimed ? TEXT("floor point") : TEXT("nothing")), G->HasAim() ? 1 : 0);
			if (bAimed && T.bAutoExitOnTap) ToggleZoomMode(TEXT("target tapped - drag to shoot"));
		}
	}
	return true;
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
	// -ZoogiPinchTest: real touch streams through the engine's InputTouch path on P1's aim step:
	// run 0 two-finger spread (enters zoom), run 1 one-finger slide (pans, no drag), run 2 one-finger tap (locks + exits).
	if (!G) return;
	// no OS input needed: pass P1's Awaken (cards kept) and, after the three runs, fire the aimed shot
	static float AwakenT = 0.f;
	if (G->IsHumanAiming() && G->Stage == EZoogiStage::Awaken && !G->bAwakenDone)
	{
		AwakenT += Dt;
		if (AwakenT > 1.5f) { AwakenT = 0.f; UE_LOG(LogZoogi, Log, TEXT("PINCHTEST auto-commit P1 Awaken")); G->MenuAction(TEXT("CardCommit"), 0); }
		return;
	}
	AwakenT = 0.f;
	if (!(G->IsHumanAiming() && G->IsAimStep() && G->Stage == EZoogiStage::Battle)) { PinchTestT = -1.f; PinchTestPhase = 0; return; }
	if (PinchTestRuns >= 3)
	{
		if (PinchTestT < 0.f) PinchTestT = 0.f;
		PinchTestT += Dt;
		if (PinchTestT > 1.2f && PinchTestPhase == 0)
		{
			PinchTestPhase = 9;
			if (!G->HasAim()) { FVector P; int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY); if (FloorPoint(FVector2D(VX * 0.5f, VY * 0.35f), P)) G->SetFreeAim(P); }
			UE_LOG(LogZoogi, Log, TEXT("PINCHTEST launch (zoom %.2fx -> eases back after launch)"), ZoomCur);
			G->HumanDrag(0.75f, true);
		}
		return;
	}
	if (PinchTestT < 0.f) PinchTestT = 0.f;
	PinchTestT += Dt;
	int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);
	const FVector2D C(VX * 0.5f, VY * 0.40f);
	auto Touch = [&](uint32 H, ETouchType::Type Ty, const FVector2D& P) { InputTouch(H, Ty, P, 1.f, FDateTime::Now(), 0); };
	const float Start = 1.5f, Len = 0.8f;
	const int32 Run = PinchTestRuns;
	if (PinchTestPhase == 0 && PinchTestT > Start)
	{
		PinchTestOff0 = ZoomOffWant;
		if (Run == 0) { Touch(0, ETouchType::Began, C - FVector2D(60.f, 0.f)); Touch(1, ETouchType::Began, C + FVector2D(60.f, 0.f)); }
		else Touch(0, ETouchType::Began, Run == 1 ? C : C + FVector2D(40.f, 30.f));
		PinchTestPhase = 1;
		UE_LOG(LogZoogi, Log, TEXT("PINCHTEST run %d (%s) start: zoom %.2fx mode=%d"), Run, Run == 0 ? TEXT("two-finger spread") : Run == 1 ? TEXT("one-finger slide") : TEXT("one-finger tap"), ZoomWant, bZoomMode ? 1 : 0);
	}
	else if (PinchTestPhase == 1)
	{
		const float A = FMath::Clamp((PinchTestT - Start) / Len, 0.f, 1.f);
		if (Run == 0) { const float S = FMath::Lerp(60.f, 280.f, A); Touch(0, ETouchType::Moved, C - FVector2D(S, 0.f)); Touch(1, ETouchType::Moved, C + FVector2D(S, 0.f)); }
		else if (Run == 1) Touch(0, ETouchType::Moved, C + FVector2D(220.f * A, 60.f * A));
		if (A >= 1.f) PinchTestPhase = 2;
	}
	else if (PinchTestPhase == 2 && PinchTestT > Start + Len + 0.4f)
	{
		if (Run == 0) Touch(1, ETouchType::Ended, C + FVector2D(280.f, 0.f));
		PinchTestPhase = 3;
	}
	else if (PinchTestPhase == 3 && PinchTestT > Start + Len + 0.7f)
	{
		Touch(0, ETouchType::Ended, Run == 0 ? C - FVector2D(280.f, 0.f) : Run == 1 ? C + FVector2D(220.f, 60.f) : C + FVector2D(40.f, 30.f));
		PinchTestPhase = 4;
	}
	else if (PinchTestPhase == 4 && PinchTestT > Start + Len + 1.6f)
	{
		bool bOk = false;
		if (Run == 0) bOk = bZoomMode && ZoomWant > 1.5f;
		else if (Run == 1) bOk = bZoomMode && FVector::Dist2D(ZoomOffWant, PinchTestOff0) > 40.f;
		else bOk = !bZoomMode && G->HasAim();
		bOk = bOk && !bDragArmed && G->HumanDragFrac < 0.f && G->Phase == EZoogiPhase::Aiming;
		UE_LOG(LogZoogi, Log, TEXT("PINCHTEST %s run %d: mode=%d zoom %.2fx (cur %.2fx) offset moved %.0f cm hasAim=%d dragArmed=%d pull=%.2f phase=%d"), bOk ? TEXT("PASS") : TEXT("FAIL"), Run,
			bZoomMode ? 1 : 0, ZoomWant, ZoomCur, FVector::Dist2D(ZoomOffWant, PinchTestOff0), G->HasAim() ? 1 : 0, bDragArmed ? 1 : 0, G->HumanDragFrac, (int32)G->Phase);
		G->TakeShot(FString::Printf(TEXT("aimzoom_test_run%d"), Run));
		++PinchTestRuns; PinchTestPhase = 0; PinchTestT = 0.f;
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
		MaxDragForce = FMath::Clamp(MaxDragForce, 0.1f, 1.f);
	}
	float TchX = 0.f, TchY = 0.f; bool bTouch = false;
	GetInputTouchState(ETouchIndex::Touch1, TchX, TchY, bTouch);
	const bool bPtrDown = IsInputKeyDown(EKeys::LeftMouseButton) || bTouch;
	const FVector2D Ptr = bTouch ? FVector2D(TchX, TchY) : Mouse;
	const bool bPtrPress = bPtrDown && !bPtrWasDown, bPtrRelease = !bPtrDown && bPtrWasDown;
	bPtrWasDown = bPtrDown;
	// Oct 14 aim zoom mode: while it is on (or two fingers are / were down) the pointers pan / pinch / tap-to-lock and
	// nothing below sees them, so drag-aim is paused and a pinch never arms or releases a drag
	// (in the Standby the zoom only looks / pans: the Standby keys - A, 1-7, Enter - and the UMG tray keep working below)
	if (UpdateZoomInput(G, ZoomStageLive(G), bTouch ? FVector2D(TchX, TchY) : Mouse, Dt) && (G->Stage != EZoogiStage::Awaken || WasInputKeyJustPressed(EKeys::Escape))) { HoverTarget.Reset(); return; }
	const bool bTouchPress = bTouch && bPtrPress && !bLMB && !(Hud && Hud->IsOverUI(Ptr)); // bLMB already covers the press

	// Buttons (menu, results, in-match corner buttons).
	if (bLMB && Hud)
	{
		FName Action; int32 Index = 0;
		if (Hud->HitTest(FVector2D(RMX, RMY), Action, Index))
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
	if (bAwaken && G->CardsOn())
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
		if (bDragArmed) { bDragArmed = false; DragFrac = 0.f; }
		return;
	}

	const bool bPtrOverUI = Hud && Hud->IsOverUI(Ptr);
	HoverTarget = (bPtrOverUI || bDragArmed) ? nullptr : PickTarget(Ptr.X, Ptr.Y);
	const bool bClick = ((bLMB || bSimClick) && !bOverUI && !bUIClick) || bTouchPress;
	auto AimAt = [&](AActor* T, const FVector2D& At)
	{
		if (T) { G->LockOn(T, false); UE_LOG(LogZoogi, Log, TEXT("INPUT aim click: target %s -> hasAim=%d"), *T->GetName(), G->HasAim() ? 1 : 0); return; } // tap an orb / a player
		FVector P = FVector::ZeroVector;
		const bool bFloor = FloorPoint(At, P);
		if (bFloor) G->SetFreeAim(P); // tap the floor: free (angled) aim at that point
		UE_LOG(LogZoogi, Log, TEXT("INPUT aim click: floor=%d at (%.0f, %.0f) -> hasAim=%d"), bFloor ? 1 : 0, P.X, P.Y, G->HasAim() ? 1 : 0);
	};
	if (bClick)
	{
		if (G->HasAim() && bPtrDown && !bSimClick)
		{
			// already aimed: this press is a drag (keeps the aim, the camera stays put) unless it is released without
			// moving, in which case it is a tap and re-aims at the press point on release
			bDragArmed = true; bPendingTap = true; TapPos = Ptr; TapTarget = HoverTarget;
			DragStart = DragNow = Ptr; DragFrac = 0.f; bDragWouldCancel = true;
		}
		else
		{
			AimAt(HoverTarget.Get(), Ptr);
			if (G->HasAim() && bPtrDown && !bSimClick) { bDragArmed = true; bPendingTap = false; DragStart = DragNow = Ptr; DragFrac = 0.f; bDragWouldCancel = true; } // scripted clicks never start a drag
		}
	}
	// slingshot: hold + drag BACK (away from the shot direction) = force, release = launch; back near zero / onto the Zoogi = cancel
	if (bDragArmed)
	{
		AZoogiBall* Me = G->GetActiveBall();
		int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);
		const float UPx = FMath::Max(VY, 1) / 1080.f;
		FVector2D BallS = FVector2D::ZeroVector, AheadS = FVector2D::ZeroVector;
		float Fr = 0.f, Imp = 0.f;
		const FVector2D Sv = G->ComputeShotVelocity(Fr, Imp);
		const FVector2D SDir = Sv.GetSafeNormal();
		bool bProj = Me && !SDir.IsNearlyZero() && ProjectWorldLocationToScreen(Me->BallLocation(), BallS) && ProjectWorldLocationToScreen(Me->BallLocation() + FVector(SDir * 200.f, 0.f), AheadS);
		const FVector2D Fwd = bProj ? (AheadS - BallS).GetSafeNormal() : FVector2D(0.f, -1.f);
		if (bPtrDown) DragNow = Ptr;
		if (bPendingTap && FVector2D::Distance(DragNow, DragStart) > MinDragPx * UPx) bPendingTap = false;
		const float BackPx = FMath::Max(0.f, -FVector2D::DotProduct(DragNow - DragStart, Fwd));
		DragFrac = FMath::Min(BackPx / (MaxDragDistancePx * UPx), 1.f) * MaxDragForce;
		bDragWouldCancel = BackPx < MinDragPx * UPx || (bProj && FVector2D::Distance(DragNow, BallS) < CancelOnZoogiPx * UPx);
		if (bDragWouldCancel) { if (G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f; }
		else G->HumanDrag(DragFrac, false);
		if (bPtrRelease || !bPtrDown)
		{
			bDragArmed = false;
			if (bPendingTap)
			{
				bPendingTap = false; DragFrac = 0.f;
				if (G->HumanDragFrac >= 0.f) G->HumanDragFrac = -1.f;
				AimAt(TapTarget.Get(), TapPos);
				return;
			}
			UE_LOG(LogZoogi, Log, TEXT("INPUT drag release: back %.0f px of %.0f -> pull %.0f%%%s"), BackPx, MaxDragDistancePx * UPx, DragFrac * 100.f, bDragWouldCancel ? TEXT(" (cancel: aim kept)") : TEXT(""));
			if (bDragWouldCancel) G->HumanDragCancel();
			else G->HumanDrag(DragFrac, true);
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

FVector AZoogiPlayerController::BoomLocation(const FVector& P, float InYaw, float InPitch, float InDist)
{
	const float Pr = FMath::DegreesToRadians(InPitch);
	return P - FRotator(0.f, InYaw, 0.f).Vector() * InDist * FMath::Cos(Pr) + FVector(0.f, 0.f, InDist * FMath::Sin(Pr));
}

void AZoogiPlayerController::ElevatorPushFit(const TArray<FVector>& PadFloors, float InYaw, float InFov, float BaseDist, FVector& OutPivot, float& OutPitch, float& OutDist)
{
	FVector C = FVector::ZeroVector;
	for (const FVector& P : PadFloors) C += P;
	C /= FMath::Max(1, PadFloors.Num());
	OutPivot = FVector(C.X, C.Y, C.Z + 40.f); // pad centroid at lift-top height
	OutPitch = 38.f;                           // a little lower than the sweep's 52: the lifts read as rising
	const float TanH = FMath::Tan(FMath::DegreesToRadians(InFov * 0.5f)) * 0.82f; // keep a margin inside the frame
	const float TanV = TanH / (16.f / 9.f);
	auto Fits = [&](float D)
	{
		const FVector Loc = BoomLocation(OutPivot, InYaw, OutPitch, D);
		const FRotator Rot = (OutPivot - Loc).Rotation();
		for (const FVector& P : PadFloors)
			for (int32 k = 0; k < 10; ++k)
			{
				// pad rim (r ~150) at the floor and the Zoogi on top of the lift (~z +130)
				const float A = k * 72.f;
				const FVector Off = k < 5 ? FVector(FMath::Cos(FMath::DegreesToRadians(A)) * 150.f, FMath::Sin(FMath::DegreesToRadians(A)) * 150.f, 0.f) : FVector(0.f, 0.f, 130.f);
				const FVector L = Rot.UnrotateVector(P + Off - Loc);
				if (L.X < 50.f || FMath::Abs(L.Y) / L.X > TanH || FMath::Abs(L.Z) / L.X > TanV) return false;
			}
		return true;
	};
	float Lo = FMath::Min(900.f, BaseDist), Hi = BaseDist;
	if (!Fits(Hi)) { OutDist = BaseDist; return; }
	for (int32 i = 0; i < 18; ++i) { const float M = 0.5f * (Lo + Hi); if (Fits(M)) Hi = M; else Lo = M; }
	OutDist = Hi;
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

void AZoogiPlayerController::UpdateCamera(float Dt)
{
	using namespace ZoogiCamTune;
	AZoogiGameMode* G = GM();
	if (!Cam || !G) return;
	const float Scale = (G->Arena && G->Arena->IsNeon()) ? NeonScale : (G->Arena ? G->Arena->CamScale : 1.f); // Oct 14: Frozen Ring 2 1.39 (frames r2300), A 1.3 courts 1.15

	if (G->Phase == EZoogiPhase::Forming)
	{
		// Stage forming: a slow sweep in to the art orientation (screen up = -Y) while the stage assembles.
		const float Pg = (G->Arena && G->Arena->Forming) ? G->Arena->Forming->GetProgress() : 1.f;
		FormingSweepPose(Pg, Scale, Yaw, Pitch, Dist, Fov, Pivot);
		Dutch = 0.f;
		// Spawn-elevator push-in over the intro's last 1.25 s (same window the GameMode starts the elevators in), timed off the
		// forming progress so the weight is exactly 0 for the intro's final frames.
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
				ElevPushPivot = FPivot; ElevPushPitch = FPitch; ElevPushDist = FDist;
				if (Pads.Num()) ElevatorPushFit(Pads, FYaw, FFov, FDist, ElevPushPivot, ElevPushPitch, ElevPushDist);
				bElevPushFit = true;
				UE_LOG(LogZoogi, Log, TEXT("CAM elevator push-in: %d pads, pivot %s pitch %.1f dist %.0f (sweep end %.0f)"), Pads.Num(), *ElevPushPivot.ToString(), ElevPushPitch, ElevPushDist, FDist);
			}
			const float W = ElevatorPushWeight(WinT);
			Pivot = FMath::Lerp(Pivot, ElevPushPivot, W);
			Pitch = FMath::Lerp(Pitch, ElevPushPitch, W);
			Dist = FMath::Lerp(Dist, ElevPushDist, W);
		}
		else if (WinT <= 0.f) bElevPushFit = false;
		if (G->FormCamMode == 1)
		{
			// Pillar close-up (forming shots): low view across the floor at the 22.5 deg Resonance pillar and its neighbour.
			Yaw = 22.5f + 25.f; Pitch = 10.f; Dist = 1250.f; Fov = 60.f;
			Pivot = FVector(560.f, 360.f, 120.f);
		}
		Look = Pivot;
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
	const bool bAnimeShot = bAnimeCam && Anime && Anime->IsBusy() && AnimeFocus.IsValid() && !bFocusGone;
	// Court height only from a shooter still in play (a falling Zoogi must not drag the camera pivots down with it).
	const bool bShooterLeaving = !Shooter || IsLeavingPlay(SL, Shooter->bOut, CourtZ, G->Arena);
	if (!bShooterLeaving && SL.Z > -200.f) CourtZ = SL.Z;
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
		}
		else
		{
			WantPivot = C0;
			DistMul = TacResolveDist * Scale;
			// Oct 15 Frozen Ring 2: the resolve / end view frames the whole outer ring (r2300) inside the HUD frame's
			// viewport (~0.65 of the screen width): 1.39 alone clipped its left / right / front edges. -ZoogiR2ViewMul=
			if (AZoogiArena::OuterOutR > 0.f)
			{
				static const float R2ViewMul = [] { float V = 1.3f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiR2ViewMul="), V); return FMath::Clamp(V, 1.f, 2.f); }();
				DistMul *= R2ViewMul;
			}
			if (G->Arena && G->Arena->IsNeonRect()) WantYaw = TacYaw = (FMath::Abs(FRotator::NormalizeAxis(Yaw - 90.f)) <= 90.f ? 90.f : -90.f);
		}
		WantLook = WantPivot;
		ProbeFrom = WantPivot + FVector(0, 0, 150.f);
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
			if (Span > 0.f)
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
	if (G->Phase == EZoogiPhase::Aiming && !bAnimeShot && bRadial)
	{
		const float RMax = (G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) + 250.f;
		auto Clamp2 = [RMax](FVector& V) { const FVector2D V2(V.X, V.Y); if (V2.Size() > RMax) { const FVector2D C = V2.GetSafeNormal() * RMax; V.X = C.X; V.Y = C.Y; } };
		Clamp2(WantPivot); Clamp2(WantLook);
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
			if (E.Size() > RMax) E = E.GetSafeNormal() * RMax;
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
	if (G->Phase == EZoogiPhase::Aiming && !bAnimeShot && !bBlending)
	{
		const float ExpD = WantDist / FMath::Max(ZoomCur, 1.f);
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
	if (G->Phase == EZoogiPhase::Aiming && G->IsHumanAiming() && G->IsAimStep() && !bAnimeShot && !(Anime && Anime->IsBusy()) && GetViewTarget() != Cam)
	{
		ViewBadT += Dt;
		if (ViewBadT > 1.5f)
		{
			UE_LOG(LogZoogi, Warning, TEXT("CAMWATCH view target was %s on the aim step -> match camera restored"), *GetNameSafe(GetViewTarget()));
			SetViewTarget(Cam); ViewBadT = 0.f; ++CamWatchFixes;
		}
	}
	else ViewBadT = 0.f;

	FRotator Rot = (ZLook - Loc).Rotation();
	Rot.Roll = Dutch;
	// Battle framing guard: keep the shooter's silhouette above the letterbox / bottom HUD band by tilting
	// down just enough, without pushing the target out of the top of the frame.
	{
		float Want = 0.f;
		if (PIdx == PBattle && Shooter)
		{
			const float Aspect = ZoogiFramed::ViewAspect(this); // framed HUD: the arena window's real view-rect aspect (full viewport otherwise)
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
	Cam->SetActorLocation(Loc);
	Cam->SetActorRotation(Rot);
	Cam->GetCameraComponent()->SetFieldOfView(FMath::Clamp(FinalFov, 30.f, 120.f));
	ApplyPostProcess(Dt, PIdx, Lock ? FVector::Dist(Loc, TL) : FVector::Dist(Loc, Look));
}
