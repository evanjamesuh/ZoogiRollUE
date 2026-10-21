#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiTargetMarker.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;

UENUM()
enum class EZoogiMarkerStyle : uint8
{
	Hidden,
	Hover,     // soft thin ring under the cursor target
	SoftLock,  // ring + highlight shell (Standby pick)
	Lock       // glowing ring, rotating reticle brackets, highlight shell, bobbing marker
};

/** Ground ring / reticle / highlight / bobbing arrow that follows a target actor. */
UCLASS()
class ZOOGIROLL_API AZoogiTargetMarker : public AActor
{
	GENERATED_BODY()
public:
	AZoogiTargetMarker();
	virtual void Tick(float DeltaSeconds) override;
	void SetTarget(AActor* InTarget, EZoogiMarkerStyle InStyle, const FLinearColor& InColor);

protected:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> InnerRing;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Ticks;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Shell;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Arrow;
	UPROPERTY() TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDs;
	TWeakObjectPtr<AActor> Target;
	EZoogiMarkerStyle Style = EZoogiMarkerStyle::Hidden;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
	float StyleAge = 0.f;
	bool bBuilt = false;
	void Build();
	UStaticMeshComponent* MakePart(const TCHAR* Shape, float Strength, bool bRing);
};
