#pragma once
// ZOOGI BLUE TRANSFORM MODEL (Evan Oct 26): the rigged Tripo model (/Game/ZoogiBlue/Mesh/SK_ZoogiBlue, 39 clips) replaces the
// static character mesh. It is the standard look: in every mode it is a plain marble (perfect-sphere shell, rolls with the
// physics sphere, never transforms); only Battle Royale lets it open into the robot (ToBot -> hover on foot thrusters,
// 8-way strafe clips, arm cannons) and back (ToBall).
//
// UZoogiMorphComponent rides on AZoogiBall: it owns a USkeletalMeshComponent, hides the old static Visual / Nubs (it mirrors
// Visual's visibility flag, so respawn blinks and ring-out flails keep working), places the model every frame after physics
// and picks the clip. AZoogiRoyaleBolt is the robot's plasma bolt (visual only: the game mode moves it and tests hits).
// -ZoogiNoMorph turns the model off (old static meshes everywhere).
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "ZoogiMorph.generated.h"

class AZoogiBall;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAnimSequence;
struct FZoogiDef;

/** Nov 1: the per-Zoogi Mecha Aurex colours (zones R / G / B + glow) by Zoogi name or roster id (also used by the front-end stage). */
ZOOGIROLL_API void ZoogiAurexPalette(const FString& Name, int32 Player, FLinearColor Z[4]);

UCLASS()
class ZOOGIROLL_API UZoogiMorphComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZoogiMorphComponent();

	/** Zoogi Blue (every mode); Battle Royale puts the model on every Zoogi. False with -ZoogiNoMorph or if the assets are missing. */
	static bool WantsModel(const FZoogiDef& Def);
	static bool ModelAvailable();

	/** Builds the skeletal mesh on the ball (ball form, Ball_Idle). False if the mesh could not load. */
	bool Setup(AZoogiBall* InBall);
	bool IsReady() const { return Mesh != nullptr && Ball != nullptr; }

	/** Battle Royale: open into the robot (true) or close into the ball. bInstant skips ToBot / ToBall (respawn). */
	void SetRobot(bool bOn, bool bInstant = false);
	bool IsRobot() const { return bRobot; }
	/** True while ToBot / ToBall plays. */
	bool IsMorphing() const { return State == EState::ToBot || State == EState::ToBall; }
	/** Robot facing (world yaw, degrees): the whole body turns to it (cannons forward). */
	void SetFacingYaw(float Yaw) { FacingYaw = Yaw; }
	float GetFacingYaw() const { return FacingYaw; }
	/** One cannon shot: plays Bot_Fire / Bot_ThrustFire_<dir> (once per pair of shots) and returns the muzzle (world). */
	FVector Fire(bool bLeft);
	FVector GetMuzzle(bool bLeft) const;
	/** 0..7 (F, FR, R, BR, B, BL, L, FL) while hovering, -1 when not thrusting. */
	int32 GetThrustDir() const { return State == EState::Thrust ? ThrustDir : -1; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Mesh;

protected:
	enum class EState : uint8 { Ball, ToBot, BotIdle, ThrustStart, Thrust, ThrustStop, ToBall };
	void Play(FName Clip, bool bLoop);
	UAnimSequence* Clip(FName Name) const;
	float ClipLength(FName Name) const;
	void Place();
	FName DirClip(const TCHAR* Prefix, int32 Dir) const;

	UPROPERTY() TObjectPtr<AZoogiBall> Ball;
	UPROPERTY() TMap<FName, TObjectPtr<UAnimSequence>> Anims;
	EState State = EState::Ball;
	bool bRobot = false;
	bool bAurex = false;       // Nov 1: Fab Mecha Aurex body (rifle clips) instead of SK_ZoogiBlue; ball form = the ball skin on the Visual
	float StateTime = 0.f;
	float StateLen = 0.f;
	FName Current;
	float FacingYaw = 0.f;
	int32 ThrustDir = 0;
	float FireHold = 0.f;      // > 0 while a fire clip plays over the idle / hover loop
	float UprightBlend = 0.f;  // 0 = rolling with the sphere, 1 = upright robot frame
	FQuat Calib = FQuat::Identity; // model forward (cannons) -> +X
	FQuat BallOffset = FQuat::Identity; // ball form: model rotation in the Visual's frame
};

/** Robot cannon round: a thin tracer streak (Oct 27, was a glowing capsule); AZoogiGameMode::RoyaleTickBolts moves it and tests hits. */
UCLASS()
class ZOOGIROLL_API AZoogiRoyaleBolt : public AActor
{
	GENERATED_BODY()

public:
	AZoogiRoyaleBolt();
	void Init(const FLinearColor& Color);
	/** Oct 27 tracer: a thin streak LengthCm long whose front tip sits at Head, pointing along Dir. */
	void SetStreak(float LengthCm, const FVector& Head, const FVector& Dir);
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
};
