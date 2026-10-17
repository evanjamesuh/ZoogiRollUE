#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZoogiGameMode.h"
#include "ZoogiAimArrow.h"
#include "ZoogiPlayerController.generated.h"

class ACameraActor;
class AZoogiBall;
class AZoogiTargetMarker;
class AZoogiArena;
struct FZoogiCombatEvent;

/**
 * Targeting / clickable aiming input (click target or floor, arrows nudge, Space confirm, RMB / Esc cancel) and the camera director.
 * Default camera = Tactical (Evan Oct 4): one readable high view with small emphasis moves; -ZoogiCinematic / the CAM button
 * restores the per-phase cinematic profiles (ZoogiCamTune in the .cpp). Anime placeholders get their own close shot.
 */
UCLASS()
class ZOOGIROLL_API AZoogiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AZoogiPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Portrait camera framing hook (Gameplay & UI orientation toggle / rotate prompt): true = portrait framing (capped vertical FOV,
	 *  width-keeping boom, HUD-safe band, whole-court fit), false = landscape framing, regardless of the view's aspect. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Camera")
	void SetPortraitCameraMode(bool bPortrait);
	/** Back to auto-detect (portrait framing whenever the view's aspect < 1). Also the default on every BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|Camera")
	void ClearPortraitCameraMode();
	/** Whether the match camera is framing for portrait right now (hook or auto-detect). */
	UFUNCTION(BlueprintPure, Category = "Zoogi|Camera")
	bool IsPortraitCameraMode() const;

	void OnTurnStarted(AZoogiBall* Ball);
	void OnShotFired(AZoogiBall* Ball);
	void OnGameOver(AZoogiBall* Winner);
	void OnMenu();
	void OnStageChanged(EZoogiStage Stage);
	/** First contact of a shot: brief hit-stop, shake and FOV punch scaled by Strength (0..1). */
	void OnImpact(float Strength);
	void OnBattleStep(EZoogiBattleStep Step);
	/** Infused first contact: hit-stop (tactical camera only stops for powered hits), bigger punch on crits. */
	void OnPoweredHit(bool bCrit);
	void OnAnimeStarted(const FZoogiCombatEvent& E);
	void OnAnimeFinished();

	AActor* GetHoverTarget() const { return HoverTarget.Get(); }

	// ---- ring-out camera safety (Oct 14 playtest: the follow camera chased a falling Zoogi into the cliffs).
	/** Oct 16: the arena's own play area (Arena->IsOffCourt: its knock-off radius, e.g. r2015 on Volcanic/Cosmic/Arabian/Tomb,
	 *  Frozen Ring 2's bridges / outer ring, the Neon rect court when that is on); no arena = r > AZoogiArena::ActiveKnockoffR. */
	static bool IsOffPlayArea(const AZoogiArena* Arena, const FVector2D& P);
	/** Radius the ring-out camera clamps follow pivots inside: the arena's knock-off radius (ActiveKnockoffR without an arena). */
	static float PlayRadius(const AZoogiArena* Arena);
	/** True once a Zoogi has left play: marked out, off the arena's play area (IsOffPlayArea) or below the floor it was on. */
	static bool IsLeavingPlay(const FVector& BallLoc, bool bOut, float CourtZ, const AZoogiArena* Arena = nullptr);
	/** Tactical Rolling follow pivot: partway toward the shooter but never off the court (inside the ring, at court
	 *  height); the arena centre once the shooter is leaving play, so the falling Zoogi leaves the shot instead. */
	static FVector TacticalRollPivot(const FVector& ShooterLoc, bool bOut, float CourtZ, const AZoogiArena* Arena = nullptr);
	/** Tactical boom (pitch, distance incl. the arena's camera scale, FOV, pivot lag) for G's arena; for the shot test. */
	static void TacticalBoom(const AZoogiGameMode* G, float& OutPitch, float& OutDist, float& OutFov, float& OutPosLag);

	// ---- spawn-elevator push-in (Oct 16, ZOOGI): the elevators rise over the last ~1.25 s of the stage intro and read too small
	// at the wide forming camera, so the camera pushes in on the pads during that window and eases back to the exact sweep pose
	// before the intro ends (gameplay starts from the unchanged view). Off in auto / test runs; -ZoogiNoElevatorPush disables it.
	static constexpr float ElevPushWindow = 1.25f;   // the GameMode starts the elevators when 1.25 s of the intro remain
	static constexpr float ElevPushIn = 0.40f;     // ease in
	static constexpr float ElevPushOutEnd = 1.17f; // back at the exact sweep pose from here (the intro's last ~5 frames)
	static constexpr float ElevPushOut = 0.40f;    // ease back
	/** The stage-forming sweep pose at progress Pg (0..1), as the controller has always drawn it. */
	static void FormingSweepPose(float Pg, float Scale, float& OutYaw, float& OutPitch, float& OutDist, float& OutFov, FVector& OutPivot);
	/** Push-in weight at WinT seconds into the elevator window: 0 at 0, 1 while the lifts rise, exactly 0 from ElevPushOutEnd. */
	static float ElevatorPushWeight(float WinT);
	/** Closest boom (pivot, pitch, distance <= BaseDist) at this yaw / FOV that keeps every spawn pad (with its lift) in frame. */
	/** Aspect / Top / Bottom (portrait): the pads must fit the real view and the HUD-safe band (rotation with ZoogiPortrait::BandRotation). */
	static void ElevatorPushFit(const TArray<FVector>& PadFloors, float Yaw, float Fov, float BaseDist, FVector& OutPivot, float& OutPitch, float& OutDist,
		float Aspect = 16.f / 9.f, float Top = 0.f, float Bottom = 0.f);
	/** Portrait (ZoogiPortraitCam): boom for a landscape-tuned pose and the horizontal FOV to use (OutHFov). Plain views keep the
	 *  landscape width at the pivot (capped vertical FOV, boom pulled back); bCourt views (overviews) instead fit the whole court /
	 *  Frozen Ring 2 (PortraitCourtRadius around Centre) across the width and inside the HUD-safe band. Dist unchanged on landscape. */
	static float PortraitBoom(const AZoogiGameMode* G, float Aspect, float Top, float Bottom, float Yaw, float Pitch, float LandscapeFov, float Dist,
		bool bCourt, const FVector& Centre, float& OutHFov);
	/** Radius whole-court views must show: the play radius (per-arena knockout) or Frozen's outer Ring 2, plus a Zoogi. */
	static float PortraitCourtRadius(const AZoogiGameMode* G);
	/** Portrait AIM chase view (tablet reference): boom elevation / length from the shooter, look-point distance ahead of it along
	 *  the aim yaw, and the capped FOV; the pose for a shooter at Shooter aiming along Yaw (ShotTest and the live camera). */
	static void PortraitChaseBoom(const AZoogiGameMode* G, float Aspect, float LandscapeFov, float& OutElev, float& OutBoom, float& OutAhead, float& OutHFov);
	static void PortraitChasePose(const AZoogiGameMode* G, const APlayerController* PC, const FVector& Shooter, float Yaw, FVector& OutLoc, FRotator& OutRot, float& OutHFov);
	/** The tactical resolve / end overview (centre of the court at CourtZ, boom along Yaw) as the controller frames it on PC's
	 *  view: bLegacy = the pre-portrait rule (landscape-tuned FOV / boom, no safe band) for BEFORE frames. */
	static void PortraitOverviewPose(const AZoogiGameMode* G, const APlayerController* PC, float Yaw, float CourtZ, bool bLegacy, FVector& OutLoc, FRotator& OutRot, float& OutFov);
	/** Portrait ShotTest probe: the last gameplay-view look point, its landscape-tuned FOV and the boom multiplier PortraitBoom applied. */
	FVector PortDbgLegacyPivot = FVector::ZeroVector; float PortDbgFov = 90.f, PortDbgLegacyPitch = 55.f, PortDbgLegacyDist = 2300.f; bool PortDbgAim = false; FVector PortDbgLook2 = FVector::ZeroVector; float PortDbgWantDist = 0.f, PortDbgArm = 1.f;
	/** Portrait aim / rest framing boom: behind the shooter at Pitch looking at LookPivot (ahead of it, up the arena); never closer
	 *  than the landscape boom, ground width >= AimWidth x play radius, shooter inside the safe band. OutHFov = capped FOV. */
	static float PortraitAimDist(const AZoogiGameMode* G, float Aspect, float Top, float Bottom, const FVector& Shooter, const FVector& LookPivot,
		float Yaw, float Pitch, float LandscapeFov, float LandscapeDist, float& OutHFov);
	static FVector BoomLocation(const FVector& Pivot, float Yaw, float Pitch, float Dist);
	/** The arena's camera boom scale (Neon / CamScale), as UpdateCamera uses it. */
	static float ArenaCamScale(const AZoogiGameMode* G);
	/** Whether the live game uses the push-in: spawn elevators on and not an auto / test / forming-shots run (-ZoogiElevatorPush forces it). */
	static bool ElevatorPushAllowed(const AZoogiGameMode* G);
	FVector2D GetCursor() const;

protected:
	AZoogiGameMode* GM() const;
	void HandleInput(float Dt);
	void UpdateCamera(float Dt);
	void UpdateMarkers();
	AActor* PickTarget(float MX, float MY) const;
	void ApplyPostProcess(float Dt, int32 Profile, float FocusDist);
	FVector ProbeCamera(const FVector& From, const FVector& To, float Dt);
	void EndSlowMo();

	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TObjectPtr<AZoogiBall> CamTarget;
	UPROPERTY() TObjectPtr<AZoogiTargetMarker> HoverMarker;
	UPROPERTY() TObjectPtr<AZoogiTargetMarker> LockMarker;
	TWeakObjectPtr<AActor> HoverTarget;

	bool bMenuCam = true;
	// Camera state (all eased toward the active profile).
	float Yaw = 0.f, YawVel = 0.f;
	float Pitch = 38.f;
	float Dist = 3600.f;
	float Fov = 90.f;
	float Dutch = 0.f;
	FVector Pivot = FVector::ZeroVector;
	FVector Look = FVector::ZeroVector;
	float ArmScale = 1.f;          // collision probe (1 = full boom)
	float MenuOrbit = 0.f;
	bool bElevPushFit = false; FVector ElevPushPivot = FVector::ZeroVector; float ElevPushPitch = 0.f, ElevPushDist = 0.f; // spawn-elevator push-in (fit once per intro)
	float DrawBaseYaw = 0.f;
	float StandbyYaw = 0.f;
	float CourtZ = ZoogiConst::ZoogiRadius; // last on-court shooter height (camera pivots stay on the floor after a fall)
	float AnimeDistMul = 1.f;
	float TacYaw = 0.f;            // tactical camera heading (set per turn, follows the aim gently)
	float AimViewAge = 0.f, AimViewAhead = 0.f; // aim view (ZoogiAimView.h): time in the view, look point ahead of the Zoogi
public:
	/** Aim view live this frame (behind the acting Zoogi along its aim arrow; AimViewYaw = the aim heading it follows). */
	bool bAimViewActive = false;
	float AimViewYaw = 0.f;
	/** ShotTest hook: run the aim view outside the aim step (Awaken test stage); the rest of the game is untouched. */
	bool bAimViewForce = false;
protected:
	float AnimeYaw = 0.f;
	TWeakObjectPtr<AActor> AnimeFocus;
	bool bAnimeCam = false;
	bool FloorPoint(const FVector2D& Screen, FVector& Out) const;
	// Phase-to-phase blend: pose snapshot taken when the camera profile changes.
	struct FCamPose { float Yaw = 0.f, Pitch = 0.f, Dist = 0.f, Fov = 90.f, Dutch = 0.f; FVector Pivot = FVector::ZeroVector, Look = FVector::ZeroVector; };
	FCamPose BlendFrom;
	int32 CurProfile = -1;
	float ProfileAge = 10.f;
	float BlendDur = 0.5f;
	float FrameFix = 0.f;
	float ImpactAge = 10.f, ImpactStrength = 0.f;          // Battle framing guard tilt (deg)
	int32 SeenLockSerial = -1;
	float SwingAge = 10.f;         // time since the last lock / switch swing started
	bool bWhip = false;
	float LockPunchAge = 10.f;     // FOV punch-in + shake at the Battle lock moment
	float FireKickAge = 10.f;      // FOV punch-out when the shot fires
	double SlowMoUntil = 0.0;
	// Post-process state.
	float PPVignette = 0.f, PPDof = 0.f, PPSat = 1.f, PPContrast = 1.f;
	FLinearColor PPGain = FLinearColor::White;

	// -ZoogiHumanTest: scripted cursor / clicks / keys drive the same targeting code on P1's turns.
	bool bHumanTest = false;
	bool bPowerTest = false;
	FVector2D SimMouse = FVector2D::ZeroVector;
	bool bSimClick = false, bSimTab = false, bSimSpace = false;
	int32 SimKey = -1;
	int32 SimTurn = 0;
	int32 SimStep = 0;
	float SimClock = 0.f;
	void UpdateHumanTest(float Dt);

	// -ZoogiUiTest: P1 is human and plays through the UMG Awaken controls with synthesized Slate mouse events
	// (odd turns) and synthesized key events (even turns); every step is verified and logged as UITEST PASS / FAIL.
	bool bUiTest = false;
	void UpdateUiTest(float Dt);
	void UiCheck(bool bOk, const FString& What);
	uint64 LastRawLMBFrame = 0;
public:
	// Oct 14 (Evan 6:52 PM) slingshot drag aim: press on a target / the floor sets the aim, hold + drag BACK sets the
	// force, release launches. Mouse and touch. [ZoogiRoll.Aim] MaxDragDistancePx (1080p px for full pull),
	// MaxDragForce (pull fraction cap), MinDragPx (below = no launch, aim kept), CancelOnZoogiPx.
	bool bDragArmed = false;
	float DragFrac = 0.f;         // live pull fraction (0..MaxDragForce)
	FVector2D DragStart = FVector2D::ZeroVector, DragNow = FVector2D::ZeroVector;
	bool bDragWouldCancel = false;
	// slingshot screen frame (ball point + shot direction) frozen at press time, so an orbiting camera cannot shift the pull mid-drag
	FVector2D DragBallS0 = FVector2D::ZeroVector, DragFwd0 = FVector2D(0.f, -1.f);
	bool bDragFrame0 = false, bDragProj0 = false;
	/** Scripted drag for -ZoogiDragTest (no OS input): screen positions in viewport px. */
	void SimDrag(const FVector2D& From, const FVector2D& To, float Seconds);
protected:
	bool bPtrWasDown = false;
	bool bPendingTap = false;
	FVector2D TapPos = FVector2D::ZeroVector;
	TWeakObjectPtr<AActor> TapTarget;
	float MaxDragDistancePx = 260.f, MaxDragForce = 1.f, MinDragPx = 22.f, CancelOnZoogiPx = 46.f;
	bool bDragCfgLoaded = false;
	// scripted drag
	bool bSimDragOn = false; FVector2D SimDragFrom = FVector2D::ZeroVector, SimDragTo = FVector2D::ZeroVector; float SimDragT = 0.f, SimDragDur = 0.f; int32 SimDragPhase = 0;
	// Oct 14 (Evan 10:28 / 10:30 PM) aim ZOOM mode: a toggle (HUD ZOOM button / Z key, or wheel / two fingers enter it)
	// while the human aims. In the mode one finger / LMB / RMB / MMB slides pan, pinch / wheel zoom toward the fingers
	// / cursor, a tap locks the aim on the tapped orb / Zoogi / floor point and leaves the mode; drag-aim is paused.
	// The camera tilts lower as it zooms in, stays inside the knockout ring and softly keeps the shooter in frame.
	// [ZoogiRoll.AimZoom] holds every limit and angle.
	struct FAimZoomTune
	{
		bool bEnabled = true, bAutoExitOnTap = true;
		float MaxZoom = 2.1f, EnterZoom = 1.6f, WheelStep = 0.18f, KeyStep = 0.25f, PinchPower = 1.f, MinPinchPx = 30.f;
		float EaseIn = 7.f, EaseOut = 4.f, PanEase = 18.f;
		float PitchWide = 55.f, PitchClose = 40.f, PitchMin = 30.f;   // camera pitch (deg down) at 1x / MaxZoom; never below PitchMin
		float EdgeMargin = 120.f;                                      // pan stays this far inside the knockout radius (cm)
		float FollowRadius = 1500.f, FollowPad = 250.f, FollowLag = 2.5f; // shooter kept within FollowRadius / zoom + pad of the view centre
		float MinHeight = 900.f;                                       // zoomed camera height floor over the court (clears pillars)
	};
	FAimZoomTune ZoomTune; bool bZoomCfg = false;
	bool bZoomMode = false;
	bool bZoomArmed = false;                     // ZOOM pressed during the human's Awaken / standby step: on at the aim step
	float ZoomWant = 1.f, ZoomCur = 1.f;         // boom distance divisor (1 = normal view)
	FVector ZoomAnchor = FVector::ZeroVector;    // world floor point kept under the fingers / cursor while zooming
	FVector ZoomOff = FVector::ZeroVector, ZoomOffWant = FVector::ZeroVector; // view-centre shift from zoom + pan
	float ZoomTilt = 0.f;                        // eased pitch blend (0 = normal, 1 = close angle)
	bool bPinch = false, bPinchHold = false, bPanDown = false, bPanMoved = false, bPanTapOk = false;
	FVector2D PanStart = FVector2D::ZeroVector, PanLast = FVector2D::ZeroVector;
	float PinchD0 = 1.f, PinchZ0 = 1.f;
	int32 ZoomLogN = 0, PanLogN = 0;
	bool bPinchTest = false; float PinchTestT = -1.f; int32 PinchTestPhase = 0; int32 PinchTestRuns = 0; FVector PinchTestOff0 = FVector::ZeroVector;
	void LoadZoomTune();
	bool UpdateZoomInput(class AZoogiGameMode* G, bool bAimStage, const FVector2D& Mouse, float Dt); // true = zoom mode owns the pointers
	void UpdatePinchTest(class AZoogiGameMode* G, float Dt);
	// Oct 15 camera watchdog (Evan's stuck high cam): restores the battle follow view when it is left far off / on another view
	float CamBadT = 0.f, ViewBadT = 0.f; int32 CamWatchFixes = 0;
	// -ZoogiCamStuckTest: forces a far tap + a broken high camera / foreign view target / menu cam on P1's aim and checks the return
	bool bCamStuckTest = false; float CamStuckT = -1.f; int32 CamStuckPhase = 0; int32 CamStuckFixes0 = 0;
	TWeakObjectPtr<AActor> CamStuckBogus;
	void UpdateCamStuckTest(class AZoogiGameMode* G, float Dt);
	void SetZoomAnchor(const FVector2D& Screen);
	void ZoomAbout(float NewWant);
	void PanBy(const FVector2D& From, const FVector2D& To);
public:
	float GetAimZoom() const { return ZoomCur; }
	bool IsZoomMode() const { return bZoomMode; }
	/** Single source for the HUD ZOOM button: on while the mode is on (any entry path: button, wheel, keys, pinch) or
	 *  armed from the Awaken / standby step (turns on when the aim step starts). */
	bool IsZoomShownOn() const { return bZoomMode || bZoomArmed; }
	bool CanZoom() const;
	static bool ZoomStageLive(const class AZoogiGameMode* G);
	/** The ZOOM button is usable (shown): the human's Awaken / standby step (arms the mode) or the aim step (toggles it). */
	bool CanShowZoom() const;
	/** HUD ZOOM button / Z key: flips what the button shows. */
	void ToggleZoomMode(const TCHAR* Why);
	/** Explicit on / off (on outside the aim step = armed for it). */
	void SetZoomMode(bool bOn, const TCHAR* Why);
protected:
	uint64 UiSnapFrame = 0;
	int32 UiPass = 0, UiFail = 0;
	int32 ShotsSeen = 0, UiShotSnap = -1;
	uint64 UiConfirmFrame = 0;
	int32 UiSnapPick = 0, UiSnapPower = 0, UiWantPick = 0;
	uint64 UiPickClickFrame = 0;
	float UiSnapCut = 0.f;
	bool bUiControlDone = false;
	FName UiAct;

	// aim-arrow (Evan, Oct 17): grab the arrow to rotate the aim round the Zoogi; presses elsewhere keep the slingshot.
	ZoogiAimArrow::FGesture ArrowGesture;
	TWeakObjectPtr<AActor> ArrowTapTarget;
	bool bArrowCamHold = false, bArrowCamHasAim = false; // camera framing frozen at the grab (the camera never turns with the arrow)
	FVector ArrowCamAim = FVector::ZeroVector;
	/** Projected arrow outline + the shooter's screen disc; false when hidden / off screen. */
	bool ArrowScreen(FVector2D OutS[7], FVector2D& OutBallS, float& OutBallRpx) const;
	/** World floor direction from the shooter's centre to the finger (false when the finger is on the ball / above the horizon). */
	bool ArrowFingerDir(const FVector2D& Screen, FVector2D& OutDir) const;
	// -ZoogiArrowTest: real touch events on P1's aim steps: grab + rotate left / right, release, drag elsewhere = launch, zoom hides
	bool bArrowTest = false; float ArrowTestT = 0.f, ArrowAwakenT = 0.f, ArrowSeqT = 0.f, ArrowExitT = -1.f; int32 ArrowTestPhase = 0, ArrowPass = 0, ArrowFail = 0, ArrowSeqN = 0;
	float ArrowYaw0 = 0.f, ArrowHeadDist = 0.f, ArrowCamYaw0 = 0.f, ArrowWantPull = 0.f, ArrowWantYaw = 0.f; FVector ArrowCamPivot0 = FVector::ZeroVector;
	FVector2D ArrowTouch = FVector2D::ZeroVector, ArrowDragFrom = FVector2D::ZeroVector, ArrowDragFwd = FVector2D::ZeroVector;
	bool bArrowSeq = false;
	void UpdateArrowTest(class AZoogiGameMode* G, float Dt);
};
