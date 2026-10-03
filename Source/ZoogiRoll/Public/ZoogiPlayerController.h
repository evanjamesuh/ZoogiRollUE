#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZoogiPlayerController.generated.h"

class ACameraActor;
class AZoogiBall;
class AZoogiGameMode;

UENUM()
enum class EZoogiCamMode : uint8
{
	Menu,
	Aim,
	Follow
};

/**
 * Mouse slingshot input and the alpha camera rig (alpha-camera-spec.md):
 * aim pose low behind the Zoogi (~30 deg, ~7 D), eased rise to a high follow
 * (~55 deg, ~24 D) after the shot, hard cut + swoop into the next turn.
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

	bool IsDragging() const { return bDragging; }
	bool GetDragGroundPoint(FVector& Out) const { Out = DragCurrent; return bDragging; }

protected:
	AZoogiGameMode* GM() const;
	bool CursorGround(FVector& Out, float PlaneZ = 0.f) const;
	void HandleInput(float Dt);
	void UpdateCamera(float Dt);

	UPROPERTY() TObjectPtr<ACameraActor> Cam;
	UPROPERTY() TObjectPtr<AZoogiBall> CamTarget;

	EZoogiCamMode CamMode = EZoogiCamMode::Menu;
	float Yaw = 0.f;
	float Pitch = 30.f;
	float Dist = 700.f;
	FVector Pivot = FVector::ZeroVector;
	FVector Look = FVector::ZeroVector;
	bool bHoldPose = false;
	float MenuOrbit = 0.f;

	bool bDragging = false;
	FVector DragStart = FVector::ZeroVector;
	FVector DragCurrent = FVector::ZeroVector;
	bool bRotating = false;
	float LastMouseX = 0.f;

	// -ZoogiHumanTest: a scripted "virtual mouse" drives the exact same drag code on P1's turn.
	bool bHumanTest = false;
	bool bPowerTest = false;
	bool bSimDown = false;
	bool bSimPrevDown = false;
	FVector2D SimMouse = FVector2D::ZeroVector;
	float SimClock = 0.f;
	void UpdateHumanTest(float Dt);
	bool LmbPressed() const;
	bool LmbDown() const;
};
