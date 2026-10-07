#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiCombo.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;
class UMaterialInterface;
class UPointLightComponent;
class AZoogiBall;

/** Damageable floor section states (Part 4 "Reactive Terrain"). */
UENUM(BlueprintType)
enum class EZoogiFloorState : uint8
{
	Normal,
	Damaged,  // tilted slab: slope + raised edge, cracks
	Cracked   // split: the far half is a steep raised lip (launch / bank face)
};

/**
 * 2v2 Combo prototype props, added on top of the Meadow arena (the arena that needed the least rework):
 * curved ricochet wall, launch ramp, raised platform, bumpers, Recovery Gate, one damageable floor
 * section (NORMAL -> DAMAGED -> CRACKED) and one Resonance Shift (the bumpers swell and kick harder).
 * Collision pieces are tagged (ZWall, ZRamp, ZPlatform, ZBumper, ZFloor) so the game mode can read hits.
 */
UCLASS()
class ZOOGIROLL_API AZoogiComboArena : public AActor
{
	GENERATED_BODY()

public:
	AZoogiComboArena();
	void Build();

	// Layout (cm, floor top at Z = 0). Per theme (SetLayout): Meadow prototype layout or the Frozen Ring layout
	// (ZoogiFrozenLayout.h, Oct 6) where the wall / ramp / platform / gate arch are arena stage pieces.
	static float WallR;
	static float WallA0;
	static float WallA1;
	static float WallH;
	static FVector2D RampMin() { return LRampMin; }
	static FVector2D RampMax() { return LRampMax; }
	static float RampH;
	static FVector2D SectionMin() { return LSecMin; }
	static FVector2D SectionMax() { return LSecMax; }
	static FVector2D PlatformXY() { return LPlat; }
	static float PlatformR;
	static float PlatformH;
	static FVector2D GateXY() { return LGate; }
	static FVector GateReturn() { return LGateReturn; }
	static const TArray<FVector2D>& BumperXY();
	/** Selects the layout for a theme (Frozen -> Frozen Ring layout, anything else -> Meadow prototype layout). */
	static void SetLayout(EZoogiArena Theme);
	static bool IsFrozenLayout() { return bFrozenLayout; }
	/** Arena the props sit on (Frozen: stage-forming registration, scars, materials). Call before Build(). */
	void SetArena(class AZoogiArena* InArena) { Arena = InArena; }
	/** Orb home markers (Frozen: rune sockets where the Arena Orbs spawn / return). */
	void SetOrbHomes(const TArray<FVector2D>& Homes) { OrbHomes = Homes; }
	virtual void Destroyed() override;
	/** Tests / shots: forces the reactive section state. */
	void ForceFloorState(EZoogiFloorState S) { FloorState = S; ApplyFloorState(); }
	const TArray<TObjectPtr<class AZoogiArenaPiece>>& GetIceBumpers() const { return IceBumpers; }

	bool IsInSection(const FVector2D& P, float Pad = 0.f) const;
	/** Heavy impact on the damageable section: NORMAL -> DAMAGED -> CRACKED (rate limited). Returns true if the state changed. */
	bool FloorImpact(const FVector& Where, const FVector& TravelDir, float Strength);
	EZoogiFloorState GetFloorState() const { return FloorState; }
	void ResonanceShift();
	bool IsShifted() const { return bShifted; }
	float BumperKickSpeed() const { return bShifted ? 800.f : 450.f; }
	float BumperRadius() const { return bShifted ? 105.f : 70.f; }
	/** Recovery Gate flash (a Zoogi is returned). */
	void GateFlash();
	virtual void Tick(float DeltaSeconds) override;
	static FString FloorStateName(EZoogiFloorState S);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

protected:
	UStaticMeshComponent* AddPiece(const TCHAR* Shape, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM);
	UStaticMeshComponent* AddBoxAt(const FVector& Center, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM);
	UStaticMeshComponent* AddCyl(const FVector2D& XY, float Radius, float Bottom, float Top, UMaterialInterface* Mat, bool bCollide, FName Tag, UPhysicalMaterial* PM);
	/** Places a slab whose top face runs from (X0, Z0) to (X1, Z1) along +X (Y span Y0..Y1), Thickness below the top face. */
	void PlaceSlab(UStaticMeshComponent* C, float X0, float Z0, float X1, float Z1, float Y0, float Y1, float Thickness);
	void ApplyFloorState();
	void BuildFrozen();
	static FVector2D LRampMin;
	static FVector2D LRampMax;
	static FVector2D LSecMin;
	static FVector2D LSecMax;
	static FVector2D LPlat;
	static FVector2D LGate;
	static FVector LGateReturn;
	static bool bFrozenLayout;
	UPROPERTY() TObjectPtr<class AZoogiArena> Arena;
	UPROPERTY() TArray<TObjectPtr<class AZoogiArenaPiece>> IceBumpers;
	TArray<FVector2D> OrbHomes;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Bumpers;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BumperCaps;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SlabA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> SlabB;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cracks;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> GatePad;
	UPROPERTY() TObjectPtr<UPointLightComponent> GateLight;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> WallPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> BumperPM;
	UPROPERTY() TObjectPtr<UPhysicalMaterial> StonePM;
	UPROPERTY() TObjectPtr<UMaterialInterface> SlabMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> SlabDamagedMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> SlabCrackedMat;
	EZoogiFloorState FloorState = EZoogiFloorState::Normal;
	float LastImpact = -10.f;
	float LipDir = 1.f; // +1: the raised edge is on the +X side
	bool bShifted = false;
	float ShiftAnim = 0.f;
	float GatePulse = 0.f;
	float Clock = 0.f;
};
