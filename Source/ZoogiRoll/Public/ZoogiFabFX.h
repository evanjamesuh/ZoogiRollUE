// Nov 2 b63 (Claude): plays an effect from the Fab packs Evan bought (Content/StylizedToonVFX3, Content/MeteorRain) as a small
// actor that can follow a Zoogi / orb, runs at real speed in slow-mo (the parry cut runs the world at ~0.03x) and dies after a
// real-time life. Every play logs the effect's measured size (max runtime bounds radius) so sizes can be tuned from the log.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiFabFX.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class ZOOGIROLL_API AZoogiFabFX : public AActor
{
	GENERATED_BODY()
public:
	AZoogiFabFX();
	/** Path = object path ("/Game/StylizedToonVFX3/Effects/NS_Wood_shield.NS_Wood_shield"); Scale = uniform actor scale;
	 *  Follow = keeps the effect on that actor (+Offset) every frame; RealLife = seconds of real time before it is removed. */
	static AZoogiFabFX* Play(UWorld* W, const TCHAR* Path, const FVector& Loc, float Scale, float RealLife, AActor* Follow = nullptr,
		const FVector& Offset = FVector::ZeroVector, const FRotator& Rot = FRotator::ZeroRotator);
	/** Plays a Sound Cue / Wave from a Fab pack at Loc (no-op when missing). */
	static void Sound(UWorld* W, const TCHAR* Path, const FVector& Loc, float Volume = 1.f);
	virtual void Tick(float Dt) override;

	UPROPERTY() TObjectPtr<UNiagaraComponent> NC;
	TWeakObjectPtr<AActor> Follow;
	FVector Offset = FVector::ZeroVector;
	double DieAt = 0.0, BornAt = 0.0;
	float StopAfter = 0.f; // > 0: stop spawning after this many real seconds (cuts a pack's late stage)
	bool bStopped = false;
	float MaxR = 0.f;
	FString Tag;
	/** b180: > 0 = pause the effect this many real seconds after it starts and keep it (no DieAt) until Release() */
	float FreezeAt = 0.f;
	bool bFrozen = false;
	/** b180: a frozen effect plays on from where it stopped (its crumble) and is removed After seconds later */
	void Release(float After);
};
