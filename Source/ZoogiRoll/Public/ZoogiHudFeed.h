// hud-feed (Evan, Oct 17): "when the game zooms in on a player's face ... the actual gameplay camera is dedicated right there"
// - a picture-in-picture LIVE camera panel in the square stone slot right of the DECK on the landscape framed HUD (hud-v3).
// The HUD side only: it draws ZoogiFramed::GetHudFeedTexture() into ZoogiFramed::HudFeedSlot() with its own frame and rounded
// mask, or an idle panel (emblem + dim LIVE pill) when there is no texture / the feed is inactive. Never drawn in portrait.
//
// Game Camera owns the feed camera + render target. Hook (by name, no compile dependency; skipped with one log line if missing):
//   UTextureRenderTarget2D* AZoogiPlayerController::GetGameplayPipTexture()  - persistent RT, passed to SetHudFeedTexture
//   void AZoogiPlayerController::SetGameplayPipActive(bool)                  - true while the landscape HUD panel is up,
//                                                                              false when hidden / portrait / out of battle / paused
//   multicast AZoogiPlayerController::OnGameplayPipShot(FName ShotType)       - optional: small label + flash on cuts
// DEV STUB (-ZoogiFeedStub, compiled out of Shipping): when Game Camera's functions are missing, a 256x256 SceneCapture2D
// close-up on the acting Zoogi stands in so the panel can be reviewed. Game Camera replaces it; it is never used when their
// functions exist. -ZoogiFeedShots (+ -ZoogiShots=<dir>) captures hudfeed_live / hudfeed_idle / hudfeed_hidden review shots.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiHudFeed.generated.h"

class APlayerController;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class FSlateWindowElementList;
struct FGeometry;

namespace ZoogiHudFeed
{
	enum class EPanel : uint8 { Hidden, Idle, Live };

	/** What the panel draws for layout L (viewport px): state, slot, picture rect (slot inset by the frame), corner radius and
	 *  the picture's UV crop (centre crop when the texture aspect differs from the square slot). */
	struct FPlan
	{
		EPanel State = EPanel::Hidden;
		FBox2D Slot = FBox2D(ForceInit), Image = FBox2D(ForceInit);
		float Radius = 0.f, ImageRadius = 0.f;
		FBox2f UV = FBox2f(FVector2f(0.f, 0.f), FVector2f(1.f, 1.f));
		UTextureRenderTarget2D* Texture = nullptr;
	};
	ZOOGIROLL_API FPlan Plan(const ZoogiFramed::FLayout& L);

	/** Paints the panel (framed HUD painter, layers Layer..Layer+3). S = local units per viewport px. */
	ZOOGIROLL_API void Paint(const FGeometry& Geo, FSlateWindowElementList& Out, int32 Layer, const ZoogiFramed::FLayout& L, float S, float Time);

	/** Glue, called by the framed HUD every tick: bHudShown = the landscape framed HUD is up and not paused. Resolves Game
	 *  Camera's PIP functions on the controller (or the dev stub), sets the feed texture + active flag, tells Game Camera. */
	ZOOGIROLL_API void Update(APlayerController* PC, bool bHudShown, float Dt);

	/** Last shot label from OnGameplayPipShot (NAME_None when none) and its age in seconds. */
	ZOOGIROLL_API void NotifyShot(FName ShotType);
	ZOOGIROLL_API FName LastShot(float& OutAge);
}

/** Per-controller glue for the HUD feed (created on demand by ZoogiHudFeed::Update): Game Camera hook calls by name and the
 *  dev-only SceneCapture stub. Visual only: never touches rules, physics, input or the gameplay camera. */
UCLASS()
class ZOOGIROLL_API UZoogiHudFeedGlue : public UActorComponent
{
	GENERATED_BODY()
public:
	UZoogiHudFeedGlue();
	void Drive(bool bHudShown, float Dt);
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Bound to Game Camera's OnGameplayPipShot(FName) when it exists. */
	UFUNCTION() void OnPipShot(FName ShotType);

private:
	void ResolveHooks();
	UTextureRenderTarget2D* CallGetTexture();
	void CallSetActive(bool bActive);
	UTextureRenderTarget2D* TickStub(bool bLive, float Dt);
	void TickShots(bool bHudShown, ZoogiHudFeed::EPanel State, float Dt);

	bool bResolved = false, bHasGetTex = false, bHasSetActive = false, bStub = false;
	int32 SentActive = -1;   // last SetGameplayPipActive value sent (-1 = none yet)
	double LastDrive = 0.0, LastTexQuery = -10.0;
	TWeakObjectPtr<UTextureRenderTarget2D> CamTex;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> StubRT;
	UPROPERTY() TObjectPtr<ASceneCapture2D> StubCam;
	FVector StubPos = FVector::ZeroVector;
	FRotator StubRot = FRotator::ZeroRotator;
	bool bStubPlaced = false;
	int32 ShotState = -1;
	float ShotStateT = 0.f;
};
