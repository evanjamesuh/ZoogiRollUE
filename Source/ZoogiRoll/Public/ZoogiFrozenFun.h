// Oct 29 evening (Evan, build 20): Frozen Ring fun pack + arena show.
//  * AZoogiIglooWall   - the Ring 1 ricochet walls rebuilt as two courses of igloo snow blocks. A Zoogi contact (or a fast orb)
//                        blows the top course off with real physics; the next contact blows the bottom course and opens the gap.
//  * AZoogiSnowman     - a big snowman on the raised hex platform: every hit knocks a limb off (left arm, right arm, head + hat),
//                        the last one explodes him away in a snow burst + fireworks.
//  * AZoogiFrozenCrowd - every old-style Zoogi (Content/Zoogis: bolt, hotstreak, lars, nightshade, pinpoint, wolfgang, wraps)
//                        filling the six bleachers, live animated (breathing, swaying, watching the ball) and reacting: hops and
//                        spins on scores / combos, gasps on ring-outs and big hits, a stadium wave now and then.
//  * UZoogiShowSys     - every map: fireworks (rockets with trails and coloured bursts) on scores, KOs, combos, the snowman,
//                        match start and the win; event hub for the crowd and the action cameras; Easy Impact Frames
//                        (Vefects) whenever a focus-line cut-in plays.
// Toggles: -ZoogiNoIgloo, -ZoogiNoSnowman, -ZoogiNoCrowd, -ZoogiNoFireworks, -ZoogiNoImpactFrames.
#pragma once

#include "CoreMinimal.h"
#include "ZoogiArenaPiece.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiFrozenFun.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UPhysicalMaterial;
class AZoogiOrb;
class AZoogiGameMode;

namespace ZoogiShow
{
	enum class EEvent : uint8 { Score, BigScore, RingOut, HardHit, Blast, Snowman, MatchStart, Win };
	/** Something worth a reaction happened (crowd + fireworks pick it up next tick). */
	ZOOGIROLL_API void Event(UWorld* World, EEvent E, const FVector& At, float Strength = 1.f);
	/** First contact of a shot (camera director): where, against what. */
	ZOOGIROLL_API void NoteImpact(UWorld* World, const FVector& At, AActor* Other, float Strength);
	ZOOGIROLL_API int32 ImpactSerial();
	ZOOGIROLL_API FVector ImpactAt();
	ZOOGIROLL_API AActor* ImpactOther();
	/** Vefects Easy Impact Frames at a focus point (Variant 1..10 = NS_Impact_Frame_Advanced_NN_Always). */
	ZOOGIROLL_API void ImpactFrame(UWorld* World, const FVector& At, int32 Variant);
	ZOOGIROLL_API bool On(const TCHAR* NoSwitch);
}

/** Two-course igloo block wall segment (replaces one Frozen ricochet wall segment). */
UCLASS()
class ZOOGIROLL_API AZoogiIglooWall : public AZoogiArenaPiece
{
	GENERATED_BODY()
public:
	AZoogiIglooWall();
	/** Arc in piece-local space (the piece sits at the origin, yaw 0): inner radius R0, thickness T, yaw A0..A1 (deg). */
	void BuildIgloo(float R0, float T, float A0, float A1, float CourseH, UMaterialInterface* BlockA, UMaterialInterface* BlockB, UMaterialInterface* Mortar, UPhysicalMaterial* PM);
	virtual bool ApplyDamage(float Amount, const FVector& At, const FVector& Dir, EZoogiArtifact Element, AZoogiBall* By = nullptr) override;
	virtual void ResetPiece() override;
	virtual void Tick(float Dt) override;
	/** Fast orbs knock blocks off too. */
	void OnOrbHit(AZoogiOrb* Orb, const FVector& At, const FVector& Normal, float Speed);
	int32 CoursesLeft() const { return Courses; }
private:
	struct FBrick { TWeakObjectPtr<UStaticMeshComponent> C; FTransform Base; int32 Course = 0; float FlyAge = -1.f; };
	void BlastCourse(int32 Course, const FVector& At, const FVector& Dir, float Power);
	TArray<FBrick> Bricks;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ColLow;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ColHigh;
	int32 Courses = 2;
	float LastBlast = -10.f;
	float Clock2 = 0.f;
};

/** The raised-platform snowman: limb by limb, then boom. */
UCLASS()
class ZOOGIROLL_API AZoogiSnowman : public AZoogiArenaPiece
{
	GENERATED_BODY()
public:
	AZoogiSnowman();
	void BuildSnowman(float Height, UMaterialInterface* SnowMat);
	virtual bool ApplyDamage(float Amount, const FVector& At, const FVector& Dir, EZoogiArtifact Element, AZoogiBall* By = nullptr) override;
	virtual void ResetPiece() override;
	virtual void Tick(float Dt) override;
	int32 GetStage() const { return Stage; }
	/** Tests: force the next stage. */
	void KnockNext(const FVector& Dir);
private:
	struct FFly { TWeakObjectPtr<UStaticMeshComponent> C; FTransform Base; TWeakObjectPtr<USceneComponent> Parent; float Age = -1.f; };
	UStaticMeshComponent* Part(const TCHAR* Shape, USceneComponent* Parent, const FVector& Loc, const FRotator& Rot, const FVector& Scale, UMaterialInterface* M);
	void Launch(UStaticMeshComponent* C, const FVector& Dir, float Speed, float Up);
	UPROPERTY() TObjectPtr<USceneComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Bottom;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Middle;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ArmL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ArmR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HatTop;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Col;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Chunks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyBits; // buttons / scarf (hidden on the explosion)
	struct FRest { TWeakObjectPtr<UStaticMeshComponent> C; FTransform Rel; TWeakObjectPtr<USceneComponent> Parent; };
	TArray<FFly> Flying;
	TArray<FRest> Rest; // every part's rest pose (reset)
	int32 Stage = 0;     // 0 whole, 1 no left arm, 2 no arms, 3 no head, 4 exploded
	float LastKnock = -10.f, Wobble = 0.f, T = 0.f;
	float H = 330.f;
	FTransform ColRel;
	bool bColRel = false;
	FVector WobbleDir = FVector::ForwardVector;
	UPROPERTY() TObjectPtr<UMaterialInterface> SnowM;
};

/** Old-style Zoogi crowd on the Frozen bleachers (instanced, animated every frame). */
UCLASS()
class ZOOGIROLL_API AZoogiFrozenCrowd : public AZoogiArenaPiece
{
	GENERATED_BODY()
public:
	AZoogiFrozenCrowd();
	/** Seats: world positions + the yaw that faces the court; Islet index per seat. */
	void BuildCrowd(const TArray<FVector>& Seats, const TArray<int32>& Islet);
	virtual void Tick(float Dt) override;
	virtual void ResetPiece() override {}
	int32 Num() const { return Members.Num(); }
	/** b268: a fan's seat (world) - the spectacle shoots confetti out of the stands. */
	bool SeatPos(int32 I, FVector& Out) const { if (!Members.IsValidIndex(I)) return false; Out = Members[I].Base; return true; }
	/** Reaction drivers (0..1, decay): set by UZoogiShowSys. */
	float Cheer = 0.f, Gasp = 0.f, Hype = 0.f, WaveA = -1000.f;
	FVector Look = FVector::ZeroVector;
private:
	struct FMember { int32 Mesh = 0, Inst = 0, Islet = 0; FVector Base; float Phase = 0.f, Energy = 1.f, Scale = 1.f, Ang = 0.f; FVector Off; bool bSpinner = false; TWeakObjectPtr<AZoogiArenaPiece> Stand; float Fall = -1.f;
		int32 Col = 0, Sign = -1, BodyI = 0, EyeI = 0, PupI = 0, StickI = -1, BoardI = -1; }; // b228: the toon fan parts
	/** b228 (Evan picked crowd option B): toon Zoogi fans (team-colour spheres with ears, eyes, hands), ~30% hold up a sign */
	bool BuildToon(const TArray<FVector>& Seats, const TArray<int32>& Islet);
	void TickToon(float Dt);
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ToonISMs;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SignComps; // b229: signs are plain meshes (the sign material has no ISM usage)
	TArray<TArray<FTransform>> ToonXf;
	bool bToon = false; int32 NCol = 0, NSign = 0;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ISMs;
	TArray<FMember> Members;
	TArray<TArray<FTransform>> Xf;
	TArray<FVector> MeshOff;
	TArray<float> MeshScale;
	float Clock3 = 0.f;
};

/** Drives UZoogiShowSys every frame (an actor tick is the most reliable clock in every world / mode). */
UCLASS()
class ZOOGIROLL_API AZoogiShowTicker : public AActor
{
	GENERATED_BODY()
public:
	AZoogiShowTicker();
	virtual void Tick(float Dt) override;
};

UCLASS()
class ZOOGIROLL_API UZoogiShowSys : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	void ShowTick(float Dt);
	void EnsureTicker();
	void Push(ZoogiShow::EEvent E, const FVector& At, float Strength);
	void Fireworks(int32 Count, float Spread = 1.f, float Delay = 0.f);
	TWeakObjectPtr<AZoogiFrozenCrowd> Crowd;
private:
	struct FRocket { TWeakObjectPtr<UStaticMeshComponent> C; FVector P, V; float Age = 0.f, Fuse = 1.f, Trail = 0.f; FLinearColor Col; int32 Burst = 0; float Wait = 0.f; };
	struct FPending { float Wait = 0.f; float Spread = 1.f; };
	void Launch(float Spread);
	void Poll(AZoogiGameMode* G, float Dt);
	UPROPERTY() TObjectPtr<AActor> Holder;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pool;
	TArray<FRocket> Rockets;
	struct FSpark { TWeakObjectPtr<UStaticMeshComponent> C; FVector P, V; float Age = 0.f, Life = 1.2f; float Size = 1.f; };
	TArray<FSpark> Sparks;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SparkPool;
	void Burst(const FVector& P, const FLinearColor& Col, int32 N, float Speed);
public:
	/** Build 29: drop every live spark (power clash / cut-in glare control). */
	void ClearSparks() { for (FSpark& S : Sparks) if (S.C.IsValid()) S.C->SetVisibility(false); Sparks.Reset(); }
	bool bQuiet = false; // build 29: no new spark bursts while a clash / cut-in shows the ultimate forms
private:
	TArray<FPending> Pending;
	void TickTest(AZoogiGameMode* G, float Dt);
	int32 SeenOrbsOut = -1, SeenBallsOut = -1, SeenStreak = 0, SeenPhase = -1;
	float TestT = -1.f;
	int32 TestStep = 0;
	TWeakObjectPtr<AActor> TestCam, PrevView;
	float Ambient = 6.f, CrowdSfx = 0.f, Clock = 0.f;
	bool bWinShown = false;
};
