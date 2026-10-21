// web-ui (Oct 20, Evan): the zoogi-ui screens (Content/ZoogiUI, from evanjamesuh/ZoogiRollUE @ zoogi-ui-screens) run inside
// Unreal's WebBrowser widget over the game: Title, Deck, Arena Selection, Story, Shop, Settings and the per-arena Loading
// screen. The page talks to the game through the "interface" bridge (script.js calls ue.interface.broadcast(name, json));
// the game pushes real data back with ZoogiUI.set* calls. The home hub (reference home, Battle tab) opens these screens;
// Arena Selection -> that arena's Loading screen -> the real 2v2 match. In matches the native arena HUD stays (the web layer
// is closed so touch aiming reaches the 3D view). [ZoogiRoll.UI] bWebScreens=False / -ZoogiNoWebUI keeps the native menus.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebBrowser.h"
#include "ZoogiWebUI.generated.h"

class UZoogiWebUILayer;

/** Bound into the page as ue.interface: script.js calls broadcast(name, jsonPayload). */
UCLASS()
class ZOOGIROLL_API UZoogiWebUIBridge : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<UZoogiWebUILayer> Owner;
	UFUNCTION() void Broadcast(const FString& Name, const FString& Json);
};

/** UWebBrowser that binds the bridge object once its Slate browser exists, with transparency on. */
UCLASS()
class ZOOGIROLL_API UZoogiWebBrowser : public UWebBrowser
{
	GENERATED_BODY()
public:
	UZoogiWebBrowser(const FObjectInitializer& OI);
	UPROPERTY() TObjectPtr<UObject> BridgeObject;
	void SetStartURL(const FString& Url) { InitialURL = Url; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class ZOOGIROLL_API UZoogiWebUILayer : public UUserWidget
{
	GENERATED_BODY()
public:
	static bool IsEnabled();
	/** Opens (creating on first use) the web layer on Screen ("title", "deck", "arena", "story", "shop", "settings"). */
	static bool Open(UWorld* World, const FString& Screen);
	static void Close(UWorld* World);
	static bool IsOpen(UWorld* World);

	void OnUIEvent(const FString& Name, const FString& Json);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;

private:
	static UZoogiWebUILayer* Get(UWorld* World, bool bCreate);
	void ShowScreen(const FString& Screen);
	void Hide();
	void Js(const FString& Script);
	void PushState();
	void StartMatchOn(const FString& WebArena);
	void MakeBrowser(const FString& InScreen);
	class AZoogiGameMode* GM() const;

	UPROPERTY() TObjectPtr<UZoogiWebBrowser> Browser;
	UPROPERTY() TObjectPtr<UZoogiWebUIBridge> Bridge;
	UPROPERTY() TObjectPtr<UObject> HomeClick;
	TWeakObjectPtr<UWidget> HomeBtn;
	bool bReady = false;          // page sent UIReady
	bool bShown = false;
	bool bKillBrowser = false;    // Android: release the hidden browser on the next tick
	FString PendingScreen;
	TArray<FString> PendingJs;
	FString Screen;
	FString PageUrl;
	float TestT = 0.f; int32 TestStep = 0; // -ZoogiWebUITest capture driver
	void TickTest(float Dt);
};
