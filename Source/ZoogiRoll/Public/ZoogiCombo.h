#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	// Layout (cm, floor top at Z = 0).
	static constexpr float WallR = 1150.f, WallA0 = 195.f, WallA1 = 245.f, WallH = 85.f;
	static FVector2D RampMin() { return FVector2D(-760.f, 290.f); }
	static FVector2D RampMax() { return FVector2D(-540.f, 510.f); }
	static constexpr float RampH = 45.f;
	static FVector2D SectionMin() { return FVector2D(-150.f, 200.f); }
	static FVector2D SectionMax() { return FVector2D(250.f, 600.f); }
	static FVector2D PlatformXY() { return FVector2D(750.f, 900.f); }
	static constexpr float PlatformR = 200.f, PlatformH = 55.f;
	static FVector2D GateXY() { return FVector2D(0.f, -1450.f); }
	static FVector GateReturn() { return FVector(0.f, -1180.f, 52.f); }
	static const TArray<FVector2D>& BumperXY();

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
