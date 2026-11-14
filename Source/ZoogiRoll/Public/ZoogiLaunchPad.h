#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTypes.h"
#include "ZoogiLaunchPad.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Oct 22 launch pads: one per map, shared geometry rules (same height, edge gap and flight on every map).
 * World units (cm); the pad is its own unscaled actor, so the numbers hold on the 1.3x maps too.
 * Evan Oct 22 6:57 PM CT: SharedPlatform GLB (/Game/Env/LaunchPad/SM_SharedPlatform) is the hover pad
 * OUTSIDE the arena (fully unobstructed aim) — not the arena floor. Resized to DeckRadius per map.
 */
namespace ZoogiLaunch
{
	inline constexpr float PadHeight = 500.f;      // deck top above the court floor (Z 0)
	inline constexpr float EdgeGap = 700.f;        // pad centre beyond the knockoff edge along its yaw
	inline constexpr float FlightDist = 1800.f;    // horizontal launch point -> landing point
	inline constexpr float LaunchAngleDeg = 30.f;  // above horizontal
	inline constexpr float DeckRadius = 170.f;
	inline constexpr float MaxGlow = 0.8f;         // pad emissives stay at or under this
}

UCLASS()
class ZOOGIROLL_API AZoogiLaunchPad : public AActor
{
	GENERATED_BODY()

public:
	AZoogiLaunchPad();

	/** Builds the themed dressing; actor X axis faces the court. Called by AZoogiArena::PlaceLaunchPad. */
	void Setup(EZoogiArena InTheme, const FVector& InLandingTarget);

	/** Where a player stands / is launched from (deck top + Zoogi radius). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") TObjectPtr<USceneComponent> LaunchPoint;
	/** Unit launch direction (toward the court centre area, LaunchAngleDeg up). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") FVector LaunchDirection = FVector::ForwardVector;
	/** Launch speed (cm/s) that lands a ballistic Zoogi on LandingTarget under the world gravity. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") float LaunchSpeed = 0.f;
	/** Speed at touchdown (cm/s), for reference / tuning. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") float LandingSpeed = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") FVector LandingTarget = FVector::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") EZoogiArena Theme = EZoogiArena::Meadow;

	UFUNCTION(BlueprintCallable, Category = "Zoogi|LaunchPad") FTransform GetLaunchTransform() const;
	/** Oct 24: the LerpzShip IS the launch pad (Evan). Dev-mode scale of the ship visual (deck/launch rules unchanged). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zoogi|LaunchPad") float ShipScale = 1.f;
	/** Dev mode: move/turn/scale the ship; recomputes the launch so the origin follows it. */
	void Relocate(const FVector& Loc, float Yaw, float InShipScale, const FVector& InLandingTarget);
	/** True when the ship mesh is the pad visual. */
	bool IsShip() const { return bShip; }
	/** Normalized launch vector pointing into the arena (includes the LaunchAngleDeg upward arc). */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|LaunchPad") FVector GetLaunchDirection() const { return LaunchDirection.GetSafeNormal(); }
	/** The actor standing on the pad top (overlap box over the deck), or nullptr. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|LaunchPad") AActor* GetOccupant() const;
	/** Trivial helper: LaunchDirection * LaunchSpeed. */
	UFUNCTION(BlueprintCallable, Category = "Zoogi|LaunchPad") FVector GetLaunchVelocity() const { return LaunchDirection * LaunchSpeed; }

private:
	UStaticMeshComponent* Part(const TCHAR* Shape, UMaterialInterface* Mat, const FVector& Loc, const FRotator& Rot, const FVector& Scale, bool bDeck = false);
	void AddLight(const FLinearColor& Color, float Intensity, const FVector& Loc);
	void BuildDressing();

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Parts;
	bool bShip = false;
	bool BuildShip();
};
