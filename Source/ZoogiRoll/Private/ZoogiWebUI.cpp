// web-ui: see ZoogiWebUI.h
#include "ZoogiWebUI.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiMenuRoot.h"   // UZoogiMenuRootClick
#include "ZoogiMenuWidget.h" // UZoogiButton
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIStyle.h"
#include "SWebBrowser.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
	// web card ids use hyphens ("bank-strike"), the game's use underscores ("bank_strike")
	FString WebCardId(FName Id) { return Id.ToString().Replace(TEXT("_"), TEXT("-")); }
	FName GameCardId(const FString& Id) { return FName(*Id.Replace(TEXT("-"), TEXT("_"))); }

	// web arena ids -> game arenas (Skyfall Isles has no level yet: it plays on the Frozen Ring)
	int32 GameArena(const FString& Id)
	{
		if (Id == TEXT("meadow")) return (int32)EZoogiArena::Meadow;
		if (Id == TEXT("volcanic")) return 2;
		if (Id == TEXT("cosmic")) return 3;
		if (Id == TEXT("pharaoh")) return 5;
		if (Id == TEXT("night")) return 6;
		if (Id == TEXT("training")) return (int32)EZoogiArena::Training;
		return (int32)EZoogiArena::Frozen; // frozen, skyfall
	}

	FString JsStr(const FString& S) { return TEXT("'") + S.ReplaceCharWithEscapedChar().Replace(TEXT("'"), TEXT("\\'")) + TEXT("'"); }

	TWeakObjectPtr<UZoogiWebUILayer> GLayer;
}

// ------------------------------------------------------------------------------------------------ bridge + browser
void UZoogiWebUIBridge::Broadcast(const FString& Name, const FString& Json)
{
	if (UZoogiWebUILayer* L = Owner.Get()) L->OnUIEvent(Name, Json);
}

UZoogiWebBrowser::UZoogiWebBrowser(const FObjectInitializer& OI) : Super(OI)
{
	bSupportsTransparency = false;
}

TSharedRef<SWidget> UZoogiWebBrowser::RebuildWidget()
{
	TSharedRef<SWidget> W = Super::RebuildWidget();
	if (WebBrowserWidget.IsValid() && BridgeObject) WebBrowserWidget->BindUObject(TEXT("interface"), BridgeObject, true);
	return W;
}

// ------------------------------------------------------------------------------------------------ layer
bool UZoogiWebUILayer::IsEnabled()
{
	static const bool b = []()
	{
		bool bIni = true;
		if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.UI"), TEXT("bWebScreens"), bIni, GGameIni);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWebUI"))) bIni = false;
		const FString Index = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ZoogiUI/index.html"));
		if (bIni && !FPaths::FileExists(Index)) { UE_LOG(LogZoogi, Warning, TEXT("WEBUI missing %s - native menus only"), *Index); bIni = false; }
		return bIni;
	}();
	return b;
}

UZoogiWebUILayer* UZoogiWebUILayer::Get(UWorld* World, bool bCreate)
{
	if (UZoogiWebUILayer* L = GLayer.Get()) if (L->GetWorld() == World) return L;
	if (!bCreate || !World || !IsEnabled()) return nullptr;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return nullptr;
	UZoogiWebUILayer* L = CreateWidget<UZoogiWebUILayer>(PC, UZoogiWebUILayer::StaticClass());
	if (!L) return nullptr;
	L->AddToViewport(30); // above the menu (10) and the HUD layers
	GLayer = L;
	return L;
}

bool UZoogiWebUILayer::Open(UWorld* World, const FString& InScreen)
{
	UZoogiWebUILayer* L = Get(World, true);
	if (!L) return false;
	L->ShowScreen(InScreen);
	return true;
}

void UZoogiWebUILayer::Close(UWorld* World) { if (UZoogiWebUILayer* L = Get(World, false)) L->Hide(); }
bool UZoogiWebUILayer::IsOpen(UWorld* World) { const UZoogiWebUILayer* L = Get(World, false); return L && L->bShown; }

AZoogiGameMode* UZoogiWebUILayer::GM() const { return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr; }

TSharedRef<SWidget> UZoogiWebUILayer::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // hidden layer never blocks the game or the menus
		WidgetTree->RootWidget = Root;
		Bridge = NewObject<UZoogiWebUIBridge>(this);
		Bridge->Owner = this;
		Browser = WidgetTree->ConstructWidget<UZoogiWebBrowser>(UZoogiWebBrowser::StaticClass());
		Browser->BridgeObject = Bridge;
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Browser)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); }
		// HOME: back to the native hub (the web screens have no hub of their own); bottom-left corner, over the art's
		// crystal decoration, clear of every web control
		UZoogiButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
		B->SetStyle(ZCClearStyle());
		B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		UBorder* Face = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Face->SetBrush(ZoogiUIStyle::RoundBrush(ZCHex(TEXT("1D2442")).CopyWithNewOpacity(0.92f), ZCGold, 3.f, 32.f));
		Face->SetHorizontalAlignment(HAlign_Center); Face->SetVerticalAlignment(VAlign_Center);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(TEXT("HOME")));
		T->SetFont(ZCFont(22.f, 2.f));
		T->SetColorAndOpacity(FSlateColor(ZCHex(TEXT("FFE08A"))));
		Face->SetContent(T);
		B->AddChild(Face);
		UZoogiMenuRootClick* C = NewObject<UZoogiMenuRootClick>(this);
		C->Fn = [this]() { UE_LOG(LogZoogi, Log, TEXT("WEBUI HOME")); Hide(); };
		HomeClick = C;
		B->OnClicked.AddDynamic(C, &UZoogiMenuRootClick::Click);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(B)) { S->SetAnchors(FAnchors(0.f, 1.f)); S->SetAlignment(FVector2D(0.f, 1.f)); S->SetPosition(FVector2D(10.f, -10.f)); S->SetSize(FVector2D(132.f, 64.f)); }
		HomeBtn = B;
		const FString Url = TEXT("file:///") + FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("ZoogiUI/index.html"));
		Browser->SetStartURL(Url + TEXT("#title")); // the Slate browser is created in Super::RebuildWidget below and opens this
		UE_LOG(LogZoogi, Log, TEXT("WEBUI layer up: %s"), *Url);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}

void UZoogiWebUILayer::Js(const FString& Script)
{
	if (!bReady || !Browser) { PendingJs.Add(Script); return; }
	Browser->ExecuteJavascript(Script);
}

void UZoogiWebUILayer::ShowScreen(const FString& InScreen)
{
	Screen = InScreen;
	bShown = true;
	if (Browser) Browser->SetVisibility(ESlateVisibility::Visible);
	if (HomeBtn.IsValid()) HomeBtn->SetVisibility(InScreen == TEXT("title") ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	PushState();
	Js(FString::Printf(TEXT("ZoogiUI.show(%s)"), *JsStr(InScreen)));
	UE_LOG(LogZoogi, Log, TEXT("WEBUI show %s (ready %d)"), *InScreen, bReady ? 1 : 0);
}

void UZoogiWebUILayer::Hide()
{
	// only the children hide: a collapsed UserWidget stops ticking (the test driver and late pushes live in NativeTick)
	bShown = false;
	if (Browser) Browser->SetVisibility(ESlateVisibility::Collapsed);
	if (HomeBtn.IsValid()) HomeBtn->SetVisibility(ESlateVisibility::Collapsed);
	UE_LOG(LogZoogi, Log, TEXT("WEBUI hidden (native hub)"));
}

void UZoogiWebUILayer::PushState()
{
	// the game is the source of truth: player, currencies, notices, decks
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return;
	const FZoogiFrontProfileData& D = FP->Data();
	const int32 Need = UZoogiFrontProfileSubsystem::XPForLevel(D.Level);
	Js(FString::Printf(TEXT("ZoogiUI.setPlayer({name:%s,level:%d,xp:%.3f})"), *JsStr(D.GuestName), D.Level, Need > 0 ? FMath::Clamp((float)D.XP / Need, 0.f, 1.f) : 0.f));
	Js(FString::Printf(TEXT("ZoogiUI.setCurrency({gems:%d,coins:%d})"), FP->Gems(), D.Gold));
	Js(FString::Printf(TEXT("ZoogiUI.setNotifications(%d)"), FP->PassClaimableCount() + FP->RoadClaimableCount()));
	FString Decks = TEXT("[");
	for (int32 i = 0; i < 3 && i < UZoogiFrontProfileSubsystem::NumDecks; ++i)
	{
		if (i) Decks += TEXT(",");
		Decks += FString::Printf(TEXT("{name:%s,cards:["), *JsStr(FP->DeckName(i)));
		const TArray<FName>& Cards = FP->Deck(i);
		for (int32 k = 0; k < Cards.Num(); ++k) Decks += (k ? TEXT(",") : TEXT("")) + JsStr(WebCardId(Cards[k]));
		Decks += TEXT("]}");
	}
	Decks += TEXT("]");
	Js(FString::Printf(TEXT("ZoogiUI.setDecks(%s)"), *Decks));
}

void UZoogiWebUILayer::StartMatchOn(const FString& WebArena)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const int32 A = GameArena(WebArena);
	if ((int32)G->MenuArena != A) G->MenuAction(TEXT("Arena"), A);
	G->ComboSetMode(true); // 2v2: you + CPU ally vs 2 CPUs (non-Frozen arenas play the Meadow combo layout for now)
	for (int32 i = 1; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = true;
	UE_LOG(LogZoogi, Log, TEXT("WEBUI arena %s -> game arena %d -> 2v2 match"), *WebArena, A);
	Hide();
	G->MenuAction(TEXT("Start"), 0);
}

void UZoogiWebUILayer::OnUIEvent(const FString& Name, const FString& Json)
{
	TSharedPtr<FJsonObject> O;
	{
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(R, O) || !O.IsValid()) O = MakeShared<FJsonObject>();
	}
	UE_LOG(LogZoogi, Log, TEXT("WEBUI event %s %s"), *Name, *Json.Left(200));
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	AZoogiGameMode* G = GM();
	if (Name == TEXT("UIReady"))
	{
		bReady = true;
		TArray<FString> Q = MoveTemp(PendingJs);
		for (const FString& S : Q) Browser->ExecuteJavascript(S);
		if (!bShown) Hide();
	}
	else if (Name == TEXT("TitleTapped")) Hide(); // title -> the native home hub
	else if (Name == TEXT("LoadingComplete")) StartMatchOn(O->GetStringField(TEXT("arena")));
	else if (Name == TEXT("DeckSaved") && FP)
	{
		const int32 Deck = (int32)O->GetNumberField(TEXT("deck"));
		TArray<FName> Cards;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(TEXT("cards"), Arr)) for (const TSharedPtr<FJsonValue>& V : *Arr) Cards.AddUnique(GameCardId(V->AsString()));
		FP->SetDeck(Deck, Cards);
		FString DeckName;
		if (O->TryGetStringField(TEXT("name"), DeckName) && !DeckName.IsEmpty()) FP->SetDeckName(Deck, DeckName);
		FP->SetActiveDeck(Deck);
		FP->Save();
		UE_LOG(LogZoogi, Log, TEXT("WEBUI deck %d saved (%d cards, active)"), Deck + 1, Cards.Num());
	}
	else if (Name == TEXT("SettingChanged") && G)
	{
		const FString Key = O->GetStringField(TEXT("key"));
		double V = 0.0;
		if (O->TryGetNumberField(TEXT("value"), V))
		{
			if (Key == TEXT("music")) G->MenuAction(TEXT("MusicVol"), FMath::RoundToInt(V));
			else if (Key == TEXT("sfx")) G->MenuAction(TEXT("SfxVol"), FMath::RoundToInt(V));
		}
	}
	else if (Name == TEXT("Purchase") && FP)
	{
		// gem / coin purchases: the page already deducted; the game ledger is the truth, then the page gets the real balance
		const int32 Price = (int32)O->GetNumberField(TEXT("price"));
		const FString Cur = O->GetStringField(TEXT("currency"));
		bool bOk = true;
		if (Cur == TEXT("gems")) bOk = FP->SpendGems(Price, FName(*(TEXT("webshop:") + O->GetStringField(TEXT("sku")))));
		else if (Cur == TEXT("coins")) { bOk = FP->Data().Gold >= Price; if (bOk) { FP->Mutable().Gold -= Price; FP->Save(); } }
		Js(FString::Printf(TEXT("ZoogiUI.setCurrency({gems:%d,coins:%d})"), FP->Gems(), FP->Data().Gold));
		if (!bOk) Js(TEXT("ZoogiUI.toast('Not enough funds')"));
	}
	else if (Name == TEXT("IAPRequest"))
	{
		Js(FString::Printf(TEXT("ZoogiUI.purchaseResult({sku:%s,ok:false})"), *JsStr(O->GetStringField(TEXT("sku")))));
		Js(TEXT("ZoogiUI.toast('Store not connected yet')"));
	}
}

void UZoogiWebUILayer::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	TickTest(Dt);
}

void UZoogiWebUILayer::TickTest(float Dt)
{
	// -ZoogiWebUITest -ZoogiShots=<dir>: title -> home -> deck / arena / story / shop / settings -> pick Frozen Ring ->
	// loading -> real match, a screenshot at each step (drives the page through its own controls via JavaScript)
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWebUITest"));
	AZoogiGameMode* G = GM();
	if (!bTest || !G || !bReady) return;
	TestT += Dt;
	auto Shot = [G](const TCHAR* N)
	{
		if (G->ShotDir.IsEmpty()) return;
		const FString P = FPaths::Combine(G->ShotDir, FString::Printf(TEXT("webui_%s.png"), N));
		FScreenshotRequest::RequestScreenshot(P, true, false);
		UE_LOG(LogZoogi, Log, TEXT("WEBUI TEST shot %s"), *P);
	};
	struct FStep { float At; TFunction<void()> Fn; };
	static TArray<FStep> Steps;
	if (Steps.Num() == 0)
	{
		Steps.Add({ 3.f, [Shot]() { Shot(TEXT("01_title")); } });
		Steps.Add({ 4.f, [this]() { Browser->ExecuteJavascript(TEXT("document.querySelector('[data-ui=\"Title.TapToStart\"]').click()")); } });
		Steps.Add({ 6.f, [Shot]() { Shot(TEXT("02_home")); } });
		const TCHAR* Screens[] = { TEXT("deck"), TEXT("arena"), TEXT("story"), TEXT("shop"), TEXT("settings") };
		float T = 7.f;
		for (int32 i = 0; i < 5; ++i)
		{
			const FString S = Screens[i];
			Steps.Add({ T, [this, S]() { Open(GetWorld(), S); } });
			Steps.Add({ T + 2.5f, [Shot, S, i]() { Shot(*FString::Printf(TEXT("%02d_%s"), i + 3, *S)); } });
			T += 3.5f;
		}
		Steps.Add({ T, [this]() { Open(GetWorld(), TEXT("arena")); } });
		Steps.Add({ T + 1.5f, [this]() { Browser->ExecuteJavascript(TEXT("document.querySelector('[data-ui=\"Arena.Card.frozen\"]').click()")); } });
		Steps.Add({ T + 2.5f, [Shot]() { Shot(TEXT("08_arena_frozen")); } });
		Steps.Add({ T + 3.f, [this]() { Browser->ExecuteJavascript(TEXT("document.getElementById('btnSelectArena').click()")); } });
		Steps.Add({ T + 5.f, [Shot]() { Shot(TEXT("09_loading")); } });
		Steps.Add({ T + 25.f, [Shot]() { Shot(TEXT("10_match")); } });
	}
	while (TestStep < Steps.Num() && TestT >= Steps[TestStep].At) Steps[TestStep++].Fn();
}
