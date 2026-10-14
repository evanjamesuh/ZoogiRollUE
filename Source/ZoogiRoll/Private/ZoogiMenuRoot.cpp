// menu: see ZoogiMenuRoot.h
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuWidget.h" // UZoogiButton (non-focusable button)
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "ZoogiUIStyle.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiPowerUnlockSubsystem.h"
#include "ZoogiRoll.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FileMediaSource.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
	// Mockup preview (1024 wide) -> 2400x1080 design units.
	constexpr float ZMK = 2400.f / 1024.f;

	FLinearColor ZMHex(const TCHAR* H) { return FLinearColor::FromSRGBColor(FColor::FromHex(H)); }
	const FLinearColor ZMCGold = ZMHex(TEXT("FFB21A"));
	const FLinearColor ZMCTeal = ZMHex(TEXT("17C2A4"));
	const FLinearColor ZMCViolet = ZMHex(TEXT("7A45F5"));
	const FLinearColor ZMCNavy = ZMHex(TEXT("1F4FC8"));
	const FLinearColor ZMCRed = ZMHex(TEXT("E5484D"));
	const FLinearColor ZMCCyan = ZMHex(TEXT("4FE3FF"));
	const FLinearColor ZMCDisabled = ZMHex(TEXT("26346A"));
	const FLinearColor ZMPNavy = ZMHex(TEXT("1A3FA8")).CopyWithNewOpacity(0.93f);
	const FLinearColor ZMPTeal = ZMHex(TEXT("0E8E9E")).CopyWithNewOpacity(0.93f);
	const FLinearColor ZMPViolet = ZMHex(TEXT("6236D6")).CopyWithNewOpacity(0.95f);
	const FLinearColor ZMTLabel = ZMHex(TEXT("BFE9FF"));
	const FLinearColor ZMTSoft = ZMHex(TEXT("DCE6FF"));

	FLinearColor ZMLighten(const FLinearColor& C, float T) { return FMath::Lerp(C, FLinearColor::White, T).CopyWithNewOpacity(C.A); }
	FLinearColor ZMDarken(const FLinearColor& C, float T) { return FMath::Lerp(C, FLinearColor::Black, T).CopyWithNewOpacity(C.A); }

	const TCHAR* ZMScreenName(EZoogiScreen S)
	{
		switch (S)
		{
		case EZoogiScreen::Home: return TEXT("Home");
		case EZoogiScreen::Play: return TEXT("Play");
		case EZoogiScreen::ZoogiSelect: return TEXT("ZoogiSelect");
		case EZoogiScreen::Powers: return TEXT("Powers");
		case EZoogiScreen::Settings: return TEXT("Settings");
		case EZoogiScreen::Collection: return TEXT("Collection");
		case EZoogiScreen::Story: return TEXT("Story");
		case EZoogiScreen::Events: return TEXT("Events");
		case EZoogiScreen::Shop: return TEXT("Shop");
		case EZoogiScreen::Social: return TEXT("Social");
		case EZoogiScreen::Create: return TEXT("Create");
		}
		return TEXT("?");
	}

	bool ZMDevFlag() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDev")); return b; }

	// Collection art (game art from the approved mockups; no Wolfgang).
	struct ZMFArtEntry { const TCHAR* Art; const TCHAR* Label; const TCHAR* RosterId; };
	const ZMFArtEntry ZMGCollection[] = {
		{ TEXT("blue"), TEXT("BLUE"), TEXT("blue") }, { TEXT("bolt"), TEXT("BOLT"), nullptr }, { TEXT("luma"), TEXT("LUMA"), TEXT("luma") },
		{ TEXT("vex"), TEXT("VEX"), TEXT("vex") }, { TEXT("sera"), TEXT("SERA"), TEXT("sera") }, { TEXT("tavi"), TEXT("TAVI"), TEXT("tavi") },
		{ TEXT("nightshade"), TEXT("NIGHTSHADE"), nullptr }, { TEXT("pinpoint"), TEXT("PINPOINT"), nullptr }, { TEXT("lars"), TEXT("LARS"), nullptr },
		{ TEXT("moss"), TEXT("MOSS"), TEXT("moss") }, { TEXT("hotstreak"), TEXT("HOTSTREAK"), nullptr }, { TEXT("wraps"), TEXT("WRAPS"), nullptr },
		{ TEXT("geodie"), TEXT("GEODIE"), nullptr }, { TEXT("chompsy"), TEXT("CHOMPSY"), nullptr }, { TEXT("helio"), TEXT("HELIO"), nullptr },
		{ TEXT("coralie"), TEXT("CORALIE"), nullptr }, { TEXT("dark_zoogy"), TEXT("DARK ZOOGY"), TEXT("darkzoogy") }, { TEXT("master_orin"), TEXT("MASTER ORIN"), TEXT("orin") },
	};

	FString ZMArenaArt(int32 A)
	{
		switch ((EZoogiArena)A)
		{
		case EZoogiArena::Frozen: return TEXT("frozen_background");
		case EZoogiArena::Meadow: case EZoogiArena::Training: return TEXT("meadows_background");
		case EZoogiArena::Volcanic: return TEXT("lava_background");
		case EZoogiArena::Cosmic: return TEXT("cosmos_background");
		default: return FString();
		}
	}
	FLinearColor ZMArenaTint(int32 A)
	{
		switch ((EZoogiArena)A)
		{
		case EZoogiArena::Arabian: return ZMHex(TEXT("C9862E"));
		case EZoogiArena::Tomb: return ZMHex(TEXT("B08A3A"));
		case EZoogiArena::Neon: return ZMHex(TEXT("3A2BC9"));
		case EZoogiArena::Training: return ZMHex(TEXT("9FD8A0"));
		default: return FLinearColor::White;
		}
	}

	FString ZMUpper(const FString& S) { return S.ToUpper(); }
}

// ---------------------------------------------------------------------------------------------------------- lifecycle
TSharedRef<SWidget> UZoogiMenuRoot::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MenuRoot"));
		WidgetTree->RootWidget = Root;
		if (Stack.Num() == 0) Stack.Add(EZoogiScreen::Home);
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiMenuTour="), TourDir);
		if (!TourDir.IsEmpty()) { IFileManager::Get().MakeDirectory(*TourDir, true); TourStep = 0; }
		if (UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this))
			UnlockHandle = U->OnUnlocksChanged.AddUObject(this, &UZoogiMenuRoot::OnUnlocksChanged);
		PowerSel = ZoogiPowerIds::Void2TitanMeteor;
		UE_LOG(LogZoogi, Log, TEXT("MENU root created (UZoogiMenuRoot)"));
	}
	return Super::RebuildWidget();
}

void UZoogiMenuRoot::NativeDestruct()
{
	if (UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this)) U->OnUnlocksChanged.Remove(UnlockHandle);
	Super::NativeDestruct();
}

AZoogiGameMode* UZoogiMenuRoot::GM() const { return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr; }

void UZoogiMenuRoot::Fire(FName Action, int32 Index)
{
	if (AZoogiGameMode* G = GM())
	{
		UE_LOG(LogZoogi, Log, TEXT("MENU action %s %d"), *Action.ToString(), Index);
		G->MenuAction(Action, Index);
	}
	bDirty = true;
}

void UZoogiMenuRoot::MarkDirty()
{
	Stack.Reset();
	Stack.Add(EZoogiScreen::Home);
	bModal = false;
	bDirty = true;
}

void UZoogiMenuRoot::Push(EZoogiScreen S)
{
	if (Top() == S) return;
	Stack.Add(S);
	UE_LOG(LogZoogi, Log, TEXT("MENU push %s (depth %d)"), ZMScreenName(S), Stack.Num());
	bDirty = true;
}

void UZoogiMenuRoot::Pop()
{
	if (Stack.Num() > 1) Stack.Pop();
	UE_LOG(LogZoogi, Log, TEXT("MENU back -> %s"), ZMScreenName(Top()));
	bDirty = true;
}

void UZoogiMenuRoot::PopToHome()
{
	Stack.SetNum(1);
	bDirty = true;
}

void UZoogiMenuRoot::HandleBack()
{
	if (bModal) { bModal = false; bDirty = true; return; }
	if (bAnimeView)
	{
		if (UZoogiAnimeSubsystem* A = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) A->StopMenuMovie();
		bAnimeView = false; bDirty = true; SetBackdropPlaying(true); return;
	}
	if (Stack.Num() > 1) { Pop(); return; }
	bModal = true; bModalQuit = true; ModalTitle = TEXT("QUIT ZOOGI ROLL?"); ModalBody = TEXT("See you next round!");
	bDirty = true;
}

void UZoogiMenuRoot::ShowComingSoon(const FString& What)
{
	bModal = true; bModalQuit = false;
	ModalTitle = TEXT("COMING SOON");
	ModalBody = What;
	bDirty = true;
}

void UZoogiMenuRoot::Refresh(float DeltaSeconds)
{
	Time += DeltaSeconds;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Android_Back)) HandleBack();
	}
	TickTour(DeltaSeconds);
	FVector2D VP(2400.f, 1080.f);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(VP);
	const AZoogiGameMode* G = GM();
	if (bDirty || (G && G->MenuRevision != BuiltRevision) || !VP.Equals(BuiltViewport, 1.f))
	{
		BuiltViewport = VP;
		if (G) BuiltRevision = G->MenuRevision;
		bDirty = false;
		Rebuild();
	}
}

// ---------------------------------------------------------------------------------------------------------- art
UTexture2D* UZoogiMenuRoot::Art(const FString& Name)
{
	if (Name.IsEmpty()) return nullptr;
	if (TObjectPtr<UTexture2D>* Hit = ArtCache.Find(Name)) return Hit->Get();
	UTexture2D* T = nullptr;
	// Menu art: Content/Data/MenuArt (game art from the approved mockups); card art: Content/Data/CardArt.
	const FString Dirs[] = { TEXT("Data/MenuArt"), TEXT("Data/CardArt") };
	const FString Exts[] = { TEXT(".png"), TEXT(".jpg") };
	for (const FString& D : Dirs)
		for (const FString& E : Exts)
		{
			const FString P = FPaths::ProjectContentDir() / D / (Name + E);
			if (!T && FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
		}
	if (!T) UE_LOG(LogZoogi, Warning, TEXT("MENU art missing: %s"), *Name);
	ArtCache.Add(Name, T);
	return T;
}

FString UZoogiMenuRoot::PortraitFor(FName ZoogiId) const
{
	const FString S = ZoogiId.ToString();
	if (S == TEXT("orin")) return TEXT("master_orin");
	if (S == TEXT("darkzoogy")) return TEXT("dark_zoogy");
	return S;
}

// ---------------------------------------------------------------------------------------------------------- helpers
UWidget* UZoogiMenuRoot::Place(UWidget* W, float X, float Y, float Wd, float Ht, EAnchor A)
{
	if (!W || !Canvas) return W;
	float DX = X * ZMK;
	if (A == EAnchor::Center) DX = X * ZMK - 1200.f + DesignW * 0.5f;
	else if (A == EAnchor::Right) DX = X * ZMK - 2400.f + DesignW;
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W))
	{
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetPosition(FVector2D(DX, Y * ZMK));
		S->SetSize(FVector2D(Wd * ZMK, Ht * ZMK));
	}
	return W;
}

UTextBlock* UZoogiMenuRoot::Txt(const FString& S, int32 Size, const FLinearColor& C, float Outline, bool bWrap)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(S));
	T->SetFont(ZoogiUIStyle::BoldFont(Size, Outline));
	T->SetColorAndOpacity(FSlateColor(C));
	T->SetShadowOffset(FVector2D(0.f, Outline > 0.f ? 3.f : 0.f));
	T->SetShadowColorAndOpacity(ZoogiUIStyle::Outline.CopyWithNewOpacity(0.8f));
	T->SetAutoWrapText(bWrap);
	return T;
}

UWidget* UZoogiMenuRoot::Label(const FString& S, float X, float Y, float Wd, float Ht, int32 Size, const FLinearColor& C, EAnchor A, int32 Justify, bool bWrap)
{
	UTextBlock* T = Txt(S, Size, C, 3.f, bWrap);
	T->SetJustification(Justify == 1 ? ETextJustify::Center : Justify == 2 ? ETextJustify::Right : ETextJustify::Left);
	T->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Place(T, X, Y, Wd, Ht, A);
}

UWidget* UZoogiMenuRoot::Panel(float X, float Y, float Wd, float Ht, const FLinearColor& Fill, EAnchor A)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(ZoogiUIStyle::RoundBrush(Fill, ZoogiUIStyle::Outline, 5.f, 26.f));
	B->SetPadding(FMargin(10.f, 8.f));
	B->SetVerticalAlignment(VAlign_Top);
	UBorder* Sheen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Sheen->SetBrush(ZoogiUIStyle::RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f), FLinearColor::Transparent, 0.f, 20.f));
	USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SB->SetHeightOverride(FMath::Min(40.f, Ht * ZMK * 0.12f));
	SB->AddChild(Sheen);
	B->SetContent(SB);
	B->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Place(B, X, Y, Wd, Ht, A);
}

static void ZMCropBrush(FSlateBrush& B, UTexture2D* Tex, float W, float H)
{
	if (!Tex || W <= 0.f || H <= 0.f) return;
	const float TA = (float)Tex->GetSizeX() / FMath::Max(1, Tex->GetSizeY());
	const float SA = W / H;
	if (SA > TA) { const float V = TA / SA; B.SetUVRegion(FBox2f(FVector2f(0.f, (1.f - V) * 0.5f), FVector2f(1.f, (1.f + V) * 0.5f))); }
	else { const float U = SA / TA; B.SetUVRegion(FBox2f(FVector2f((1.f - U) * 0.5f, 0.f), FVector2f((1.f + U) * 0.5f, 1.f))); }
}

static FSlateBrush ZMImgBrush(UTexture2D* Tex, float W, float H, float Radius)
{
	FSlateBrush B;
	B.SetResourceObject(Tex);
	B.ImageSize = FVector2D(W, H);
	if (Radius > 0.f)
	{
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(FLinearColor::Transparent), 0.f);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	}
	ZMCropBrush(B, Tex, W, H);
	return B;
}

UWidget* UZoogiMenuRoot::Image(UTexture2D* Tex, float X, float Y, float Wd, float Ht, const FLinearColor& Tint, EAnchor A, float Radius)
{
	if (!Tex) return nullptr;
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	I->SetBrush(ZMImgBrush(Tex, Wd * ZMK, Ht * ZMK, Radius));
	I->SetColorAndOpacity(Tint);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Place(I, X, Y, Wd, Ht, A);
}

UWidget* UZoogiMenuRoot::Bar(float X, float Y, float Wd, float Ht, float Frac, const FLinearColor& A, const FLinearColor& B)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* Track = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Track->SetBrush(ZoogiUIStyle::RoundBrush(ZoogiUIStyle::Track, ZoogiUIStyle::Outline, 3.f, Ht * ZMK * 0.5f));
	if (UOverlaySlot* S = O->AddChildToOverlay(Track)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (Frac > 0.001f)
	{
		USizeBox* FillBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FillBox->SetWidthOverride(FMath::Max(Ht * ZMK, (Wd * ZMK - 6.f) * FMath::Clamp(Frac, 0.f, 1.f)));
		UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Fill->SetBrush(ZoogiUIStyle::RoundBrush(FMath::Lerp(A, B, 0.5f), FLinearColor::Transparent, 0.f, Ht * ZMK * 0.5f));
		FillBox->AddChild(Fill);
		if (UOverlaySlot* S = O->AddChildToOverlay(FillBox)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(3.f)); }
	}
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Place(O, X, Y, Wd, Ht);
}

UZoogiMenuRootClick* UZoogiMenuRoot::BindClick(UButton* B, TFunction<void()> Fn)
{
	if (!Fn) return nullptr;
	UZoogiMenuRootClick* C = NewObject<UZoogiMenuRootClick>(this);
	C->Fn = MoveTemp(Fn);
	Clicks.Add(C);
	B->OnClicked.AddDynamic(C, &UZoogiMenuRootClick::Click);
	return C;
}

UWidget* UZoogiMenuRoot::Btn(const FString& Label, const FLinearColor& FillIn, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, int32 FontSize,
	bool bSelected, bool bDisabled, EAnchor A, const FString& Icon, const FString& Sub)
{
	const FLinearColor Fill = bDisabled ? ZMCDisabled : FillIn;
	const float R = FMath::Min(26.f, Ht * ZMK * 0.3f);
	UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	FButtonStyle St;
	const FLinearColor Edge = bSelected ? ZMCCyan : ZoogiUIStyle::Outline;
	const float EdgeW = bSelected ? 7.f : 5.f;
	St.SetNormal(ZoogiUIStyle::RoundBrush(Fill, Edge, EdgeW, R));
	St.SetHovered(ZoogiUIStyle::RoundBrush(ZMLighten(Fill, bDisabled ? 0.f : 0.12f), Edge, EdgeW, R));
	St.SetPressed(ZoogiUIStyle::RoundBrush(ZMDarken(Fill, 0.2f), FLinearColor::White, EdgeW, R));
	St.SetDisabled(ZoogiUIStyle::RoundBrush(ZMCDisabled, ZoogiUIStyle::Outline, 5.f, R));
	St.SetNormalPadding(FMargin(8.f, 6.f));
	St.SetPressedPadding(FMargin(8.f, 10.f, 8.f, 2.f));
	B->SetStyle(St);
	B->SetClickMethod(EButtonClickMethod::PreciseClick);
	B->SetTouchMethod(EButtonTouchMethod::PreciseTap);

	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// gloss strip (the shared look: white gloss along the top)
	UBorder* Gloss = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Gloss->SetBrush(ZoogiUIStyle::RoundBrush(FLinearColor(1.f, 1.f, 1.f, bDisabled ? 0.06f : 0.22f), FLinearColor::Transparent, 0.f, R * 0.7f));
	USizeBox* GB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GB->SetHeightOverride(FMath::Min(34.f, Ht * ZMK * 0.22f));
	GB->AddChild(Gloss);
	if (UOverlaySlot* S = O->AddChildToOverlay(GB)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(10.f, 0.f)); }

	const FLinearColor TC = bDisabled ? ZoogiUIStyle::MutedText : FLinearColor::White;
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UTexture2D* IT = Icon.IsEmpty() ? nullptr : Art(Icon))
	{
		const float IS = FontSize * 1.5f;
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZMImgBrush(IT, IS, IS, 0.f));
		if (UHorizontalBoxSlot* HS = H->AddChildToHorizontalBox(I)) { HS->SetVerticalAlignment(VAlign_Center); HS->SetPadding(FMargin(0.f, 0.f, Label.IsEmpty() ? 0.f : 14.f, 0.f)); }
	}
	if (!Label.IsEmpty())
	{
		UTextBlock* T = Txt(Label, FontSize, TC, 3.f);
		T->SetJustification(ETextJustify::Center);
		if (UHorizontalBoxSlot* HS = H->AddChildToHorizontalBox(T)) HS->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(H)) VS->SetHorizontalAlignment(HAlign_Center);
	if (!Sub.IsEmpty())
	{
		UTextBlock* T = Txt(Sub, FMath::Max(16, FontSize / 2), TC.CopyWithNewOpacity(0.85f), 2.f);
		if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(T)) VS->SetHorizontalAlignment(HAlign_Center);
	}
	if (UOverlaySlot* S = O->AddChildToOverlay(V)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	B->AddChild(O);
	if (UButtonSlot* S = Cast<UButtonSlot>(O->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	BindClick(B, MoveTemp(Fn));
	return Place(B, X, Y, Wd, Ht, A);
}

UWidget* UZoogiMenuRoot::ImgTile(UTexture2D* Tex, const FString& Caption, TFunction<void()> Fn, float X, float Y, float Wd, float Ht, bool bSelected, bool bLocked,
	const FLinearColor& Frame, bool bCaptionBottomLeft, int32 CaptionSize, const FLinearColor& ImgTint)
{
	UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	const FLinearColor Edge = bSelected ? ZMCCyan : ZoogiUIStyle::Outline;
	const float EdgeW = bSelected ? 8.f : 5.f;
	FButtonStyle St;
	St.SetNormal(ZoogiUIStyle::RoundBrush(Frame, Edge, EdgeW, 20.f));
	St.SetHovered(ZoogiUIStyle::RoundBrush(ZMLighten(Frame, 0.15f), Edge, EdgeW, 20.f));
	St.SetPressed(ZoogiUIStyle::RoundBrush(ZMDarken(Frame, 0.2f), FLinearColor::White, EdgeW, 20.f));
	St.SetNormalPadding(FMargin(EdgeW + 1.f));
	St.SetPressedPadding(FMargin(EdgeW + 1.f, EdgeW + 4.f, EdgeW + 1.f, EdgeW - 2.f));
	B->SetStyle(St);
	B->SetClickMethod(EButtonClickMethod::PreciseClick);
	B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (Tex)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZMImgBrush(Tex, Wd * ZMK - 18.f, Ht * ZMK - 18.f, 14.f));
		I->SetColorAndOpacity(bLocked ? ImgTint * FLinearColor(0.38f, 0.40f, 0.48f, 1.f) : ImgTint);
		if (UOverlaySlot* S = O->AddChildToOverlay(I)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	}
	if (bLocked)
	{
		UImage* L = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const float LS = FMath::Min(Wd, Ht) * ZMK * 0.42f;
		L->SetBrush(ZMImgBrush(Art(TEXT("icon_lock")), LS, LS, 0.f));
		if (UOverlaySlot* S = O->AddChildToOverlay(L)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
	}
	if (!Caption.IsEmpty())
	{
		UTextBlock* T = Txt(Caption, CaptionSize, FLinearColor::White, 3.f);
		if (UOverlaySlot* S = O->AddChildToOverlay(T))
		{
			S->SetHorizontalAlignment(bCaptionBottomLeft ? HAlign_Left : HAlign_Center);
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(12.f, 0.f, 8.f, 8.f));
		}
	}
	B->AddChild(O);
	if (UButtonSlot* S = Cast<UButtonSlot>(O->Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	BindClick(B, MoveTemp(Fn));
	return Place(B, X, Y, Wd, Ht);
}

UWidget* UZoogiMenuRoot::Tabs(const TArray<FString>& Names, int32& Current, float X, float Y, float TabW, float Gap, bool bVertical)
{
	UWidget* Last = nullptr;
	int32* Cur = &Current;
	for (int32 i = 0; i < Names.Num(); ++i)
	{
		const bool bSel = Current == i;
		const float TX = bVertical ? X : X + i * (TabW + Gap);
		const float TY = bVertical ? Y + i * (52.f + Gap) : Y;
		Last = Btn(Names[i], bSel ? (bVertical ? ZMCViolet : ZMCTeal) : ZMCNavy, [this, Cur, i]() { *Cur = i; bDirty = true; }, TX, TY, TabW, 52.f, 28, bSel);
	}
	return Last;
}

// ---------------------------------------------------------------------------------------------------------- build
void UZoogiMenuRoot::Rebuild()
{
	if (!Root || !WidgetTree) return;
	Root->ClearChildren();
	Clicks.Reset();
	Canvas = nullptr;
	DesignW = FMath::Max(1440.f, 1080.f * BuiltViewport.X / FMath::Max(1.f, BuiltViewport.Y));

	// Full-bleed backdrop: game art per screen (or the optional menu video), with a navy wash.
	FString Bg = TEXT("frozen_background");
	switch (Top())
	{
	case EZoogiScreen::Powers: case EZoogiScreen::Story: Bg = TEXT("cosmos_background"); break;
	case EZoogiScreen::Collection: case EZoogiScreen::Create: case EZoogiScreen::Social: Bg = TEXT("meadows_background"); break;
	case EZoogiScreen::Shop: Bg = TEXT("lava_background"); break;
	default: break;
	}
	// Evan 9:28 AM CT: HOME uses the same opaque Frozen art backdrop as PLAY (covers the live menu-map preview).
	const bool bLiveMap = false;
	UObject* BgRes = (EnsureBackdrop() && BackdropTexture) ? (UObject*)BackdropTexture.Get() : bLiveMap ? nullptr : (UObject*)Art(Bg);
	if (BgRes)
	{
		UScaleBox* Fill = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		Fill->SetStretch(EStretch::ScaleToFill);
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B;
		B.SetResourceObject(BgRes);
		UTexture2D* T2 = Cast<UTexture2D>(BgRes);
		B.ImageSize = T2 ? FVector2D(T2->GetSizeX(), T2->GetSizeY()) : FVector2D(1920.f, 1080.f);
		I->SetBrush(B);
		Fill->AddChild(I);
		Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* S = Root->AddChildToOverlay(Fill)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	}
	UBorder* Wash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Wash->SetBrushColor(FLinearColor(0.02f, 0.08f, 0.35f, bLiveMap ? 0.25f : 0.55f));
	Wash->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* S = Root->AddChildToOverlay(Wash)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

	// Safe zone (notches) -> fit a DesignW x 1080 canvas.
	USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass());
	if (UOverlaySlot* S = Root->AddChildToOverlay(Safe)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Fit->SetStretch(EStretch::ScaleToFit);
	Safe->AddChild(Fit);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(DesignW);
	Size->SetHeightOverride(1080.f);
	Fit->AddChild(Size);
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Size->AddChild(Canvas);

	if (bAnimeView) { BuildAnimeView(); return; }
	switch (Top())
	{
	case EZoogiScreen::Home: BuildHome(); break;
	case EZoogiScreen::Play: BuildPlay(); break;
	case EZoogiScreen::ZoogiSelect: BuildZoogiSelect(); break;
	case EZoogiScreen::Powers: BuildPowers(); break;
	case EZoogiScreen::Settings: BuildSettings(); break;
	case EZoogiScreen::Collection: BuildCollection(); break;
	case EZoogiScreen::Story: BuildStory(); break;
	case EZoogiScreen::Events: BuildEvents(); break;
	case EZoogiScreen::Shop: BuildShop(); break;
	case EZoogiScreen::Social: BuildSocial(); break;
	case EZoogiScreen::Create: BuildCreate(); break;
	}
	if (bModal) BuildModal();
}

void UZoogiMenuRoot::BuildTopBar(const FString& Title)
{
	if (!Title.IsEmpty())
	{
		Btn(FString(), ZMCNavy, [this]() { HandleBack(); }, 55.f, 20.f, 80.f, 52.f, 30, false, false, EAnchor::Left, TEXT("icon_back"));
		Label(Title, 152.f, 22.f, 400.f, 50.f, 64, FLinearColor::White, EAnchor::Left);
	}
	const UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this);
	auto Pill = [this](float X, float W, const TCHAR* Icon, const FString& Val)
	{
		UBorder* P = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		P->SetBrush(ZoogiUIStyle::RoundBrush(ZMHex(TEXT("0B1E5C")).CopyWithNewOpacity(0.92f), ZoogiUIStyle::Outline, 4.f, 40.f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(P, X, 25.f, W, 34.f, EAnchor::Right);
		Image(Art(Icon), X + 6.f, 30.f, 24.f, 24.f, FLinearColor::White, EAnchor::Right);
		Label(Val, X + 34.f, 31.f, W - 40.f, 26.f, 30, FLinearColor::White, EAnchor::Right);
	};
	Pill(670.f, 90.f, TEXT("icon_coin"), TEXT("12,480")); // placeholder coins
	Pill(812.f, 70.f, TEXT("icon_gem"), FString::FromInt(U ? U->GetGems() : 340)); // placeholder gems
	Btn(FString(), ZMCNavy, [this]() { if (Top() != EZoogiScreen::Settings) Push(EZoogiScreen::Settings); }, 898.f, 20.f, 72.f, 52.f, 30, false, false, EAnchor::Right, TEXT("icon_gear"));
}

// ---- HOME
void UZoogiMenuRoot::BuildHome()
{
	AZoogiGameMode* G = GM();
	if (!G || G->MenuPlayers.Num() == 0) return;
	BuildTopBar(FString());
	Image(Art(TEXT("logo_t")), 118.f, 30.f, 120.f, 100.f);

	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	const FZoogiDef& D = Roster[G->MenuPlayers[0].ZoogiIndex % Roster.Num()];
	{
		UBorder* Glow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Glow->SetBrush(ZoogiUIStyle::RoundBrush(FLinearColor(0.75f, 0.92f, 1.f, 0.35f), FLinearColor(1.f, 1.f, 1.f, 0.9f), 10.f, 260.f));
		Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Glow, 428.f, 78.f, 220.f, 220.f);
		Image(Art(PortraitFor(D.Id)), 446.f, 96.f, 184.f, 184.f, FLinearColor::White, EAnchor::Center, 210.f);
	}
	Btn(FString(), ZMCNavy, [this]() { Fire("ZoogiPrev", 0); }, 274.f, 188.f, 52.f, 62.f, 30, false, false, EAnchor::Center, TEXT("icon_back"));
	Btn(FString(), ZMCNavy, [this]() { Fire("ZoogiNext", 0); }, 700.f, 188.f, 52.f, 62.f, 30, false, false, EAnchor::Center, TEXT("icon_play"));
	{
		Panel(436.f, 346.f, 212.f, 42.f, ZMPViolet);
		Label(ZMUpper(D.Name), 446.f, 354.f, 90.f, 30.f, 40, FLinearColor::White);
		const UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this);
		const FName Eq = U ? U->GetEquipped(D.Id) : NAME_None;
		const FZoogiPowerDef* PD = U && !Eq.IsNone() ? U->FindPower(Eq) : nullptr;
		const FString Chip = PD ? FString::Printf(TEXT("%s %d - EQUIPPED"), *PD->Type.ToString().ToUpper(), PD->Tier) : ZMUpper(D.Ability);
		UBorder* C = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		C->SetBrush(ZoogiUIStyle::RoundBrush(ZMHex(TEXT("2A1680")), FLinearColor::Transparent, 0.f, 20.f));
		C->SetHorizontalAlignment(HAlign_Center); C->SetVerticalAlignment(VAlign_Center);
		C->SetContent(Txt(Chip, 18, ZMHex(TEXT("BDF4FF")), 2.f));
		C->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(C, 520.f, 357.f, 120.f, 22.f);
	}
	Btn(TEXT("STORY"), ZMCTeal, [this]() { Push(EZoogiScreen::Story); }, 108.f, 202.f, 150.f, 52.f, 36);
	Btn(TEXT("POWERS"), ZMCViolet, [this]() { Push(EZoogiScreen::Powers); }, 108.f, 262.f, 150.f, 52.f, 36);
	Btn(TEXT("COLLECTION"), ZMCNavy, [this]() { Push(EZoogiScreen::Collection); }, 108.f, 322.f, 150.f, 52.f, 34);
	Panel(106.f, 386.f, 494.f, 64.f, ZMPNavy);
	const TCHAR* Dock[] = { TEXT("EVENTS"), TEXT("SHOP"), TEXT("SOCIAL"), TEXT("CREATE"), TEXT("RADIO") };
	const EZoogiScreen DockS[] = { EZoogiScreen::Events, EZoogiScreen::Shop, EZoogiScreen::Social, EZoogiScreen::Create, EZoogiScreen::Home };
	for (int32 i = 0; i < 5; ++i)
	{
		const EZoogiScreen S = DockS[i];
		Btn(Dock[i], ZMCNavy, [this, S]() { if (S == EZoogiScreen::Home) ShowComingSoon(TEXT("Radio: the weekly radio player isn't in the Unreal build yet.")); else Push(S); },
			114.f + i * 97.f, 392.f, 91.f, 52.f, 26);
	}
	Panel(768.f, 86.f, 152.f, 106.f, ZMPTeal);
	Label(TEXT("DAILY BONUS"), 778.f, 94.f, 140.f, 16.f, 24, FLinearColor::White);
	Label(TEXT("Day 4 streak - claim 250"), 778.f, 112.f, 140.f, 16.f, 20, ZMTSoft);
	Btn(TEXT("CLAIM"), ZMCGold, [this]() { ShowComingSoon(TEXT("Daily bonus needs the progression backend.")); }, 778.f, 134.f, 132.f, 52.f, 30);
	Panel(768.f, 205.f, 152.f, 88.f, ZMPNavy);
	Label(TEXT("BATTLE PASS"), 778.f, 213.f, 140.f, 16.f, 24, FLinearColor::White);
	Label(TEXT("Tier 12 / 40"), 778.f, 231.f, 140.f, 16.f, 22, ZMTSoft);
	Bar(778.f, 250.f, 132.f, 9.f, 12.f / 40.f, ZoogiUIStyle::EnergyA, ZoogiUIStyle::CardEnergyA);
	Label(TEXT("Next: Void trail skin"), 778.f, 266.f, 140.f, 16.f, 18, ZMTSoft);
	Btn(TEXT("PLAY"), ZMCGold, [this]() { Push(EZoogiScreen::Play); }, 726.f, 320.f, 194.f, 120.f, 72, false, false, EAnchor::Center, TEXT("icon_play"));
	const FString Last = FString::Printf(TEXT("Last: %s - %s - %s"), G->bComboMode ? TEXT("2v2 Combo") : TEXT("Classic"),
		*ZoogiUtil::ArenaName(G->MenuArena), CurrentPlayMode() == 1 ? TEXT("local") : TEXT("vs CPU"));
	Label(Last, 604.f, 400.f, 118.f, 44.f, 18, ZMTSoft, EAnchor::Center, 2, true);
}

// ---- PLAY setup
int32 UZoogiMenuRoot::CurrentPlayMode() const
{
	const AZoogiGameMode* G = GM();
	if (!G) return 0;
	if (G->bComboMode) return 3;
	int32 Humans = 0;
	for (int32 i = 0; i < G->MenuNumPlayers && G->MenuPlayers.IsValidIndex(i); ++i) Humans += G->MenuPlayers[i].bCPU ? 0 : 1;
	return Humans >= 2 ? 1 : 0;
}

void UZoogiMenuRoot::ApplyPlayMode(int32 Mode)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	if (Mode == 2) { ShowComingSoon(TEXT("Practice (free roll) isn't in the Unreal build yet.")); return; }
	if (Mode == 4) { ShowComingSoon(TEXT("ONLINE - Ringer Royale needs a server. Coming soon!")); return; }
	if (Mode == 3) { if (!G->bComboMode) Fire("Mode", 1); return; }
	if (G->bComboMode) Fire("Mode", 0);
	// CLASSIC: P1 human, the rest CPU. LOCAL: P1 + P2 human (2-4 on one device). Via the existing ToggleCPU action.
	if (G->MenuPlayers.IsValidIndex(0) && G->MenuPlayers[0].bCPU) Fire("ToggleCPU", 0);
	for (int32 i = 1; i < G->MenuPlayers.Num(); ++i)
	{
		const bool bWantCPU = (Mode == 0) || (i >= 2 && G->MenuPlayers[i].bCPU);
		if (G->MenuPlayers[i].bCPU != bWantCPU) Fire("ToggleCPU", i);
	}
}

void UZoogiMenuRoot::CycleSlot(int32 i)
{
	AZoogiGameMode* G = GM();
	if (!G || !G->MenuPlayers.IsValidIndex(i)) return;
	// HUMAN -> CPU -> OFF -> HUMAN (P1/P2 are always on; 2v2 combo is always 4 players).
	const bool bOn = i < G->MenuNumPlayers;
	if (!bOn) { Fire("Players", i + 1); if (G->MenuPlayers[i].bCPU) Fire("ToggleCPU", i); return; }
	if (!G->MenuPlayers[i].bCPU) { Fire("ToggleCPU", i); return; }
	if (i >= 2 && !G->bComboMode) { Fire("Players", i); return; }
	Fire("ToggleCPU", i);
}

void UZoogiMenuRoot::BuildPlay()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	BuildTopBar(TEXT("PLAY"));
	Panel(106.f, 86.f, 240.f, 296.f, ZMPNavy);
	Label(TEXT("1 - MODE"), 118.f, 94.f, 200.f, 14.f, 22, ZMTLabel);
	const TCHAR* Modes[] = { TEXT("CLASSIC"), TEXT("LOCAL"), TEXT("PRACTICE"), TEXT("2v2 COMBO"), TEXT("ONLINE") };
	const TCHAR* Subs[] = { TEXT("vs CPU"), TEXT("2-4 on one device"), TEXT("coming soon"), TEXT("teams"), TEXT("coming soon") };
	const FLinearColor MC[] = { ZMCGold, ZMCTeal, ZMCNavy, ZMCViolet, ZMCNavy };
	const int32 Cur = CurrentPlayMode();
	for (int32 m = 0; m < 5; ++m)
	{
		const float Y = 110.f + m * 54.f;
		Btn(FString(), MC[m], [this, m]() { ApplyPlayMode(m); }, 118.f, Y, 216.f, 52.f, 30, Cur == m, m == 2 || m == 4);
		Label(Modes[m], 132.f, Y + 14.f, 120.f, 24.f, 34, FLinearColor::White);
		Label(Subs[m], 236.f, Y + 19.f, 90.f, 18.f, 17, FLinearColor::White, EAnchor::Center, 2);
	}
	// 2 - arena: all 8 arenas, 4 x 2 (the 4 with art match the mockup)
	Panel(358.f, 86.f, 308.f, 182.f, ZMPTeal);
	Label(TEXT("2 - ARENA"), 370.f, 94.f, 200.f, 14.f, 22, ZMTLabel);
	for (int32 a = 0; a < (int32)EZoogiArena::MAX && a < 8; ++a)
	{
		const float X = 368.f + (a % 4) * 73.f, Y = 110.f + (a / 4) * 78.f;
		const FString A = ZMArenaArt(a);
		static const TCHAR* Short[] = { TEXT("MEADOW"), TEXT("FROZEN RING"), TEXT("VOLCANIC"), TEXT("COSMIC"), TEXT("ARABIAN"), TEXT("TOMB"), TEXT("NIGHT CIRCUIT"), TEXT("TRAINING") };
		const FString Name = a < 8 ? FString(Short[a]) : ZoogiUtil::ArenaName((EZoogiArena)a).ToUpper();
		ImgTile(A.IsEmpty() ? nullptr : Art(A), Name, [this, a]() { Fire("Arena", a); }, X, Y, 70.f, 72.f, G->MenuArena == (EZoogiArena)a, false,
			A.IsEmpty() ? ZMArenaTint(a) : ZMHex(TEXT("0B1E5C")), true, 16, A.IsEmpty() ? FLinearColor::White : ZMArenaTint(a));
	}
	Panel(358.f, 276.f, 308.f, 106.f, ZMPNavy);
	Label(TEXT("ROUNDS & RULES"), 372.f, 284.f, 200.f, 14.f, 24, ZMTLabel);
	Btn(TEXT("-"), ZMCNavy, [this, G]() { Fire("Rounds", G->MenuRounds - 1); }, 370.f, 308.f, 52.f, 52.f, 44);
	Label(FString::FromInt(G->MenuRounds), 422.f, 318.f, 40.f, 36.f, 52, FLinearColor::White, EAnchor::Center, 1);
	Btn(TEXT("+"), ZMCNavy, [this, G]() { Fire("Rounds", G->MenuRounds + 1); }, 462.f, 308.f, 52.f, 52.f, 44);
	Btn(TEXT("CPU: NORMAL"), ZMCTeal, [this]() { ShowComingSoon(TEXT("CPU difficulty: the CPU has one level today (NORMAL).")); }, 526.f, 308.f, 130.f, 52.f, 28);
	// 3 - zoogis (P1-P4): card = pick a Zoogi, tag = HUMAN / CPU / OFF
	Panel(678.f, 86.f, 244.f, 252.f, ZMPViolet);
	Label(TEXT("3 - ZOOGIS"), 690.f, 94.f, 200.f, 14.f, 22, ZMTLabel);
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	for (int32 i = 0; i < 4 && i < G->MenuPlayers.Num(); ++i)
	{
		const float X = 688.f + (i % 2) * 116.f, Y = 110.f + (i / 2) * 112.f;
		const bool bOn = i < G->MenuNumPlayers;
		const FZoogiDef& D = Roster[G->MenuPlayers[i].ZoogiIndex % Roster.Num()];
		ImgTile(Art(PortraitFor(D.Id)), FString(), [this, i, bOn]() { if (bOn) { SelectSlot = i; PendingZoogi = -1; Push(EZoogiScreen::ZoogiSelect); } else CycleSlot(i); },
			X, Y, 108.f, 56.f, false, false, bOn ? ZMLighten(ZoogiUtil::PlayerColor(i), 0.35f) : ZMCDisabled, true, 20, bOn ? FLinearColor::White : FLinearColor(0.25f, 0.28f, 0.42f, 1.f));
		const FString Tag = FString::Printf(TEXT("P%d - %s"), i + 1, !bOn ? TEXT("OFF") : G->MenuPlayers[i].bCPU ? TEXT("CPU") : TEXT("HUMAN"));
		Btn(Tag, !bOn ? ZMCDisabled : G->MenuPlayers[i].bCPU ? ZMCNavy : ZMCTeal, [this, i]() { CycleSlot(i); }, X, Y + 54.f, 108.f, 52.f, 26, false, !bOn);
	}
	Label(TEXT("Tap a card to pick a Zoogi - tap the tag to cycle HUMAN / CPU / OFF"), 682.f, 340.f, 238.f, 22.f, 16, FLinearColor::White, EAnchor::Center, 0, true);
	Btn(TEXT("POWERS"), ZMCNavy, [this]() { Push(EZoogiScreen::Powers); }, 582.f, 388.f, 126.f, 52.f, 30);
	Btn(TEXT("START"), ZMCGold, [this]() { UE_LOG(LogZoogi, Log, TEXT("MENU START pressed (PLAY setup)")); Fire("Start", 0); }, 726.f, 362.f, 194.f, 80.f, 64, false, false, EAnchor::Center, TEXT("icon_play"));
}

// ---- ZOOGI select
void UZoogiMenuRoot::BuildZoogiSelect()
{
	AZoogiGameMode* G = GM();
	if (!G || !G->MenuPlayers.IsValidIndex(SelectSlot)) return;
	BuildTopBar(TEXT("CHOOSE ZOOGI"));
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	const int32 CurIdx = G->MenuPlayers[SelectSlot].ZoogiIndex % Roster.Num();
	if (PendingZoogi < 0) PendingZoogi = CurIdx;
	Panel(106.f, 86.f, 478.f, 354.f, ZMPNavy);
	Label(FString::Printf(TEXT("ROSTER - P%d"), SelectSlot + 1), 118.f, 94.f, 200.f, 14.f, 22, ZMTLabel);
	for (int32 r = 0; r < Roster.Num(); ++r)
	{
		const float X = 118.f + (r % 4) * 115.f, Y = 112.f + (r / 4) * 107.f;
		ImgTile(Art(PortraitFor(Roster[r].Id)), ZMUpper(Roster[r].Name), [this, r]() { PendingZoogi = r; bDirty = true; }, X, Y, 106.f, 98.f, r == PendingZoogi, false, FLinearColor::White, true, 22);
	}
	const FZoogiDef& D = Roster[PendingZoogi % Roster.Num()];
	Panel(596.f, 86.f, 326.f, 236.f, ZMPViolet);
	Label(ZMUpper(D.Name), 610.f, 98.f, 190.f, 36.f, 56, FLinearColor::White);
	Label(FString::Printf(TEXT("%s - mass %.2f"), *D.Type, D.MassRatio()), 610.f, 136.f, 190.f, 16.f, 22, ZMTSoft);
	const TCHAR* SN[] = { TEXT("SPD"), TEXT("PWR"), TEXT("DEF"), TEXT("CTL") };
	const int32 SV[] = { D.Speed, D.Power, D.Defense, D.Control };
	for (int32 s = 0; s < 4; ++s)
	{
		Label(SN[s], 612.f, 155.f + s * 23.f, 40.f, 16.f, 22, ZMTLabel);
		Bar(656.f, 157.f + s * 23.f, 134.f, 10.f, SV[s] / 100.f, ZoogiUIStyle::EnergyA, ZoogiUIStyle::CardEnergyA);
	}
	ImgTile(Art(PortraitFor(D.Id)), FString(), nullptr, 805.f, 98.f, 104.f, 94.f, false, false, FLinearColor::White);
	const UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this);
	const FName Eq = U ? U->GetEquipped(D.Id) : NAME_None;
	const FZoogiPowerDef* PD = U && !Eq.IsNone() ? U->FindPower(Eq) : nullptr;
	Btn(FString(), ZMCNavy, [this]() { Push(EZoogiScreen::Powers); }, 610.f, 258.f, 300.f, 54.f, 24);
	if (PD) Image(Art(PD->CardArt), 620.f, 263.f, 26.f, 44.f);
	Label(PD ? FString::Printf(TEXT("POWER: %s %d - %s"), *PD->Type.ToString().ToUpper(), PD->Tier, *PD->Name.ToString().ToUpper()) : FString::Printf(TEXT("POWER: %s"), *ZMUpper(D.Ability)),
		652.f, 266.f, 250.f, 18.f, 24, FLinearColor::White);
	Label(PD ? FString(TEXT("Equipped - change in POWERS")) : D.AbilityDesc, 652.f, 285.f, 250.f, 24.f, 16, ZMTSoft, EAnchor::Center, 0, true);
	Btn(TEXT("SKINS"), ZMCNavy, [this]() { ShowComingSoon(TEXT("Skins arrive with the shop backend.")); }, 598.f, 338.f, 114.f, 52.f, 30);
	Btn(TEXT("CONFIRM"), ZMCGold, [this, CurIdx]()
	{
		const int32 N = ZoogiUtil::Roster().Num();
		const int32 Fwd = (PendingZoogi - CurIdx + N) % N;
		// existing ZoogiNext / ZoogiPrev actions, shortest way round
		if (Fwd <= N / 2) for (int32 k = 0; k < Fwd; ++k) Fire("ZoogiNext", SelectSlot);
		else for (int32 k = 0; k < N - Fwd; ++k) Fire("ZoogiPrev", SelectSlot);
		PendingZoogi = -1;
		Pop();
	}, 726.f, 340.f, 194.f, 100.f, 64);
}

// ---- POWERS
void UZoogiMenuRoot::BuildPowers()
{
	AZoogiGameMode* G = GM();
	UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this);
	if (!G || !U || G->MenuPlayers.Num() == 0) return;
	BuildTopBar(TEXT("POWERS"));
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	const FZoogiDef& Z = Roster[G->MenuPlayers[0].ZoogiIndex % Roster.Num()];
	Panel(106.f, 86.f, 410.f, 354.f, ZMPNavy);
	Label(TEXT("ALL POWERS"), 118.f, 100.f, 160.f, 14.f, 24, ZMTLabel);
	TArray<FString> F = { TEXT("VOID"), TEXT("ALL TYPES") };
	Tabs(F, PowerFilter, 304.f, 90.f, 100.f, 6.f);
	TArray<const FZoogiPowerDef*> Shown;
	for (const FZoogiPowerDef& D : U->GetAllPowers()) if (PowerFilter == 1 || D.Type == "Void") Shown.Add(&D);
	if (PowerFilter == 0) for (const FZoogiPowerDef& D : U->GetAllPowers()) if (D.Type != "Void") Shown.Add(&D); // void first, then the rest
	for (int32 i = 0; i < Shown.Num() && i < 6; ++i)
	{
		const FZoogiPowerDef* D = Shown[i];
		const FName Id = D->PowerId;
		const float X = 118.f + (i % 3) * 132.f, Y = 148.f + (i / 3) * 145.f;
		ImgTile(Art(D->CardArt), FString(), [this, Id]() { PowerSel = Id; bDirty = true; }, X, Y, 124.f, 140.f, PowerSel == Id, !U->IsUnlocked(Id), ZMHex(TEXT("1A1A2E")), false, 20);
		if (U->GetEquipped(Z.Id) == Id)
		{
			UBorder* C = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			C->SetBrush(ZoogiUIStyle::RoundBrush(ZMCTeal, ZoogiUIStyle::Outline, 3.f, 14.f));
			C->SetHorizontalAlignment(HAlign_Center); C->SetVerticalAlignment(VAlign_Center);
			C->SetContent(Txt(TEXT("EQUIPPED"), 16, FLinearColor::White, 2.f));
			C->SetVisibility(ESlateVisibility::HitTestInvisible);
			Place(C, X + 30.f, Y + 8.f, 64.f, 16.f);
		}
	}
	const FZoogiPowerDef* S = U->FindPower(PowerSel);
	if (!S) return;
	const bool bUnlocked = U->IsUnlocked(PowerSel);
	const bool bEquipped = U->GetEquipped(Z.Id) == PowerSel;
	Panel(528.f, 86.f, 394.f, 238.f, ZMPViolet);
	ImgTile(Art(S->CardArt), TEXT("PREVIEW"), [this]() { ShowComingSoon(TEXT("Power preview video: Animations & Powers' visuals land here.")); },
		542.f, 98.f, 172.f, 214.f, false, !bUnlocked, ZMHex(TEXT("15122A")), false, 26);
	Label(FString::Printf(TEXT("%s - TIER %d"), *S->Type.ToString().ToUpper(), S->Tier), 726.f, 100.f, 190.f, 14.f, 22, ZMTLabel);
	Label(S->Name.ToString().ToUpper(), 726.f, 114.f, 190.f, 60.f, 44, FLinearColor::White, EAnchor::Center, 0, true);
	{
		UBorder* C = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		C->SetBrush(ZoogiUIStyle::RoundBrush(bUnlocked ? ZMCTeal : ZMHex(TEXT("2A1680")), ZoogiUIStyle::Outline, 3.f, 16.f));
		C->SetHorizontalAlignment(HAlign_Center); C->SetVerticalAlignment(VAlign_Center);
		C->SetContent(Txt(bEquipped ? TEXT("EQUIPPED") : bUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"), 20, bUnlocked ? FLinearColor::White : ZMCGold, 2.f));
		C->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(C, 726.f, 180.f, 80.f, 20.f);
	}
	Label(S->Desc.ToString(), 726.f, 208.f, 188.f, 80.f, 19, FLinearColor::White, EAnchor::Center, 0, true);
	Panel(528.f, 334.f, 186.f, 56.f, ZMPNavy);
	FText Why;
	const bool bCan = U->CanUnlock(PowerSel, Why);
	if (!bUnlocked)
	{
		const int32 Have = S->RequirementStat.IsNone() ? 0 : U->GetStat(S->RequirementStat);
		Label(FString::Printf(TEXT("Need: %s  (%d / %d)"), *U->RequirementText(PowerSel).ToString(), Have, S->RequirementValue), 538.f, 341.f, 170.f, 22.f, 18, FLinearColor::White, EAnchor::Center, 0, true);
		Bar(538.f, 370.f, 166.f, 9.f, S->RequirementValue > 0 ? float(Have) / S->RequirementValue : 0.f, ZoogiUIStyle::EnergyA, ZoogiUIStyle::CardEnergyA);
	}
	else
	{
		Label(U->IsUnlockAllCheat() ? TEXT("Unlocked (-ZoogiUnlockAll)") : TEXT("Requirement reached"), 538.f, 344.f, 170.f, 18.f, 20, FLinearColor::White);
		Label(FString::Printf(TEXT("Equipped on: %s"), bEquipped ? *ZMUpper(Z.Name) : TEXT("-")), 538.f, 364.f, 170.f, 18.f, 20, FLinearColor::White);
	}
	const FName PId = PowerSel, ZId = Z.Id;
	if (!bUnlocked)
	{
		Btn(TEXT("EQUIP"), ZMCNavy, [this]() { ShowComingSoon(TEXT("Unlock this power first.")); }, 528.f, 396.f, 186.f, 52.f, 30, false, true, EAnchor::Center, TEXT("icon_lock"));
		Btn(TEXT("UNLOCK"), ZMCGold, [this, U, PId]()
		{
			FText W;
			if (!U->Unlock(PId))
			{
				U->CanUnlock(PId, W);
				bModal = true; bModalQuit = false; ModalTitle = TEXT("NOT YET"); ModalBody = W.IsEmpty() ? FString(TEXT("Unlock failed.")) : W.ToString(); bDirty = true;
			}
		}, 726.f, 340.f, 194.f, 100.f, 56, false, !bCan, EAnchor::Center, FString(), S->GemCost > 0 ? FString::Printf(TEXT("%d GEMS"), S->GemCost) : FString());
	}
	else
	{
		Btn(TEXT("EQUIP ON..."), ZMCNavy, [this]() { SelectSlot = 0; PendingZoogi = -1; Push(EZoogiScreen::ZoogiSelect); }, 528.f, 396.f, 186.f, 52.f, 30);
		Btn(bEquipped ? TEXT("EQUIPPED") : TEXT("EQUIP"), ZMCTeal, [U, ZId, PId, bEquipped]() { U->SetEquipped(ZId, bEquipped ? NAME_None : PId); },
			726.f, 340.f, 194.f, 100.f, 56, false, false, EAnchor::Center, bEquipped ? TEXT("icon_check") : FString());
	}
}

// ---- SETTINGS
void UZoogiMenuRoot::SetLocalSetting(const TCHAR* Key, int32 Value)
{
	if (GConfig) { GConfig->SetInt(TEXT("ZoogiRoll.MenuSettings"), Key, Value, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
	bDirty = true;
}

int32 UZoogiMenuRoot::GetLocalSetting(const TCHAR* Key, int32 Default) const
{
	int32 V = Default;
	if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), Key, V, GGameUserSettingsIni);
	return V;
}

void UZoogiMenuRoot::BuildSettings()
{
	BuildTopBar(TEXT("SETTINGS"));
	TArray<FString> T = { TEXT("AUDIO"), TEXT("GRAPHICS"), TEXT("CONTROLS"), TEXT("ACCOUNT"), TEXT("ABOUT") };
	Tabs(T, SettingsTab, 108.f, 84.f, 168.f, 12.f, true);
	Panel(296.f, 86.f, 626.f, 354.f, ZMPNavy);
	auto Slider = [this](int32 Row, const TCHAR* Name, int32 Pct, TFunction<void(int32)> Set)
	{
		const float Y = 100.f + Row * 58.f;
		Label(Name, 310.f, Y + 17.f, 90.f, 20.f, 26, ZMTLabel);
		Btn(TEXT("-"), ZMCNavy, [Set, Pct]() { Set(FMath::Max(0, Pct - 10)); }, 400.f, Y, 52.f, 52.f, 40);
		Bar(462.f, Y + 18.f, 330.f, 16.f, Pct / 100.f, ZoogiUIStyle::EnergyA, ZoogiUIStyle::CardEnergyA);
		Btn(TEXT("+"), ZMCNavy, [Set, Pct]() { Set(FMath::Min(100, Pct + 10)); }, 802.f, Y, 52.f, 52.f, 40);
		Label(FString::Printf(TEXT("%d%%"), Pct), 856.f, Y + 14.f, 58.f, 24.f, 34, FLinearColor::White, EAnchor::Center, 2);
	};
	auto Toggle = [this](float X, float Y, const TCHAR* Name, const TCHAR* Key, int32 Def)
	{
		const bool bOn = GetLocalSetting(Key, Def) != 0;
		Label(Name, X, Y + 17.f, 136.f, 20.f, 24, ZMTLabel);
		const FString K2 = Key;
		Btn(bOn ? TEXT("ON") : TEXT("OFF"), bOn ? ZMCTeal : ZMCDisabled, [this, K2, bOn]() { SetLocalSetting(*K2, bOn ? 0 : 1); }, X + 138.f, Y, 72.f, 52.f, 28);
	};
	if (SettingsTab == 0)
	{
		Slider(0, TEXT("MASTER"), GetLocalSetting(TEXT("Master"), 80), [this](int32 V) { SetLocalSetting(TEXT("Master"), V); });
		Slider(1, TEXT("MUSIC"), GetLocalSetting(TEXT("Music"), 60), [this](int32 V) { SetLocalSetting(TEXT("Music"), V); });
		Slider(2, TEXT("SFX"), FMath::RoundToInt(ZoogiUtil::GetSfxVolume() * 100.f), [this](int32 V) { Fire("SfxVol", V); }); // existing action
		Slider(3, TEXT("VOICE"), GetLocalSetting(TEXT("Voice"), 50), [this](int32 V) { SetLocalSetting(TEXT("Voice"), V); });
		Toggle(310.f, 332.f, TEXT("MENU RADIO"), TEXT("MenuRadio"), 1);
		Toggle(620.f, 332.f, TEXT("HAPTICS"), TEXT("Haptics"), 1);
		Toggle(310.f, 386.f, TEXT("MUTE ALL"), TEXT("MuteAll"), 0);
		Btn(TEXT("OPEN RADIO"), ZMCNavy, [this]() { ShowComingSoon(TEXT("The radio player isn't in the Unreal build yet.")); }, 622.f, 382.f, 254.f, 52.f, 30);
	}
	else if (SettingsTab == 1)
	{
		UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		const int32 Q = S ? S->GetOverallScalabilityLevel() : 3;
		Label(TEXT("QUALITY"), 310.f, 120.f, 120.f, 20.f, 26, ZMTLabel);
		const TCHAR* QN[] = { TEXT("LOW"), TEXT("MED"), TEXT("HIGH"), TEXT("EPIC") };
		for (int32 q = 0; q < 4; ++q) Btn(QN[q], q == Q ? ZMCTeal : ZMCNavy, [this, S, q]() { if (S) { S->SetOverallScalabilityLevel(q); S->ApplySettings(false); } bDirty = true; }, 440.f + q * 110.f, 104.f, 100.f, 52.f, 28, q == Q);
		const float Fps = S ? S->GetFrameRateLimit() : 0.f;
		Label(TEXT("FRAME RATE"), 310.f, 178.f, 120.f, 20.f, 26, ZMTLabel);
		for (int32 f = 0; f < 2; ++f)
		{
			const int32 V = f == 0 ? 30 : 60;
			Btn(FString::Printf(TEXT("%d FPS"), V), FMath::IsNearlyEqual(Fps, (float)V) ? ZMCTeal : ZMCNavy, [this, S, V]() { if (S) { S->SetFrameRateLimit((float)V); S->ApplySettings(false); } bDirty = true; },
				440.f + f * 160.f, 162.f, 150.f, 52.f, 28);
		}
		Toggle(310.f, 230.f, TEXT("CAMERA SHAKE"), TEXT("CameraShake"), 1);
	}
	else if (SettingsTab == 2)
	{
		Toggle(310.f, 104.f, TEXT("SWIPE AIM"), TEXT("SwipeAim"), 1);
		Toggle(620.f, 104.f, TEXT("LEFT-HAND"), TEXT("LeftHand"), 0);
		const int32 Sc = GetLocalSetting(TEXT("ButtonScale"), 100);
		Label(TEXT("BUTTON SIZE"), 310.f, 178.f, 150.f, 20.f, 26, ZMTLabel);
		for (int32 b = 0; b < 3; ++b)
		{
			const int32 V = 90 + b * 15;
			Btn(FString::Printf(TEXT("%d%%"), V), V == Sc ? ZMCTeal : ZMCNavy, [this, V]() { SetLocalSetting(TEXT("ButtonScale"), V); }, 470.f + b * 110.f, 162.f, 100.f, 52.f, 28, V == Sc);
		}
		Btn(TEXT("REPLAY TUTORIAL"), ZMCViolet, [this]() { ShowComingSoon(TEXT("The controls tutorial isn't in the Unreal build yet.")); }, 310.f, 240.f, 260.f, 52.f, 30);
		Label(TEXT("Tap an enemy or orb to lock on - Tab: cycle - SPACE: fire - E / Q: power - Esc / Android back: back"), 310.f, 310.f, 590.f, 40.f, 20, ZMTSoft, EAnchor::Center, 0, true);
	}
	else if (SettingsTab == 3)
	{
		Label(TEXT("Accounts, cloud saves and profiles need the online backend."), 310.f, 110.f, 590.f, 40.f, 26, FLinearColor::White, EAnchor::Center, 0, true);
		Btn(TEXT("SIGN IN"), ZMCViolet, [this]() { ShowComingSoon(TEXT("Sign in needs the online backend.")); }, 310.f, 170.f, 220.f, 52.f, 32);
	}
	else
	{
		Label(TEXT("ZOOGI ROLL"), 310.f, 100.f, 400.f, 30.f, 48, FLinearColor::White);
		Label(TEXT("Unreal Engine 5.6 build - local"), 310.f, 136.f, 400.f, 20.f, 22, ZMTSoft);
		Label(TEXT("CREDITS"), 310.f, 166.f, 400.f, 20.f, 28, ZMTLabel);
		Label(TEXT("Created by Evan James - game design, characters and art direction.\nZoogi Roll team: Gameplay & UI, Game Camera, Animations & Powers, the arena builders, Comic, Playtester.\nEngine: Unreal Engine by Epic Games."),
			310.f, 190.f, 600.f, 150.f, 20, FLinearColor::White, EAnchor::Center, 0, true);
		Btn(TEXT("QUIT GAME"), ZMCRed, [this]() { bModal = true; bModalQuit = true; ModalTitle = TEXT("QUIT ZOOGI ROLL?"); ModalBody = TEXT("See you next round!"); bDirty = true; }, 700.f, 380.f, 210.f, 52.f, 30);
	}
}

// ---- COLLECTION
void UZoogiMenuRoot::BuildCollection()
{
	BuildTopBar(TEXT("COLLECTION"));
	TArray<FString> T = { TEXT("ZOOGIPEDIA"), TEXT("MY STUFF"), TEXT("SKINS"), TEXT("GALLERY") };
	Tabs(T, CollectionTab, 106.f, 82.f, 120.f, 8.f);
	Panel(106.f, 140.f, 492.f, 300.f, ZMPNavy);
	const int32 N = UE_ARRAY_COUNT(ZMGCollection);
	if (CollectionTab == 0)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float X = 116.f + (i % 6) * 80.f, Y = 148.f + (i / 6) * 96.f;
			ImgTile(Art(ZMGCollection[i].Art), ZMGCollection[i].Label, [this, i]() { CollectionSel = i; bDirty = true; }, X, Y, 74.f, 90.f, CollectionSel == i, false, FLinearColor::White, true, 15);
		}
	}
	else Label(CollectionTab == 1 ? TEXT("MY STUFF - owned Zoogis and items: coming soon") : CollectionTab == 2 ? TEXT("SKINS / CUSTOMIZE - coming soon") : TEXT("GALLERY - community gallery (online): coming soon"),
		126.f, 260.f, 452.f, 60.f, 32, FLinearColor::White, EAnchor::Center, 1, true);
	const ZMFArtEntry& E = ZMGCollection[FMath::Clamp(CollectionSel, 0, N - 1)];
	Panel(610.f, 86.f, 312.f, 240.f, ZMPTeal);
	Label(E.Label, 624.f, 98.f, 170.f, 36.f, 50, FLinearColor::White);
	const FZoogiDef* D = nullptr;
	if (E.RosterId) for (const FZoogiDef& R : ZoogiUtil::Roster()) if (R.Id == FName(E.RosterId)) D = &R;
	Label(D ? FString::Printf(TEXT("%s - owned"), *D->Type) : FString(TEXT("Classic look")), 624.f, 136.f, 170.f, 30.f, 20, FLinearColor::White, EAnchor::Center, 0, true);
	Label(D ? FString::Printf(TEXT("%s: %s"), *D->Ability, *D->AbilityDesc) : FString(TEXT("Lore, stats, powers, skins owned and best scores. Customize opens the skin / trail editor.")),
		624.f, 168.f, 170.f, 84.f, 18, FLinearColor::White, EAnchor::Center, 0, true);
	if (D) Label(FString::Printf(TEXT("SPD %d - PWR %d - DEF %d - CTL %d"), D->Speed, D->Power, D->Defense, D->Control), 624.f, 270.f, 290.f, 18.f, 20, FLinearColor::White);
	ImgTile(Art(E.Art), FString(), nullptr, 798.f, 98.f, 110.f, 100.f, false, false, FLinearColor::White);
	Btn(TEXT("CREATE"), ZMCViolet, [this]() { Push(EZoogiScreen::Create); }, 610.f, 388.f, 104.f, 52.f, 30);
	Btn(TEXT("CUSTOMIZE"), ZMCGold, [this]() { ShowComingSoon(TEXT("The skin / trail editor isn't in the Unreal build yet.")); }, 726.f, 340.f, 194.f, 100.f, 50);
}

// ---- STORY
void UZoogiMenuRoot::BuildStory()
{
	BuildTopBar(TEXT("STORY"));
	struct FEp { const TCHAR* Bg; const TCHAR* Who; const TCHAR* Title; bool bLocked; };
	const FEp Eps[] = {
		{ TEXT("frozen_background"), TEXT("luma"), TEXT("THE FROZEN RING"), false }, { TEXT("lava_background"), TEXT("vex"), TEXT("EMBER RISING"), false },
		{ TEXT("meadows_background"), TEXT("tavi"), TEXT("STORM SEASON"), true }, { TEXT("cosmos_background"), TEXT("dark_zoogy"), TEXT("VOID CALLING"), true } };
	for (int32 e = 0; e < 4; ++e)
	{
		const float X = 106.f + e * 204.f;
		ImgTile(Art(Eps[e].Bg), FString(), [this, e]() { StorySel = e; bDirty = true; }, X, 94.f, 194.f, 258.f, StorySel == e, false, ZMHex(TEXT("0B1E5C")), true, 30,
			Eps[e].bLocked ? FLinearColor(0.45f, 0.45f, 0.55f, 1.f) : FLinearColor::White);
		Image(Art(Eps[e].Who), X + 22.f, 140.f, 150.f, 150.f, Eps[e].bLocked ? FLinearColor(0.4f, 0.4f, 0.45f, 1.f) : FLinearColor::White, EAnchor::Center, 60.f);
		if (Eps[e].bLocked) Image(Art(TEXT("icon_lock")), X + 72.f, 170.f, 50.f, 50.f);
		Label(FString::Printf(TEXT("EP %d"), e + 1), X + 14.f, 102.f, 60.f, 16.f, 22, ZMCCyan);
		Label(Eps[e].Title, X + 12.f, 312.f, 180.f, 30.f, 32, FLinearColor::White);
	}
	Btn(TEXT("WATCH ANIME"), ZMCNavy, [this]()
	{
		if (UZoogiAnimeSubsystem* A = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>())
		{
			AnimeClip = A->PlayMenuMovie(1);
			bAnimeView = !AnimeClip.IsEmpty();
			SetBackdropPlaying(!bAnimeView);
		}
		if (!bAnimeView) ShowComingSoon(TEXT("No anime clips found (Content/Anime/Movies)."));
		bDirty = true;
	}, 106.f, 368.f, 150.f, 52.f, 28);
	Btn(TEXT("3D COMIC"), ZMCNavy, [this]() { ShowComingSoon(TEXT("The 3D comic reader isn't in the Unreal build yet.")); }, 266.f, 368.f, 142.f, 52.f, 28);
	const bool bLocked = StorySel >= 2;
	Btn(TEXT("READ"), ZMCGold, [this, bLocked]() { ShowComingSoon(bLocked ? TEXT("Finish the earlier episodes to unlock this one.") : TEXT("Episode reader: coming soon. WATCH ANIME plays the clips today.")); },
		726.f, 340.f, 194.f, 100.f, 64, false, bLocked, EAnchor::Center, TEXT("icon_play"));
}

// ---- EVENTS
void UZoogiMenuRoot::BuildEvents()
{
	BuildTopBar(TEXT("EVENTS"));
	TArray<FString> T = { TEXT("CHALLENGES"), TEXT("BATTLE PASS"), TEXT("DAILY"), TEXT("TOURNEYS") };
	Tabs(T, EventsTab, 106.f, 82.f, 128.f, 8.f);
	Panel(106.f, 140.f, 600.f, 300.f, ZMPNavy);
	if (EventsTab == 0)
	{
		const TCHAR* CN[] = { TEXT("Win 3 Classic matches"), TEXT("Knock 20 orbs off the edge"), TEXT("Land a Titan Meteor"), TEXT("Play a Local match") };
		const float CP[] = { 1.f, 0.65f, 0.f, 0.4f };
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = 148.f + i * 72.f;
			Panel(116.f, Y, 580.f, 66.f, i == 0 ? ZMPTeal : ZMHex(TEXT("163591")));
			Label(CN[i], 130.f, Y + 9.f, 400.f, 18.f, 26, FLinearColor::White);
			Bar(130.f, Y + 40.f, 380.f, 10.f, CP[i], ZoogiUIStyle::EnergyA, ZoogiUIStyle::CardEnergyA);
			if (CP[i] >= 1.f) Btn(TEXT("CLAIM"), ZMCGold, [this]() { ShowComingSoon(TEXT("Rewards need the progression backend.")); }, 560.f, Y + 7.f, 126.f, 52.f, 28);
			else Btn(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(CP[i] * 100.f)), ZMCDisabled, nullptr, 560.f, Y + 7.f, 126.f, 52.f, 28, false, true);
		}
	}
	else Label(EventsTab == 1 ? TEXT("BATTLE PASS - tiers and rewards: coming soon") : EventsTab == 2 ? TEXT("DAILY BONUS - login streak: coming soon") : TEXT("TOURNEYS - online tournaments: coming soon"),
		126.f, 260.f, 560.f, 60.f, 32, FLinearColor::White, EAnchor::Center, 1, true);
	Panel(718.f, 86.f, 204.f, 236.f, ZMPViolet);
	Label(TEXT("WEEKLY"), 730.f, 96.f, 180.f, 16.f, 24, FLinearColor::White);
	Label(TEXT("Ends in 3d 4h"), 730.f, 114.f, 180.f, 16.f, 20, ZMTSoft);
	Image(Art(TEXT("heavy_roll")), 776.f, 140.f, 88.f, 146.f, FLinearColor::White, EAnchor::Center, 10.f);
	Btn(TEXT("CLAIM ALL"), ZMCGold, [this]() { ShowComingSoon(TEXT("Rewards need the progression backend.")); }, 726.f, 340.f, 194.f, 100.f, 50);
}

// ---- SHOP
void UZoogiMenuRoot::BuildShop()
{
	BuildTopBar(TEXT("SHOP"));
	TArray<FString> T = { TEXT("FEATURED"), TEXT("ZOOGIS"), TEXT("SKINS"), TEXT("GEMS") };
	Tabs(T, ShopTab, 106.f, 82.f, 120.f, 8.f);
	const TCHAR* IArt[] = { TEXT("sera"), TEXT("tavi"), TEXT("pinpoint"), TEXT("nightshade") };
	const TCHAR* IName[] = { TEXT("SERA"), TEXT("TAVI"), TEXT("PINPOINT"), TEXT("NIGHTSHADE") };
	const TCHAR* IIcon[] = { TEXT("icon_gem"), TEXT("icon_coin"), TEXT("icon_gem"), TEXT("icon_gem") };
	const TCHAR* IPrice[] = { TEXT("450"), TEXT("8,000"), TEXT("300"), TEXT("600") };
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = 106.f + i * 152.f;
		ImgTile(nullptr, FString(), [this, i]() { ShopSel = i; bDirty = true; }, X, 146.f, 144.f, 230.f, ShopSel == i, false, i == 2 ? ZMPTeal : ZMPNavy);
		Image(Art(IArt[i]), X + 14.f, 158.f, 116.f, 106.f, FLinearColor::White, EAnchor::Center, 14.f);
		Label(IName[i], X + 8.f, 272.f, 128.f, 20.f, 28, FLinearColor::White, EAnchor::Center, 1);
		Btn(IPrice[i], ZMCGold, [this, i]() { ShopSel = i; ShowComingSoon(TEXT("Purchases need the economy backend (coins and gems are placeholders).")); }, X + 12.f, 308.f, 120.f, 52.f, 28, false, false, EAnchor::Center, IIcon[i]);
	}
	ImgTile(nullptr, FString(), [this]() { ShopSel = 4; bDirty = true; }, 714.f, 146.f, 206.f, 176.f, ShopSel == 4, false, ZMPViolet);
	Label(TEXT("BUNDLE"), 728.f, 156.f, 100.f, 16.f, 24, FLinearColor::White);
	Image(Art(TEXT("lars")), 770.f, 180.f, 96.f, 90.f, FLinearColor::White, EAnchor::Center, 48.f);
	Btn(TEXT("BUY"), ZMCGold, [this]() { ShowComingSoon(TEXT("Purchases need the economy backend (coins and gems are placeholders).")); }, 726.f, 340.f, 194.f, 100.f, 60, false, false, EAnchor::Center, TEXT("icon_gem"));
}

// ---- SOCIAL
void UZoogiMenuRoot::BuildSocial()
{
	BuildTopBar(TEXT("SOCIAL"));
	TArray<FString> T = { TEXT("PROFILE"), TEXT("FRIENDS"), TEXT("SCORES"), TEXT("CLANS"), TEXT("REPLAYS") };
	Tabs(T, SocialTab, 106.f, 82.f, 112.f, 8.f);
	Panel(106.f, 146.f, 292.f, 294.f, ZMPNavy);
	ImgTile(Art(TEXT("luma")), FString(), nullptr, 118.f, 156.f, 90.f, 82.f, false, false, FLinearColor::White);
	Label(TEXT("EVAN"), 120.f, 246.f, 260.f, 30.f, 48, FLinearColor::White);
	Label(TEXT("Games 126 - Wins 71 - Orbs 960 - KOs 204\nHigh score 3,420 - Streak 4 days  (sample)"), 120.f, 282.f, 270.f, 44.f, 18, ZMTSoft, EAnchor::Center, 0, true);
	Btn(TEXT("SIGN IN"), ZMCViolet, [this]() { ShowComingSoon(TEXT("Sign in needs the online backend.")); }, 116.f, 380.f, 132.f, 52.f, 28);
	Btn(TEXT("CHAT"), ZMCNavy, [this]() { ShowComingSoon(TEXT("Chat needs the online backend.")); }, 258.f, 380.f, 130.f, 52.f, 28);
	Panel(410.f, 146.f, 512.f, 176.f, ZMPTeal);
	if (SocialTab == 0 || SocialTab == 2)
	{
		Label(TEXT("LEADERBOARD (sample - online coming soon)"), 424.f, 156.f, 480.f, 16.f, 22, FLinearColor::White);
		const TCHAR* Names[] = { TEXT("#1  Zap_Kid"), TEXT("#2  EVAN"), TEXT("#3  RollQueen"), TEXT("#4  OrbGoblin") };
		const TCHAR* Sc[] = { TEXT("5,120"), TEXT("3,420"), TEXT("3,300"), TEXT("2,980") };
		for (int32 i = 0; i < 4; ++i)
		{
			Label(Names[i], 424.f, 182.f + i * 32.f, 300.f, 22.f, 26, FLinearColor::White);
			Label(Sc[i], 800.f, 182.f + i * 32.f, 110.f, 22.f, 26, FLinearColor::White, EAnchor::Center, 2);
		}
	}
	else Label(SocialTab == 1 ? TEXT("FRIENDS - coming soon (online)") : SocialTab == 3 ? TEXT("CLANS - coming soon (online)") : TEXT("REPLAYS - coming soon"),
		424.f, 220.f, 480.f, 40.f, 32, FLinearColor::White, EAnchor::Center, 1);
	Btn(TEXT("ADD FRIEND"), ZMCGold, [this]() { ShowComingSoon(TEXT("Friends need the online backend.")); }, 726.f, 340.f, 194.f, 100.f, 46);
}

// ---- CREATE
void UZoogiMenuRoot::BuildCreate()
{
	BuildTopBar(TEXT("CREATE"));
	const TCHAR* TN[] = { TEXT("CREATE ZOOGI"), TEXT("MAP EDITOR"), TEXT("ARENA SHOWCASE"), TEXT("ARENA EDITOR") };
	const TCHAR* TS[] = { TEXT("Design a custom Zoogi"), TEXT("Place walls, zones, models"), TEXT("Fly-through of every arena"), TEXT("Edit layout presets") };
	const FLinearColor TC[] = { ZMCViolet, ZMCTeal, ZMCNavy, ZMCNavy };
	const bool TDev[] = { false, true, false, true };
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = 106.f + i * 204.f;
		const bool bHidden = TDev[i] && !ZMDevFlag();
		ImgTile(nullptr, FString(), [this, i]() { CreateSel = i; bDirty = true; }, X, 100.f, 194.f, 226.f, CreateSel == i, bHidden, bHidden ? ZMCDisabled : TC[i]);
		Label(TN[i], X + 10.f, 250.f, 174.f, 26.f, 32, FLinearColor::White, EAnchor::Center, 1);
		Label(bHidden ? TEXT("Developer tool (-ZoogiDev)") : TS[i], X + 10.f, 286.f, 174.f, 20.f, 19, ZMTSoft, EAnchor::Center, 1);
	}
	Label(TEXT("Developer tools (map / arena editor) are hidden behind the -ZoogiDev flag."), 106.f, 340.f, 600.f, 20.f, 20, ZMTSoft);
	const int32 Sel = CreateSel;
	const bool bHiddenSel = TDev[Sel] && !ZMDevFlag();
	Btn(TEXT("OPEN"), ZMCGold, [this, Sel, bHiddenSel]()
	{
		if (bHiddenSel) ShowComingSoon(TEXT("This editor is a developer tool. Run with -ZoogiDev."));
		else ShowComingSoon(Sel == 0 ? TEXT("Create Zoogi isn't in the Unreal build yet.") : Sel == 2 ? TEXT("Arena showcase: coming soon to the menu.") : TEXT("The in-game editor isn't in the Unreal build yet (dev)."));
	}, 726.f, 340.f, 194.f, 100.f, 64, false, bHiddenSel);
}

// ---- modal + anime
void UZoogiMenuRoot::BuildModal()
{
	UButton* Scrim = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	FButtonStyle St;
	FSlateBrush Dim;
	Dim.TintColor = FSlateColor(FLinearColor(0.f, 0.01f, 0.06f, 0.65f));
	St.SetNormal(Dim); St.SetHovered(Dim); St.SetPressed(Dim);
	Scrim->SetStyle(St);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Scrim)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(-400.f)); }
	Panel(312.f, 120.f, 400.f, 220.f, ZMPNavy);
	Label(ModalTitle, 330.f, 140.f, 364.f, 40.f, 56, FLinearColor::White, EAnchor::Center, 1);
	Label(ModalBody, 334.f, 192.f, 356.f, 70.f, 26, ZMTSoft, EAnchor::Center, 1, true);
	if (bModalQuit)
	{
		Btn(TEXT("CANCEL"), ZMCNavy, [this]() { bModal = false; bDirty = true; }, 334.f, 270.f, 170.f, 54.f, 34);
		Btn(TEXT("QUIT"), ZMCRed, [this]() { Fire("Quit", 0); }, 520.f, 270.f, 170.f, 54.f, 34);
	}
	else Btn(TEXT("OK"), ZMCGold, [this]() { bModal = false; bDirty = true; }, 427.f, 270.f, 170.f, 54.f, 40);
}

void UZoogiMenuRoot::BuildAnimeView()
{
	UBorder* Black = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Black->SetBrushColor(FLinearColor::Black);
	if (UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Black)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(-400.f)); }
	UZoogiAnimeSubsystem* A = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>();
	if (A && A->MovieTexture)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B;
		B.SetResourceObject(A->MovieTexture);
		B.ImageSize = FVector2D(1280.f, 720.f);
		Img->SetBrush(B);
		Place(Img, 182.f, 20.f, 660.f, 371.f);
	}
	auto Step = [this](int32 Dir)
	{
		if (UZoogiAnimeSubsystem* AS = GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>()) { AnimeClip = AS->PlayMenuMovie(Dir); bAnimeView = !AnimeClip.IsEmpty(); }
		bDirty = true;
	};
	Btn(TEXT("PREV"), ZMCNavy, [Step]() { Step(-1); }, 300.f, 400.f, 120.f, 52.f, 30);
	Label(AnimeClip, 430.f, 414.f, 164.f, 20.f, 18, FLinearColor::White, EAnchor::Center, 1);
	Btn(TEXT("NEXT"), ZMCNavy, [Step]() { Step(1); }, 604.f, 400.f, 120.f, 52.f, 30);
	Btn(TEXT("BACK"), ZMCGold, [this]() { HandleBack(); }, 55.f, 20.f, 120.f, 52.f, 30, false, false, EAnchor::Left);
}

// ---------------------------------------------------------------------------------------------------------- tour
void UZoogiMenuRoot::TickTour(float Dt)
{
	if (TourStep < 0) return;
	TourClock += Dt;
	const float StepTime = 2.5f;
	// Step n: set the screen up at T, capture it 1.5 s later (after the rebuild has rendered).
	const float T0 = 5.f + TourStep * StepTime;
	if (TourClock < T0) return;
	UZoogiPowerUnlockSubsystem* U = UZoogiPowerUnlockSubsystem::Get(this);
	const bool bUnlockedRun = U && U->IsUnlockAllCheat();
	const FString Tag = bUnlockedRun ? TEXT("_unlockall") : TEXT("");
	static const TCHAR* Names[] = { TEXT("01_home"), TEXT("02_play_setup"), TEXT("03_zoogi_select"), TEXT("04_powers"), TEXT("05_powers_after_equip"), TEXT("06_settings"), TEXT("06b_settings_about"),
		TEXT("07_collection"), TEXT("08_story_comics"), TEXT("09_events"), TEXT("10_shop"), TEXT("11_social"), TEXT("12_create"), TEXT("13_home_back_quit_modal"), TEXT("14_play_before_start"), TEXT("16_home_after_tour") };
	const int32 NumShots = UE_ARRAY_COUNT(Names);
	if (bTourSetupDone)
	{
		if (TourClock < T0 + 1.5f) return;
		if (TourStep < NumShots)
		{
			const FString P = FPaths::Combine(TourDir, FString::Printf(TEXT("menu_%s%s.png"), Names[TourStep], *Tag));
			FScreenshotRequest::RequestScreenshot(P, true, false);
			UE_LOG(LogZoogi, Log, TEXT("MENU TOUR screenshot %s (screen %s, modal %d)"), *P, ZMScreenName(Top()), bModal ? 1 : 0);
		}
		bTourSetupDone = false;
		++TourStep;
		return;
	}
	bTourSetupDone = true;
	switch (TourStep)
	{
	case 0: PopToHome(); break;
	case 1: Push(EZoogiScreen::Play); break;
	case 2: SelectSlot = 0; PendingZoogi = -1; Push(EZoogiScreen::ZoogiSelect); break;
	case 3: PopToHome(); PowerSel = ZoogiPowerIds::Void2TitanMeteor; Push(EZoogiScreen::Powers); break;
	case 4:
		if (U && GM() && GM()->MenuPlayers.Num() > 0)
		{
			const FZoogiDef& Z = ZoogiUtil::Roster()[GM()->MenuPlayers[0].ZoogiIndex % ZoogiUtil::Roster().Num()];
			FText W;
			const bool bCan = U->CanUnlock(ZoogiPowerIds::Void2TitanMeteor, W);
			// unlocked run: equip Titan Meteor on P1's Zoogi (SetEquipped); locked run: SetEquipped must refuse. Never Unlock() here (no gem spend).
			const bool bOk = U->SetEquipped(Z.Id, ZoogiPowerIds::Void2TitanMeteor);
			UE_LOG(LogZoogi, Log, TEXT("MENU TOUR titan: IsUnlocked=%d CanUnlock=%d (%s) SetEquipped(%s)=%d GetEquipped=%s Req='%s'"),
				U->IsUnlocked(ZoogiPowerIds::Void2TitanMeteor) ? 1 : 0, bCan ? 1 : 0, *W.ToString(), *Z.Id.ToString(), bOk ? 1 : 0,
				*U->GetEquipped(Z.Id).ToString(), *U->RequirementText(ZoogiPowerIds::Void2TitanMeteor).ToString());
		}
		bDirty = true; break;
	case 5: PopToHome(); SettingsTab = 0; Push(EZoogiScreen::Settings); break;
	case 6: SettingsTab = 4; bDirty = true; break;
	case 7: PopToHome(); Push(EZoogiScreen::Collection); break;
	case 8: PopToHome(); Push(EZoogiScreen::Story); break;
	case 9: PopToHome(); Push(EZoogiScreen::Events); break;
	case 10: PopToHome(); Push(EZoogiScreen::Shop); break;
	case 11: PopToHome(); Push(EZoogiScreen::Social); break;
	case 12: PopToHome(); Push(EZoogiScreen::Create); break;
	case 13: HandleBack(); HandleBack(); break; // BACK -> HOME, BACK on HOME -> quit modal
	case 14: HandleBack(); Push(EZoogiScreen::Play); break; // close the modal, open PLAY
	case 15: PopToHome(); break;
	case 16:
		bTourSetupDone = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiMenuTourStart")))
		{
			Push(EZoogiScreen::Play);
			UE_LOG(LogZoogi, Log, TEXT("MENU TOUR: pressing START on PLAY setup"));
			Fire("Start", 0);
			const FString P = FPaths::Combine(TourDir, FString::Printf(TEXT("menu_15_match_started%s.png"), *Tag));
			FTimerHandle H;
			GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [P]()
			{
				FScreenshotRequest::RequestScreenshot(P, true, false);
				UE_LOG(LogZoogi, Log, TEXT("MENU TOUR screenshot %s"), *P);
			}), 12.f, false);
		}
		UE_LOG(LogZoogi, Log, TEXT("MENU TOUR done"));
		TourStep = -100;
		return;
	}
}

// ---------------------------------------------------------------------------------------------------------- backdrop video (ported from UZoogiMenuWidget)
bool UZoogiMenuRoot::EnsureBackdrop()
{
	if (BackdropState != 0) return BackdropState > 0;
	BackdropState = -1;
	bool bVideo = false;
	if (GConfig) GConfig->GetBool(TEXT("ZoogiRoll.Menu"), TEXT("bMenuVideo"), bVideo, GGameIni);
	if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiMenuVideo"))) bVideo = true;
	if (!bVideo) return false;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anime/Movies/Menu_Background.mp4"));
	if (!FPaths::FileExists(Path)) return false;
	BackdropPlayer = NewObject<UMediaPlayer>(this);
	BackdropPlayer->PlayOnOpen = true;
	BackdropPlayer->SetLooping(true);
	BackdropTexture = NewObject<UMediaTexture>(this);
	BackdropTexture->AutoClear = true;
	BackdropTexture->SetMediaPlayer(BackdropPlayer);
	BackdropTexture->UpdateResource();
	BackdropSource = NewObject<UFileMediaSource>(this);
	BackdropSource->SetFilePath(Path);
	const bool bOk = BackdropPlayer->OpenSource(BackdropSource);
	BackdropState = bOk ? 1 : -1;
	return bOk;
}

void UZoogiMenuRoot::SetBackdropPlaying(bool bPlay)
{
	if (!BackdropPlayer || BackdropState <= 0) return;
	if (bPlay && !BackdropPlayer->IsPlaying()) BackdropPlayer->Play();
	else if (!bPlay && BackdropPlayer->IsPlaying()) BackdropPlayer->Pause();
}
