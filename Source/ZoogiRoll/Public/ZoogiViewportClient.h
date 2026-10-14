// framed: game viewport client that constrains the local player's REAL view rect to the centred arena window
// (splitscreen-style ULocalPlayer::Origin / Size) while the framed HUD is active, and spawns the framed UMG layer.
// With the frame inactive (default, or in the main menu) it does nothing and behaves exactly like UGameViewportClient.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "ZoogiViewportClient.generated.h"

class UZoogiFramedHUDWidget;

UCLASS()
class ZOOGIROLL_API UZoogiViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

private:
	bool bRectApplied = false;
	TEnumAsByte<EAspectRatioAxisConstraint> SavedAxis = AspectRatio_MaintainXFOV;
	float TrayOpen = 1.f;
	TWeakObjectPtr<UZoogiFramedHUDWidget> Widget;
	bool bLoggedFirst = false;
};
