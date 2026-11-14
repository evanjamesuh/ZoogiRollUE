#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiArena.h"
#include "ZoogiDevConfig.h"
#include "ZoogiDevMode.generated.h"

/**
 * Developer Mode (Oct 24 v1, Oct 27 v2 for the phone - Evan's Dev Mode spec, Phase 1).
 * A small DEV button (bottom right, every screen) asks for the dev PIN once per device, then opens the touch panel; the game
 * pauses while it is open. Objects: long-press (or tap) to select, drag to move, pinch to resize, two-finger twist to rotate,
 * plus -/+ buttons for exact steps, snapping and an alignment grid. UNDO / REDO / RESET / SAVE / LOAD / EXPORT all go through
 * the versioned store in ZoogiDevConfig (saved in the app's storage, so it works on iPhone and survives restarts).
 * Saved layouts apply automatically on a dev-unlocked device; EXPORT copies a JSON package to the clipboard for Claude, who
 * bakes it into ZoogiLayoutBaked.inc (C:\dev\bake-layouts.ps1) so every player gets it.
 * PC: -ZoogiDev (unlocked + open), F10, mouse = one finger, Q/E rotate, Z/X size, Del delete, Ctrl+S save, T top view.
 * Editable now: star bumpers + the launch ship (FZoogiDevProp, stable ids). Cameras / HUD / spawns / physics: later phases.
 */
namespace ZoogiLayout
{
	ZOOGIROLL_API bool IsDevMode();          // the dev panel is open
	ZOOGIROLL_API void SetDevMode(bool bOn);
	ZOOGIROLL_API FString PathFor(const FString& Slug);   // legacy Windows bake folder (C:/dev/zoogi-layouts)
	ZOOGIROLL_API bool Load(const FString& Slug, TArray<AZoogiArena::FZoogiDevProp>& Out);
	ZOOGIROLL_API bool Save(const AZoogiArena* Arena, FString& OutPath);
	/** Dev store key for an arena's prop layout (scope "level:<slug>"). */
	ZOOGIROLL_API const TCHAR* LayoutKey();
	ZOOGIROLL_API TSharedPtr<class FJsonValue> ToJson(const TArray<AZoogiArena::FZoogiDevProp>& Props);
	ZOOGIROLL_API bool FromJson(const TSharedPtr<class FJsonValue>& V, TArray<AZoogiArena::FZoogiDevProp>& Out);
}

UENUM()
enum class EZoogiDevPanel : uint8 { Closed, Pin, Open };

UCLASS()
class ZOOGIROLL_API AZoogiDevMode : public AActor
{
	GENERATED_BODY()
public:
	AZoogiDevMode();
	static AZoogiDevMode* Ensure(UWorld* World);
	/** b167: the pause menu DEV MODE row - open the panel (PIN pad when locked) or close it */
	static void ToggleFromMenu(UWorld* World);
	virtual void Tick(float Dt) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---- panel
	EZoogiDevPanel GetPanel() const { return Panel; }
	void OnDevButton();
	void OpenPanel();
	void ClosePanel();
	void PinDigit(int32 D);
	void PinClear();
	void PinCancel();
	FText PinText() const;
	bool IsEditing() const { return Panel == EZoogiDevPanel::Open; }
	void ToggleDev();
	void SetEditing(bool bOn) { if (bOn) OpenPanel(); else ClosePanel(); }

	// ---- touch / mouse (viewport pixels)
	void TouchDown(int32 Finger, const FVector2D& Px);
	void TouchMove(int32 Finger, const FVector2D& Px);
	void TouchUp(int32 Finger, const FVector2D& Px);

	// ---- object tools
	bool SelectAtPixel(const FVector2D& Px);
	void Nudge(float DX, float DY);
	void Rotate(float Deg);
	void ScaleBy(float F);
	void AddBumper();
	void Duplicate();
	void DeleteSelected();
	void ResetSelected();
	// Oct 29: BUMPER on / off + REBOUND level for the selected object
	void ToggleBumper();
	void StepBounce(int32 Dir);
	FText BumperText() const;
	FText BounceText() const;
	bool SelectedSolid() const;
	void ResetAll();
	void DoUndo();
	void DoRedo();
	void DoSave();
	void DoLoad();
	void DoExport();
	void DoLock();
	void ToggleTopView();
	// Oct 27 Frozen Ring: SPAWNS tab
	void TestSpawn(int32 Index);
	FText RoamText() const;
	// free-roam camera (Oct 27, Evan: "free roam while in dev mode so I can see things"): left-side drag moves, drag looks,
	// pinch flies forward / back, UP / DOWN hold buttons, speed steps; HIDE PANEL clears the screen.
	void ToggleFreeCam();
	bool IsFreeCam() const { return FlyCam.IsValid(); }
	void SetFlyVert(int32 V) { FlyVert = FMath::Clamp(V, -1, 1); }
	void CycleFlySpeed();
	FText FlySpeedText() const;
	bool StickActive() const { return StickFinger >= 0; }
	FVector2D StickOriginPx = FVector2D::ZeroVector, StickPx = FVector2D::ZeroVector;
	float StickRadiusPx() const;
	bool bPanelHidden = false;
	float FlySpeed = 1200.f;
	void PanTop(const FVector2D& D);
	void ZoomTop(float F);
	bool bSnap = true;
	bool bGrid = false;
	bool bFine = false;
	/** Panel tab: 0 OBJECTS, 1 CAMERA, 2 HUD, 3 SPAWNS. */
	int32 Tab = 0;

	// ---- CAMERA tab (ZoogiCamConfig): live, per game mode, profiles A / B / C
	FString CamMode() const;
	TCHAR CamProfile() const;
	void CamSetProfile(TCHAR P);
	void CamStep(int32 ParamIndex, float Dir);
	void CamReset();
	FText CamHeader() const;
	FText CamValueText(int32 ParamIndex) const;
	bool CamParamVisible(int32 ParamIndex) const;

	FText StatusText() const;
	FText SelectedText() const;
	bool IsTopView() const { return TopCam.IsValid(); }
	bool HasSelection() const;
	/** World -> viewport pixel (for the grid painter); false when behind the camera. */
	bool Project(const FVector& W, FVector2D& OutPx) const;
	FVector GridCenter() const;

	// legacy names kept for the old smoke script / keys
	void SelectAtCursor();
	bool bDragging = false;

private:
	class AZoogiArena* GetArena() const;
	class APlayerController* PC() const;
	bool CursorPixel(FVector2D& Out) const;
	bool PixelOnPlane(const FVector2D& Px, float Z, FVector& Out) const;
	void ShowOverlay(bool bShow);
	void UpdateMarker();
	void RunAuto(float Dt);
	void CommitLayout(const FString& Why);
	void ApplyFromStore();
	ZoogiDevConfig::FContext MakeCtx() const;
	FString Slug() const;
	float Step() const { return bFine ? 5.f : 25.f; }
	void SnapProp(AZoogiArena::FZoogiDevProp& P) const;

	EZoogiDevPanel Panel = EZoogiDevPanel::Closed;
	FString PinEntry;
	FString PinMsg;
	int32 Selected = INDEX_NONE;
	FVector GrabOffset = FVector::ZeroVector;
	FString LastMsg;
	TSharedPtr<class SWidget> Overlay;
	TWeakObjectPtr<class ACameraActor> TopCam;
	TWeakObjectPtr<class ACameraActor> FlyCam;
	int32 StickFinger = -1;
	double StickDownTime = 0.0;
	int32 FlyVert = 0;
	float FlyYaw = 0.f, FlyPitch = 0.f;
	void TickFreeCam();
	TWeakObjectPtr<AActor> PrevView;
	float TopHeight = 6500.f;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Marker;
	TArray<double> Taps;
	float AutoT = 0.f; int32 AutoStep = 0;
	double NextObsScan = 0.0;
	int32 ObsTestStep = 0; double ObsTestAt = 0.0;
	void RunObsTest();

	// gestures
	TMap<int32, FVector2D> Fingers;
	int32 PressFinger = -1;
	FVector2D PressPx = FVector2D::ZeroVector;
	double PressTime = 0.0;
	bool bPressMoved = false;
	bool bChanged = false;           // the current gesture moved / scaled / rotated something (commit on release)
	float PinchDist0 = 0.f, PinchAng0 = 0.f, PinchScale0 = 1.f, PinchYaw0 = 0.f;
	FVector2D PanLast = FVector2D::ZeroVector;
};
