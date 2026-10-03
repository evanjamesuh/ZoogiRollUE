#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZoogiGameMode.h"
#include "ZoogiPlayerController.generated.h"

class ACameraActor;
class AZoogiBall;
class AZoogiTargetMarker;

/**
 * Lock-on targeting input (click / Tab / Space / RMB / Esc) and the per-stage camera director:
 * every turn stage (Draw, Standby, Main, Battle, End) has its own camera profile (ZoogiCamTune in the .cpp),
 * blended with spring-arm lag and a collision probe.
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

	AActor* GetHoverTarget() const { return HoverTarget.Get(); }
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
	EZoogiStage SimStage = EZoogiStage::End;
	int32 SimStep = 0;
	float SimClock = 0.f;
	void UpdateHumanTest(float Dt);
};
