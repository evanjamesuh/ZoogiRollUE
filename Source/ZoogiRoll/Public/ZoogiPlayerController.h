#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.generated.h"

class ACameraActor;
class AZoogiBall;
class AZoogiTargetMarker;
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
	/** True once a Zoogi has left play: marked out, past the knockoff ring (radial arenas) or below the floor it was on. */
	static bool IsLeavingPlay(const FVector& BallLoc, bool bOut, float CourtZ, bool bRadialArena = true);
	/** Tactical Rolling follow pivot: partway toward the shooter but never off the court (inside the ring, at court
	 *  height); the arena centre once the shooter is leaving play, so the falling Zoogi leaves the shot instead. */
	static FVector TacticalRollPivot(const FVector& ShooterLoc, bool bOut, float CourtZ, bool bRadialArena = true);
	/** Tactical boom (pitch, distance incl. the arena's camera scale, FOV, pivot lag) for G's arena; for the shot test. */
	static void TacticalBoom(const AZoogiGameMode* G, float& OutPitch, float& OutDist, float& OutFov, float& OutPosLag);
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
	float DrawBaseYaw = 0.f;
	float StandbyYaw = 0.f;
	float CourtZ = ZoogiConst::ZoogiRadius; // last on-court shooter height (camera pivots stay on the floor after a fall)
	float AnimeDistMul = 1.f;
	float TacYaw = 0.f;            // tactical camera heading (set per turn, follows the aim gently)
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
	uint64 UiSnapFrame = 0;
	int32 UiPass = 0, UiFail = 0;
	int32 ShotsSeen = 0, UiShotSnap = -1;
	uint64 UiConfirmFrame = 0;
	int32 UiSnapPick = 0, UiSnapPower = 0, UiWantPick = 0;
	uint64 UiPickClickFrame = 0;
	float UiSnapCut = 0.f;
	bool bUiControlDone = false;
	FName UiAct;
};
