// Oct 29 (Evan): "in dev add a bumper intensity scale so i can choose the rebound force and select what i want to have as a bumper".
// Any solid Dev Mode object (a star bumper or any obstacle) can be switched to an ACTIVE bumper with a rebound level 1..10:
// a Zoogi or orb that hits it leaves along the reflected direction at max(speed in x Mul, Kick) (capped 3200 cm/s).
//   level:  1     3     5     7     10
//   x mul:  1.0   1.4   1.8   2.2   2.8      min kick 360 .. 1350 cm/s
// Fixed rules (game logic, no randomness): the velocity a frame before the contact, the contact normal from the closest point of
// the bumper's collision, 0.2 s per-object cooldown. Saved with the dev layout ("bumper" / "bounce" fields).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiBumpers.generated.h"

class AZoogiArena;
class UPrimitiveComponent;

UCLASS()
class ZOOGIROLL_API UZoogiBumperSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Level 1..10 = active bumper, 0 = off. */
	void SetBumper(UPrimitiveComponent* C, int32 Level);
	static float Mul(int32 Level) { return 0.8f + 0.2f * FMath::Clamp(Level, 1, 10); }
	static float Kick(int32 Level) { return 250.f + 110.f * FMath::Clamp(Level, 1, 10); }
	static constexpr float MaxOut = 3200.f;
	int32 Rebounds = 0;

	UFUNCTION()
	void OnBumperHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> Levels;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FVector> LastVel;
	TMap<TWeakObjectPtr<AActor>, double> Cool;
};

namespace ZoogiBumpers
{
	/** Re-applies DevProps[Index]'s bumper setting to its solid components. */
	ZOOGIROLL_API void Sync(AZoogiArena* Arena, int32 Index);
}
