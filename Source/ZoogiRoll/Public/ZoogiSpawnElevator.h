// Spawn elevator (Evan, Oct 14 6:42 PM): at match start each Zoogi rises on its arena pack's spawn-platform elevator
// (SourceArt/SpawnPlatform/<Map>, imported to /Game/Env/SpawnPlatform/<Map>/...). PRESENTATION ONLY: no collision, no
// physics; the real Zoogi stays exactly where it spawned (only hidden ~1.2 s while a visual copy rides the lift).
// There is no floor hole: the pad sits on the floor, its petals swing down under the floor, a dark hex mouth fakes the
// opening and the floor itself hides everything below z 0. Skipped in -ZoogiAuto / battle-test / parity runs unless
// -ZoogiSpawnElevator; [ZoogiRoll.SpawnElevator] bEnabled=False or -ZoogiNoSpawnElevator turns it off.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiSpawnElevator.generated.h"

class AZoogiBall;
class AZoogiGameMode;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class ZOOGIROLL_API AZoogiSpawnElevator : public AActor
{
	GENERATED_BODY()
public:
	AZoogiSpawnElevator();
	static bool IsEnabled();
	/** One elevator per live Zoogi at its spawn point. Call when the match's first turn starts. */
	static void PlayAll(AZoogiGameMode* GM);
	virtual void Tick(float Dt) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Init(AZoogiBall* InBall, const FString& Map, float InDelay);
	UStaticMeshComponent* Part(UStaticMesh* M);
	TWeakObjectPtr<AZoogiBall> Ball;
	UPROPERTY() TObjectPtr<USceneComponent> Pad;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Base;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lift;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mouth;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Petals;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	struct FProxy { TWeakObjectPtr<UStaticMeshComponent> Src; TObjectPtr<UStaticMeshComponent> Dst; };
	TArray<FProxy> Proxies;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ProxyComps;
	FLinearColor Accent = FLinearColor::White;
	float LiftRest = -122.f;
	float Age = 0.f;
	float Delay = 0.f;
	float Rise = 1.2f;
	bool bHid = false, bShown = false, bBurst = false;
	int32 Shot = 0;
};