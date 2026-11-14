// Glove crane respawn (Evan via ZOOGI, Oct 22): a knocked-off Zoogi is picked up by a floating stage crane with a cartoon
// white glove and set down exactly on its launch pad (~1.5 s). PRESENTATION ONLY: rules are untouched.
//
// HOOKS (ui/launch-flow e7d6779 + arena/launch-pads 25f92c0, merged into fx/glove-crane):
//   AZoogiGameMode::OnZoogiNeedsCarryToPad (FZoogiCarryToPad: AZoogiBall* Zoogi, const FTransform& PadTransform)
//       fired by SendToPad (RespawnBall under the launch flow) while the Zoogi is out/hidden; the game waits up to
//       CraneTimeout (6 s) then places it itself. AZoogiGloveCrane::Bind adds the handler in AZoogiGameMode::BeginPlay.
//   AZoogiGameMode::NotifyZoogiPlacedOnPad(Zoogi) called at release (only while LaunchFlow.IsCarryPending, so a
//       timeout / turn-start placement is never repeated); the game snaps the Zoogi onto the pad and re-arms physics.
//   Pad: PadTransform comes from ZoogiLaunchPads::GetPadTransform, which now prefers
//       AZoogiArena::GetLaunchPad()->GetLaunchTransform() (the real AZoogiLaunchPad), so the glove drops exactly where
//       the game seats the Zoogi. AZoogiLaunchPad::GetOccupant() is checked and logged (an occupied pad is not blocked).
// Not bound (=> instant placement, existing timing) in -ZoogiAuto / -ZoogiBattleTest / -ZoogiNoForm / automation runs
// unless -ZoogiGloveCrane / -ZoogiGloveTest; off with -ZoogiNoGloveCrane or [ZoogiRoll.GloveCrane] bEnabled=False.
// Test trigger: -ZoogiGloveTest knocks each Zoogi out in turn (every 2.2 s from 9 s) and sends it to its pad.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoogiGloveCrane.generated.h"

class AZoogiBall;
class UStaticMeshComponent;

UCLASS()
class ZOOGIROLL_API AZoogiGloveCrane : public AActor
{
	GENERATED_BODY()
public:
	AZoogiGloveCrane();
	static bool IsEnabled();
	/** Binds the crane to GM->OnZoogiNeedsCarryToPad when enabled (call once from AZoogiGameMode::BeginPlay). */
	static void Bind(class AZoogiGameMode* GM);
	static void HandleRequest(AZoogiBall* Ball, const FTransform& PadTransform, TWeakObjectPtr<class AZoogiGameMode> GM); // delegate params, then the bound payload
	virtual void Tick(float Dt) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Init(AZoogiBall* InBall, const FVector& InPad);
	bool StillPending() const;
	UStaticMeshComponent* Part(UStaticMesh* M, USceneComponent* Parent, const FVector& Loc, const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator);
	void Release();
	TWeakObjectPtr<AZoogiBall> Ball;
	UPROPERTY() TObjectPtr<USceneComponent> Glove;      // pivot = palm centre
	UPROPERTY() TObjectPtr<USceneComponent> Hold;       // where the carried Zoogi's centre hangs
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Cable;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Trolley;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Fingers; // curl pivots
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ProxyComps;
	struct FProxy { TWeakObjectPtr<UStaticMeshComponent> Src; TObjectPtr<UStaticMeshComponent> Dst; FTransform Rel; };
	TArray<FProxy> Proxies;
	FVector From, Grab, Pad, Up;
	TWeakObjectPtr<class AZoogiGameMode> GameMode;
	float Age = 0.f, Size = 1.f;
	bool bPadOnScr = true;
	bool bReleased = false;
	int32 Shot = 0;
	static int32 GCount;
};