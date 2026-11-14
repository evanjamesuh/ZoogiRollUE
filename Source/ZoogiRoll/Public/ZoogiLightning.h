// Nov 1 b57 (Evan: "After a critical hit blue lightning streaks from the orb and lightning from the zoogi the color of the player"):
// a short crackling lightning burst: jagged glowing bolts radiating from an actor (follows it), re-jagged every few frames,
// fading out. Waits while an anime cut-in covers the screen so it plays right after it. Real-time (also runs while frozen).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiLightning.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ZOOGIROLL_API AZoogiLightning : public AActor
{
	GENERATED_BODY()
public:
	AZoogiLightning();
	static AZoogiLightning* Burst(UWorld* World, AActor* From, const FLinearColor& Color, int32 Bolts, float Length, float Life);
	virtual void Tick(float DeltaSeconds) override;
private:
	static constexpr int32 SegsPerBolt = 6;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segs;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
	TWeakObjectPtr<AActor> From;
	FVector Origin = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	TArray<FVector> Dirs;
	int32 Bolts = 6;
	float Len = 300.f, Life = 1.f, Age = 0.f, Wait = 0.f, NextJag = 0.f;
	FRandomStream R;
	void Rebuild(float K);
};
