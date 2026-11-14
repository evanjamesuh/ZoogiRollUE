// Nov 1 b58 (Evan): a posed "form" puppet - the Mecha Aurex (or a clash ultimate form) posed in code, because the packs ship no
// hang / climb / swim clips. Used for:
//   * the parry / BLOCK slow cut: the Zoogi visibly transforms into its Aurex and holds a guard (ZoogiActionFun UpdateReact)
//   * the edge hang mini-game: hanging off the court with both hands, struggling up (ZoogiEdgeHang)
//   * the clash loser's backstroke: flies backwards on its back, arms windmilling (ZoogiStrikeFun StartFlung / TickFlung)
// Poses are aim-based (each limb bone is turned so it points along a wanted direction), so they work on any humanoid rig whose
// bones the name table knows (UE4 mannequin: Aurex; Mixamo names: the Tripo ultimate forms). Runs in REAL time (ignores slow-mo).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiAurexPuppet.generated.h"

class UPoseableMeshComponent;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class AZoogiBall;
class UPrimitiveComponent;

UENUM()
enum class EZoogiPuppetPose : uint8 { Stand, Guard, Hang, Climb, Drop, Cheer, Backstroke, Splat, Peek }; // b142 Peek: the Cosmos boss over the rim // b67 Splat: star-limbed flat on a wall (placed by the caller)

UCLASS(NotBlueprintable)
class ZOOGIROLL_API AZoogiAurexPuppet : public AActor
{
	GENERATED_BODY()
public:
	AZoogiAurexPuppet();
	/** Spawns the Zoogi's Aurex (palette per Zoogi) at Feet facing Yaw, hides the ball's visuals (restored by Dismiss) and pops in.
	 *  From = optional skeletal component to copy (mesh + materials) instead of the Aurex (clash ultimate forms). Height in cm. */
	static AZoogiAurexPuppet* SpawnFor(UWorld* W, AZoogiBall* B, const FVector& Feet, float Yaw, EZoogiPuppetPose Pose, float Height = 178.f, USkeletalMeshComponent* From = nullptr, bool bPop = true);
	/** b142: a puppet with no Zoogi behind it (the Cosmos boss) - mesh as given, the caller sets the materials. */
	static AZoogiAurexPuppet* SpawnBoss(UWorld* W, USkeletalMesh* SM, const FVector& Feet, float Yaw, float Height);
	/** Pops out (scale down + flash when bFx) then destroys itself; the ball's visuals come back. */
	void Dismiss(bool bFx);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	EZoogiPuppetPose Pose = EZoogiPuppetPose::Stand;
	float Pull = 0.f;              // hang strain 0..1 (arms bend, body rises)
	bool bPinHands = false;        // hang / climb: both hands stay on HandPin (world)
	FVector HandPin = FVector::ZeroVector;
	bool bFollowBall = false;      // guard: feet stay under the ball
	FVector FaceDir = FVector::ForwardVector; // world facing (Stand / Guard / Hang / Climb / Cheer)
	FVector Travel = FVector::ForwardVector;  // Backstroke: the head leads along this
	FVector Center = FVector::ZeroVector;     // Backstroke: body centre (world)
	FLinearColor Tint = FLinearColor::White;
	bool bLookAt = false;          // b142: the head turns to LookAt (world), up to 60 deg
	FVector LookAt = FVector::ZeroVector;
	float Lean = 0.f;              // b142 Peek: extra lean over the edge
	TWeakObjectPtr<AZoogiBall> Ball;
	bool IsReady() const { return bReady; }
	float Height = 178.f;
	FVector HandsOffset() const { return HandMidCS; }

	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Body;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
private:
	bool Setup(USkeletalMesh* SM);
	void Solve(float T, float Dt);
	int32 Find(std::initializer_list<const TCHAR*> Names) const;
	enum EBone { Spine3, Neck, Head, UArmL, LArmL, HandL, UArmR, LArmR, HandR, ThighL, CalfL, FootL, ThighR, CalfR, FootR, NumBones };
	int32 Idx[NumBones];
	TArray<FTransform> RefLocal;
	TArray<int32> Parent;
	TArray<FName> Names;
	TArray<FVector> CurDir; // smoothed wanted direction per EBone (mesh space)
	FVector F = FVector(0, 1, 0), Lat = FVector(1, 0, 0); // mesh space: forward, character's left
	float Scale = 1.f, BottomZ = 0.f, Age = 0.f, OutAge = -1.f, PopIn = 0.3f;
	bool bReady = false, bFirst = true;
	FVector HandMidCS = FVector::ZeroVector;
	FQuat BindHeadCS = FQuat::Identity, HeadTurn = FQuat::Identity; // b142 look-at
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden;
	TArray<TWeakObjectPtr<AActor>> Fx; // pop flashes: kept at real speed in the slow-mo (they lingered as a big bubble)
};
