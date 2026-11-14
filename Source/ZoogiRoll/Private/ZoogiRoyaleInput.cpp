// BATTLE ROYALE controls + camera (AZoogiPlayerController, see ZoogiRoyale.h).
// Touch (Marble Clash style, Oct 26): a finger that lands on the left half drives the move circle (it starts at the lower-left
// home spot; a press away from it floats the circle under the finger), a finger on the right half drags to aim (turns the
// camera; the crosshair stays at the screen centre and the robot's cannons fire on their own when it is on a rival), the
// up-arrow button JUMPs, the swap button TRANSFORMs ball <-> robot. Several fingers at once (Touch1..Touch6).
// PC: WASD / arrows move, Q / E or right-mouse drag aim, Space jump, F / Tab transform, Esc menu. The mouse also works as a
// finger (DefaultInput bUseMouseForTouch).
#include "ZoogiPlayerController.h"
#include "ZoogiShooter.h" // b237
extern float GZoogiShotKick; extern double GZoogiShotAt; // b264 ZoogiRoyale.cpp
#include "ZoogiRoyale.h"
#include "ZoogiCamConfig.h"
#include "ZoogiGameMode.h"
#include "ZoogiHUD.h"
#include "ZoogiBall.h"
#include "ZoogiRoll.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiApplePad.h"
#include "ZoogiOrb.h"
#include "ZoogiFront.h"

namespace
{
	constexpr int32 RoyalePPProfile = 8; // ZoogiCamTune::PTactical (ZoogiPlayerController.cpp): the readable gameplay grade
	constexpr int32 RoyaleMouseSlot = 5; // RoyaleTouchWas[5] = left mouse button when it does not arrive as Touch1
	uint32 GRoyalePressPending = 0;      // a press is classified one frame later, with the finger's fresh position
	bool GRoyaleWasRespawning = false;   // the human's Zoogi was waiting to respawn last frame
	// Nov 1 b51 (Evan: "when I move my finger the camera should move, not wait for me to return to the centre ... just as if my
	// finger moved the view on the screen"): the on-screen Apple right stick is a TRACKPAD. The camera turns by how far the thumb
	// MOVES (stick value change x gain), so sliding back turns back at once - no crossing the centre, no hold-to-turn wait.
	// Holding the thumb against the rim keeps turning that way (the finger ran out of pad). Lifting / touching down never turns
	// the camera (the stick snapping to / from the centre is not a finger move). A real controller keeps hold-to-turn (rescaled).
	// -ZoogiStickLookGain=<deg per full stick travel> (default 150). Point-to-look (build 50) is gone.
	bool GLookStickHeld = false;          // the thumb is on the Apple right stick
	FVector2D GLookStickPrev = FVector2D::ZeroVector;
	// radial dead zone + rescale: no jump from 0 to 20% at the edge of the dead zone, full speed before the rim
	FVector2D StickCurve(const FVector2D& S, float Dead, float Full)
	{
		const float M = S.Size();
		if (M <= Dead) return FVector2D::ZeroVector;
		return S / M * FMath::Clamp((M - Dead) / FMath::Max(0.05f, Full - Dead), 0.f, 1.f);
	}
	float GRoyaleCamBack = -1.f;         // Oct 27 chase camera: current distance behind (pulled in by walls, eases back out)
	// Oct 31 b41 (Evan: AAA dynamic camera refs): spring state for the vertical damping, landing dip, speed framing, rival pull-back
	float GCamZVel = 0.f;                // vertical damping spring velocity (pivot Z)
	float GCamPrevVz = 0.f;              // the Zoogi's vertical speed last frame (landing detection)
	float GCamDip = 0.f, GCamDipVel = 0.f; // landing dip spring (cm, + = down)
	float GCamSpeed01 = 0.f;             // smoothed speed (0..1) -> wider FOV + longer boom
	float GCamCrowd01 = 0.f;             // smoothed "rival close" (0..1) -> adaptive boom pull-back
	FVector2D GCamLead = FVector2D::ZeroVector; // look-ahead along the velocity
	inline void Spring(float& X, float& V, float Target, float Omega, float Dt) // critically damped
	{
		const float F = 1.f + 2.f * Dt * Omega, OO = Omega * Omega, HOO = Dt * OO, HHOO = Dt * HOO;
		const float Det = 1.f / (F + HHOO);
		const float X2 = (F * X + Dt * V + HHOO * Target) * Det;
		V = (V + HOO * (Target - X)) * Det; X = X2;
	}
}

void AZoogiPlayerController::OnRoyaleStarted()
{
	bMenuCam = false;
	EndSlowMo();
	HoverTarget.Reset();
	AZoogiGameMode* G = GM();
	AZoogiBall* B = G ? G->RoyaleFocusBall() : nullptr;
	RoyaleYaw = B ? B->SpawnYaw : 0.f;
	RoyaleCamYaw = RoyaleYaw;
	RoyalePitchAdj = 0.f;
	RoyaleRobotBlend = 0.f;
	RoyaleMoveTouch = RoyaleLookTouch = -1;
	for (bool& bWas : RoyaleTouchWas) bWas = true; // a finger still down from the START tap must lift before it counts
	GRoyalePressPending = 0;
	GRoyaleWasRespawning = false;
	GLookStickHeld = false;
	GLookStickPrev = FVector2D::ZeroVector;
	bRoyaleRMB = false;
	RoyaleKnobOff = FVector2D::ZeroVector;
	bRoyaleCamInit = false;
	GRoyaleCamBack = -1.f;
	GCamZVel = GCamPrevVz = GCamDip = GCamDipVel = GCamSpeed01 = GCamCrowd01 = 0.f; GCamLead = FVector2D::ZeroVector;
	// Oct 27 (iPhone): UE's stock virtual joysticks (DefaultTouchInterface) sat over the right side and ate the taps on
	// JUMP / TRANSFORM - that faint "right stick". The royale draws and reads its own touch controls: no stock overlay.
	ActivateTouchInterface(nullptr);
	CamTarget = B;
	if (Cam) SetViewTarget(Cam);
	UE_LOG(LogZoogi, Log, TEXT("ROYALE controls on (camera yaw %.0f)"), RoyaleYaw);
}

void AZoogiPlayerController::RoyalePress(int32 Slot, const FVector2D& Pt, const FVector2D& View, AZoogiGameMode* G)
{
	const ZoogiRoyaleUI::FPad Pad = ZoogiRoyaleUI::Layout(View);
	// HUD buttons (MENU, results REMATCH / MENU) first
	if (const AZoogiHUD* Hud = Cast<AZoogiHUD>(GetHUD()))
	{
		FName Action; int32 Index = 0;
		if (Hud->HitTest(Pt, Action, Index))
		{
			// next tick, so the menu that MENU / REMATCH brings up never sees this same press
			TWeakObjectPtr<AZoogiGameMode> WG = G;
			GetWorldTimerManager().SetTimerForNextTick([WG, Action, Index]() { if (WG.IsValid()) WG->MenuAction(Action, Index); });
			return;
		}
	}
	if (G->Phase == EZoogiPhase::GameOver) return;
	// thumbs on a phone land a little off the art: generous hit circles (the two buttons are far enough apart)
	const bool bApple = ZoogiApplePad::Active(); // Apple controller on screen: its stick / buttons own those spots, right-half drags still look
	if (!bApple && FVector2D::Distance(Pt, Pad.JumpC) <= Pad.JumpR * 1.5f) { G->MenuAction(TEXT("RoyaleJump"), 0); return; }
	if (!bApple && FVector2D::Distance(Pt, Pad.MorphC) <= Pad.MorphR * 1.5f) { G->MenuAction(TEXT("RoyaleTransform"), 0); return; }
	if (Pt.X < View.X * 0.5f)
	{
		// Nov 1 b53: with the Apple left stick hidden (settings) the left half is an invisible floating move stick again
		if ((bApple && (ZoogiControls::StickHide() & 1) == 0) || RoyaleMoveTouch >= 0) return;
		RoyaleMoveTouch = Slot;
		// press on / near the circle: it stays home; anywhere else on the left half: the circle comes to the finger
		if (G->bRoyaleMarble) RoyaleStickBase = Pt; // TRAINING MARBLE controls: the pull starts where the thumb lands
		else if (FVector2D::Distance(Pt, Pad.StickHome) <= Pad.StickR * 1.35f) RoyaleStickBase = Pad.StickHome;
		else RoyaleStickBase = FVector2D(FMath::Clamp(Pt.X, Pad.StickR, View.X * 0.5f - Pad.StickR * 0.5f), FMath::Clamp(Pt.Y, Pad.StickR + 90.f * Pad.U, View.Y - Pad.StickR));
		RoyaleStickNow = Pt;
	}
	else if (RoyaleLookTouch < 0)
	{
		RoyaleLookTouch = Slot;
		RoyaleLookLast = Pt;
	}
}

void AZoogiPlayerController::RoyaleMarbleRelease(AZoogiGameMode* G, const ZoogiRoyaleUI::FPad& Pad)
{
	const FVector2D D = RoyaleStickNow - RoyaleStickBase;
	const float MaxPull = Pad.StickR * 1.2f, Dead = 22.f * Pad.U;
	if (D.Size() < Dead) return; // a tap, not a pull
	const float Frac = FMath::Clamp((D.Size() - Dead) / (MaxPull - Dead), 0.f, 1.f);
	const float YR = FMath::DegreesToRadians(RoyaleCamYaw);
	const FVector2D Fwd(FMath::Cos(YR), FMath::Sin(YR)), Right(-FMath::Sin(YR), FMath::Cos(YR));
	const FVector2D Dir = (Fwd * D.Y - Right * D.X).GetSafeNormal(); // screen pull is the opposite of the shot
	if (G->RoyaleMarbleLaunch(Dir, Frac)) RoyaleYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)); // the camera turns to chase the shot
}

void AZoogiPlayerController::RoyaleInput(float Dt)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	int32 VX = 1920, VY = 1080;
	GetViewportSize(VX, VY);
	const FVector2D View((float)FMath::Max(VX, 64), (float)FMath::Max(VY, 64));
	const ZoogiRoyaleUI::FPad Pad = ZoogiRoyaleUI::Layout(View);
	const bool bOver = G->Phase == EZoogiPhase::GameOver;

	{	// Nov 1 b52 capture: -ZoogiRoyalePauseTest opens the pause panel once the fight is on (frame royale_pause.png)
		static bool bPauseTestDone = false;
		if (!bPauseTestDone && G->Royale.Step == EZoogiRoyaleStep::Fight && G->PhaseTime > 8.f && FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyalePauseTest"))) { bPauseTestDone = true; ZoogiFront::OpenRoyalePause(GetWorld()); }
	}
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		// next tick: the native menu comes up this frame and would also take this Esc (its QUIT? prompt)
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		GetWorldTimerManager().SetTimerForNextTick([WG]() { if (WG.IsValid() && WG->IsRoyaleMatch()) WG->MenuAction(TEXT("Menu"), 0); });
		return;
	}
	if (bOver && (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::R) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))) { G->MenuAction(TEXT("Rematch"), 0); return; }
	if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom))
		G->MenuAction(TEXT("RoyaleJump"), 0);
	if (WasInputKeyJustPressed(EKeys::F) || WasInputKeyJustPressed(EKeys::Tab) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left))
		G->MenuAction(TEXT("RoyaleTransform"), 0);
	// Oct 31 b41: Apple controller / any pad - B = attack (bolt, or the held relic), Y = AWAKEN; E / Q on keyboards
	// b45 Gatling: HOLD to fire (the fire cadence gates it); Left Ctrl on keyboards (Q / E turn the camera)
	if (IsInputKeyDown(EKeys::Gamepad_FaceButton_Right) || IsInputKeyDown(EKeys::Gamepad_RightTrigger) || IsInputKeyDown(EKeys::LeftControl))
		G->MenuAction(TEXT("RoyaleAttack"), 0);
	if (WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top) || WasInputKeyJustPressed(EKeys::G))
		G->MenuAction(TEXT("RoyaleAwaken"), 0);
	// aiming in robot form is a little finer than turning the chase camera
	// Nov 1 b52: player-tuned sensitivity + invert (Royale pause panel, ZoogiControls); up / down flipped from build 51 (Evan: "up is down")
	const float AimSens = T.LookSensitivity * FMath::Lerp(1.f, 0.75f, RoyaleRobotBlend) * ZoogiControls::LookSens();
	const float PitchSign = ZoogiControls::InvertY() ? 1.f : -1.f;

	// ---- fingers (and the mouse when it is not already Touch1)
	auto Track = [&](int32 Slot, bool bDown, const FVector2D& Pt, bool bTapEdge)
	{
		// a tap that went down and up between two frames (quick taps / clicks): classify it right away
		if (bTapEdge && !bDown && !RoyaleTouchWas[Slot])
		{
			UE_LOG(LogZoogi, Log, TEXT("ROYALE tap slot %d at (%.0f, %.0f)"), Slot, Pt.X, Pt.Y);
			RoyalePress(Slot, Pt, View, G);
			if (Slot == RoyaleMoveTouch) RoyaleMoveTouch = -1;
			if (Slot == RoyaleLookTouch) RoyaleLookTouch = -1;
			return;
		}
		const bool bPress = bDown && !RoyaleTouchWas[Slot];
		const bool bRelease = !bDown && RoyaleTouchWas[Slot];
		RoyaleTouchWas[Slot] = bDown;
		const uint32 Bit = 1u << Slot;
		if (bPress) GRoyalePressPending |= Bit;
		else if ((GRoyalePressPending & Bit) && (bDown || bRelease))
		{
			GRoyalePressPending &= ~Bit;
			UE_LOG(LogZoogi, Log, TEXT("ROYALE press slot %d at (%.0f, %.0f) view %.0fx%.0f"), Slot, Pt.X, Pt.Y, View.X, View.Y);
			RoyalePress(Slot, Pt, View, G);
			if (Slot == RoyaleMoveTouch) RoyaleStickNow = Pt;
		}
		if (bDown && !(GRoyalePressPending & Bit))
		{
			if (Slot == RoyaleMoveTouch) RoyaleStickNow = Pt;
			if (Slot == RoyaleLookTouch)
			{
				const FVector2D D = Pt - RoyaleLookLast;
				RoyaleLookLast = Pt;
				RoyaleYaw += D.X * AimSens / Pad.U;
				RoyalePitchAdj = FMath::Clamp(RoyalePitchAdj + PitchSign * D.Y * 0.06f * ZoogiControls::LookSens() / Pad.U, -32.f, 30.f);
			}
		}
		if (bRelease)
		{
			// TRAINING MARBLE controls: letting go of the pull launches (pull down = roll forward, like the classic slingshot)
			if (Slot == RoyaleMoveTouch && G->bRoyaleMarble) RoyaleMarbleRelease(G, Pad);
			if (Slot == RoyaleMoveTouch) RoyaleMoveTouch = -1;
			if (Slot == RoyaleLookTouch) RoyaleLookTouch = -1;
		}
	};
	bool bTouch1 = false;
	for (int32 t = 0; t < RoyaleMouseSlot; ++t)
	{
		float X = 0.f, Y = 0.f; bool bDown = false;
		GetInputTouchState((ETouchIndex::Type)t, X, Y, bDown);
		if (t == 0) bTouch1 = bDown;
		// mouse-as-touch (DefaultInput bUseMouseForTouch): Touch1 keeps the last tap's position on a new press until the
		// pointer moves, so on a PC the real cursor is the finger
		float MX = 0.f, MY = 0.f;
		const bool bTapEdge = WasInputKeyJustPressed(EKeys::TouchKeys[t]);
		// Oct 26 (iPhone test): desktop only. On iOS / Android the cursor position is a stale copy of the previous touch,
		// so a tap on JUMP / TRANSFORM was classified at the last finger spot and the buttons never fired.
#if PLATFORM_DESKTOP
		if (t == 0 && (bDown || bTapEdge || RoyaleTouchWas[0]) && GetMousePosition(MX, MY)) { X = MX; Y = MY; }
#endif
		Track(t, bDown, FVector2D(X, Y), bTapEdge);
	}
	{
		float MX = 0.f, MY = 0.f;
		const bool bHasMouse = GetMousePosition(MX, MY);
		Track(RoyaleMouseSlot, bHasMouse && !bTouch1 && IsInputKeyDown(EKeys::LeftMouseButton), FVector2D(MX, MY), bHasMouse && !bTouch1 && WasInputKeyJustPressed(EKeys::LeftMouseButton));
		// right mouse drag = look
		const bool bRMB = bHasMouse && IsInputKeyDown(EKeys::RightMouseButton);
		if (bRMB && bRoyaleRMB)
		{
			RoyaleYaw += (MX - RoyaleRMBLast.X) * AimSens / Pad.U;
			RoyalePitchAdj = FMath::Clamp(RoyalePitchAdj + PitchSign * (MY - RoyaleRMBLast.Y) * 0.06f / Pad.U, -32.f, 30.f);
		}
		bRoyaleRMB = bRMB;
		RoyaleRMBLast = FVector2D(MX, MY);
	}

	// ---- keys / pad look
	if (IsInputKeyDown(EKeys::Q)) RoyaleYaw -= T.KeyLookRate * Dt;
	if (IsInputKeyDown(EKeys::E)) RoyaleYaw += T.KeyLookRate * Dt;
	{
		const FVector2D RS(GetInputAnalogKeyState(EKeys::Gamepad_RightX), GetInputAnalogKeyState(EKeys::Gamepad_RightY));
		if (ZoogiApplePad::Active())
		{
			static float Gain = -1.f;
			if (Gain < 0.f) { Gain = 150.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiStickLookGain="), Gain); }
			const float SGain = Gain * ZoogiControls::StickSens();
			const float M = RS.Size();
			if (M < 0.04f) GLookStickHeld = false; // lifted: the snap back to the centre is not a move
			else if (!GLookStickHeld) { GLookStickHeld = true; GLookStickPrev = RS; } // touched down: start from here, no jump
			else
			{
				const FVector2D D = RS - GLookStickPrev;
				// a sudden collapse to near-zero is the release frame (the virtual stick springs back), not a swipe
				if (!(M < 0.12f && GLookStickPrev.Size() > 0.35f))
				{
					RoyaleYaw += D.X * SGain * FMath::Lerp(1.f, 0.75f, RoyaleRobotBlend);
					RoyalePitchAdj = FMath::Clamp(RoyalePitchAdj + PitchSign * D.Y * SGain * 0.2f, -32.f, 30.f);
				}
				// thumb pinned at the rim: keep turning toward it, faster the harder it presses out
				if (M > 0.93f && FMath::Abs(RS.X) > 0.5f) RoyaleYaw += FMath::Sign(RS.X) * FMath::Lerp(90.f, 240.f, FMath::Clamp((FMath::Abs(RS.X) - 0.5f) / 0.5f, 0.f, 1.f)) * Dt;
				GLookStickPrev = RS;
			}
		}
		else RoyaleYaw += StickCurve(RS, 0.15f, 0.95f).X * T.KeyLookRate * 1.3f * Dt; // physical controller: hold to turn
	}
	RoyaleYaw = FRotator::NormalizeAxis(RoyaleYaw);

	// ---- move axis (x = screen right, y = screen up)
	FVector2D Axis = FVector2D::ZeroVector;
	if (RoyaleMoveTouch >= 0)
	{
		FVector2D D = RoyaleStickNow - RoyaleStickBase;
		// Nov 1: the ring follows a thumb that runs past the rim, so turning back never means dragging all the way past the old centre
		if (!G->bRoyaleMarble && D.Size() > Pad.StickR) { RoyaleStickBase = RoyaleStickNow - D.GetSafeNormal() * Pad.StickR; D = RoyaleStickNow - RoyaleStickBase; }
		if (D.Size() > Pad.StickR) D = D.GetSafeNormal() * Pad.StickR;
		RoyaleKnobOff = G->bRoyaleMarble ? (RoyaleStickNow - RoyaleStickBase).GetClampedToMaxSize(Pad.StickR * 1.2f) : D;
		const float Mag = D.Size() / Pad.StickR;
		const float Dead = 0.1f;
		if (Mag > Dead) Axis = FVector2D(D.X, -D.Y).GetSafeNormal() * FMath::Min(1.f, (Mag - Dead) / (1.f - Dead) * 1.15f * ZoogiControls::MoveSens());
	}
	else
	{
		RoyaleKnobOff = FVector2D::ZeroVector;
		RoyaleStickBase = Pad.StickHome;
	}
	FVector2D Keys = FVector2D::ZeroVector;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) Keys.Y += 1.f;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Keys.Y -= 1.f;
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) Keys.X += 1.f;
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) Keys.X -= 1.f;
	if (!Keys.IsNearlyZero()) Axis = Keys.GetSafeNormal();
	const FVector2D PadStick(GetInputAnalogKeyState(EKeys::Gamepad_LeftX), GetInputAnalogKeyState(EKeys::Gamepad_LeftY));
	if (PadStick.Size() > 0.12f) Axis = StickCurve(PadStick, 0.12f, FMath::Clamp(0.92f / ZoogiControls::MoveSens(), 0.35f, 1.f)); // rescaled dead zone; MOVE STICK setting moves the full-speed point
	if (bOver) Axis = FVector2D::ZeroVector;

	// camera-relative -> world (yaw 0 looks down +X, screen right = +Y)
	const float YR = FMath::DegreesToRadians(RoyaleCamYaw);
	const FVector2D Fwd(FMath::Cos(YR), FMath::Sin(YR)), Right(-FMath::Sin(YR), FMath::Cos(YR));
	G->RoyaleSetHumanMove(Fwd * Axis.Y + Right * Axis.X);
	HoverTarget.Reset();
}

void AZoogiPlayerController::RoyaleCamera(float Dt)
{
	AZoogiGameMode* G = GM();
	AZoogiBall* B = G ? G->RoyaleFocusBall() : nullptr;
	if (!Cam || !G || !B) return;
	const FZoogiRoyaleTune& T = FZoogiRoyaleTune::Get();
	int32 VX = 1920, VY = 1080;
	GetViewportSize(VX, VY);
	const bool bPortrait = VY > VX;
	const bool bOver = G->Phase == EZoogiPhase::GameOver;
	// the pivot stays on the court: a Zoogi that falls off leaves the shot instead of dragging the camera down the cliff
	const FVector BL = B->BallLocation();
	const bool bOnCourt = !B->bOut && BL.Z > G->RoyaleFloorZ - 60.f;
	if (!bRoyaleCamInit) { RoyalePivot = BL; RoyaleCamYaw = RoyaleYaw; bRoyaleCamInit = true; }
	// the human's Zoogi just dropped back in: face the middle from the new spot (the old heading could point off the edge)
	{
		const bool bResp = G->Royale.P.IsValidIndex(G->Royale.HumanIndex) && G->Royale.P[G->Royale.HumanIndex].RespawnIn >= 0.f;
		if (GRoyaleWasRespawning && !bResp && B->PlayerIndex == G->Royale.HumanIndex)
		{
			RoyaleYaw = RoyaleCamYaw = FMath::RadiansToDegrees(FMath::Atan2(-BL.Y, -BL.X));
			RoyalePivot = BL;
		}
		GRoyaleWasRespawning = bResp;
	}
	if (bOnCourt)
	{
		// the chase camera sits close: follow the Zoogi tightly on the floor (it keeps its size on screen), softer up / down
		const FVector2D XY = FMath::Vector2DInterpTo(FVector2D(RoyalePivot), FVector2D(BL), Dt, ZoogiCamConfig::Get(ZoogiCamConfig::ModeName(this)).Follow);
		// Oct 31 b41 vertical damping: a critically damped spring on Z (no jitter on steps / bumps / ramps), faster for big drops
		float Pz = RoyalePivot.Z;
		const float Dz = BL.Z - Pz;
		Spring(Pz, GCamZVel, BL.Z, FMath::Abs(Dz) > 350.f ? 9.f : 4.5f, Dt);
		RoyalePivot = FVector(XY.X, XY.Y, Pz);
	}
	// robot form: a lower, closer over-the-shoulder aim camera (blends in / out with the transform)
	{
		const int32 Me = B->PlayerIndex;
		const bool bRobot = G->Royale.P.IsValidIndex(Me) && G->Royale.P[Me].bRobot && !bOver;
		RoyaleRobotBlend = FMath::FInterpTo(RoyaleRobotBlend, bRobot ? 1.f : 0.f, Dt, 5.f);
	}
	if (bOver) RoyaleYaw = FRotator::NormalizeAxis(RoyaleYaw + 14.f * Dt); // slow victory orbit
	{
		static const bool bBotHuman = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRoyaleBotHuman"));
		const int32 Me = B->PlayerIndex;
		if (bBotHuman && !bOver && Me == G->Royale.HumanIndex && G->Royale.P.IsValidIndex(Me))
		{
			const FZoogiRoyalePlayer& RP = G->Royale.P[Me];
			const FVector BV = B->GetBallVelocity();
			const float Want = RP.bRobot ? RP.FacingYaw : (BV.Size2D() > 150.f ? FMath::RadiansToDegrees(FMath::Atan2(BV.Y, BV.X)) : RoyaleYaw);
			RoyaleYaw = FMath::FixedTurn(RoyaleYaw, Want, 160.f * Dt);
		}
	}
	// Oct 27: every camera number comes from the Camera Configuration Manager (shipped values, or the dev profile when unlocked)
	const FZoogiCamSettings& CS = ZoogiCamConfig::Get(ZoogiCamConfig::ModeName(this));
	RoyaleCamYaw = FRotator::NormalizeAxis(RoyaleCamYaw + FMath::FindDeltaAngleDegrees(RoyaleCamYaw, RoyaleYaw) * FMath::Min(1.f, Dt * CS.Turn));
	// Oct 27 (Evan, Marble Clash camera): a low chase camera just behind and above the Zoogi, looking level ahead, so the
	// Zoogi sits at the bottom centre of the screen and the crosshair (screen centre) aims straight down the arena.
	// Walls behind pull the camera in (and it eases back out). Tuning: ChaseBack / ChaseUp / ChasePitch / ChaseFov.
	const float CountT = G->Royale.Step == EZoogiRoyaleStep::Countdown ? 1.f - FMath::Clamp(G->Royale.StepTime / FMath::Max(T.CountdownSeconds, 0.1f), 0.f, 1.f) : 0.f;
	float Back = FMath::Lerp(CS.Back, CS.RobotBack, RoyaleRobotBlend) * (bPortrait ? 1.25f : 1.f);
	float Up = FMath::Lerp(CS.Up, CS.RobotUp, RoyaleRobotBlend);
	Back *= 1.f + 1.6f * CountT; // swoops down behind the Zoogi for GO
	bool bShooterCam = false; float ShooterSide = 0.f, PitchBase = CS.Pitch, FovBase = CS.Fov;
	Up *= 1.f + 2.2f * CountT;
	if (bOver) { Back *= 1.9f; Up *= 2.4f; }
	// Oct 31 b41: the human's mech is the 185 cm crystal hero (ZoogiRoyaleHero.cpp), not the small transform bot - frame all of it
	{
		static const bool bNoHero = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHero"));
		bShooterCam = !bNoHero && B->PlayerIndex == G->Royale.HumanIndex && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldRoyaleCam"));
		if (bShooterCam)
		{
			// Oct 31 b44 (Evan refs): BALL = low and close right behind the ball, ball big at the bottom, looking level down the arena;
			// STANDING (mech) = third-person shooter over the right shoulder, the Zoogi on the left third, crosshair in the middle.
			const float Bl = RoyaleRobotBlend;
			const float Mul = (1.f + 1.6f * CountT) * (bOver ? 1.9f : 1.f), UpMul = (1.f + 2.2f * CountT) * (bOver ? 2.4f : 1.f);
			// b45 (Evan: "too much body - nothing other than the reference"): tight over the right shoulder at head height - only the head,
			// shoulder and the gun in the lower left, the arena fills the frame
			const bool bVoyCam = ZoogiShooter::On(G); // b237: the Voyager third-person framing
			Back = FMath::Lerp(290.f, bVoyCam ? 240.f : 150.f, Bl) * (bPortrait ? 1.2f : 1.f) * Mul;
			Up = FMath::Lerp(70.f, bVoyCam ? 150.f : 140.f, Bl) * UpMul;
			ShooterSide = FMath::Lerp(0.f, bVoyCam ? 74.f : 66.f, Bl) * (bOver ? 0.f : 1.f - CountT) * (bPortrait ? 0.75f : 1.f);
			PitchBase = FMath::Lerp(4.f, 6.f, Bl);
			FovBase = FMath::Lerp(84.f, bVoyCam ? 80.f : 72.f, Bl);
			// b264 (Evan: keep Voyager's camera): AIM ZOOM - while a target is locked / you are shooting the camera tucks in over the
			// shoulder and narrows; every shot kicks the view up a little (recoil) and it settles back
			if (bVoyCam)
			{
				static float GAimZoom = 0.f;
				const int32 Me = B->PlayerIndex;
				const bool bAimNow = G->Royale.P.IsValidIndex(Me) && G->Royale.P[Me].bRobot && !bOver && (G->Royale.P[Me].Lock >= 0 || G->GetWorld()->GetTimeSeconds() - GZoogiShotAt < 0.6);
				GAimZoom = FMath::FInterpTo(GAimZoom, bAimNow ? 1.f : 0.f, Dt, bAimNow ? 9.f : 4.f);
				Back *= 1.f - 0.32f * GAimZoom;
				ShooterSide *= 1.f - 0.12f * GAimZoom;
				FovBase -= 16.f * GAimZoom;
				GZoogiShotKick = FMath::FInterpTo(GZoogiShotKick, 0.f, Dt, 10.f);
				PitchBase += GZoogiShotKick;
			}
		}
	}
	const float RPitch = FMath::Clamp(PitchBase + RoyalePitchAdj + (bOver ? 14.f : 0.f) + 10.f * CountT, -30.f, 45.f); // Nov 1 b52: look much further up / down
	const FRotator Rot(-RPitch, RoyaleCamYaw, 0.f);
	const FVector Fwd2 = FRotator(0.f, RoyaleCamYaw, 0.f).Vector();
	// ---- Oct 31 b41 AAA dynamic states (Evan refs): speed framing, look-ahead, landing shake + dip, rival pull-back
	const FVector BV = B->GetBallVelocity();
	{
		const float Sp01 = FMath::Clamp((float)BV.Size2D() / 1800.f, 0.f, 1.f);
		GCamSpeed01 = FMath::FInterpTo(GCamSpeed01, bOver ? 0.f : Sp01, Dt, Sp01 > GCamSpeed01 ? 2.5f : 1.5f);
		const FVector2D LeadWant = bOver ? FVector2D::ZeroVector : FVector2D(BV.X, BV.Y).GetClampedToMaxSize(1800.f) * 0.11f * (bShooterCam ? 1.f - RoyaleRobotBlend : 1.f);
		GCamLead = FMath::Vector2DInterpTo(GCamLead, LeadWant, Dt, 2.f);
		// landing: was falling fast, now stopped -> shake + dip scaled with the fall speed
		const bool bGround = FMath::Abs(BV.Z) < 120.f;
		if (bGround && GCamPrevVz < -520.f && bOnCourt)
		{
			const float K = FMath::Clamp((-GCamPrevVz - 520.f) / 1500.f, 0.f, 1.f);
			ImpactAge = 0.f; ImpactStrength = 0.6f + 1.8f * K;
			GCamDipVel += 260.f + 620.f * K;
		}
		GCamPrevVz = BV.Z;
		Spring(GCamDip, GCamDipVel, 0.f, 11.f, Dt);
		// a rival close to you: the boom eases back so both stay framed
		float Near = 0.f;
		for (const AZoogiBall* X : G->Balls)
			if (X && X != B && !X->bOut) Near = FMath::Max(Near, 1.f - FMath::Clamp(((float)FVector::Dist(X->BallLocation(), BL) - 250.f) / 750.f, 0.f, 1.f));
		GCamCrowd01 = FMath::FInterpTo(GCamCrowd01, bOver ? 0.f : Near, Dt, 2.f);
		const float Dyn = bShooterCam ? 1.f - 0.9f * RoyaleRobotBlend : 1.f; // b45: the shoulder camera stays tight
		Back *= 1.f + (0.14f * GCamSpeed01 + 0.32f * GCamCrowd01) * Dyn;
		Up *= 1.f + 0.18f * GCamCrowd01 * Dyn;
	}
	const FVector RLook = RoyalePivot + FVector(GCamLead.X, GCamLead.Y, Up - GCamDip * 0.35f);
	// wall check: from above the Zoogi back to the wanted spot
	float WantBack = Back;
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZoogiRoyaleChaseCam), false);
		for (const AZoogiBall* X : G->Balls) if (X) Q.AddIgnoredActor(X);
		for (const AZoogiOrb* O : G->Orbs) if (O) Q.AddIgnoredActor(O);
		FHitResult Hit;
		// Oct 31 b41 smart collision: a fatter probe along the real boom (pitched), retract to the hit, then auto-recover smoothly
		const FVector BoomEnd = RLook + FVector(-Fwd2.Y, Fwd2.X, 0.f) * ShooterSide - Fwd2 * Back; // b44: + the shoulder offset
		if (GetWorld()->SweepSingleByObjectType(Hit, RLook, BoomEnd, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(30.f), Q))
			WantBack = FMath::Max(bShooterCam ? 18.f : 90.f, Back * Hit.Time - 12.f); // b45: the shoulder camera may tuck right in (a 90 cm floor left it inside the volcano)
	}
	if (GRoyaleCamBack < 0.f) GRoyaleCamBack = WantBack;
	GRoyaleCamBack = WantBack < GRoyaleCamBack ? FMath::FInterpTo(GRoyaleCamBack, WantBack, Dt, 22.f) : FMath::FInterpTo(GRoyaleCamBack, WantBack, Dt, 2.2f); // fast smooth retract, gentle recovery
	FVector Loc = RLook + (FVector(-Fwd2.Y, Fwd2.X, 0.f) * ShooterSide - Fwd2 * Back) * (GRoyaleCamBack / FMath::Max(Back, 1.f));
	Loc.Z -= GCamDip;
	Loc.Z = FMath::Max(Loc.Z, G->RoyaleFloorZ + 40.f);
	ImpactAge += Dt;
	if (ImpactAge < 0.35f && ImpactStrength > 0.f)
	{
		const float S = ImpactStrength * (1.f - ImpactAge / 0.35f) * 7.f * CS.Shake;
		Loc += FVector(FMath::FRandRange(-S, S), FMath::FRandRange(-S, S), FMath::FRandRange(-S, S) * 0.5f);
	}
	Cam->SetActorLocationAndRotation(Loc, Rot);
	// the crosshair is the screen centre: hand its ray to the game (robot aim + auto-fire lock)
	if (B->PlayerIndex == G->Royale.HumanIndex) G->RoyaleSetHumanAim(Loc, Rot.Vector());
	Cam->GetCameraComponent()->SetFieldOfView((bPortrait ? FovBase * 0.66f : FovBase) + 7.f * GCamSpeed01 + 4.f * GCamCrowd01); // speed / rival framing
	ApplyPostProcess(Dt, RoyalePPProfile, FVector::Dist(Loc, RoyalePivot));
	if (GetViewTarget() != Cam) SetViewTarget(Cam);
}
