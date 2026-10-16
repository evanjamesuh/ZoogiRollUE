#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTitanShell.generated.h"

class AZoogiBall;
class UProceduralMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UPrimitiveComponent;
class UMaterialInterface;

/** Titan Awaken look (Titan card art): a shell of chunky grey stone slabs that slam onto the Zoogi's sphere, glowing
 *  orange-hot seams between them, embers / pebbles / dust while it rolls, a stone-crack burst on hits and a shatter when
 *  Titan ends. The Zoogi stays a limbless sphere: the "golem" is armour only.
 *  Owned by UZoogiArtifactComponent (Infused == Titan or the "titan" card armed for that player). Slabs are attached to the
 *  ball's Visual mesh, so they roll with it and keep the face window (model +X) open.
 *  Assets: /Game/VFX/TitanFX (Scripts/titan/titan_fx.py) + KriptoFX Effect17 (golem rock texture, collision burst).
 *  Purely visual: never touches arena destruction. Disabled with -ZoogiNoKFX. */
UCLASS()
class ZOOGIROLL_API AZoogiTitanShell : public AActor
{
	GENERATED_BODY()
public:
	AZoogiTitanShell();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	static bool Enabled();
	static AZoogiTitanShell* Activate(AZoogiBall* Ball);
	/** A powered hit by this Zoogi: crack flare + chip burst. */
	void Impact(float Strength);
	/** Titan ended: slabs crack off, fall and fade, then the actor destroys itself. */
	void Shatter();

protected:
	enum class EState : uint8 { Forming, Active, Shattering };
	EState State = EState::Forming;
	TWeakObjectPtr<AZoogiBall> Ball;
	FRandomStream Rng;
	float Age = 0.f, StateAge = 0.f;
	float Impulse = 0.f;      // seam flare from hits
	float Crack = 0.f;        // crack-texture flare
	float LastImpact = -10.f;
	float NextEmber = 0.3f, NextPebble = 1.f, NextDust = 0.f;
	bool bThumped = false, bSlammed = false, bShotRolling = false;
	bool bTest = false;

	UPROPERTY() TObjectPtr<USceneComponent> Pivot;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Stone;

	struct FSlab
	{
		TObjectPtr<UProceduralMeshComponent> Mesh;
		FVector Dir = FVector::ForwardVector;   // pivot-local
		FVector Home = FVector::ZeroVector;     // pivot-local centre
		FVector From = FVector::ZeroVector;     // pivot-local start
		FRotator Spin = FRotator::ZeroRotator;  // start tumble
		float Delay = 0.f;
		FVector Vel = FVector::ZeroVector;      // shatter (world)
		FVector AngVel = FVector::ZeroVector;   // deg/s
		float Size = 1.f;
	};
	TArray<FSlab> Slabs;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> SlabMeshes;

	struct FChip { TObjectPtr<UProceduralMeshComponent> Mesh; FVector Vel; FVector AngVel; float Age = 0.f; float Life = 1.2f; float Size = 1.f; };
	TArray<FChip> Chips;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> ChipMeshes;

	void Build();
	void BuildSlabMesh(UProceduralMeshComponent* M, const FVector& Dir, float Half, float Aspect, float Twist, float Radius, bool bChip);
	void SpawnChip(const FVector& WorldPos, const FVector& Vel, float Size);
	void TickChips(float Dt);
	void Dust(const FVector& P, float Scale);
	void Embers(const FVector& P, float Scale);
	float Ground() const;
	FVector Center() const;
	void Shot(const TCHAR* Name, float Delay);
	// -ZoogiTitanTest review captures: close scene-capture PNGs (titancap_<name>.png in -ZoogiShots dir)
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Cap;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> CapRT;
	TArray<TPair<float, FString>> CapQueue; // absolute Age -> name
	FVector CapFrom = FVector::ZeroVector;
	int32 AimCaps = 0, RollCaps = 0;
	static int32 CapSerial;
	void Capture(const FString& Name, float Delay);
	void TickCaptures();
	void CaptureView(const FVector& Eye, const FVector& Look, float Fov, const FString& Name);
	int32 FrontCaps = 0, FacingCaps = 0;

	// While the shell is on, the Zoogi's own extras (artifact glow shell / guard bubble / orbiters / plates, nubs, auras, visor
	// glass and other translucent model slots) are hidden so every Zoogi reads like the card; restored on shatter.
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Suppressed;
	TArray<TPair<int32, TWeakObjectPtr<UMaterialInterface>>> HiddenSlots;
	UPROPERTY() TObjectPtr<UMaterialInterface> HiddenMat;
	FLinearColor TurnColor = FLinearColor::White;
	bool bSuppressed = false;
	void Suppress(bool bOn);
};
