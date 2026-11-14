// arena-hud: see ZoogiArenaHUDWidget.h
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiRedirect.h"
#include "ZoogiRaid.h"
#include "ZoogiSkillLab.h"
#include "ZoogiStory.h"
#include "ZoogiFramedLayout.h"
#include "ZoogiHaptics.h"
#include "ZoogiPhoneControls.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiArtifacts.h"
#include "ZoogiAnimeSubsystem.h"
#include "MediaTexture.h"
#include "ZoogiComicSliceOverlay.h"
#include "ZoogiTypes.h"
#include "Misc/ScopeExit.h"
#include "ZoogiBall.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiGameMode.h"
#include "ZoogiDevMode.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"
#include "ZoogiSkills.h"
#include "ZoogiScoringUI.h"
#include "ZoogiMenuRoot.h"   // UZoogiMenuRootClick (lambda click forwarder)
#include "ZoogiMenuWidget.h" // UZoogiButton (never takes keyboard focus)
#include "ZoogiRoll.h"
#include "ZoogiUIStyle.h"
#include "ZoogiActionFun.h"
#include "ZoogiP5Style.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"
#include "ZoogiP5Command.h"

namespace ZoogiArenaHUD
{
	static float GDetReveal = 0.f, GDetBottom = 0.f;
	float DetailsReveal(float& OutBottomPx) { OutBottomPx = GDetBottom; return GDetReveal; }
	static double GTopBannerUntil = 0.0;
	void NoteTopBanner() { GTopBannerUntil = FPlatformTime::Seconds() + 0.12; }
	bool TopBannerOn() { return FPlatformTime::Seconds() < GTopBannerUntil; }
	bool IsEnabled()
	{
		static const bool b = []()
		{
			FString Style = TEXT("Arena");
			if (GConfig) GConfig->GetString(TEXT("ZoogiRoll.UI"), TEXT("HudStyle"), Style, GGameIni);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiHudStyle="), Style);
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiFramedHUD"))) Style = TEXT("Framed");
			return Style.Equals(TEXT("Arena"), ESearchCase::IgnoreCase) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoHUD"));
		}();
		return b;
	}

	static float GBandTop = 0.f, GBandBottom = 0.f; static double GBandStamp = -1.0;
	static TWeakObjectPtr<UWidget> GTacPlate;
	static TWeakObjectPtr<UWidget> GAvatar[4];
	static TWeakObjectPtr<UWidget> GMenuPlate; // b79
	static TWeakObjectPtr<UZoogiArenaHUDWidget> GMenuOwner; // b81
	bool PressMenuAt(const FVector2D& Abs)
	{
		const UWidget* W = GMenuPlate.Get();
		UZoogiArenaHUDWidget* O = GMenuOwner.Get();
		if (!W || !O || !W->GetCachedWidget().IsValid()) return false;
		const FGeometry& G = W->GetCachedGeometry();
		const FVector2D P = FVector2D(G.GetAbsolutePosition()), S = FVector2D(G.GetAbsoluteSize());
		const float Pad = 14.f;
		if (S.X < 4.f || Abs.X < P.X - Pad || Abs.Y < P.Y - Pad || Abs.X > P.X + S.X + Pad || Abs.Y > P.Y + S.Y + Pad) return false;
		O->ToggleMenu();
		return true;
	}
	bool OverChrome(const UWorld* World, const FVector2D& ViewportPx)
	{
		UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
		TSharedPtr<SViewport> VW = VC ? VC->GetGameViewportWidget() : nullptr;
		if (!VW.IsValid()) return false;
		const FGeometry& VG = VW->GetCachedGeometry();
		if (VG.GetLocalSize().X <= 0.f) return false;
		const FVector2D Abs = VG.LocalToAbsolute(ViewportPx / FMath::Max(VG.Scale, 0.01f));
		auto In = [&Abs](const UWidget* W, float Pad)
		{
			if (!W || !W->IsVisible() || !W->GetCachedWidget().IsValid() || W->GetRenderOpacity() < 0.05f) return false;
			const FGeometry& G = W->GetCachedGeometry();
			const FVector2D P = FVector2D(G.GetAbsolutePosition()), S = FVector2D(G.GetAbsoluteSize());
			return S.X > 4.f && Abs.X >= P.X - Pad && Abs.Y >= P.Y - Pad && Abs.X <= P.X + S.X + Pad && Abs.Y <= P.Y + S.Y + Pad;
		};
		if (In(GMenuPlate.Get(), 14.f)) return true;
		for (int32 i = 0; i < 4; ++i) if (In(GAvatar[i].Get(), 8.f)) return true;
		return false;
	}
	float TacOpacity()
	{
		const UWidget* W = GTacPlate.Get();
		return W ? FMath::Clamp(W->GetRenderOpacity(), 0.f, 1.f) : 1.f;
	}
	bool AvatarRect(int32 Player, FVector2D& OutAbsPos, FVector2D& OutAbsSize)
	{
		UWidget* W = Player >= 0 && Player < 4 ? GAvatar[Player].Get() : nullptr;
		if (!W || !W->IsVisible() || !W->GetCachedWidget().IsValid()) return false;
		if (const UWidget* Par = W->GetParent()) if (!Par->IsVisible()) return false;
		const FGeometry& G = W->GetCachedGeometry();
		OutAbsPos = FVector2D(G.GetAbsolutePosition()); OutAbsSize = FVector2D(G.GetAbsoluteSize());
		return OutAbsSize.X > 4.f && OutAbsSize.Y > 4.f;
	}
	bool TacRect(FVector2D& OutAbsPos, FVector2D& OutAbsSize)
	{
		UWidget* W = GTacPlate.Get();
		if (!W || !W->IsVisible() || !W->GetCachedWidget().IsValid()) return false;
		const FGeometry& G = W->GetCachedGeometry();
		OutAbsPos = FVector2D(G.GetAbsolutePosition()); OutAbsSize = FVector2D(G.GetAbsoluteSize());
		return OutAbsSize.X > 4.f && OutAbsSize.Y > 4.f;
	}
	void SetBoardBand(float Top, float Bottom) { GBandTop = Top; GBandBottom = Bottom; GBandStamp = FPlatformTime::Seconds(); }
	bool BoardBand(const UWorld* World, float& OutTop, float& OutBottom)
	{
		if (!IsActive(World) || FPlatformTime::Seconds() - GBandStamp > 1.0) return false;
		OutTop = GBandTop; OutBottom = GBandBottom; return true;
	}

	FBox2D AnimeVideoRect(const FVector2D& Viewport)
	{
		const float W = Viewport.X, H = Viewport.Y;
		const bool bPortrait = H >= W;
		const float MH = FMath::Min(W * 0.92f * 9.f / 16.f, H * (bPortrait ? 0.30f : 0.42f));
		const float MW = MH * 16.f / 9.f;
		const float Top = H * 0.025f;
		return FBox2D(FVector2D((W - MW) * 0.5f, Top), FVector2D((W + MW) * 0.5f, Top + MH));
	}

	float BackEase(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		const float C1 = 1.4f, C3 = C1 + 1.f, U = T - 1.f;
		return 1.f + C3 * U * U * U + C1 * U * U;
	}

	float SplitEase(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T < 0.5f ? 4.f * T * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 3.f) * 0.5f;
	}

	bool IsActive(const UWorld* World)
	{
		if (!IsEnabled() || !World) return false;
		const AZoogiGameMode* G = World->GetAuthGameMode<AZoogiGameMode>();
		return G && G->Phase != EZoogiPhase::Menu && G->Balls.Num() > 0 && !G->IsRoyaleMatch() && !ZoogiSkillLab::Open(); // Battle Royale draws its own HUD; b108: so does the lab
	}
}

namespace
{
	// palette on top of the Clash reskin (ZoogiMenuClashStyle.h)
	const FLinearColor AHStone = ZCHex(TEXT("2B2E36")), AHStoneDeep = ZCHex(TEXT("17181D")), AHRim = ZCHex(TEXT("FFE9A8"));
	const FLinearColor AHTeamA = ZCHex(TEXT("59E04A")), AHTeamB = ZCHex(TEXT("E5383B"));
	const FLinearColor AHSapphire = ZCHex(TEXT("2F7BF0")), AHSapphireHi = ZCHex(TEXT("9FE4FF")), AHArcane = ZCHex(TEXT("8E63F5"));
	const FLinearColor AHParch = ZCHex(TEXT("ECE3CC"));

	FString PortraitKey(FName Id)
	{
		const FString S = Id.ToString();
		if (S == TEXT("orin")) return TEXT("master_orin");
		if (S == TEXT("darkzoogy")) return TEXT("dark_zoogy");
		return S;
	}
}

// ------------------------------------------------------------------------------------------------ lifecycle
void UZoogiArenaHUDWidget::Ensure(UWorld* World)
{
	static TWeakObjectPtr<UZoogiArenaHUDWidget> Live;
	if (!ZoogiArenaHUD::IsEnabled() || !World) return;
	if (Live.IsValid() && Live->GetWorld() == World) return;
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController() || !World->GetAuthGameMode<AZoogiGameMode>()) return;
	if (UZoogiArenaHUDWidget* W = CreateWidget<UZoogiArenaHUDWidget>(PC, UZoogiArenaHUDWidget::StaticClass()))
	{
		W->AddToViewport(4);
		Live = W;
		UE_LOG(LogZoogi, Log, TEXT("ARENA HUD layer up (HudStyle=Arena)"));
	}
}

TSharedRef<SWidget> UZoogiArenaHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // drags on the arena reach the game
		WidgetTree->RootWidget = Canvas;
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Super::RebuildWidget();
}

AZoogiGameMode* UZoogiArenaHUDWidget::GM() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
}

int32 UZoogiArenaHUDWidget::LocalPlayer() const
{
	// the phone's player: the first human Zoogi (P1 in "you + CPU ally vs 2 CPUs"); all-CPU runs follow P1
	const AZoogiGameMode* G = GM();
	if (!G) return 0;
	for (const AZoogiBall* B : G->Balls) if (B && !B->bCPU) return B->PlayerIndex;
	return 0;
}

// ------------------------------------------------------------------------------------------------ primitives (design units)
UWidget* UZoogiArenaHUDWidget::At(UWidget* W, float X, float Y, float Wd, float Ht)
{
	if (!W || !Canvas) return W;
	if (UCanvasPanelSlot* Sl = Canvas->AddChildToCanvas(W))
	{
		Sl->SetAnchors(FAnchors(0.f, 0.f));
		Sl->SetPosition(FVector2D(X, Y) * S);
		Sl->SetSize(FVector2D(Wd, Ht) * S);
	}
	return W;
}

UBorder* UZoogiArenaHUDWidget::Box(const FLinearColor& Fill, const FLinearColor& Edge, float EdgeW, float R)
{
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(ZoogiUIStyle::RoundBrush(Fill, Edge, EdgeW * S, R * S));
	B->SetPadding(FMargin(0.f));
	B->SetVisibility(ESlateVisibility::HitTestInvisible);
	return B;
}

UTextBlock* UZoogiArenaHUDWidget::Txt(const FString& Str, float Size, const FLinearColor& C, float Outline, int32 Justify)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Str));
	T->SetFont(ZCFont(Size * S, FMath::Max(Outline > 0.f ? 1.f : 0.f, Outline * S)));
	T->SetColorAndOpacity(FSlateColor(C));
	T->SetShadowOffset(FVector2D(0.f, Outline > 0.f ? FMath::Max(1.f, Size * 0.08f * S) : 0.f));
	T->SetShadowColorAndOpacity(ZCDark.CopyWithNewOpacity(Outline > 0.f ? 0.85f : 0.f));
	T->SetJustification(Justify == 1 ? ETextJustify::Center : Justify == 2 ? ETextJustify::Right : ETextJustify::Left);
	T->SetVisibility(ESlateVisibility::HitTestInvisible);
	return T;
}

UTextBlock* UZoogiArenaHUDWidget::Text(const FString& Str, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C, float Outline, int32 Justify)
{
	UTextBlock* T = Txt(Str, Size, C, Outline, Justify);
	UBorder* B = Box(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
	B->SetHorizontalAlignment(Justify == 1 ? HAlign_Center : Justify == 2 ? HAlign_Right : HAlign_Left);
	B->SetVerticalAlignment(VAlign_Center);
	B->SetContent(T);
	At(B, X, Y, Wd, Ht);
	return T;
}

UImage* UZoogiArenaHUDWidget::Img(UTexture2D* T, float X, float Y, float Wd, float Ht, float Radius, bool bCrop, const FLinearColor& Tint)
{
	if (!T) return nullptr;
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	I->SetBrush(ZCImgBrush(T, Wd * S, Ht * S, Radius * S, bCrop));
	I->SetColorAndOpacity(Tint);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(I, X, Y, Wd, Ht);
	return I;
}

UWidget* UZoogiArenaHUDWidget::Stone(float X, float Y, float Wd, float Ht, float R, const FLinearColor& Fill, bool bGoldRim)
{
	// carved stone plate: dark outline, optional metallic gold rim (bright top -> deep bottom), stone grain, gold inlay
	UWidget* First = nullptr;
	float In = 0.f;
	if (bGoldRim)
	{
		First = At(Box(AHRim, ZCDark, 3.f, R), X, Y, Wd, Ht);
		Img(Tex(TEXT("vgrad")), X + 3.f, Y + 3.f, Wd - 6.f, Ht - 6.f, FMath::Max(2.f, R - 3.f), false, ZCGoldDeep);
		In = 7.f;
	}
	UWidget* Body = At(Box(Fill, ZCDark, 2.f, FMath::Max(2.f, R - In)), X + In, Y + In, Wd - In * 2.f, Ht - In * 2.f);
	if (!First) First = Body;
	if (Wd > 40.f && Ht > 30.f && Fill.A > 0.5f)
		if (UTexture2D* St = Tex(TEXT("stone")))
		{
			UImage* G = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush B = ZCImgBrush(St, (Wd - In * 2.f) * S, (Ht - In * 2.f) * S, FMath::Max(2.f, R - In) * S, false);
			B.SetUVRegion(FBox2f(FVector2f(0.f, 0.f), FVector2f(FMath::Clamp(Wd / 1024.f, 0.05f, 1.f), FMath::Clamp(Ht / 1024.f, 0.05f, 1.f))));
			G->SetBrush(B);
			G->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f));
			G->SetVisibility(ESlateVisibility::HitTestInvisible);
			At(G, X + In, Y + In, Wd - In * 2.f, Ht - In * 2.f);
		}
	if (bGoldRim) At(Box(FLinearColor::Transparent, ZCGold.CopyWithNewOpacity(0.32f), 1.5f, FMath::Max(2.f, R - In - 1.f)), X + In + 1.f, Y + In + 1.f, Wd - In * 2.f - 2.f, Ht - In * 2.f - 2.f);
	return First;
}

UButton* UZoogiArenaHUDWidget::Hit(float X, float Y, float Wd, float Ht, TFunction<void()> Fn)
{
	UButton* B = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	B->SetStyle(ZCClearStyle());
	B->SetClickMethod(EButtonClickMethod::PreciseClick);
	B->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	UZoogiMenuRootClick* C = NewObject<UZoogiMenuRootClick>(this);
	C->Fn = MoveTemp(Fn);
	Clicks.Add(C);
	B->OnClicked.AddDynamic(C, &UZoogiMenuRootClick::Click);
	At(B, X, Y, Wd, Ht);
	return B;
}

UTexture2D* UZoogiArenaHUDWidget::FileTex(const FString& Dir, const FString& Name)
{
	const FString Key = Dir + TEXT("/") + Name;
	if (TObjectPtr<UTexture2D>* Hit = TexCache.Find(Key)) return Hit->Get();
	UTexture2D* T = nullptr;
	for (const TCHAR* E : { TEXT(".png"), TEXT(".jpg") })
	{
		const FString P = FPaths::ProjectContentDir() / Dir / (Name + E);
		if (!T && FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
	}
	TexCache.Add(Key, T);
	return T;
}

UTexture2D* UZoogiArenaHUDWidget::CardArt(FName CardId)
{
	// zoogi-ui card art (the approved style, Content/ZoogiUI/assets/cards/<id-with-hyphens>.jpg) first, older Data/CardArt as fallback
	if (UTexture2D* T = FileTex(TEXT("ZoogiUI/assets/cards"), CardId.ToString().Replace(TEXT("_"), TEXT("-")))) return T;
	return FileTex(TEXT("Data/CardArt"), CardId.ToString());
}

// b78: small white wolf head (ears up, snout down, eye holes) - tinted at draw time
static ZoogiUIFx::FPixels ZCWolfHeadPixels()
{
	ZoogiUIFx::FPixels P; P.W = P.H = 96; P.Px.SetNumZeroed(P.W * P.H);
	const TArray<FVector2D> Poly = { {12, 6}, {34, 30}, {62, 30}, {84, 6}, {84, 46}, {76, 64}, {60, 76}, {48, 92}, {36, 76}, {20, 64}, {12, 46} };
	auto Inside = [&Poly](float X, float Y)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
			if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
		return bIn;
	};
	auto Eye = [](float X, float Y) { return FVector2D::Distance(FVector2D(X, Y), FVector2D(34, 48)) < 6.f || FVector2D::Distance(FVector2D(X, Y), FVector2D(62, 48)) < 6.f; };
	for (int32 y = 0; y < P.H; ++y)
		for (int32 x = 0; x < P.W; ++x)
		{
			int32 N = 0;
			for (int32 s = 0; s < 16; ++s) { const float Sx = x + ((s % 4) + 0.5f) / 4.f, Sy = y + ((s / 4) + 0.5f) / 4.f; if (Inside(Sx, Sy) && !Eye(Sx, Sy)) ++N; }
			if (N) P.Px[y * P.W + x] = FColor(255, 255, 255, (uint8)(N * 255 / 16));
		}
	return P;
}

UTexture2D* UZoogiArenaHUDWidget::Tex(const FString& Key)
{
	if (TObjectPtr<UTexture2D>* Hit = TexCache.Find(Key)) return Hit->Get();
	UTexture2D* T = nullptr;
	const FLinearColor GoldA = ZCHex(TEXT("FFE66B")), GoldB = ZCHex(TEXT("E39410"));
	TArray<FZCShape> Sh;
	int32 W = 128, H = 128; float UW = 100.f, UH = 100.f;
	if (Key == TEXT("wolfhead")) T = ZoogiUIFx::ToTexture(ZCWolfHeadPixels());
	else if (Key == TEXT("stone")) T = ZoogiUIFx::ToTexture(ZCStonePixels());
	else if (Key == TEXT("vgrad")) T = ZoogiUIFx::ToTexture(ZCVGradPixels());
	else if (Key == TEXT("crystal"))
	{
		W = 80; H = 128; UW = 50.f; UH = 80.f;
		Sh.Add(ZCPoly({ { 25, 2 }, { 46, 28 }, { 36, 78 }, { 14, 78 }, { 4, 28 } }, ZCHex(TEXT("E8FBFF")), ZCHex(TEXT("1557D6")), 2.5f));
		Sh.Add(ZCPoly({ { 25, 6 }, { 25, 74 }, { 15, 74 }, { 8, 28 } }, FLinearColor(1.f, 1.f, 1.f, 0.25f), FLinearColor(1.f, 1.f, 1.f, 0.05f), 0.f, false));
	}
	else if (Key == TEXT("crystal_off"))
	{
		W = 80; H = 128; UW = 50.f; UH = 80.f;
		Sh.Add(ZCPoly({ { 25, 2 }, { 46, 28 }, { 36, 78 }, { 14, 78 }, { 4, 28 } }, ZCHex(TEXT("4A5160")), ZCHex(TEXT("1E2128")), 2.5f, false));
	}
	else if (Key == TEXT("star4"))
	{
		Sh.Add(ZCPoly({ { 50, 2 }, { 60, 40 }, { 98, 50 }, { 60, 60 }, { 50, 98 }, { 40, 60 }, { 2, 50 }, { 40, 40 } }, FLinearColor::White, ZCHex(TEXT("BDE8FF")), 0.f, false));
	}
	else if (Key == TEXT("gem"))
	{
		Sh.Add(ZCPoly({ { 50, 4 }, { 90, 40 }, { 50, 96 }, { 10, 40 } }, ZCHex(TEXT("BFEAFF")), ZCHex(TEXT("1F5BD6")), 4.f));
		Sh.Add(ZCPoly({ { 50, 4 }, { 90, 40 }, { 50, 40 }, { 10, 40 } }, FLinearColor(1.f, 1.f, 1.f, 0.3f), FLinearColor(1.f, 1.f, 1.f, 0.1f), 0.f, false));
	}
	else if (Key == TEXT("hourglass"))
	{
		Sh.Add(ZCRect(18, 6, 82, 14, GoldA, GoldB, 3.f));
		Sh.Add(ZCRect(18, 86, 82, 94, GoldA, GoldB, 3.f));
		Sh.Add(ZCPoly({ { 26, 14 }, { 74, 14 }, { 54, 50 }, { 74, 86 }, { 26, 86 }, { 46, 50 } }, ZCHex(TEXT("F4E3B0")), ZCHex(TEXT("C9A55A")), 3.f, false));
	}
	else if (Key == TEXT("crest"))
	{
		W = 168; H = 68; UW = 100.f; UH = 40.5f;
		Sh.Add(ZCPoly({ { 6, 22 }, { 30, 12 }, { 42, 20 }, { 50, 4 }, { 58, 20 }, { 70, 12 }, { 94, 22 }, { 70, 30 }, { 58, 26 }, { 50, 38 }, { 42, 26 }, { 30, 30 } }, GoldA, GoldB, 2.5f));
		Sh.Add(ZCPoly({ { 50, 10 }, { 58, 21 }, { 50, 33 }, { 42, 21 } }, ZCHex(TEXT("BFEAFF")), ZCHex(TEXT("1F5BD6")), 2.f));
	}
	else if (Key == TEXT("chevron"))
	{
		W = 96; H = 128; UW = 60.f; UH = 80.f;
		Sh.Add(ZCPoly({ { 36, 4 }, { 56, 4 }, { 24, 40 }, { 56, 76 }, { 36, 76 }, { 4, 40 } }, AHSapphireHi, AHSapphire, 2.5f, false));
	}
	if (!T && Sh.Num()) T = ZoogiUIFx::ToTexture(ZCRaster(W, H, UW, UH, Sh));
	TexCache.Add(Key, T);
	return T;
}

// ------------------------------------------------------------------------------------------------ build
void UZoogiArenaHUDWidget::Build()
{
	DetailsBottomD = 0.f;
	if (!Canvas) return;
	Canvas->ClearChildren();
	Clicks.Reset();
	for (FUnitRefs& U : Units) U = FUnitRefs();
	BigBottom = 0.f;
	TacGlow.Reset(); TacPlate.Reset(); TacLbl.Reset();
	GaugeSegs.Reset(); Crystals.Reset(); AwakenPips.Reset(); Cards.Reset();
	SplitParts.Reset(); SplitOrder = 0; AvatarW.Reset();
	const AZoogiGameMode* G = GM();
	if (!G) return;
	for (TWeakObjectPtr<UWidget>& W : StepGlow) W = nullptr;
	if (bBoard) BuildBoard();
	else { BuildTopBar(); BuildTray(); }
	bBuiltOver = G->Phase == EZoogiPhase::GameOver;
	if (!bBuiltOver) P5Results = nullptr; // next game over plays the results entrance again
	if (bBuiltOver) BuildResults();
	if (bPaused) BuildPause();
	bBuiltPause = bPaused;
}

void UZoogiArenaHUDWidget::BuildResults()
{
	// END OF MATCH (button map): winner banner, every Zoogi (team, score, Ring Marks), REMATCH -> new match, MENU -> BATTLE tab
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;
	if (ZoogiStory::InMission())
	{
		// Oct 30: a STORY MODE battle - the story shows its own MISSION CLEAR / FAILED screen
		for (int32 k = 0; k < Canvas->GetChildrenCount(); ++k) if (UWidget* Ch = Canvas->GetChildAt(k)) Ch->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (ZoogiP5::MenuOn())
	{
		// Oct 30 (P5 brief step 3): the P5-style RESULTS screen - one Slate layer over the whole HUD, kept across HUD rebuilds so the
		// entrance plays once. -ZoogiClassicMenu = the stone panel below.
		if (!P5Results)
		{
			P5Results = NewObject<UZoogiP5ResultsWidget>(this);
			ZoogiP5::FResultsSpec& R = P5Results->Spec;
			const int32 Me = LocalPlayer();
			auto TeamOf = [G](int32 i) { return G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(i) : (i == 0 ? 0 : 1); };
			const int32 WinTeam = G->bComboMode ? G->ComboWinner : -1;
			const bool bDraw = G->bComboMode ? WinTeam < 0 : !G->Balls.IsValidIndex(G->WinnerIndex);
			const bool bWeWon = !bDraw && (G->bComboMode ? WinTeam == TeamOf(Me) : G->WinnerIndex == Me);
			R.Outcome = bDraw ? 2 : (bWeWon ? 0 : 1);
			R.Head = bDraw ? TEXT("DRAW!") : (bWeWon ? TEXT("VICTORY!") : TEXT("DEFEAT"));
			int32 HeroI = -1;
			for (int32 i = 0; i < G->Balls.Num(); ++i)
			{
				const AZoogiBall* B = G->Balls[i];
				if (!B) continue;
				const bool bOk = bDraw || (G->bComboMode ? TeamOf(i) == WinTeam : i == G->WinnerIndex);
				if (bOk && (HeroI < 0 || B->Score > G->Balls[HeroI]->Score)) HeroI = i;
			}
			if (G->bComboMode) R.Sub = WinTeam == 0 ? TEXT("TEAM A WINS!") : (WinTeam == 1 ? TEXT("TEAM B WINS!") : TEXT("DEAD EVEN"));
			else R.Sub = (!bDraw && HeroI >= 0) ? FString::Printf(TEXT("%s WINS!"), *G->Balls[HeroI]->Def.Name.ToUpper()) : FString(TEXT("DEAD EVEN"));
			if (G->bComboMode) R.Reason = G->ComboOverReason.ToUpper();
			UZoogiFunSys* Fun = UZoogiFunSys::Get(G);
			if (HeroI >= 0)
			{
				const AZoogiBall* HB = G->Balls[HeroI];
				R.HeroName = HB->Def.Name.ToUpper();
				R.HeroTag = FString::Printf(TEXT("P%d  /  %s  /  %s"), HeroI + 1, TeamOf(HeroI) == 0 ? TEXT("TEAM A") : TEXT("TEAM B"), HB->bCPU ? TEXT("CPU") : TEXT("PLAYER"));
				R.WinCol = ZoogiP5::ZoogiElementColor(HB->Def.Name);
				UTexture2D* Ult = Fun ? Fun->UltAvatar(HB) : nullptr; // the ultimate-form face when it has one (Evan's avatars)
				R.Hero = Ult ? Ult : FileTex(TEXT("Data/MenuArt"), PortraitKey(HB->Def.Id));
				R.bHeroCutout = Ult != nullptr;
				if (R.Hero.IsValid()) P5Results->Keep.Add(R.Hero.Get());
			}
			TArray<int32> Order;
			for (int32 i = 0; i < G->Balls.Num(); ++i) if (G->Balls[i]) Order.Add(i);
			Order.Sort([G, WinTeam, &TeamOf](int32 X, int32 Y)
			{
				if (G->bComboMode && WinTeam >= 0) { const bool WX = TeamOf(X) == WinTeam, WY = TeamOf(Y) == WinTeam; if (WX != WY) return WX; }
				return G->Balls[X]->Score > G->Balls[Y]->Score;
			});
			for (const int32 i : Order)
			{
				const AZoogiBall* B = G->Balls[i];
				ZoogiP5::FResultRow Row;
				Row.Name = B->Def.Name.ToUpper();
				Row.Score = B->Score;
				Row.Player = i;
				Row.bYou = !B->bCPU;
				Row.bWin = !bDraw && (G->bComboMode ? TeamOf(i) == WinTeam : i == G->WinnerIndex);
				Row.Col = TeamOf(i) == 0 ? AHTeamA : AHTeamB;
				Row.Face = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id));
				Row.Line = G->bComboMode
					? FString::Printf(TEXT("P%d  /  %s  /  %d ORBS  /  %d OUTS"), i + 1, TeamOf(i) == 0 ? TEXT("TEAM A") : TEXT("TEAM B"), B->OrbsScored, B->KnockOuts)
					: FString::Printf(TEXT("P%d  /  %d ORBS  /  %d KOS  /  %d MARKS"), i + 1, B->OrbsScored, B->KnockOuts, B->RingMarks);
				R.Rows.Add(Row);
			}
			const ZoogiScoringUI::FMatchStats& St = ZoogiScoringUI::Stats();
			R.Stats.Add({ TEXT("ORBS"), St.Orbs });
			R.Stats.Add({ TEXT("STARS"), St.Stars });
			R.Stats.Add({ TEXT("KOS"), St.KOs });
			R.Stats.Add({ TEXT("BEST COMBO"), St.BestCombo });
			R.Stats.Add({ TEXT("PERFECTS"), St.Perfects });
			if (ZoogiRaid::HasResult())
			{	// b151 RAID: team result, MVP, damage per Zoogi
				const bool bWin = ZoogiRaid::Cleared();
				R.Outcome = bWin ? 0 : 1;
				R.Head = bWin ? TEXT("RAID CLEARED!") : TEXT("RAID FAILED");
				R.Sub = HeroI >= 0 ? FString::Printf(TEXT("MVP  %s"), *G->Balls[HeroI]->Def.Name.ToUpper()) : FString(TEXT("TEAM EFFORT"));
				for (ZoogiP5::FResultRow& Rw : R.Rows) { Rw.Line = FString::Printf(TEXT("P%d  /  %d DAMAGE  /  %d HITS"), Rw.Player + 1, ZoogiRaid::DamageOf(Rw.Player), ZoogiRaid::HitsOf(Rw.Player)); Rw.bWin = bWin && Rw.Player == HeroI; }
				R.Stats.Reset();
				R.Stats.Add({ TEXT("BOSS HP LEFT"), ZoogiRaid::HPLeft() });
				R.Stats.Add({ TEXT("TEAM LIVES"), ZoogiRaid::LivesLeft() });
				R.Stats.Add({ TEXT("ORBS"), St.Orbs });
				R.Stats.Add({ TEXT("BEST COMBO"), St.BestCombo });
			}
			TWeakObjectPtr<UZoogiArenaHUDWidget> Self(this);
			auto Act = [Self](const TCHAR* Action) -> TFunction<void()>
			{
				return [Self, Action]() { if (Self.IsValid()) if (AZoogiGameMode* GM2 = Self->GM()) { UE_LOG(LogZoogi, Log, TEXT("ARENA HUD %s"), Action); GM2->MenuAction(Action, 0); } };
			};
			R.Buttons.Add({ TEXT("REMATCH"), TEXT("ONE MORE!"), 0, Act(TEXT("Rematch")) });
			R.Buttons.Add({ TEXT("CHANGE ZOOGI"), TEXT("SWITCH UP!"), 1, Act(TEXT("ChangeZoogi")) });
			R.Buttons.Add({ TEXT("MENU"), TEXT("BACK!"), 2, Act(TEXT("Menu")) });
			R.Ctx = GM();
			R.Shot = [Self](const FString& Nm) { if (Self.IsValid()) if (AZoogiGameMode* GM2 = Self->GM()) GM2->TakeShot(Nm); };
			ZoogiP5::PlayWipe(GetWorld(), nullptr, R.WinCol, R.Outcome == 0 ? TEXT("FINISH!") : (R.Outcome == 1 ? TEXT("GAME OVER") : TEXT("TIME!")));
			UE_LOG(LogZoogi, Log, TEXT("P5 RESULTS screen: %s / %s (hero %s, %s face, %d rows)"), *R.Head, *R.Sub, *R.HeroName, R.bHeroCutout ? TEXT("ultimate") : TEXT("menu"), R.Rows.Num());
		}
		for (int32 k = 0; k < Canvas->GetChildrenCount(); ++k) if (UWidget* Ch = Canvas->GetChildAt(k)) Ch->SetVisibility(ESlateVisibility::Collapsed); // the results screen replaces the match HUD
		At(P5Results, 0.f, 0.f, W, H);
		if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(P5Results->Slot)) CS->SetZOrder(100000); // above every split part (the ROUND line keeps a raised z)
		return;
	}
	At(Box(FLinearColor(0.01f, 0.008f, 0.006f, 0.6f), FLinearColor::Transparent, 0.f, 0.f), 0.f, 0.f, W, H);
	Hit(0.f, 0.f, W, H, []() {});
	const int32 N = G->Balls.Num();
	const float PW = FMath::Min(900.f, W - 48.f), RowH = bPortrait ? 112.f : 80.f, PH = 230.f + N * (RowH + 12.f) + 260.f;
	const float PX = W * 0.5f - PW * 0.5f, PY = bPortrait ? FMath::Max(270.f, H * 0.5f - PH * 0.5f) : FMath::Max(8.f, H * 0.5f - PH * 0.5f);
	Stone(PX, PY, PW, PH, 30.f, AHStone, true);
	Img(Tex(TEXT("crest")), W * 0.5f - 52.f, PY + 2.f, 104.f, 42.f, 0.f, false);
	int32 WinTeam = G->bComboMode ? G->ComboWinner : -1;
	FString Head;
	if (G->bComboMode) Head = WinTeam == 0 ? TEXT("TEAM A WINS!") : WinTeam == 1 ? TEXT("TEAM B WINS!") : TEXT("DRAW!");
	else Head = G->Balls.IsValidIndex(G->WinnerIndex) && G->Balls[G->WinnerIndex] ? FString::Printf(TEXT("%s WINS!"), *G->Balls[G->WinnerIndex]->Def.Name.ToUpper()) : TEXT("DRAW!");
	const bool bWeWon = G->bComboMode ? (WinTeam == AZoogiGameMode::TeamOfIndex(LocalPlayer())) : (G->WinnerIndex == LocalPlayer());
	Text(bWeWon ? TEXT("VICTORY") : TEXT("DEFEAT"), PX, PY + 40.f, PW, 70.f, 52.f, bWeWon ? ZCHex(TEXT("FFD35C")) : ZCHex(TEXT("FF8A80")), 5.f);
	Text(Head, PX, PY + 108.f, PW, 50.f, 32.f, FLinearColor::White, 4.f);
	if (G->bComboMode && !G->ComboOverReason.IsEmpty()) Text(G->ComboOverReason, PX + 30.f, PY + 160.f, PW - 60.f, 40.f, 19.f, ZCMuted, 2.f)->SetAutoWrapText(true);
	for (int32 i = 0; i < N; ++i)
	{
		const AZoogiBall* B = G->Balls[i];
		if (!B) continue;
		const int32 Team = G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(i) : (i == 0 ? 0 : 1);
		const bool bWin = G->bComboMode ? Team == WinTeam : i == G->WinnerIndex;
		const float RY = PY + 214.f + i * (RowH + 12.f), RX = PX + 30.f, RW = PW - 60.f;
		Stone(RX, RY, RW, RowH, 18.f, (Team == 0 ? ZCHex(TEXT("1F3A28")) : ZCHex(TEXT("3E1E20"))).CopyWithNewOpacity(0.95f), bWin);
		const float Pt = RowH - 28.f, Mid = RY + RowH * 0.5f; // portrait size and row centre line
		At(Box(Team == 0 ? AHTeamA : AHTeamB, ZCDark, 4.f, Pt * 0.5f), RX + 16.f, Mid - Pt * 0.5f, Pt, Pt);
		if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) Img(P, RX + 22.f, Mid - Pt * 0.5f + 6.f, Pt - 12.f, Pt - 12.f, (Pt - 12.f) * 0.5f, true);
		const float LX = RX + 34.f + Pt;
		Text(FString::Printf(TEXT("P%d  %s%s"), i + 1, *B->Def.Name.ToUpper(), B->bCPU ? TEXT("") : TEXT("  (YOU)")), LX, Mid - 42.f, RW - 330.f, 44.f, 28.f, FLinearColor::White, 3.f, 0);
		Text(FString::Printf(TEXT("%s   %d ring marks"), Team == 0 ? TEXT("TEAM A") : TEXT("TEAM B"), B->RingMarks), LX, Mid + 2.f, RW - 330.f, 36.f, 20.f, ZCSoft, 2.f, 0);
		Text(FText::AsNumber(B->Score).ToString(), RX + RW - 210.f, RY, 100.f, RowH, 38.f, ZCHex(TEXT("FFD35C")), 3.f, 2);
		if (bWin) { At(Box(ZCHex(TEXT("FFD54A")), ZCDark, 3.f, 10.f), RX + RW - 100.f, Mid - 22.f, 86.f, 44.f); Text(TEXT("WIN"), RX + RW - 100.f, Mid - 22.f, 86.f, 44.f, 22.f, ZCDark, 0.f); }
	}
	// #39 personal stats (local player this match)
	{
		const ZoogiScoringUI::FMatchStats& St = ZoogiScoringUI::Stats();
		const float SY = PY + 214.f + N * (RowH + 12.f) + 8.f;
		Stone(PX + 30.f, SY, PW - 60.f, 88.f, 16.f, ZCHex(TEXT("16171B")).CopyWithNewOpacity(0.95f), false);
		Text(TEXT("YOUR STATS"), PX + 46.f, SY + 6.f, 200.f, 28.f, 18.f, ZCHex(TEXT("F0C46A")), 2.f, 0);
		Text(FString::Printf(TEXT("Orbs %d   Stars %d   KOs %d   Falls %d"), St.Orbs, St.Stars, St.KOs, St.Falls), PX + 46.f, SY + 34.f, PW - 120.f, 24.f, 16.f, AHParch, 1.f, 0);
		Text(FString::Printf(TEXT("Best combo x%d   Combo bonus +%d   Shots %d  (P%d G%d Gd%d M%d)"), St.BestCombo, St.ComboBonus, St.Shots, St.Perfects, St.Greats, St.Goods, St.Misses), PX + 46.f, SY + 56.f, PW - 120.f, 24.f, 15.f, ZCSoft, 1.f, 0);
	}
	// queue #20: Rematch + Change Zoogi + Menu
	const float BY = PY + PH - 124.f, Gap = 14.f, BW = (PW - 60.f - Gap * 2.f) / 3.f;
	auto EndBtn = [&](float BX, const FLinearColor& Face, const TCHAR* Label, const TCHAR* Action)
	{
		Stone(BX, BY, BW, 96.f, 30.f, Face, true);
		Text(Label, BX, BY, BW, 96.f, FCString::Strlen(Label) > 10 ? 26.f : 32.f, FLinearColor::White, 4.f);
		Hit(BX, BY, BW, 96.f, [this, Action]() { if (AZoogiGameMode* GM2 = GM()) { UE_LOG(LogZoogi, Log, TEXT("ARENA HUD %s"), Action); GM2->MenuAction(Action, 0); } });
	};
	EndBtn(PX + 30.f, ZCHex(TEXT("E9A520")), TEXT("REMATCH"), TEXT("Rematch"));
	EndBtn(PX + 30.f + BW + Gap, AHSapphire, TEXT("CHANGE ZOOGI"), TEXT("ChangeZoogi"));
	EndBtn(PX + 30.f + (BW + Gap) * 2.f, ZCHex(TEXT("4A4D55")), TEXT("MENU"), TEXT("Menu"));
}

void UZoogiArenaHUDWidget::BuildTopBar()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X;
	const bool bL = !bPortrait;
	// side plates + Episode panel (portrait reference: 1080 wide; landscape keeps the same pieces at the corners)
	// b171d (Evan: "big corner badges ... with the crystal counter and sp"): cards replace the stone plates; -ZoogiOldBadges
	const bool bBigB = BigBadges();
	const float BigW = 200.f, BigH = 296.f;
	const FMargin SafeB = ZoogiFramed::SafeInsets(FVector2D(W * S, Design.Y * S));
	const float BigTop = FMath::Max(SafeB.Top / FMath::Max(S, 0.001f) + 12.f, 14.f);
	const float PlateW = bBigB ? 2.f * BigW + 40.f : 386.f;
	const float PlateH = bBigB ? BigTop + BigH + 44.f : 238.f;
	int32 Mark = Canvas->GetChildrenCount();
	if (!bBigB)
	{
		Stone(-12.f, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
		MarkSplit(Mark, FVector2D(-PlateW - 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
		Stone(W - PlateW, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
		MarkSplit(Mark, FVector2D(PlateW + 40.f, 0.f)); Mark = Canvas->GetChildrenCount();
	}
	// Oct 23 (Evan): NO mid-screen EPISODE box. Phase/round + crystals at TOP under DRAW banner.
	const float H = Design.Y;
	const FMargin SafePxEp = ZoogiFramed::SafeInsets(FVector2D(W * S, H * S));
	const float SafeTopEp = SafePxEp.Top / FMath::Max(S, 0.001f);
	const float BannerRestApprox = FMath::Max(SafeTopEp + 40.f, bPortrait ? 88.f : 72.f) + 32.f;
	const float EpW = 280.f, EpX = W * 0.5f - EpW * 0.5f;
	// sit just under DRAW banner plate+fill (~BH 44 + bar); clear of arena center
	const float EpY = BannerRestApprox + 44.f + 22.f;
	BuildEpisode(EpX, EpY, EpW);
	MarkSplit(Mark, FVector2D::ZeroVector, 2); // stay put at top
	// units: team A left (P1, P2), team B right (P3, P4); 1v1 / 3-player matches fill from the left
	// units: team A left (P1, P2), team B right (P3, P4); 1v1 / 3-player matches fill from the left
	// #37 L/R mirror: swap team columns when ScoringUI MirrorLayout is on
	const bool bMirror = ZoogiScoringUI::MirrorLayout();
	const float CxLeft[2] = { 112.f, 287.f }, CxRight[2] = { W - 287.f, W - 112.f };
	const float* CxA = bMirror ? CxRight : CxLeft;
	const float* CxB = bMirror ? CxLeft : CxRight;
	for (int32 i = 0; i < G->Balls.Num() && i < 4; ++i)
	{
		const int32 Team = G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(i) : (i == 0 ? 0 : 1);
		const int32 USlot = G->Balls.Num() >= 4 ? (i % 2) : (i == 0 ? 0 : FMath::Clamp(i - 1, 0, 1));
		const int32 UMark = Canvas->GetChildrenCount();
		if (bBigB)
		{	// outer slot first; 1v1 / 3 players: the right side starts at its outer slot too
			const bool bLeftSide = (Team == 0) != bMirror;
			const bool bOuter = bLeftSide ? USlot == 0 : (G->Balls.Num() < 4 ? USlot == 0 : USlot == 1);
			BuildBigBadge(i, bLeftSide ? (bOuter ? 16.f : 30.f + BigW) : (bOuter ? W - 16.f - BigW : W - 30.f - 2.f * BigW), BigTop, BigW, BigH);
		}
		else BuildUnit(i, Team == 0 ? CxA[USlot] : CxB[USlot], 14.f);
		MarkSplit(UMark, FVector2D((bMirror ? (Team == 0 ? 1 : -1) : (Team == 0 ? -1 : 1)) * (PlateW + 40.f), 0.f));
	}
	// combo callout (left, under the plate): 2v2 team Resonance / classic chained scores (BuildComboBadge)
	BuildComboBadge(0.f, PlateH + 210.f); // b269 (Evan: nothing covered): below the AWAKEN bar (PlateH + 34 sat on it)
	// Settings cog + Tactical cam (right, under the plate) + match clock. Kind 2 cluster; no II bars; gap >= 14.
	// (Oct 28, Evan: landscape positions stay as they are - only portrait moved the row under the round counter.)
	{
		const float PY = PlateH + 18.f;
		BuildChromeCluster(W - 92.f, PY, true); // settings rightmost; TAC immediately left
		At(Box(ZCHex(TEXT("16171B")).CopyWithNewOpacity(0.92f), ZCHex(TEXT("3E434F")), 2.f, 26.f), W - 172.f, PY + 88.f, 190.f, 54.f);
		Img(Tex(TEXT("hourglass")), W - 158.f, PY + 97.f, 34.f, 36.f, 0.f, false);
		ClockTxt = Text(TEXT("00:00"), W - 118.f, PY + 88.f, 110.f, 54.f, 26.f, AHParch, 2.f, 0);
	}
	(void)bL;
}

void UZoogiArenaHUDWidget::BuildUnit(int32 Player, float Cx, float Top)
{
	const AZoogiGameMode* G = GM();
	const AZoogiBall* B = G && G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (!B) return;
	const int32 Team = G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(Player) : (Player == 0 ? 0 : 1);
	const FLinearColor RingC = Team == 0 ? AHTeamA : AHTeamB;
	FUnitRefs& U = Units[Player];
	const float RS = 140.f, RX = Cx - RS * 0.5f, RY = Top;
	const int32 AvMark = Canvas->GetChildrenCount();
	U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 6.f, RS * 0.5f + 8.f), RX - 8.f, RY - 8.f, RS + 16.f, RS + 16.f);
	U.Ring = At(Box(ZCHex(TEXT("121418")), RingC, 8.f, RS * 0.5f), RX, RY, RS, RS);
	DetailsBottomD = FMath::Max(DetailsBottomD, RY + RS + 90.f); // b69: + the SP line above the crystals
	if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) { U.Avi = Img(P, RX + 10.f, RY + 10.f, RS - 20.f, RS - 20.f, (RS - 20.f) * 0.5f, true); U.AviBase = P; U.bAviUlt = false; }
	else Text(B->Def.Name.Left(2).ToUpper(), RX, RY, RS, RS, 48.f, FLinearColor::White, 3.f);
	MarkAvatar(AvMark);
	Hit(RX - 8.f, RY - 8.f, RS + 16.f, RS + 16.f, [this]() { PeekDetails(); }); // Oct 22: tap = briefly show details
	// element badge: the Zoogi's affinity element (what its AWAKEN infuses)
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	const FLinearColor EC = ZoogiArtifacts::Color(Aff);
	At(Box(EC, AHParch, 4.f, 23.f), Cx + 30.f, RY + RS - 46.f, 46.f, 46.f);
	Img(Tex(TEXT("star4")), Cx + 40.f, RY + RS - 36.f, 26.f, 26.f, 0.f, false);
	// self-bar polish: deeper track, clearer fill, readable score, breathing room
	U.BarW = 136.f;
	At(Box(ZCHex(TEXT("080A0E")).CopyWithNewOpacity(0.94f), ZCHex(TEXT("4A4F5C")), 2.5f, 11.f), Cx - 68.f, RY + RS + 10.f, U.BarW, 22.f);
	At(Box(ZCHex(TEXT("12151C")), FLinearColor::Transparent, 0.f, 9.f), Cx - 65.f, RY + RS + 13.f, U.BarW - 6.f, 16.f);
	UBorder* Fill = Box(RingC, FLinearColor::Transparent, 0.f, 8.f);
	U.Bar = At(Fill, Cx - 65.f, RY + RS + 13.f, U.BarW - 6.f, 16.f);
	U.Score = Text(TEXT("0"), Cx - 68.f, RY + RS + 8.f, U.BarW, 26.f, 18.f, FLinearColor::White, 2.5f);
	// #45 weight bar (MassRatio) under score - separated so labels never sit on the fill
	{
		const float W01 = ZoogiScoringUI::Weight01(B);
		const TCHAR* WL = ZoogiScoringUI::WeightLabel(B->Def.MassRatio());
		At(Box(ZCHex(TEXT("080A0E")).CopyWithNewOpacity(0.9f), ZCHex(TEXT("3B3E45")), 2.f, 7.f), Cx - 68.f, RY + RS + 36.f, 136.f, 14.f);
		const FLinearColor WC = W01 > 0.66f ? ZCHex(TEXT("FF8A65")) : (W01 < 0.33f ? ZCHex(TEXT("81D4FA")) : ZCHex(TEXT("FFE082")));
		At(Box(WC, FLinearColor::Transparent, 0.f, 5.f), Cx - 66.f, RY + RS + 38.f, FMath::Max(4.f, 132.f * W01), 10.f);
		Text(WL, Cx - 68.f, RY + RS + 50.f, 136.f, 16.f, 12.f, AHParch, 1.f, 1);
	}
	// Awaken pips (4): larger hit-friendly dots under the weight label
	const int32 N = AZoogiGameMode::AwakenChargeMax;
	const float PS = 24.f, Gap = 10.f, PW = N * PS + (N - 1) * Gap, PX = Cx - PW * 0.5f;
	for (int32 k = 0; k < N; ++k)
		U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 90.f, PS, PS));
	U.PipMore = Text(TEXT(""), PX + PW + 4.f, RY + RS + 88.f, 44.f, 26.f, 15.f, ZCHex(TEXT("FFD35C")), 2.f, 0); // b141: crystals over 4
	{	// b78 (Evan: "the small wolf heads under the crystals, matching the number by the Wolf Pack button")
		const float WS = 22.f, WG = 6.f, WW = 4 * WS + 3 * WG;
		for (int32 k = 0; k < 4; ++k) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), Cx - WW * 0.5f + k * (WS + WG), RY + RS + 90.f + PS + 6.f, WS, WS, 0.f, false, FLinearColor::Transparent)) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
	}
	U.SP = Text(TEXT("SP -"), Cx - 68.f, RY + RS + 68.f, 136.f, 20.f, 15.f, ZCHex(TEXT("7FE3FF")), 2.f, 1); // b69 (Evan): SP left / SP per turn, above the crystals
	(void)Team;
}

bool UZoogiArenaHUDWidget::BigBadges()
{
	static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldBadges"));
	return b;
}

// b171d (Evan: "big corner badges but show with the crystal counter and sp" / "yes to this big corner design"): one card per
// player - face, name, score, SP bar (5 segments), crystals (4 gems + xN); gold frame + YOUR TURN under whoever is shooting
void UZoogiArenaHUDWidget::BuildBigBadge(int32 Player, float X, float Y, float Wd, float Ht)
{
	const AZoogiGameMode* G = GM();
	const AZoogiBall* B = G && G->Balls.IsValidIndex(Player) ? G->Balls[Player].Get() : nullptr;
	if (!B) return;
	FUnitRefs& U = Units[Player];
	U.bBig = true;
	const float k = Wd / 210.f;
	const FLinearColor PC = ZoogiUtil::PlayerColor(Player);
	U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 6.f * k, 22.f * k), X - 7.f * k, Y - 7.f * k, Wd + 14.f * k, Ht + 14.f * k);
	At(Box(ZCHex(TEXT("0E0A18")).CopyWithNewOpacity(0.93f), PC, 5.f * k, 18.f * k), X, Y, Wd, Ht);
	const float Rs = 124.f * k, RX = X + (Wd - Rs) * 0.5f, RY = Y + 10.f * k;
	const int32 AvMark = Canvas->GetChildrenCount();
	U.Ring = At(Box(PC, PC, 3.f * k, Rs * 0.5f), RX, RY, Rs, Rs);
	if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) { U.Avi = Img(P, RX + 6.f * k, RY + 6.f * k, Rs - 12.f * k, Rs - 12.f * k, (Rs - 12.f * k) * 0.5f, true); U.AviBase = P; U.bAviUlt = false; }
	else Text(B->Def.Name.Left(2).ToUpper(), RX, RY, Rs, Rs, 48.f * k, FLinearColor::White, 3.f);
	MarkAvatar(AvMark);
	Hit(X, Y, Wd, Ht, [this]() { PeekDetails(); });
	const FString Nm = B->Def.Name.ToUpper();
	Text(Nm, X + 4.f * k, Y + 136.f * k, Wd - 8.f * k, 40.f * k, (Nm.Len() > 8 ? 23.f : 30.f) * k, FLinearColor::White, 2.5f, 1);
	U.Score = Text(TEXT("0"), X, Y + 172.f * k, Wd, 48.f * k, 40.f * k, ZCHex(TEXT("FFD35C")), 3.f, 1);
	Text(TEXT("SP"), X + 12.f * k, Y + 224.f * k, 40.f * k, 24.f * k, 18.f * k, ZCHex(TEXT("78C8FF")), 1.5f, 0);
	for (int32 j = 0; j < 10; ++j) U.SpSegs.Add(At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 2.f * k), X + 52.f * k + j * 14.5f * k, Y + 229.f * k, 12.f * k, 15.f * k)); // b181: 10 SP segments (was 5)
	const int32 NPip = AZoogiGameMode::AwakenChargeMax;
	for (int32 j = 0; j < NPip && j < 4; ++j)
	{
		UWidget* Gm = At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 3.f * k), X + 16.f * k + j * 31.f * k, Y + 260.f * k, 19.f * k, 19.f * k);
		if (Gm) Gm->SetRenderTransformAngle(45.f);
		U.Pips.Add(Gm);
	}
	U.CrysN = Text(TEXT("x0"), X + Wd - 62.f * k, Y + 253.f * k, 54.f * k, 32.f * k, 20.f * k, ZCHex(TEXT("DCB4FF")), 1.5f, 0);
	{	// b78 wolf heads (banked Wolf Pack summons): a row under the card, hidden until there is one
		const float WS = 20.f * k, WG = 5.f * k, WW = 4 * WS + 3 * WG;
		for (int32 j = 0; j < 4; ++j) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), X + Wd - WW - 8.f * k + j * (WS + WG), Y + Ht + 8.f * k, WS, WS, 0.f, false, FLinearColor::Transparent)) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
	}
	{	// YOUR TURN: gold arrow (diamond) on the bottom edge + label
		UWidget* Ar = At(Box(ZCHex(TEXT("FFD35C")), FLinearColor::Transparent, 0.f, 3.f * k), X + Wd * 0.5f - 13.f * k, Y + Ht - 6.f * k, 26.f * k, 26.f * k);
		if (Ar) { Ar->SetRenderTransformAngle(45.f); Ar->SetVisibility(ESlateVisibility::Collapsed); U.ActiveExtra.Add(Ar); }
		UTextBlock* Tt = Text(B->bCPU ? TEXT("CPU TURN") : TEXT("YOUR TURN"), X - 20.f * k, Y + Ht + 22.f * k, Wd + 40.f * k, 26.f * k, 18.f * k, FLinearColor::White, 2.f, 1);
		if (Tt) { Tt->SetVisibility(ESlateVisibility::Collapsed); U.ActiveExtra.Add(Tt); }
	}
	AvatarW.Add(U.Active); for (TWeakObjectPtr<UWidget>& Wx : U.ActiveExtra) AvatarW.Add(Wx);
	DetailsBottomD = FMath::Max(DetailsBottomD, Y + Ht + 50.f * k);
	BigBottom = FMath::Max(BigBottom, Y + Ht + 50.f * k);
}

void UZoogiArenaHUDWidget::SyncBigBadge(AZoogiGameMode* G, FUnitRefs& U, int32 i)
{
	if (!U.bBig || !G) return;
	const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;
	if (U.Active.IsValid()) { const ESlateVisibility V = bAct ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (U.Active->GetVisibility() != V) U.Active->SetVisibility(V); }
	for (TWeakObjectPtr<UWidget>& Wd : U.ActiveExtra)
		if (Wd.IsValid()) { const ESlateVisibility V = bAct ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (Wd->GetVisibility() != V) Wd->SetVisibility(V); }
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(i) : nullptr;
	const int32 Max = CS ? FMath::Max(1, CS->Rules().Tune().EnergyPerTurn) : 5;
	const int32 NSeg = U.SpSegs.Num();
	const int32 Lit = PS ? FMath::Clamp(FMath::RoundToInt((float)FMath::Max(0, PS->Energy) * NSeg / Max), 0, NSeg) : 0;
	for (int32 k = 0; k < NSeg; ++k) if (UBorder* Bd = Cast<UBorder>(U.SpSegs[k].Get())) Bd->SetBrushColor(k < Lit ? ZCHex(TEXT("3CAAFF")) : ZCHex(TEXT("2D2D3C")));
	const int32 Charge = G->AwakenCharge[i];
	for (int32 k = 0; k < U.Pips.Num(); ++k)
		if (UBorder* Bd = Cast<UBorder>(U.Pips[k].Get())) Bd->SetBrushColor(k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : ZCHex(TEXT("BE5AFF"))) : ZCHex(TEXT("37324A")));
	if (UTextBlock* T = U.CrysN.Get()) { const FString Cs = FString::Printf(TEXT("x%d"), FMath::Max(0, Charge)); if (!T->GetText().ToString().Equals(Cs)) T->SetText(FText::FromString(Cs)); }
}

// b69 (Evan: "Add SP and remaining SP to the profile above the crystal counter"): "SP left / per turn" (cards on), else hidden
void UZoogiArenaHUDWidget::SyncAviClip(AZoogiGameMode* G, FUnitRefs& U, int32 i)
{
	// b90d (Evan: "I meant the entire square, not the tiny circle - animate a corner square for the clip"): one clip panel, a square in
	// that player's profile corner (over its plate), springing out of the corner; the avatar portrait itself stays as it is.
	if (i != 0 || !G || !Canvas) return; // once per tick (called from the unit loop)
	UZoogiAnimeSubsystem* An = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr;
	int32 Who = -1;
	UObject* Want = nullptr; float Asp = 16.f / 9.f;
	if (An && An->MovieTexture && An->AviClip(Who)) Want = An->MovieTexture.Get(); // anime clip / skill summon
	else if (const UZoogiComicSliceOverlay* Co = G->GetWorld()->GetSubsystem<UZoogiComicSliceOverlay>())
	{
		UTexture2D* St = nullptr;
		if (Co->AviSlice(Who, St) && St) { Want = St; Asp = St->GetSurfaceWidth() / FMath::Max(1.f, St->GetSurfaceHeight()); }
		else Who = -1;
	}
	if (!Units[FMath::Clamp(Who, 0, 3)].Ring.IsValid()) { Want = nullptr; }
	// the panel widgets (rebuilt with the HUD)
	if (!ClipImg.IsValid() || ClipImg->GetParent() != Canvas)
	{
		UBorder* Fr = Box(ZCHex(TEXT("0B0C10")), FLinearColor::White, 5.f, 22.f);
		UImage* Im = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		for (UWidget* Wd : { (UWidget*)Fr, (UWidget*)Im })
		{
			Wd->SetVisibility(ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* Sl = Canvas->AddChildToCanvas(Wd)) { Sl->SetAnchors(FAnchors(0.f, 0.f)); Sl->SetZOrder(900); }
		}
		ClipFrame = Fr; ClipImg = Im; ClipK = 0.f; ClipRes = nullptr;
	}
	static float LastT = 0.f;
	const float Dt = FMath::Clamp(Time - LastT, 0.f, 0.1f); LastT = Time;
	if (Want) { ClipWho = Who; if (Want != ClipRes.Get()) { ClipRes = Want; ClipAsp = Asp; } }
	ClipK = FMath::Clamp(ClipK + (Want ? 1.f : -1.f) * Dt / 0.18f, 0.f, 1.f);
	UImage* Im = ClipImg.Get(); UBorder* Fr = ClipFrame.Get();
	if (ClipK <= 0.f || ClipWho < 0 || !Units[ClipWho].Ring.IsValid() || !ClipRes.IsValid())
	{
		if (Im && Im->GetVisibility() != ESlateVisibility::Collapsed) { Im->SetVisibility(ESlateVisibility::Collapsed); if (Fr) Fr->SetVisibility(ESlateVisibility::Collapsed); }
		if (!Want) ClipRes = nullptr;
		return;
	}
	// where: the square covers that player's profile corner (avatar + its plate), on the screen side the avatar sits on
	const FGeometry& Rg = Units[ClipWho].Ring->GetCachedGeometry();
	const FGeometry& Cg = Canvas->GetCachedGeometry();
	const FVector2D RP = Cg.AbsoluteToLocal(Rg.GetAbsolutePosition()), RS = Rg.GetAbsoluteSize() / FMath::Max(Cg.Scale, 0.01f);
	const FVector2D CS = Cg.GetLocalSize();
	const float Side = FMath::Clamp(RS.X * 2.5f, 160.f, CS.Y * 0.42f), Pad = RS.X * 0.12f;
	const bool bRight = RP.X + RS.X * 0.5f > CS.X * 0.5f;
	const FVector2D Pos(bRight ? FMath::Max(0.f, RP.X + RS.X + Pad - Side) : FMath::Min(RP.X - Pad, CS.X - Side), FMath::Max(0.f, RP.Y - Pad));
	const float E = 1.f - FMath::Pow(1.f - ClipK, 3.f);
	for (UWidget* Wd : { (UWidget*)Fr, (UWidget*)Im })
	{
		if (!Wd) continue;
		const bool bImg = Wd == Im;
		if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(Wd->Slot)) { Sl->SetPosition(Pos + (bImg ? FVector2D(6.f, 6.f) : FVector2D::ZeroVector)); Sl->SetSize(FVector2D(Side, Side) - (bImg ? FVector2D(12.f, 12.f) : FVector2D::ZeroVector)); }
		Wd->SetRenderTransformPivot(FVector2D(bRight ? 1.f : 0.f, 0.f)); // springs out of the corner
		Wd->SetRenderScale(FVector2D(FMath::Max(0.05f, E), FMath::Max(0.05f, E)));
		if (Wd->GetVisibility() != ESlateVisibility::HitTestInvisible) Wd->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Fr) Fr->SetBrushColor(ZCHex(TEXT("0B0C10")));
	if (Fr) Fr->SetContentColorAndOpacity(FLinearColor::White);
	if (Im && Im->GetBrush().GetResourceObject() != ClipRes.Get())
	{
		FSlateBrush Br;
		Br.SetResourceObject(ClipRes.Get());
		Br.ImageSize = FVector2D(Side, Side);
		Br.DrawAs = ESlateBrushDrawType::RoundedBox;
		Br.OutlineSettings = FSlateBrushOutlineSettings(FVector4(16.f, 16.f, 16.f, 16.f), FSlateColor(ZoogiUtil::PlayerColor(ClipWho)), 4.f);
		Br.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		if (ClipAsp >= 1.f) { const float Uw = 1.f / ClipAsp; Br.SetUVRegion(FBox2f(FVector2f(0.5f - Uw * 0.5f, 0.f), FVector2f(0.5f + Uw * 0.5f, 1.f))); }
		else Br.SetUVRegion(FBox2f(FVector2f(0.f, 0.5f - ClipAsp * 0.5f), FVector2f(1.f, 0.5f + ClipAsp * 0.5f)));
		Im->SetBrush(Br);
		UE_LOG(LogZoogi, Log, TEXT("ANIME %s -> P%d profile corner square (%.0f px)"), *ClipRes->GetName(), ClipWho + 1, Side);
	}
}

void UZoogiArenaHUDWidget::SetSPText(AZoogiGameMode* G, FUnitRefs& U, int32 i)
{
	UTextBlock* T = U.SP.Get();
	if (!T || !G) return;
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(i) : nullptr;
	if (!PS) { if (T->GetVisibility() != ESlateVisibility::Collapsed) T->SetVisibility(ESlateVisibility::Collapsed); return; }
	if (T->GetVisibility() == ESlateVisibility::Collapsed) T->SetVisibility(ESlateVisibility::HitTestInvisible);
	const int32 Max = FMath::Max(1, CS->Rules().Tune().EnergyPerTurn);
	const UZoogiFunSys* FunHp = UZoogiFunSys::Get(G);
	const FString Str = FunHp ? FString::Printf(TEXT("SP %d/%d  HP %d"), FMath::Max(0, PS->Energy), Max, FMath::RoundToInt(FunHp->PlayerHP(i))) : FString::Printf(TEXT("SP %d / %d"), FMath::Max(0, PS->Energy), Max); // b90c: HP beside SP
	if (!T->GetText().ToString().Equals(Str)) T->SetText(FText::FromString(Str));
}

void UZoogiArenaHUDWidget::BuildEpisode(float X, float Y, float Wd)
{
	// Oct 23 (Evan): kill dark EPISODE box / mid-screen gauge panel. Keep charge crystals;
	// phase/round text ABOVE crystals; caller places this stack at TOP only (under DRAW banner).
	GaugeSegs.Reset();
	GaugeStar.Reset();
	// phase / round line above crystals (YOUR TURN lives on TurnTagTxt in BuildTray)
	// b173 (Evan: "No need for the yellow text round and turns now that we have the finder"): gone with the big badges
	if (BigBadges()) RoundTxt.Reset(); else RoundTxt = Text(TEXT("ROUND 1 / 8"), X, Y, Wd, 28.f, 18.f, ZCHex(TEXT("FFE66B")), 2.f);
	RoundOfTxt.Reset();
	const float CryY = Y + 30.f;
	const float CrySpan = 5 * 28.f;
	const float CryX0 = X + (Wd - CrySpan) * 0.5f;
	// Oct 27 (Evan, build 9): charge crystals removed from the classic HUD.
	(void)CryY; (void)CryX0;
}

void UZoogiArenaHUDWidget::BuildTray()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;
	(void)G;

	// Oct 23 (Evan): no bottom card tray (detail / hand / ACTIVATE / AWAKEN / energy). Only the phase / DRAW banner.
	TrayY0 = H;
	TrayHidden = 0.f;
	TrayRoot.Reset();
	Cards.Reset();
	ActivateCost.Reset(); ActivateLbl.Reset(); ActivateDim.Reset();
	AwakenGlow.Reset(); AwakenPips.Reset();

	// ---- phase banner + progress (always on). Rest near the TOP (safe-area aware); slides up from mid on turn change.
	const FMargin SafePx = ZoogiFramed::SafeInsets(FVector2D(W * S, H * S));
	const float SafeTop = SafePx.Top / FMath::Max(S, 0.001f);
	const float BW = bPortrait ? 340.f : 360.f, BX = W * 0.5f - BW * 0.5f, BH = 44.f;
	// Tag line sits BannerY - 32; keep both under the notch and clear of the play area.
	BannerRestY = FMath::Max(SafeTop + 40.f, bPortrait ? 88.f : 72.f) + 32.f;
	const float BannerY = BannerRestY;
	BannerSlideFromY = FMath::Clamp(H * 0.42f - BannerRestY, 180.f, 420.f);
	BannerSlideT = 0.f; // animate up on first paint / rebuild
	UCanvasPanel* Outer = Canvas;
	UCanvasPanel* BannerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BannerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	At(BannerCanvas, 0.f, 0.f, W, H);
	BannerRoot = BannerCanvas;
	BannerShift = 0.f;
	Canvas = BannerCanvas;
	{
		// Oct 27 (Evan, build 9): the phase tracker plate (< DRAG TO AIM > + progress bar) is removed; only the YOUR TURN line stays.
		BannerTxt.Reset(); BannerFill.Reset();
		BannerFillW = BW - 86.f;
		if (BigBadges()) TurnTagTxt.Reset(); else TurnTagTxt = Text(TEXT(""), BX - 60.f, BannerY - 32.f, BW + 120.f, 30.f, 20.f, ZCHex(TEXT("FFE66B")), 2.f); // b173: no yellow YOUR TURN line
	}
	Canvas = Outer;
}

void UZoogiArenaHUDWidget::BuildCard(int32 CardSlot, float X, float Y, float Wd, float Ht)
{
	const AZoogiGameMode* G = GM();
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = CS ? CS->Rules().Player(LocalPlayer()) : nullptr;
	if (!PS || !PS->Hand.IsValidIndex(CardSlot)) return;
	const FZoogiCardInstance& Inst = PS->Hand[CardSlot];
	const FZoogiCardDef* D = CS->Def(Inst.CardId);
	FCardRefs R;
	R.Glow = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFC94D")), 7.f, 18.f), X - 7.f, Y - 7.f, Wd + 14.f, Ht + 14.f);
	R.Root = Stone(X, Y, Wd, Ht, 14.f, ZCHex(TEXT("0D0F14")), true);
	const float In = 12.f, NameH = Ht * 0.19f;
	if (UTexture2D* Art = CardArt(Inst.CardId)) Img(Art, X + In, Y + In, Wd - In * 2.f, Ht - In * 2.f - NameH, 8.f, true);
	At(Box(ZCHex(TEXT("1D2026")), ZCHex(TEXT("3C404A")), 2.f, 6.f), X + 9.f, Y + Ht - NameH - 10.f, Wd - 18.f, NameH);
	Text(D ? D->Name.ToUpper() : Inst.CardId.ToString().ToUpper(), X + 9.f, Y + Ht - NameH - 10.f, Wd - 18.f, NameH, FMath::Clamp(Wd * 0.085f, 9.5f, 22.f), FLinearColor::White, 2.f);
	const float CB = FMath::Clamp(Wd * 0.24f, 30.f, 46.f);
	At(Box(AHSapphire, ZCHex(TEXT("D8E2F5")), 3.f, CB * 0.5f), X + 2.f, Y + 2.f, CB, CB);
	Text(FString::FromInt(D ? D->Cost : 0), X + 2.f, Y + 2.f, CB, CB, CB * 0.58f, FLinearColor::White, 2.f);
	R.Dim = At(Box(FLinearColor(0.02f, 0.02f, 0.03f, 0.6f), FLinearColor::Transparent, 0.f, 14.f), X, Y, Wd, Ht);
	Hit(X, Y, Wd, Ht, [this, CardSlot]() { PressCard(CardSlot); });
	Cards.Add(R);
	(void)G;
}

void UZoogiArenaHUDWidget::BuildDetail(float X, float Y, float Wd, float Ht)
{
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = CS ? CS->Rules().Player(LocalPlayer()) : nullptr;
	Stone(X, Y, Wd, Ht, 22.f, AHStone.CopyWithNewOpacity(0.98f), false);
	if (!PS || PS->Hand.Num() == 0)
	{
		Text(TEXT("Cards you draw appear here. Tap a card to read it, ACTIVATE to commit, AWAKEN when the four pips glow."), X + 24.f, Y + 20.f, Wd - (bPortrait ? 330.f : 48.f), Ht - 40.f, 20.f, AHParch, 0.f, 0)->SetAutoWrapText(true);
		return;
	}
	DetailSlot = FMath::Clamp(DetailSlot, 0, PS->Hand.Num() - 1);
	const FZoogiCardInstance& Inst = PS->Hand[DetailSlot];
	const FZoogiCardDef* D = CS->Def(Inst.CardId);
	const float TW = 124.f, TH = 150.f, TX = X + 24.f, TY = Y + (Ht - TH) * 0.5f;
	At(Box(ZCHex(TEXT("9AA0AC")), ZCDark, 2.f, 10.f), TX - 5.f, TY - 5.f, TW + 10.f, TH + 10.f);
	if (UTexture2D* Art = CardArt(Inst.CardId)) Img(Art, TX, TY, TW, TH, 6.f, true);
	const float LX = TX + TW + 20.f, LW = bPortrait ? (Wd - (LX - X) - 320.f) : (Wd - (LX - X) - 20.f);
	Text(D ? D->Name.ToUpper() : Inst.CardId.ToString().ToUpper(), LX, Y + 22.f, LW + 120.f, 44.f, 30.f, FLinearColor::White, 3.f, 0);
	const FString Tag = D ? FString(ZoogiCards::TypeName(D->Type)).ToUpper() : FString();
	At(Box(ZCHex(TEXT("4A33A0")), ZCHex(TEXT("9D7CFF")), 2.f, 14.f), LX, Y + 72.f, FMath::Max(110.f, 18.f * Tag.Len() + 30.f), 30.f);
	Text(Tag, LX, Y + 72.f, FMath::Max(110.f, 18.f * Tag.Len() + 30.f), 30.f, 16.f, ZCHex(TEXT("ECE4FF")), 0.f);
	UTextBlock* Desc = Text(D ? D->Rule : FString(), LX, Y + 110.f, LW, Ht - 124.f, 19.f, ZCHex(TEXT("D9DCE3")), 0.f, 0);
	if (Desc) Desc->SetAutoWrapText(true);
}


void UZoogiArenaHUDWidget::RefreshChromeCam()
{
	const AZoogiGameMode* G = GM();
	const bool bTac = G && !G->bCinematicCam && G->bBirdEyeCam; // lit = Tac bird's-eye, unlit = Normal (Oct 24)
	const FLinearColor Plate = bTac ? ZCHex(TEXT("1F5BE0")).CopyWithNewOpacity(0.96f) : ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.95f);
	const FLinearColor Edge = bTac ? ZCHex(TEXT("9FE4FF")) : ZCHex(TEXT("5C6170"));
	if (UBorder* B = Cast<UBorder>(TacPlate.Get()))
		B->SetBrush(ZoogiUIStyle::RoundBrush(Plate, Edge, 3.f * S, 36.f * S));
	if (UBorder* Glow = Cast<UBorder>(TacGlow.Get()))
	{
		Glow->SetBrush(ZoogiUIStyle::RoundBrush(ZCHex(TEXT("4FA8FF")).CopyWithNewOpacity(bTac ? 0.35f : 0.f), FLinearColor::Transparent, 0.f, 42.f * S));
		(void)bTac; // Oct 28: visibility belongs to TickSplit (the row shows / hides with the profiles); the brush alpha shows the lit state
	}
	if (TacLbl.IsValid())
	{
		TacLbl->SetColorAndOpacity(FSlateColor(bTac ? ZCHex(TEXT("F2F8FF")) : AHParch));
		TacLbl->SetText(FText::FromString(G && G->bCinematicCam ? TEXT("CINE") : (bTac ? TEXT("TAC") : TEXT("CAM")))); // Oct 27: the button shows the view it is in
	}
}

void UZoogiArenaHUDWidget::BuildChromeRow(float CenterX, float Y)
{
	// Oct 28 (Evan): [DEV] [TAC] [settings] centred on CenterX: TAC in the middle, the cog to its right; the DEV button (its own
	// Slate overlay, ZoogiDevMode) lines up left of TAC. Kind 4: slides / fades in with the profiles (avatar tap or match start)
	// and leaves with them; its tap targets collapse while it is hidden.
	float Gap = 14.f;
	if (GConfig) GConfig->GetFloat(TEXT("ZoogiRoll.ArenaHUD"), TEXT("ChromeTacGap"), Gap, GGameIni);
	Gap = FMath::Clamp(Gap, 12.f, 28.f);
	const float Sz = 72.f;
	const float TacX = CenterX - Sz * 0.5f, CogX = TacX + Sz + Gap;
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome"))) { BuildChromeCluster(CogX, Y, true, false); (void)TacX; return; } // b81: MENU never fades away with the profiles
	const int32 Mark = Canvas->GetChildrenCount();
	BuildChromeCluster(CogX, Y, true, false);
	(void)TacX;
	MarkSplit(Mark, FVector2D(0.f, -46.f), 4);
}

void UZoogiArenaHUDWidget::BuildChromeCluster(float SettingsX, float SettingsY, bool bSettingsOnRight, bool bMark)
{
	// One Settings cog + Tactical cam beside it (gap >= 14 design px). Kind 2: stays hittable when HUD minimizes.
	// Cam: ONLY MenuAction("CamStyle") - same as FramedHUD OnCam / C key. Do not poke bCinematicCam.
	float Gap = 14.f, CogOffX = 0.f, CogOffY = 0.f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("ZoogiRoll.ArenaHUD"), TEXT("ChromeTacGap"), Gap, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.ArenaHUD"), TEXT("ChromeCogOffX"), CogOffX, GGameIni);
		GConfig->GetFloat(TEXT("ZoogiRoll.ArenaHUD"), TEXT("ChromeCogOffY"), CogOffY, GGameIni);
	}
	Gap = FMath::Clamp(Gap, 12.f, 28.f);
	const float Sz = 72.f, HitPad = 8.f;
	const float CogX = SettingsX + CogOffX, CogY = SettingsY + CogOffY;
	const float TacX = bSettingsOnRight ? (CogX - Gap - Sz) : (CogX + Sz + Gap);
	const float TacY = CogY;

	const int32 Mark = Canvas->GetChildrenCount();

	// b79 (Evan: "turn the cog button into an actual P5 menu button, put DEV and CAM inside it"): a tilted black / red / white MENU
	// plate opens the P5 pause menu (DEV MODE + CAMERA rows live there now). -ZoogiOldChrome = the cog + TAC/CAM button
	static const bool bOldChrome = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome"));
	if (!bOldChrome)
	{
		// b81 (Evan: "make MENU selectable no matter what, straight, label not tilted"): no tilt, never part of the profile fade
		const float MW = 150.f, MH = 62.f, MX = CogX + Sz - MW, MY = CogY + (Sz - MH) * 0.5f;
		At(Box(ZCHex(TEXT("0A0A0C")), FLinearColor::Transparent, 0.f, 0.f), MX + 6.f, MY + 6.f, MW, MH);
		At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 0.f), MX - 4.f, MY - 4.f, MW + 8.f, MH + 8.f);
		UWidget* Plate = At(Box(ZCHex(TEXT("E0101E")), FLinearColor::Transparent, 0.f, 0.f), MX, MY, MW, MH);
		Text(TEXT("MENU"), MX, MY - 2.f, MW, MH, 34.f, FLinearColor::White, 2.f, 1);
		ZoogiArenaHUD::GMenuPlate = Plate;
		ZoogiArenaHUD::GMenuOwner = this;
		Hit(MX - HitPad, MY - HitPad, MW + HitPad * 2.f, MH + HitPad * 2.f, [this]() { SetPaused(!bPaused); });
		if (bMark) MarkSplit(Mark, FVector2D::ZeroVector, 2);
		(void)TacX; (void)TacY;
		return;
	}
	// ---- Settings cog (circular dark plate + icon_gear / gear.png) - opens BuildPause
	At(Box(ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.96f), ZCHex(TEXT("8A90A0")), 3.f, Sz * 0.5f), CogX, CogY, Sz, Sz);
	At(Box(ZCHex(TEXT("0B0D12")).CopyWithNewOpacity(0.55f), FLinearColor::Transparent, 0.f, (Sz - 10.f) * 0.5f), CogX + 5.f, CogY + 5.f, Sz - 10.f, Sz - 10.f);
	UTexture2D* Gear = FileTex(TEXT("Data/MenuArt"), TEXT("icon_gear"));
	if (!Gear) Gear = FileTex(TEXT("ZoogiUI/assets/icons"), TEXT("gear"));
	if (Gear) Img(Gear, CogX + 16.f, CogY + 16.f, Sz - 32.f, Sz - 32.f, 0.f, false);
	else Text(TEXT("SET"), CogX, CogY, Sz, Sz, 20.f, AHParch, 2.f, 1);
	Hit(CogX - HitPad, CogY - HitPad, Sz + HitPad * 2.f, Sz + HitPad * 2.f, [this]() { SetPaused(!bPaused); });

	// ---- Tactical cam (active glow when !bCinematicCam)
	TacGlow = At(Box(ZCHex(TEXT("4FA8FF")).CopyWithNewOpacity(0.35f), FLinearColor::Transparent, 0.f, 42.f), TacX - 6.f, TacY - 6.f, Sz + 12.f, Sz + 12.f);
	TacPlate = At(Box(ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.95f), ZCHex(TEXT("5C6170")), 3.f, Sz * 0.5f), TacX, TacY, Sz, Sz);
	TacLbl = Text(TEXT("TAC"), TacX, TacY, Sz, Sz, 22.f, AHParch, 2.f, 1);
	ZoogiArenaHUD::GTacPlate = TacPlate.Get(); // Oct 27: the DEV button lines up with it
	LastCamKey = -1;
	Hit(TacX - HitPad, TacY - HitPad, Sz + HitPad * 2.f, Sz + HitPad * 2.f, [this]()
	{
		if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("CamStyle"), 0);
		RefreshChromeCam();
	});

	if (bMark) MarkSplit(Mark, FVector2D::ZeroVector, 2);
	RefreshChromeCam();
}

// b70 (Evan: "Restyle pause menu to P5 style"): black / red / white slanted plates in a loose fan, a big tilted PAUSE title,
// red sliders on black. Same actions as the old stone panel (-ZoogiOldPause).
void UZoogiArenaHUDWidget::BuildPauseP5()
{
	const float W = Design.X, H = Design.Y;
	const FLinearColor Red = ZCHex(TEXT("E0101E")), Blk = ZCHex(TEXT("0A0A0C")), Wht = FLinearColor::White;
	auto Tilt = [](UWidget* Wg, float Deg) { if (Wg) { Wg->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); Wg->SetRenderTransformAngle(Deg); } return Wg; };
	At(Box(FLinearColor(0.02f, 0.f, 0.01f, 0.82f), FLinearColor::Transparent, 0.f, 0.f), 0.f, 0.f, W, H);
	Hit(0.f, 0.f, W, H, []() {}); // swallow taps behind the panel
	// a huge red slash across the screen + a thin white one
	Tilt(At(Box(Red.CopyWithNewOpacity(0.9f), FLinearColor::Transparent, 0.f, 0.f), -W * 0.25f, H * 0.22f, W * 1.5f, 150.f), -14.f);
	Tilt(At(Box(Wht.CopyWithNewOpacity(0.85f), FLinearColor::Transparent, 0.f, 0.f), -W * 0.25f, H * 0.22f + 168.f, W * 1.5f, 10.f), -14.f);
	const float PW = FMath::Min(620.f, W - 60.f), X0 = W * 0.5f - PW * 0.5f;
	float Y = FMath::Max(40.f, H * 0.5f - 560.f);
	// title: white plate, black shadow plate, red PAUSE
	Tilt(At(Box(Blk, FLinearColor::Transparent, 0.f, 0.f), X0 + 18.f, Y + 14.f, PW * 0.7f, 110.f), -7.f);
	Tilt(At(Box(Wht, FLinearColor::Transparent, 0.f, 0.f), X0, Y, PW * 0.7f, 110.f), -7.f);
	Tilt(Text(TEXT("PAUSE"), X0, Y, PW * 0.7f, 110.f, 78.f, Red, 0.f), -7.f);
	Y += 150.f;
	struct FRow { const TCHAR* Label; int32 Style; TFunction<void()> Fn; };
	TArray<FRow> Rows;
	Rows.Add({ TEXT("RESUME"), 0, [this]() { SetPaused(false); } });
	if (const AZoogiGameMode* Gt = GM()) if (Gt->bSoloTraining && !Gt->bRoyaleMode)
	{	// b133 (Evan: "option in training pause menu to add CPU opponents"): each tap adds one (max 3 + you), the match restarts
		static FString BotLabel;
		const int32 Nb = FMath::Clamp(Gt->MenuNumPlayers - 1, 0, 3);
		BotLabel = Nb >= 3 ? FString(TEXT("CPU OPPONENTS: 3  (TAP = NONE)")) : FString::Printf(TEXT("CPU OPPONENTS: %d  (TAP = +1)"), Nb);
		Rows.Add({ *BotLabel, 2, [this, Nb]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("TrainBots"), (Nb + 1) % 4); } });
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome")))
	{	// b79: the camera + DEV buttons moved in here from the HUD
		const AZoogiGameMode* Gc = GM();
		static FString CamLabel; CamLabel = FString::Printf(TEXT("CAMERA: %s"), (Gc && Gc->bCinematicCam) ? TEXT("CINEMATIC") : ((Gc && Gc->bBirdEyeCam) ? TEXT("TACTICAL") : TEXT("NORMAL")));
		Rows.Add({ *CamLabel, 2, [this]() { if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("CamStyle"), 0); BuiltSize = FVector2D::ZeroVector; } });
		Rows.Add({ ZoogiLayout::IsDevMode() ? TEXT("CLOSE DEV MODE") : TEXT("DEV MODE"), 1, [this]() { SetPaused(false); AZoogiDevMode::ToggleFromMenu(GetWorld()); } }); // b167: opens the real panel (was: only a flag)
	}
	Rows.Add({ TEXT("RESTART MATCH"), 1, [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Rematch"), 0); } });
	Rows.Add({ TEXT("CHANGE ZOOGI"), 2, [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("ChangeZoogi"), 0); } });
	Rows.Add({ TEXT("MAIN MENU"), 1, [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Menu"), 0); } });
	Rows.Add({ TEXT("EXIT GAME"), 2, [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Quit"), 0); } });
	const float RowStep = Rows.Num() > 7 ? 72.f : (Rows.Num() > 5 ? 80.f : 92.f); // b79: 7 rows fit; b133: 8 in training
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const float Ang = (i % 2 == 0) ? -5.f : -2.5f, Dx = (i % 2 == 0) ? 0.f : 46.f;
		const float BW = PW - 60.f - Dx * 0.5f, BX = X0 + 20.f + Dx, BH = RowStep - 16.f;
		const FLinearColor Face = Rows[i].Style == 0 ? Red : (Rows[i].Style == 1 ? Wht : Blk);
		const FLinearColor Ink = Rows[i].Style == 1 ? Blk : Wht;
		const FLinearColor Shadow = Rows[i].Style == 0 ? Blk : Red;
		Tilt(At(Box(Shadow, FLinearColor::Transparent, 0.f, 0.f), BX + 10.f, Y + 10.f, BW, BH), Ang);
		Tilt(At(Box(Face, Rows[i].Style == 2 ? Wht : FLinearColor::Transparent, Rows[i].Style == 2 ? 3.f : 0.f, 0.f), BX, Y, BW, BH), Ang);
		Tilt(Text(Rows[i].Label, BX + 24.f, Y, BW - 24.f, BH, Rows[i].Style == 0 ? 40.f : 32.f, Ink, 0.f, 0), Ang);
		Hit(BX, Y, BW, BH, Rows[i].Fn);
		Y += RowStep;
	}
	// audio: white labels, red fills on black tracks, black / white +- chips
	Y += 10.f;
	Tilt(Text(TEXT("AUDIO"), X0 + 20.f, Y, 200.f, 40.f, 26.f, Red, 2.f, 0), -4.f);
	Y += 46.f;
	const float SX = X0 + 20.f, SW = PW - 40.f;
	auto AudioRow = [&](const TCHAR* Label, const TCHAR* Key, int32 DefaultPct)
	{
		const int32 Pct = ZoogiScoringUI::GetAudioPct(Key, DefaultPct);
		Text(Label, SX, Y, 120.f, 44.f, 20.f, Wht, 1.f, 0);
		Tilt(At(Box(Blk, Wht, 2.f, 0.f), SX + 130.f, Y + 12.f, SW - 300.f, 20.f), -2.f);
		Tilt(At(Box(Red, FLinearColor::Transparent, 0.f, 0.f), SX + 133.f, Y + 15.f, FMath::Max(4.f, (SW - 306.f) * (Pct / 100.f)), 14.f), -2.f);
		Text(FString::Printf(TEXT("%d%%"), Pct), SX + SW - 166.f, Y, 66.f, 44.f, 20.f, Wht, 1.f, 2);
		for (int32 k = 0; k < 2; ++k)
		{
			const float Bx = SX + SW - (k == 0 ? 92.f : 46.f);
			Tilt(At(Box(k == 0 ? Blk : Red, Wht, 2.f, 0.f), Bx, Y + 2.f, 40.f, 40.f), -6.f);
			Text(k == 0 ? TEXT("-") : TEXT("+"), Bx, Y + 2.f, 40.f, 40.f, 26.f, Wht, 1.f);
			const int32 NewPct = k == 0 ? FMath::Max(0, Pct - 10) : FMath::Min(100, Pct + 10);
			Hit(Bx, Y + 2.f, 40.f, 40.f, [this, Key, NewPct]() { ZoogiScoringUI::SetAudioPct(Key, NewPct); ZoogiScoringUI::ApplyAudio(); BuiltSize = FVector2D::ZeroVector; });
		}
		Y += 52.f;
	};
	AudioRow(TEXT("MASTER"), TEXT("ClashMaster"), 50);
	AudioRow(TEXT("SFX"), TEXT("ClashSfx"), 50);
	AudioRow(TEXT("MUSIC"), TEXT("ClashMusic"), 50);
	const bool bMir = ZoogiScoringUI::MirrorLayout();
	Tilt(At(Box(bMir ? Red : Blk, Wht, 2.f, 0.f), SX, Y + 6.f, SW, 58.f), -3.f);
	Tilt(Text(bMir ? TEXT("HUD LAYOUT: MIRRORED (L/R)") : TEXT("HUD LAYOUT: DEFAULT"), SX, Y + 6.f, SW, 58.f, 22.f, Wht, 0.f), -3.f);
	Hit(SX, Y + 6.f, SW, 58.f, [this, bMir]() { ZoogiScoringUI::SetMirrorLayout(!bMir); BuiltSize = FVector2D::ZeroVector; });
}

void UZoogiArenaHUDWidget::BuildPause()
{
	static const bool bOldPause = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPause"));
	if (!bOldPause) { BuildPauseP5(); return; }
	const float W = Design.X, H = Design.Y;
	At(Box(FLinearColor(0.01f, 0.008f, 0.006f, 0.72f), FLinearColor::Transparent, 0.f, 0.f), 0.f, 0.f, W, H);
	Hit(0.f, 0.f, W, H, []() {}); // swallow taps behind the panel
	// taller panel: action rows + #36 audio sliders + #37 L/R mirror (pause Hit wiring untouched)
	const float PW = FMath::Min(640.f, W - 80.f), PH = 980.f, PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - PH * 0.5f;
	Stone(PX, PY, PW, PH, 30.f, AHStone, true);
	Img(Tex(TEXT("crest")), W * 0.5f - 42.f, PY + 2.f, 84.f, 34.f, 0.f, false);
	Text(TEXT("PAUSED"), PX, PY + 34.f, PW, 70.f, 46.f, ZCHex(TEXT("F2C14E")), 4.f);
	struct FRow { const TCHAR* Label; FLinearColor Face; TFunction<void()> Fn; };
	TArray<FRow> Rows;
	Rows.Add({ TEXT("RESUME"), AHSapphire, [this]() { SetPaused(false); } });
	Rows.Add({ TEXT("RESTART MATCH"), ZCHex(TEXT("4A4D55")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Rematch"), 0); } });
	Rows.Add({ TEXT("CHANGE ZOOGI"), ZCHex(TEXT("2F5BB0")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("ChangeZoogi"), 0); } });
	Rows.Add({ TEXT("MAIN MENU"), ZCHex(TEXT("4A4D55")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Menu"), 0); } });
	Rows.Add({ TEXT("EXIT GAME"), ZCHex(TEXT("C2362F")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Quit"), 0); } });
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const float BY = PY + 110.f + i * 88.f, BX = PX + 60.f, BWd = PW - 120.f;
		Stone(BX, BY, BWd, 74.f, 24.f, Rows[i].Face, true);
		Text(Rows[i].Label, BX, BY, BWd, 74.f, 28.f, FLinearColor::White, 4.f);
		Hit(BX, BY, BWd, 74.f, Rows[i].Fn);
	}
	// #36 audio sliders (Master / SFX / Music) - +/- only; does not rework pause Hit
	{
		const float SX = PX + 60.f, SW = PW - 120.f;
		float SY = PY + 110.f + Rows.Num() * 88.f + 8.f;
		Text(TEXT("AUDIO"), SX, SY, SW, 36.f, 22.f, ZCHex(TEXT("F0C46A")), 2.f, 0);
		SY += 40.f;
		auto AudioRow = [&](const TCHAR* Label, const TCHAR* Key, int32 DefaultPct)
		{
			const int32 Pct = ZoogiScoringUI::GetAudioPct(Key, DefaultPct);
			Text(Label, SX, SY, 110.f, 40.f, 18.f, AHParch, 1.f, 0);
			At(Box(ZCHex(TEXT("0D0F12")), ZCHex(TEXT("3B3E45")), 2.f, 6.f), SX + 120.f, SY + 12.f, SW - 280.f, 16.f);
			At(Box(AHSapphire, FLinearColor::Transparent, 0.f, 4.f), SX + 122.f, SY + 14.f, FMath::Max(4.f, (SW - 284.f) * (Pct / 100.f)), 12.f);
			Text(FString::Printf(TEXT("%d%%"), Pct), SX + SW - 150.f, SY, 60.f, 40.f, 18.f, FLinearColor::White, 1.f, 2);
			Stone(SX + SW - 88.f, SY + 2.f, 36.f, 36.f, 10.f, ZCHex(TEXT("2A2D34")), true);
			Text(TEXT("-"), SX + SW - 88.f, SY + 2.f, 36.f, 36.f, 22.f, FLinearColor::White, 2.f);
			Hit(SX + SW - 88.f, SY + 2.f, 36.f, 36.f, [this, Key, Pct]()
			{
				ZoogiScoringUI::SetAudioPct(Key, FMath::Max(0, Pct - 10));
				ZoogiScoringUI::ApplyAudio();
				BuiltSize = FVector2D::ZeroVector;
			});
			Stone(SX + SW - 44.f, SY + 2.f, 36.f, 36.f, 10.f, ZCHex(TEXT("2A2D34")), true);
			Text(TEXT("+"), SX + SW - 44.f, SY + 2.f, 36.f, 36.f, 22.f, FLinearColor::White, 2.f);
			Hit(SX + SW - 44.f, SY + 2.f, 36.f, 36.f, [this, Key, Pct]()
			{
				ZoogiScoringUI::SetAudioPct(Key, FMath::Min(100, Pct + 10));
				ZoogiScoringUI::ApplyAudio();
				BuiltSize = FVector2D::ZeroVector;
			});
			SY += 48.f;
		};
		AudioRow(TEXT("MASTER"), TEXT("ClashMaster"), 50);
		AudioRow(TEXT("SFX"), TEXT("ClashSfx"), 50);
		AudioRow(TEXT("MUSIC"), TEXT("ClashMusic"), 50);
		// #37 battle HUD L/R mirror toggle
		const bool bMir = ZoogiScoringUI::MirrorLayout();
		Stone(SX, SY + 4.f, SW, 56.f, 16.f, bMir ? AHSapphire : ZCHex(TEXT("2A2D34")), true);
		Text(bMir ? TEXT("HUD LAYOUT: MIRRORED (L/R)") : TEXT("HUD LAYOUT: DEFAULT"), SX, SY + 4.f, SW, 56.f, 20.f, FLinearColor::White, 2.f);
		Hit(SX, SY + 4.f, SW, 56.f, [this, bMir]()
		{
			ZoogiScoringUI::SetMirrorLayout(!bMir);
			BuiltSize = FVector2D::ZeroVector; // force rebuild so CxA/CxB swap
		});
	}
}

void UZoogiArenaHUDWidget::PressCard(int32 CardSlot)
{
	DetailSlot = CardSlot;
	BuiltHand.Reset(); // detail panel follows the tap
	AZoogiGameMode* G = GM();
	if (!G) return;
	if (G->CardsPlayWindowOpen()) G->MenuAction(TEXT("CardPlay"), CardSlot); // tap = play (no ACTIVATE), pre-launch or mid-roll
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD card %d tapped"), CardSlot);
}

void UZoogiArenaHUDWidget::PressActivate()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD ACTIVATE"));
	G->MenuAction(TEXT("CardCommit"), 0);
	BuiltHand.Reset();
}

void UZoogiArenaHUDWidget::PressAwaken()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD AWAKEN (charge %d/%d)"), G->AwakenCharge[FMath::Clamp(LocalPlayer(), 0, 3)], AZoogiGameMode::AwakenChargeMax);
	G->MenuAction(TEXT("AwakenUse"), 0);
}

void UZoogiArenaHUDWidget::SetPaused(bool bOn)
{
	if (bPaused == bOn) return;
	bPaused = bOn;
	UGameplayStatics::SetGamePaused(this, bOn);
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD %s"), bOn ? TEXT("paused") : TEXT("resumed"));
}

// ------------------------------------------------------------------------------------------------ refresh
FString UZoogiArenaHUDWidget::HandSignature() const
{
	const AZoogiGameMode* G = GM();
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (G && CS && G->CardsOn()) ? CS->Rules().Player(LocalPlayer()) : nullptr;
	FString Sig = FString::Printf(TEXT("%d|%d|%d|"), G ? G->Balls.Num() : 0, DetailSlot, LocalPlayer());
	if (PS) for (const FZoogiCardInstance& I : PS->Hand) Sig += FString::FromInt(I.InstanceId) + TEXT(",");
	Sig += BoardTrayCompact() ? TEXT("|c") : TEXT("|f"); // Oct 22: rebuild when the portrait tray switches compact / full
	return Sig;
}

bool UZoogiArenaHUDWidget::TrayWanted() const
{
	// Oct 23 (Evan): bottom battle card tray removed - GUARD detail / hand / ACTIVATE / energy no longer cover the arena.
	return false;
}

void UZoogiArenaHUDWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	Time += Dt;
	const bool bOn = ZoogiArenaHUD::IsActive(GetWorld());
	// never collapse this widget itself: a collapsed UserWidget stops ticking and would never see the match start (menu -> match).
	// Only the content canvas hides.
	if (Canvas) Canvas->SetVisibility(bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bOn) { if (bPaused) SetPaused(false); MatchClock = 0.f; BuiltSize = FVector2D::ZeroVector; TopOnT = -1.f; return; }
	const FVector2D Px = Geo.GetLocalSize();
	if (Px.X < 10.f || Px.Y < 10.f) return;
	const bool bP = Px.Y >= Px.X;
	const FString Sig = HandSignature();
	const bool bOver = GM() && GM()->Phase == EZoogiPhase::GameOver;
	if (!Px.Equals(BuiltSize, 1.f) || bP != bPortrait || Sig != BuiltHand || bPaused != bBuiltPause || bOver != bBuiltOver)
	{
		bPortrait = bP;
		bBoard = bPortrait && FileTex(TEXT("Data/MenuArt"), TEXT("board_ref")) != nullptr;
		S = bBoard ? Px.X / 941.f : bPortrait ? Px.X / 1080.f : Px.Y / 1080.f;
		Design = Px / S;
		BuiltSize = Px; BuiltHand = Sig;
		Build();
		for (int32 i = 0; i < 4; ++i) ZoogiArenaHUD::GAvatar[i] = Units[i].Ring; // Oct 28: comic bubbles stream out of these
	}
	if (bBoard) RefreshBoard(Dt); else Refresh(Dt);
	TickSplit(Dt);
	// Oct 22: HUD rumble. Impact shake (decays ~0.28 s, scaled by impulse; [ZoogiRoll.FrameShake]) + a growing jitter while
	// the slingshot is pulled back. Render translation only (layout and hit rects untouched). Settings > FRAME SHAKE.
	if (Canvas)
	{
		FVector2D Off = ZoogiFramed::FrameShakeOffset(Dt);
		const float Pull = ZoogiHaptics::AimPull();
		if (Pull > 0.f && ZoogiFramed::FrameShakeEnabled())
		{
			const float A = (0.6f + 2.6f * Pull * Pull) * ZoogiHaptics::AimHudMul(); // 1080p px; stronger when vibratorless
			Off += FVector2D(FMath::Sin(Time * (55.f + 40.f * Pull)), FMath::Cos(Time * (47.f + 36.f * Pull))) * A;
		}
		Off *= S;
		Canvas->SetRenderTranslation(Off);
		if (Off.Size() > 0.5f && Time - HudShakeLog > 0.25f)
		{
			HudShakeLog = Time;
			UE_LOG(LogZoogi, Log, TEXT("HUDSHAKE offset %.1f,%.1f px (aim pull %.2f)"), Off.X, Off.Y, Pull);
		}
	}
	TickTestDriver(Dt);
}

// ------------------------------------------------------------------------------------------------ test driver
void UZoogiArenaHUDWidget::TestShot(const TCHAR* Name)
{
	const AZoogiGameMode* G = GM();
	if (!G || G->ShotDir.IsEmpty()) return;
	const FString P = FPaths::Combine(G->ShotDir, FString::Printf(TEXT("ahud_%s_%02d_%s.png"), bPortrait ? TEXT("p") : TEXT("l"), ++TestShotN, Name));
	FScreenshotRequest::RequestScreenshot(P, true, false);
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD TEST shot %s"), *P);
}

void UZoogiArenaHUDWidget::TickTestDriver(float Dt)
{
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiArenaHUDTest"));
	AZoogiGameMode* G = GM();
	if (!bTest || !G || bPaused) return;
	const AZoogiBall* B = G->GetActiveBall();
	// one key per (turn, active player, stage / step) so each beat restarts its own timer
	const int32 Key = G->TurnCounter * 64 + (B ? B->PlayerIndex : 7) * 8 + (G->Phase == EZoogiPhase::Aiming ? (int32)G->Stage : 5 + (int32)G->Phase % 3);
	if (Key != TestTurnKey) { TestTurnKey = Key; TestT = 0.f; TestDone = 0; }
	TestT += Dt;
	auto Once = [this](uint32 Bit) { if (TestDone & Bit) return false; TestDone |= Bit; return true; };
	const bool bMine = B && !B->bCPU && B->PlayerIndex == LocalPlayer();
	if (G->Phase == EZoogiPhase::GameOver) { if (TestT > 2.5f && Once(1)) TestShot(TEXT("match_over")); return; }
	if (G->Phase == EZoogiPhase::Rolling) { if (TestT > 0.9f && TestRolls < 4 && Once(1)) { ++TestRolls; TestShot(TEXT("roll_active")); } return; }
	if (G->Phase != EZoogiPhase::Aiming || !B) return;
	if (!bMine) { if (G->Stage == EZoogiStage::Battle && TestT > 0.6f && TestShotN < 3 && Once(1)) TestShot(TEXT("cpu_turn")); return; }
	if (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone)
	{
		if (TestT > 1.0f && Once(1)) TestShot(TEXT("standby_open"));
		if (TestT > 1.3f && Once(2) && G->CardsAwakenReady(B->PlayerIndex)) { PressAwaken(); }
		if (TestT > 1.6f && Once(4))
		{
			// tap the first affordable card through the HUD's own handler
			const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
			const FZoogiCardPlayerState* PS = CS ? CS->Rules().Player(B->PlayerIndex) : nullptr;
			for (int32 k = 0; PS && k < PS->Hand.Num(); ++k)
				if (const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId)) if (D->Cost <= PS->Energy) { PressCard(k); break; }
		}
		if (TestT > 2.3f && Once(8)) TestShot(TEXT("card_selected"));
		if (TestT > 3.0f && Once(16)) PressActivate();
		return;
	}
	if (G->Stage == EZoogiStage::Battle && G->BattleStep == EZoogiBattleStep::Aim)
	{
		// cards optional before aiming: odd turns pick a card + ACTIVATE while aiming, even turns fire with no card
		static int32 LastTurn = -1, MyTurns = 0; // alternates per human turn
		if (G->TurnCounter != LastTurn) { LastTurn = G->TurnCounter; ++MyTurns; }
		const bool bPlayCard = MyTurns % 2 == 1;
		if (bPlayCard && G->CardsWindowOpen() && TestT > 0.3f && Once(32))
		{
			const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
			const FZoogiCardPlayerState* PS = CS ? CS->Rules().Player(B->PlayerIndex) : nullptr;
			for (int32 k = 0; PS && k < PS->Hand.Num(); ++k)
				if (const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId)) if (D->Cost <= PS->Energy) { PressCard(k); break; }
		}
		if (bPlayCard && TestT > 0.6f && Once(64)) { TestShot(TEXT("aim_card_selected")); }
		// tap = play: no ACTIVATE step
		if (!bPlayCard && (MyTurns % 4) == 0 && G->CardsWindowOpen() && TestT > 0.3f && Once(512))
		{
			const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
			const FZoogiCardPlayerState* PS = CS ? CS->Rules().Player(B->PlayerIndex) : nullptr;
			for (int32 k = 0; PS && k < PS->Hand.Num(); ++k)
				if (const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId)) if (D->Cost <= PS->Energy) { PressCard(k); break; } // tapped, never ACTIVATEd
		}
		if (!bPlayCard && TestT > 0.6f && Once(256)) TestShot(TEXT("aim_no_card"));
		if (TestT > 0.8f && Once(1)) G->MenuAction(TEXT("AimNext"), 0);
		if (TestT > 1.4f && Once(2)) TestShot(TEXT("aim"));
		if (TestT > 1.8f && Once(4)) G->MenuAction(TEXT("AimConfirm"), 0);
		if (TestT > 4.0f && Once(8)) { G->MenuAction(TEXT("AimNext"), 0); G->MenuAction(TEXT("AimConfirm"), 0); } // retry if the lock was lost
	}
}

void UZoogiArenaHUDWidget::Refresh(float Dt)
{
	RefreshChromeCam();
	AZoogiGameMode* G = GM();
	if (!G) return;
	const int32 Me = LocalPlayer();
	const bool bLive = G->Phase != EZoogiPhase::GameOver && !bPaused;
	if (bLive && G->Phase != EZoogiPhase::Forming && !(ZoogiScoringUI::PauseClockWhileRolling() && G->Phase == EZoogiPhase::Rolling)) MatchClock += Dt;
	auto Show = [](TWeakObjectPtr<UWidget> W, bool b) { if (W.IsValid()) W->SetVisibility(b ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); };
	auto SetFill = [](TWeakObjectPtr<UWidget> W, const FLinearColor& C) { if (UBorder* B = Cast<UBorder>(W.Get())) B->SetBrushColor(C); };

	// ---- units
	int32 Top = 100;
	for (const AZoogiBall* B : G->Balls) if (B) Top = FMath::Max(Top, B->Score);
	for (int32 i = 0; i < 4; ++i)
	{
		FUnitRefs& U = Units[i];
		const AZoogiBall* B = G->Balls.IsValidIndex(i) ? G->Balls[i].Get() : nullptr;
		if (!B) continue;
		const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;
		Show(U.Active, bAct);
		if (U.Active.IsValid()) U.Active->SetRenderOpacity(0.55f + 0.45f * FMath::Abs(FMath::Sin(Time * 3.f)));
		if (U.Ring.IsValid()) U.Ring->SetRenderOpacity(B->bOut ? 0.4f : 1.f);
		if (U.Bar.IsValid())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(U.Bar->Slot))
			{
				const float F = FMath::Clamp((float)FMath::Max(0, B->Score) / Top, 0.03f, 1.f);
				Sl->SetSize(FVector2D((U.BarW - 4.f) * F, 14.f) * S);
			}
		if (U.Score.IsValid()) U.Score->SetText(FText::AsNumber(B->Score));
		SetSPText(G, U, i); // b69
		SyncAviClip(G, U, i); // b88
		if (const UZoogiFunSys* Fun = UZoogiFunSys::Get(G)) // build 27: a live POWER CLASH tag on the two fighters' score units
			if (Fun->IsClashFighter(i))
			{
				if (U.Score.IsValid()) U.Score->SetText(FText::FromString(TEXT("CLASH!")));
				if (U.Ring.IsValid()) U.Ring->SetRenderOpacity(0.6f + 0.4f * FMath::Abs(FMath::Sin(Time * 9.f)));
			}
		if (UZoogiFunSys* Fun = UZoogiFunSys::Get(G)) // build 28: the avatar shows the ultimate-form face while that Zoogi is transformed
			if (UImage* Av = U.Avi.Get())
			{
				const bool bUlt = Fun->IsTransformed(i);
				if (bUlt != U.bAviUlt && !U.bAviClip)
				{
					UTexture2D* T = bUlt ? Fun->UltAvatar(B) : U.AviBase.Get();
					if (T)
					{
						FSlateBrush Br = Av->GetBrush();
						Br.SetResourceObject(T);
						Av->SetBrush(Br);
						U.bAviUlt = bUlt;
					}
				}
			}
		{	// b78: one wolf head per banked Wolf Pack summon (the SUPERPOWER badge number)
			const int32 NW = ZoogiSkills::WolfCount(G, i);
			for (int32 k = 0; k < U.Wolves.Num(); ++k)
				if (UWidget* Wi = U.Wolves[k].Get()) { const ESlateVisibility V = k < NW ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (Wi->GetVisibility() != V) Wi->SetVisibility(V); if (UImage* Im = Cast<UImage>(Wi)) Im->SetColorAndOpacity(k < NW ? ZCHex(TEXT("B57CFF")) : FLinearColor::Transparent); } // b80: tint alpha too - the profile reveal may re-show its children
		}
		const int32 Charge = G->AwakenCharge[i];
		for (int32 k = 0; k < U.Pips.Num(); ++k)
			SetFill(U.Pips[k], k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : AHArcane) : ZCHex(TEXT("050607")));
		if (UTextBlock* Pm = U.PipMore.Get())
		{	// b141: Energy Buff stacks crystals past the 4 pips
			const FString Ms = Charge > U.Pips.Num() && U.Pips.Num() > 0 ? FString::Printf(TEXT("+%d"), Charge - U.Pips.Num()) : FString();
			if (!Pm->GetText().ToString().Equals(Ms)) Pm->SetText(FText::FromString(Ms));
		}
		SyncBigBadge(G, U, i); // b171d
	}

	// ---- Episode (top): round line + charge crystals (no mid-screen gauge panel)
	{
		const int32 Team = G->bComboMode ? G->ComboTeam : (G->ActivePlayer == 0 ? 0 : 1);
		const float Frac = FMath::Clamp(G->ComboEnergy[FMath::Clamp(Team, 0, 1)] / ZoogiComboTune::GaugeMax, 0.f, 1.f);
		const int32 Lit = FMath::RoundToInt(Frac * GaugeSegs.Num());
		for (int32 k = 0; k < GaugeSegs.Num(); ++k)
		{
			SetFill(GaugeSegs[k], k < Lit ? (Team == 0 ? AHSapphire : ZCHex(TEXT("D9443C"))) : ZCHex(TEXT("1B1E25")));
		}
		if (GaugeStar.IsValid() && GaugeSegs.Num())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(GaugeStar->Slot))
				if (UCanvasPanelSlot* S0 = Cast<UCanvasPanelSlot>(GaugeSegs[0]->Slot))
				{
					const float X0 = S0->GetPosition().X / S, Span = GaugeSegs.Num() * (S0->GetSize().X / S + 4.f);
					Sl->SetPosition(FVector2D(X0 + Span * Frac - 28.f, Sl->GetPosition().Y / S) * S);
				}
		if (RoundTxt.IsValid()) RoundTxt->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds)));
		if (RoundOfTxt.IsValid()) RoundOfTxt->SetText(FText::GetEmpty());
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Me) : nullptr;
		const int32 Energy = PS ? PS->Energy : 0;
		for (int32 k = 0; k < Crystals.Num(); ++k)
			if (UImage* I = Cast<UImage>(Crystals[k].Get())) I->SetColorAndOpacity(k < Energy ? FLinearColor::White : FLinearColor(0.25f, 0.27f, 0.32f, 0.8f));
	}

	// ---- combo callout: the acting team's Resonance (2v2) / chained scores this shot (classic)
	TickComboBadge(Dt);

	{
		const int32 CamKey = (G->bCinematicCam ? 2 : 0) + (G->bBirdEyeCam ? 1 : 0);
		if (CamKey != LastCamKey && TacLbl.IsValid()) { LastCamKey = CamKey; RefreshChromeCam(); }
	}
	// ---- clock
	if (ClockTxt.IsValid())
	{
		const int32 Sec = FMath::FloorToInt(MatchClock);
		ClockTxt->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Sec / 60, Sec % 60)));
	}

	// ---- banner: phase + progress
	{
		const AZoogiBall* AB = G->GetActiveBall();
		const bool bMine = AB && !AB->bCPU && AB->PlayerIndex == Me;
		const FString Who = AB ? FString::Printf(TEXT("P%d %s"), AB->PlayerIndex + 1, *AB->Def.Name.ToUpper()) : FString();
		FString Label; float Frac = 0.f;
		switch (G->Phase)
		{
		case EZoogiPhase::Forming: Label = TEXT("ARENA FORMING"); Frac = FMath::Fmod(Time * 0.3f, 1.f); break;
		case EZoogiPhase::GameOver: Label = TEXT("MATCH OVER"); Frac = 1.f; break;
		case EZoogiPhase::Rolling: Label = TEXT("ROLL ACTIVE"); Frac = FMath::Clamp(G->PhaseTime / ZoogiStageTune::ResolveTimeout, 0.f, 1.f); break;
		case EZoogiPhase::Resolving: Label = TEXT("RESOLVE"); Frac = FMath::Clamp(G->StageTime / 2.f, 0.f, 1.f); break;
		default:
			switch (G->Stage)
			{
			case EZoogiStage::Draw: Label = TEXT("DRAW"); Frac = FMath::Clamp(G->StageTime / ZoogiStageTune::DrawHuman, 0.f, 1.f); break;
			case EZoogiStage::Awaken: Label = bMine ? (ZoogiP5Cmd::On() ? (G->bAwakenDone ? TEXT("GO!") : TEXT("COMMAND")) : (G->bAwakenDone ? TEXT("CARDS SET") : TEXT("CHOOSE CARDS"))) : TEXT("STANDBY"); Frac = FMath::Clamp(G->StageTime / 6.f, 0.f, 1.f); break;
			case EZoogiStage::Battle:
				Label = G->BattleStep == EZoogiBattleStep::PowerUp ? TEXT("POWER UP") : G->BattleStep == EZoogiBattleStep::Rolling ? TEXT("ROLL ACTIVE") : bMine ? TEXT("DRAG TO AIM") : TEXT("AIMING");
				Frac = G->BattleStep == EZoogiBattleStep::Aim ? G->AimFraction() : FMath::Clamp(G->StepTime / 1.f, 0.f, 1.f);
				break;
			case EZoogiStage::Resolve: Label = TEXT("RESOLVE"); Frac = FMath::Clamp(G->StageTime / 2.f, 0.f, 1.f); break;
			default: Label = TEXT("TURN END"); Frac = 1.f; break;
			}
		}
		if (BannerTxt.IsValid()) BannerTxt->SetText(FText::FromString(Label));
		if (TurnTagTxt.IsValid())
		{
			const FString Tag = G->Phase == EZoogiPhase::GameOver ? FString() : (bMine ? FString::Printf(TEXT("YOUR TURN  \u00B7  %s"), *Who) : Who);
			TurnTagTxt->SetText(FText::FromString(Tag));
			TurnTagTxt->SetColorAndOpacity(FSlateColor(bMine ? ZCHex(TEXT("FFE66B")) : AHParch.CopyWithNewOpacity(0.75f)));
			const int32 Key = G->Phase == EZoogiPhase::GameOver ? -2 : ((AB ? AB->PlayerIndex : 0) * 10 + (bMine ? 1 : 0) + (int32)G->Stage * 100 + (int32)G->BattleStep);
			if (Key != TurnBannerKey)
			{
				TurnBannerKey = Key;
				if (bMine && !Tag.IsEmpty()) TurnBannerPop = 0.55f;
				// slide the DRAW / turn plate up from mid-screen to the top rest (~0.32s ease-out)
				if (G->Phase != EZoogiPhase::GameOver) BannerSlideT = 0.f;
			}
			if (TurnBannerPop > 0.f)
			{
				TurnBannerPop = FMath::Max(0.f, TurnBannerPop - Dt);
				const float K = 1.f + 0.18f * FMath::Sin(FMath::Clamp(1.f - TurnBannerPop / 0.55f, 0.f, 1.f) * UE_PI);
				TurnTagTxt->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(K, K), FVector2D::ZeroVector, 0.f));
			}
			else TurnTagTxt->SetRenderTransform(FWidgetTransform());
		}
		// ease-out cubic: mid -> top rest
		BannerSlideT = FMath::Clamp(BannerSlideT + Dt / 0.32f, 0.f, 1.f);
		const float SlideA = 1.f - FMath::Pow(1.f - BannerSlideT, 3.f);
		if (BannerRoot.IsValid())
		{
			const float OffY = (1.f - SlideA) * BannerSlideFromY * S;
			BannerRoot->SetRenderTranslation(FVector2D(0.f, OffY));
			if (bMine && G->Phase != EZoogiPhase::GameOver)
				BannerRoot->SetRenderOpacity(0.92f + 0.08f * (0.5f + 0.5f * FMath::Sin(Time * 3.5f)));
			else
				BannerRoot->SetRenderOpacity(1.f);
		}
		if (BannerFill.IsValid())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(BannerFill->Slot)) Sl->SetSize(FVector2D(FMath::Max(4.f, BannerFillW * Frac), 8.f) * S);
	}

	// ---- tray removed (Oct 23): keep TrayT collapsed; no card / ACTIVATE / AWAKEN chrome
	{
		TrayT = 0.f;
		if (TrayRoot.IsValid()) TrayRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ------------------------------------------------------------------------------------------------ Board Battle (portrait)
UImage* UZoogiArenaHUDWidget::RefCrop(float X0, float Y0, float X1, float Y1, float X, float Y, float Wd, float Ht)
{
	UTexture2D* Ref = FileTex(TEXT("Data/MenuArt"), TEXT("board_ref"));
	if (!Ref) return nullptr;
	UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush B; B.SetResourceObject(Ref); B.ImageSize = FVector2D(Wd, Ht) * S;
	B.SetUVRegion(FBox2f(FVector2f(X0 / 941.f, Y0 / 1672.f), FVector2f(X1 / 941.f, Y1 / 1672.f)));
	I->SetBrush(B);
	I->SetVisibility(ESlateVisibility::HitTestInvisible);
	At(I, X, Y, Wd, Ht);
	return I;
}

namespace
{
	const FLinearColor BRCover = ZCHex(TEXT("121726")), BRCoverDeep = ZCHex(TEXT("0D111C"));
	const FLinearColor BRPlayer[4] = { ZCHex(TEXT("3A8BFF")), ZCHex(TEXT("F2C744")), ZCHex(TEXT("E5383B")), ZCHex(TEXT("59C94A")) };
	const float BRPanel[4][2] = { { 22.f, 185.f }, { 650.f, 185.f }, { 22.f, 368.f }, { 650.f, 368.f } };
	const float BRStep[5][4] = { { 100.f, 121.f, 232.f, 155.f }, { 245.f, 121.f, 372.f, 155.f }, { 388.f, 118.f, 552.f, 156.f }, { 565.f, 121.f, 695.f, 155.f }, { 712.f, 121.f, 842.f, 155.f } };
}

void UZoogiArenaHUDWidget::BuildBoard()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;

	// ---- top band: logo + phase stepper (reference art), live stepper states. Oct 22: the band's painted scenery is cut
	// out (MenuArt/board_top_t.png: logo, crystals, stepper and gear on transparency) so the arena shows through
	int32 Mark = Canvas->GetChildrenCount();
	// Oct 28 (Evan, classic portrait): no logo band / DRAW-AWAKEN-BATTLE-RESOLVE-END phase tracker here either (build 9 took it
	// off the landscape HUD); the arena shows through the top of the screen
	for (int32 k = 0; k < 5; ++k) { StepGlow[k].Reset(); StepTxt[k].Reset(); }
	(void)BRStep;
	// Oct 28 (Evan): no arena-name plate (its FROZEN RING line sat on top of ROUND n / 8) - the top is see-through like the
	// landscape HUD; DEV / TAC / settings are built under the round counter below
	ArenaTxt.Reset();
	(void)Mark;

	// ---- player profiles: ONE horizontal row (same Y baseline), team A left / team B right - mirrors landscape
	// BuildBattle discipline (even spacing, no awkward 2x2 stack over buttons). Compact chips clear arena plates.
	{
		const float RowY = 186.f;
		const float RS = 92.f;
		const bool bMirror = ZoogiScoringUI::MirrorLayout();
		const float CxLeft[2] = { 78.f, 188.f }, CxRight[2] = { W - 188.f, W - 78.f };
		const float* CxA = bMirror ? CxRight : CxLeft;
		const float* CxB = bMirror ? CxLeft : CxRight;
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 PMark = Canvas->GetChildrenCount();
			FUnitRefs& U = Units[i];
			const AZoogiBall* B = G->Balls.IsValidIndex(i) ? G->Balls[i].Get() : nullptr;
			if (!B)
			{
				MarkSplit(PMark, FVector2D(0.f, 0.f));
				continue;
			}
			const int32 Team = G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(i) : (i == 0 ? 0 : 1);
			const int32 USlot = G->Balls.Num() >= 4 ? (i % 2) : (i == 0 ? 0 : FMath::Clamp(i - 1, 0, 1));
			if (BigBadges())
			{	// b171d: big corner badges across the top (4 fit a phone); ROUND / MENU sit under them
				const float BW = 206.f, BH = 304.f, Gp = 12.f, M0 = FMath::Max(8.f, (W - 4.f * BW - 3.f * Gp) * 0.5f);
				const FMargin SafeB = ZoogiFramed::SafeInsets(FVector2D(W * S, H * S));
				const float BT = FMath::Max(SafeB.Top / FMath::Max(S, 0.001f) + 10.f, 60.f);
				const bool bLeftSide = (Team == 0) != bMirror;
				const bool bOuter = bLeftSide ? USlot == 0 : (G->Balls.Num() < 4 ? USlot == 0 : USlot == 1);
				const float BX = bLeftSide ? (bOuter ? M0 : M0 + BW + Gp) : (bOuter ? W - M0 - BW : W - M0 - 2.f * BW - Gp);
				BuildBigBadge(i, BX, BT, BW, BH);
				MarkSplit(PMark, FVector2D(BX + BW * 0.5f < W * 0.5f ? -(BX + BW + 40.f) : (W - BX + 40.f), 0.f));
				EpTxt[i] = nullptr;
				continue;
			}
			const float Cx = (Team == 0 ? CxA[USlot] : CxB[USlot]);
			const float RX = Cx - RS * 0.5f, RY = RowY;
			ON_SCOPE_EXIT { MarkSplit(PMark, FVector2D(Cx < W * 0.5f ? -320.f : 320.f, 0.f)); };
			const FLinearColor RingC = Team == 0 ? BRPlayer[0] : BRPlayer[1];
			{	// avatar stays on minimize; tap = peek details
				const int32 AvM = Canvas->GetChildrenCount();
				U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 5.f, RS * 0.5f + 6.f), RX - 6.f, RY - 6.f, RS + 12.f, RS + 12.f);
				U.Ring = At(Box(ZCHex(TEXT("121418")), RingC, 5.f, RS * 0.5f), RX, RY, RS, RS);
				DetailsBottomD = FMath::Max(DetailsBottomD, RY + RS + 86.f); // score bar + P1/YOU + SP + pips under the avatar
				if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) { U.Avi = Img(P, RX + 8.f, RY + 8.f, RS - 16.f, RS - 16.f, (RS - 16.f) * 0.5f, true); U.AviBase = P; U.bAviUlt = false; }
				MarkAvatar(AvM);
				Hit(RX - 6.f, RY - 6.f, RS + 12.f, RS + 12.f, [this]() { PeekDetails(); });
			}
			// self-bar polish: dark track + team fill + score (hit-friendly row)
			U.BarW = 108.f;
			At(Box(ZCHex(TEXT("0A0D16")), ZCHex(TEXT("3B4256")), 2.f, 10.f), Cx - 54.f, RY + RS + 8.f, U.BarW, 20.f);
			U.Bar = At(Box(RingC, FLinearColor::Transparent, 0.f, 8.f), Cx - 52.f, RY + RS + 10.f, 10.f, 16.f);
			U.Score = Text(TEXT("0"), Cx - 54.f, RY + RS + 6.f, U.BarW, 24.f, 18.f, FLinearColor::White, 2.f);
			Text(FString::Printf(TEXT("P%d"), i + 1), Cx - 54.f, RY + RS + 28.f, 40.f, 18.f, 13.f, AHParch, 1.f, 0);
			Text(B->bCPU ? TEXT("CPU") : TEXT("YOU"), Cx - 12.f, RY + RS + 28.f, 66.f, 18.f, 12.f, ZCHex(TEXT("D8D2FF")), 1.f, 0);
			const int32 NPip = AZoogiGameMode::AwakenChargeMax;
			const float PS = 16.f, PGap = 6.f, PW = NPip * PS + (NPip - 1) * PGap, PX = Cx - PW * 0.5f;
			for (int32 k = 0; k < NPip; ++k)
				U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("555B6E")), 1.5f, PS * 0.5f), PX + k * (PS + PGap), RY + RS + 66.f, PS, PS));
			U.PipMore = Text(TEXT(""), PX + PW + 3.f, RY + RS + 64.f, 40.f, 20.f, 12.f, ZCHex(TEXT("FFD35C")), 1.5f, 0); // b141: crystals over 4
			{	// b78: wolf heads under the crystals (banked Wolf Pack summons)
				const float WS = 16.f, WG = 5.f, WW = 4 * WS + 3 * WG;
				for (int32 k = 0; k < 4; ++k) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), Cx - WW * 0.5f + k * (WS + WG), RY + RS + 66.f + PS + 4.f, WS, WS, 0.f, false, FLinearColor::Transparent)) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
			}
			U.SP = Text(TEXT("SP -"), Cx - 54.f, RY + RS + 46.f, U.BarW, 18.f, 13.f, ZCHex(TEXT("7FE3FF")), 1.5f, 1); // b69: SP above the crystals
			EpTxt[i] = nullptr;
		}
	}

	// Oct 23 (Evan): bottom battle card tray removed on Board portrait too (tray_ref / compact row gone).
	BoardBotY = H;
	Cards.Reset();
	EnergyTxt.Reset();
	AwakenGlow.Reset(); AwakenPips.Reset();
	ActivateCost.Reset(); ActivateDim.Reset();

	// Oct 23 (Evan): phase/round + crystals at TOP only (under logo band); no mid-screen EPISODE box.
	{
		const float EpW = 280.f, EpX = W * 0.5f - EpW * 0.5f;
		const float BandH = 172.f * W / 941.f;
		const float EpY = BigBottom > 0.f ? BigBottom + 18.f : FMath::Max(BandH + 8.f, 96.f); // b171d: under the big badges
		const int32 EpMark = Canvas->GetChildrenCount();
		BuildEpisode(EpX, EpY, EpW);
		MarkSplit(EpMark, FVector2D::ZeroVector, 2);
		BuildChromeRow(W * 0.5f, EpY + 38.f); // Oct 28: DEV / TAC / settings under ROUND, only with the profiles
	}
	// Oct 29 (build 19): the big slanted COMBO + number, left, under the profile row
	BuildComboBadge(0.f, BigBottom > 0.f ? BigBottom + 270.f : 492.f); // b267 (Evan: nothing covered): below the AWAKEN bar (it sat on it)
}

// Oct 29 (Evan, build 19: "bring back the larger stylized combo text"): the big slanted COMBO + gold number. It used to show
// only in 2v2 (team Resonance) on the landscape top bar; now classic shows it too (scores chained in one shot, from the 2nd)
// and the portrait board builds it under the left profiles. Pops in with an overshoot on every step up; classic fades ~2.8 s
// after the last step.
void UZoogiArenaHUDWidget::BuildComboBadge(float X0, float CY)
{
	UWidget* Streak = Img(Tex(TEXT("vgrad")), X0 - 40.f, CY + 40.f, 420.f, 74.f, 37.f, false, AHSapphire.CopyWithNewOpacity(0.6f));
	UTextBlock* Word = Text(TEXT("COMBO"), X0 + 30.f, CY, 260.f, 56.f, 42.f, ZCHex(TEXT("FFE08A")), 4.f, 0);
	UTextBlock* Shade = Text(TEXT("0"), X0 + 157.f, CY + 33.f, 240.f, 124.f, 108.f, ZCHex(TEXT("A8460F")), 0.f, 0); // depth copy
	ComboNum = Text(TEXT("0"), X0 + 150.f, CY + 26.f, 240.f, 124.f, 108.f, ZCHex(TEXT("FFD35C")), 6.f, 0);
	if (Word) Word->SetRenderTransformAngle(-12.f);
	if (Shade) Shade->SetRenderTransformAngle(-12.f);
	if (ComboNum.IsValid()) ComboNum->SetRenderTransformAngle(-12.f);
	ComboRoot = Streak;
	TurnTag = Word ? Word->GetParent() : nullptr;   // the COMBO word's box
	ComboNumBox = ComboNum.IsValid() ? ComboNum->GetParent() : nullptr;
	ComboShade = Shade;
	ComboShadeBox = Shade ? Shade->GetParent() : nullptr;
	ComboLastN = 0; ComboPopT = 9.f; ComboHoldT = 0.f;
	auto Hide = [](UWidget* W) { if (W) W->SetVisibility(ESlateVisibility::Collapsed); };
	Hide(Streak); Hide(TurnTag.Get()); Hide(ComboNumBox.Get()); Hide(ComboShadeBox.Get());
}

void UZoogiArenaHUDWidget::TickComboBadge(float Dt)
{
	const AZoogiGameMode* G = GM();
	if (!G) return;
	int32 N = 0;
	bool bStay = false;
	if (G->bComboMode) { N = G->ComboResonance[FMath::Clamp(G->ComboTeam, 0, 1)]; bStay = N > 0; }
	else if (!G->bRoyaleMode)
	{
		const int32 Sg = ZoogiScoringUI::ComboStreak(); N = Sg >= 2 ? Sg : 0;
		static const bool bSlab = FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboSlab"));
		if (!bSlab && G->Phase == EZoogiPhase::Rolling) N = FMath::Max(N, ZoogiRedirect::Combo()); // b177j: the reaction combo shows here too (one combo, one style, left)
	}
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboBadgeTest"));
	if (bTest && !G->bComboMode)
	{
		// capture aid: steps 2 -> 3 -> 4 on a loop so a short run shows the pop and the hold
		const double Tm = FPlatformTime::Seconds();
		const int32 Cyc = (int32)FMath::Fmod(Tm, 9.0);
		N = Cyc < 1 ? 0 : Cyc < 3 ? 2 : Cyc < 5 ? 3 : 4;
	}
	if (N != ComboLastN)
	{
		if (N > ComboLastN) { ComboPopT = 0.f; ComboHoldT = 2.8f; }
		ComboLastN = N;
		if (N > 0)
		{
			const FText Tx = FText::AsNumber(N);
			if (ComboNum.IsValid()) ComboNum->SetText(Tx);
			if (ComboShade.IsValid()) ComboShade->SetText(Tx);
		}
	}
	ComboPopT += Dt;
	ComboHoldT = FMath::Max(0.f, ComboHoldT - Dt);
	const bool bShow = N > 0 && (bStay || ComboHoldT > 0.f);
	const float Fade = bStay ? 1.f : FMath::Clamp(ComboHoldT / 0.45f, 0.f, 1.f);
	const float Pt = ComboPopT;
	float Sc = 1.f;
	if (Pt < 0.12f) Sc = FMath::Lerp(0.35f, 1.6f, Pt / 0.12f);
	else if (Pt < 0.3f) Sc = FMath::Lerp(1.6f, 0.92f, (Pt - 0.12f) / 0.18f);
	else if (Pt < 0.45f) Sc = FMath::Lerp(0.92f, 1.f, (Pt - 0.3f) / 0.15f);
	auto Apply = [&](UWidget* W, const FVector2D& Sv)
	{
		if (!W) return;
		W->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bShow) return;
		W->SetRenderOpacity(Fade);
		W->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, Sv, FVector2D::ZeroVector, 0.f));
	};
	const float Sw = 1.f + (Sc - 1.f) * 0.45f;
	Apply(ComboRoot.Get(), FVector2D(FMath::Min(Sc, 1.25f), 1.f));
	Apply(TurnTag.Get(), FVector2D(Sw, Sw));
	Apply(ComboShadeBox.Get(), FVector2D(Sc, Sc));
	Apply(ComboNumBox.Get(), FVector2D(Sc, Sc));
}

bool UZoogiArenaHUDWidget::BoardTrayCompact() const
{
	const AZoogiGameMode* G = GM();
	return G && (G->Phase == EZoogiPhase::Rolling || (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle));
}

void UZoogiArenaHUDWidget::BuildBoardTrayCompact()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;
	// one row, ~10% of the portrait height (the full tray is ~14%); card tiles stay >= 100 x 140 design units (touch-friendly)
	const float TH = 160.f, TX = 6.f, TW = W - 12.f, TY = H - TH - 8.f;
	BoardBotY = TY;
	At(Box(ZCHex(TEXT("0E1526")).CopyWithNewOpacity(0.9f), ZCHex(TEXT("2B3866")), 2.f, 14.f), TX, TY, TW, TH);
	const float AwS = 132.f, AwX = TX + TW - AwS - 10.f, AwY = TY + (TH - AwS) * 0.5f;
	const float EnW = 84.f, EnX = AwX - EnW - 8.f;
	const float RowX = TX + 10.f, RowW = EnX - 8.f - RowX, Gap = 8.f;
	const float SlotW = (RowW - 4.f * Gap) / 5.f, CH = TH - 16.f, CW = FMath::Min(SlotW, CH * 208.f / 308.f), CY = TY + 8.f;
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (CS && G && G->CardsOn()) ? CS->Rules().Player(LocalPlayer()) : nullptr;
	const int32 N = PS ? PS->Hand.Num() : 0;
	for (int32 k = 0; k < 5; ++k)
	{
		const float SX = RowX + k * (SlotW + Gap), AX = SX + (SlotW - CW) * 0.5f;
		FCardRefs R;
		if (k >= N) { At(Box(ZCHex(TEXT("141B30")), ZCHex(TEXT("232838")), 1.5f, 6.f), AX, CY, CW, CH); Cards.Add(R); continue; }
		const FZoogiCardInstance& Inst = PS->Hand[k];
		const FZoogiCardDef* D = CS->Def(Inst.CardId);
		R.Glow = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFC94D")), 3.f, 8.f), AX - 4.f, CY - 4.f, CW + 8.f, CH + 8.f);
		if (UTexture2D* Art = CardArt(Inst.CardId)) Img(Art, AX, CY, CW, CH, 4.f, true);
		const float NH = 22.f, BR = 17.f;
		At(Box(FLinearColor(0.02f, 0.03f, 0.07f, 0.82f), FLinearColor::Transparent, 0.f, 3.f), AX, CY + CH - NH, CW, NH);
		Text(D ? D->Name.ToUpper() : Inst.CardId.ToString().ToUpper(), AX, CY + CH - NH, CW, NH, FMath::Clamp(CW * 0.1f, 9.f, 13.f), FLinearColor::White, 1.f);
		At(Box(ZCHex(TEXT("1F5FE0")), ZCHex(TEXT("F2C14E")), 2.f, BR), AX - 4.f, CY - 4.f, BR * 2.f, BR * 2.f);
		Text(FString::FromInt(D ? D->Cost : 0), AX - 4.f, CY - 4.f, BR * 2.f, BR * 2.f, BR * 1.15f, FLinearColor::White, 2.f);
		R.Dim = At(Box(FLinearColor(0.02f, 0.02f, 0.04f, 0.62f), FLinearColor::Transparent, 0.f, 4.f), AX, CY, CW, CH);
		R.Root = R.Dim.Get();
		Hit(SX, TY, SlotW, TH, [this, k]() { PressCard(k); }); // whole slot column = tap target
		Cards.Add(R);
	}
	// energy
	At(Box(ZCHex(TEXT("0C1428")), ZCHex(TEXT("2B3866")), 1.5f, 8.f), EnX, TY + 30.f, EnW, TH - 60.f);
	Text(TEXT("ENERGY"), EnX, TY + 32.f, EnW, 24.f, 13.f, ZCHex(TEXT("AFC0E6")), 1.f);
	EnergyTxt = Text(TEXT("5/5"), EnX, TY + 54.f, EnW, TH - 88.f, 30.f, FLinearColor::White, 2.f);
	// AWAKEN orb + pips (same refresh hooks as the full tray).
	// Phone Awaken orb path: hide large gold button; charge pips stay.
	{
		const float C = AwS * 0.5f;
		const bool bPhoneAwaken = ZoogiPhoneControls::ShouldHideArenaAwakenButton();
		if (!bPhoneAwaken)
		{
			AwakenGlow = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFC94D")), 5.f, C + 5.f), AwX - 5.f, AwY - 5.f, AwS + 10.f, AwS + 10.f);
			At(Box(ZCHex(TEXT("3A1C8C")), ZCHex(TEXT("B9873A")), 3.f, C), AwX, AwY, AwS, AwS);
			Text(TEXT("AWAKEN"), AwX, AwY + AwS * 0.32f, AwS, 34.f, 20.f, ZCHex(TEXT("F5C75C")), 3.f);
			Hit(AwX, TY, AwS + 10.f, TH, [this]() { PressAwaken(); });
		}
		const int32 NP = FMath::Min(AZoogiGameMode::AwakenChargeMax, 4);
		const float PR = 9.f, PG = 6.f, PW = NP * PR * 2.f + (NP - 1) * PG, PX = AwX + C - PW * 0.5f, PY = AwY + AwS * 0.66f;
		for (int32 k = 0; k < NP; ++k) AwakenPips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8A6A2E")), 1.5f, PR), PX + k * (PR * 2.f + PG), PY, PR * 2.f, PR * 2.f));
	}
}
void UZoogiArenaHUDWidget::RefreshBoard(float Dt)
{
	RefreshChromeCam();
	AZoogiGameMode* G = GM();
	if (!G) return;
	TickComboBadge(Dt);
	const int32 Me = LocalPlayer();
	auto Show = [](TWeakObjectPtr<UWidget> W, bool b) { if (W.IsValid()) W->SetVisibility(b ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); };
	auto SetFill = [](TWeakObjectPtr<UWidget> W, const FLinearColor& C) { if (UBorder* B = Cast<UBorder>(W.Get())) B->SetBrushColor(C); };
	// the portrait camera keeps the play focus between the arena / round plates and the tray
	ZoogiArenaHUD::SetBoardBand(270.f / FMath::Max(1.f, Design.Y), (Design.Y - BoardBotY + 8.f) / FMath::Max(1.f, Design.Y));

	// stepper
	int32 Step = 0;
	if (G->Phase == EZoogiPhase::Rolling) Step = 2;
	else if (G->Phase == EZoogiPhase::Resolving) Step = 3;
	else if (G->Phase == EZoogiPhase::Aiming) Step = G->Stage == EZoogiStage::Draw ? 0 : G->Stage == EZoogiStage::Awaken ? 1 : G->Stage == EZoogiStage::Battle ? 2 : G->Stage == EZoogiStage::Resolve ? 3 : 4;
	else if (G->Phase == EZoogiPhase::GameOver) Step = 4;
	for (int32 k = 0; k < 5; ++k)
	{
		const bool bOn = k == Step && G->Phase != EZoogiPhase::Forming;
		if (UBorder* B = Cast<UBorder>(StepGlow[k].Get()))
		{
			B->SetBrush(ZoogiUIStyle::RoundBrush(bOn ? ZCHex(TEXT("1F5BE0")) : ZCHex(TEXT("1B2342")), bOn ? ZCHex(TEXT("9FD4FF")) : ZCHex(TEXT("2B3866")), (bOn ? 2.5f : 1.5f) * S, 6.f * S));
			B->SetRenderOpacity(bOn ? 0.85f + 0.15f * FMath::Abs(FMath::Sin(Time * 3.f)) : 1.f);
		}
		if (StepTxt[k].IsValid()) StepTxt[k]->SetColorAndOpacity(FSlateColor(bOn ? FLinearColor::White : ZCHex(TEXT("AFC0E6"))));
	}
	// plates
	if (ArenaTxt.IsValid()) ArenaTxt->SetText(FText::FromString(ZoogiUtil::ArenaName(G->MenuArena).ToUpper()));
	// Top phase+crystals (portrait): round line + crystal energy (no mid EPISODE gauge panel)
	if (RoundTxt.IsValid())
		RoundTxt->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds)));
	if (RoundOfTxt.IsValid()) RoundOfTxt->SetText(FText::GetEmpty());
	{
		const UZoogiCardSubsystem* CSEp = UZoogiCardSubsystem::Get(this);
		const FZoogiCardPlayerState* PSEp = (CSEp && G->CardsOn()) ? CSEp->Rules().Player(Me) : nullptr;
		const int32 EnergyEp = PSEp ? PSEp->Energy : 0;
		for (int32 k = 0; k < Crystals.Num(); ++k)
			if (UImage* I = Cast<UImage>(Crystals[k].Get())) I->SetColorAndOpacity(k < EnergyEp ? FLinearColor::White : FLinearColor(0.25f, 0.27f, 0.32f, 0.8f));
	}
	// players
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(this);
	for (int32 i = 0; i < 4; ++i)
	{
		FUnitRefs& U = Units[i];
		const AZoogiBall* B = G->Balls.IsValidIndex(i) ? G->Balls[i].Get() : nullptr;
		if (!B) { Show(U.Active, false); continue; }
		const bool bAct = G->ActivePlayer == i && G->Phase != EZoogiPhase::GameOver;
		Show(U.Active, bAct);
		if (U.Active.IsValid()) U.Active->SetRenderOpacity(0.55f + 0.45f * FMath::Abs(FMath::Sin(Time * 3.f)));
		if (U.Score.IsValid()) U.Score->SetText(FText::AsNumber(B->Score));
		SetSPText(G, U, i); // b69
		SyncAviClip(G, U, i); // b88
		const float E = SM ? SM->GetEnergy(i) : 0.f;
		if (EpTxt[i].IsValid()) EpTxt[i]->SetText(FText::FromString(FString::Printf(TEXT("EPISODE %d/100"), FMath::RoundToInt(E))));
		if (U.Bar.IsValid())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(U.Bar->Slot)) Sl->SetSize(FVector2D(FMath::Max(10.f, U.BarW * FMath::Clamp(E / 100.f, 0.f, 1.f)), 16.f) * S);
		{	// b78: one wolf head per banked Wolf Pack summon (the SUPERPOWER badge number)
			const int32 NW = ZoogiSkills::WolfCount(G, i);
			for (int32 k = 0; k < U.Wolves.Num(); ++k)
				if (UWidget* Wi = U.Wolves[k].Get()) { const ESlateVisibility V = k < NW ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (Wi->GetVisibility() != V) Wi->SetVisibility(V); if (UImage* Im = Cast<UImage>(Wi)) Im->SetColorAndOpacity(k < NW ? ZCHex(TEXT("B57CFF")) : FLinearColor::Transparent); } // b80: tint alpha too - the profile reveal may re-show its children
		}
		const int32 Charge = G->AwakenCharge[i];
		for (int32 k = 0; k < U.Pips.Num(); ++k) SetFill(U.Pips[k], k < Charge ? BRPlayer[i] : ZCHex(TEXT("232838")));
		SyncBigBadge(G, U, i); // b171d
	}
	// tray readouts
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Me) : nullptr;
	if (DeckTxt.IsValid()) DeckTxt->SetText(FText::AsNumber(PS ? PS->DrawPile.Num() : 0));
	if (DiscardTxt.IsValid()) DiscardTxt->SetText(FText::AsNumber(PS ? PS->Discard.Num() : 0));
	if (EnergyTxt.IsValid()) EnergyTxt->SetText(FText::FromString(FString::Printf(TEXT("%d/5"), PS ? PS->Energy : 0)));
	const bool bStandby = G->CardsPlayWindowOpen();
	int32 Cost = 0;
	for (int32 k = 0; k < Cards.Num(); ++k)
	{
		bool bSel = false, bAfford = true;
		if (PS && PS->Hand.IsValidIndex(k))
		{
			bSel = PS->Selected.Contains(PS->Hand[k].InstanceId);
			const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId);
			if (bSel && D) Cost += D->Cost;
			bAfford = bSel || !D || D->Cost <= PS->Energy;
		}
		Show(Cards[k].Glow, bSel);
		Show(Cards[k].Dim, !bAfford || !bStandby || !G->CardsSlotPlayableNow(k));
		if (Cards[k].Root.IsValid()) Cards[k].Root->SetRenderTranslation(FVector2D(0.f, bSel ? -10.f * S : 0.f));
	}
	if (ActivateCost.IsValid()) ActivateCost->SetText(FText::AsNumber(Cost));
	Show(ActivateDim, !bStandby);
	const bool bReady = G->CardsAwakenReady(Me) && !G->bStandbyAwaken;
	if (AwakenGlow.IsValid())
	{
		if (ZoogiPhoneControls::ShouldHideArenaAwakenButton())
		{
			AwakenGlow->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			AwakenGlow->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			AwakenGlow->SetRenderOpacity(0.45f + 0.55f * FMath::Abs(FMath::Sin(Time * 2.6f)));
		}
	}
	const int32 Charge = G->AwakenCharge[FMath::Clamp(Me, 0, 3)];
	for (int32 k = 0; k < AwakenPips.Num(); ++k) SetFill(AwakenPips[k], k < Charge ? ZCHex(TEXT("FFD970")) : ZCHex(TEXT("2B2D33")));
	(void)Dt;
}

// ------------------------------------------------------------------------------------------------ video split (Oct 22)
void UZoogiArenaHUDWidget::MarkAvatar(int32 FirstChild)
{
	if (!Canvas) return;
	for (int32 k = FMath::Max(0, FirstChild); k < Canvas->GetChildrenCount(); ++k)
		if (UWidget* W = Canvas->GetChildAt(k)) AvatarW.Add(W);
}

void UZoogiArenaHUDWidget::MarkSplit(int32 FirstChild, const FVector2D& Dir, uint8 Kind)
{
	if (!Canvas) return;
	const int32 Order = SplitOrder++;
	for (int32 k = FMath::Max(0, FirstChild); k < Canvas->GetChildrenCount(); ++k)
		if (UWidget* W = Canvas->GetChildAt(k))
		{
			// avatars and tap targets never fade away (a hidden button would still catch taps, a faded avatar can't be found)
			const bool bKeep = Kind != 0 && Kind != 3 && Kind != 4 && (W->IsA<UButton>() || AvatarW.Contains(TWeakObjectPtr<UWidget>(W))); // Kind 0: gear Hit leaves with the logo (Gameplay nit)
			SplitParts.Add({ W, Dir, Order, bKeep ? (uint8)2 : Kind });
		}
}

void UZoogiArenaHUDWidget::PeekDetails()
{
	static const bool bAutoMini = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHudAutoMini"));
	if (!bAutoMini) { bUserMini = !bUserMini; UE_LOG(LogZoogi, Log, TEXT("ARENA HUD profiles %s (avatar tap)"), bUserMini ? TEXT("hidden") : TEXT("shown")); return; } // b67: tap toggles
	PeekUntil = Time + ZoogiArenaHUD::PeekSeconds;
	UE_LOG(LogZoogi, Log, TEXT("ARENA HUD peek details (%.1fs)"), ZoogiArenaHUD::PeekSeconds);
}

void UZoogiArenaHUDWidget::TickSplit(float Dt)
{
	// a crit / anime cut-in on screen: profiles + score panels slide outward (~0.3 s, eased), back together when it ends
	const UZoogiAnimeSubsystem* Anime = GetWorld() ? GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr;
	static const bool bAnimeTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTop"));
	const bool bVideo = ((bAnimeTop || (Anime && Anime->bTopCard)) && Anime && Anime->bPlaying && Anime->HasMovie()) || ZoogiArenaHUD::TopBannerOn(); // b145a: a top-card clip slides them out too // b88c: clips play in the avatars, so the profiles stay put; b116: a top banner slides them out
	const float Prev = SplitT;
	SplitT = FMath::Clamp(SplitT + (bVideo ? 1.f : -1.f) * Dt / ZoogiArenaHUD::SplitSeconds, 0.f, 1.f);
	if ((Prev == 0.f) != (SplitT == 0.f)) UE_LOG(LogZoogi, Log, TEXT("ARENA HUD panels %s"), bVideo ? TEXT("split (video)") : TEXT("together"));
	const float E = ZoogiArenaHUD::SplitEase(SplitT);
	if (Prev < 1.f && SplitT >= 1.f && SplitShots < 3) { ++SplitShots; TestShot(TEXT("video_split")); }

	// Oct 22 (Evan): top-of-screen intro. Each piece flies in on its own (staggered, slight overshoot) and snaps together;
	// the logo / title then leaves for good and the rest minimizes to the avatars; tapping an avatar peeks the details.
	using namespace ZoogiArenaHUD;
	if (TopOnT < 0.f) { TopOnT = Time; LogoT = MiniT = 0.f; PeekUntil = -1.f; TopShotsDone = 0; bUserMini = false; UE_LOG(LogZoogi, Log, TEXT("ARENA HUD top intro")); }
	const float A = Time - TopOnT;
	const float PrevLogo = LogoT, PrevMini = MiniT;
	LogoT = FMath::Clamp(LogoT + (A > LogoHold ? 1.f : -1.f) * Dt / 0.5f, 0.f, 1.f);
	static const bool bAutoMini = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHudAutoMini"));
	const bool bMini = bAutoMini ? (A > MiniDelay && Time > PeekUntil) : bUserMini; // b67 (Evan): "Profile starts and remains visible unless tapped/toggled"
	MiniT = FMath::Clamp(MiniT + (bMini ? 1.f : -1.f) * Dt / 0.3f, 0.f, 1.f);
	if (PrevLogo < 1.f && LogoT >= 1.f) UE_LOG(LogZoogi, Log, TEXT("ARENA HUD logo gone"));
	if ((PrevMini < 1.f) != (MiniT < 1.f)) UE_LOG(LogZoogi, Log, TEXT("ARENA HUD %s"), MiniT >= 1.f ? TEXT("minimized (avatars only)") : TEXT("details shown"));
	const float LE = SplitEase(LogoT), ME = SplitEase(MiniT);
	GDetReveal = 1.f - ME; GDetBottom = DetailsBottomD * S; // Oct 30: the SUPER meter follows the details down / up
	// -ZoogiShots=<dir>: frames of the fly-in, assembled, minimized and (with -ZoogiHudPeekTest) a peek
	static const float ShotAt[] = { 0.25f, 0.5f, 0.8f, 1.6f, 5.0f, 7.0f };
	static const TCHAR* ShotName[] = { TEXT("top_in_a"), TEXT("top_in_b"), TEXT("top_in_c"), TEXT("top_together"), TEXT("top_logo_out"), TEXT("top_minimized") };
	for (int32 k = 0; k < 6; ++k)
		if (!(TopShotsDone & (1u << k)) && A >= ShotAt[k]) { TopShotsDone |= 1u << k; TestShot(ShotName[k]); }
	static const bool bPeekTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiHudPeekTest"));
	if (bPeekTest && !(TopShotsDone & (1u << 6)) && A >= 8.f) { TopShotsDone |= 1u << 6; PeekDetails(); }
	if (bPeekTest && !(TopShotsDone & (1u << 7)) && A >= 8.6f) { TopShotsDone |= 1u << 7; TestShot(TEXT("top_peek")); }
	if (bPeekTest && !(TopShotsDone & (1u << 8)) && A >= 11.8f) { TopShotsDone |= 1u << 8; TestShot(TEXT("top_peek_after")); }
	// Settings/Exit path after minimize: open the existing pause menu (MAIN MENU / EXIT GAME)
	if (bPeekTest && !(TopShotsDone & (1u << 9)) && A >= 12.2f) { TopShotsDone |= 1u << 9; SetPaused(true); TestShot(TEXT("top_settings")); }

	for (FSplitPart& P : SplitParts)
	{
		UWidget* W = P.W.Get();
		if (!W) continue;
		const float It = FMath::Clamp((A - 0.15f - P.Order * IntroStagger) / IntroPiece, 0.f, 1.f);
		const float In = 1.f - BackEase(It);                 // 1 = fully off-screen along Dir, 0 = home (dips below 0 = overshoot)
		float Off = In + E, Alpha = FMath::Clamp(It * 2.5f, 0.f, 1.f);
		if (P.Kind == 0) { Off += LE; Alpha *= 1.f - LE; }
		else if (P.Kind == 1) { Alpha *= 1.f - ME; Off += ME; } // fade + slide fully off along Dir so plates / bars / pips leave
		else if (P.Kind == 3) { Off = 0.f; Alpha = LE; }
		else if (P.Kind == 4) { Alpha *= 1.f - ME; Off += ME; } // Oct 28: DEV / TAC / settings row, with the profiles
		W->SetRenderTranslation(P.Dir * S * Off);
		if (Alpha < 0.999f) { W->SetRenderOpacity(Alpha); P.bFaded = true; }
		else if (P.bFaded) { W->SetRenderOpacity(1.f); P.bFaded = false; }
		// Collapsed when gone: UMG sometimes keeps a faded Border's children readable at opacity 0.
		// Kind 0/3 Hits are UButtons -- also collapse them so a faded Kind-0 gear does not eat taps after the logo leaves,
		// and the Kind-3 Settings stand-in only accepts taps once it is shown.
		if (P.Kind == 0 || P.Kind == 1 || P.Kind == 3 || P.Kind == 4)
		{
			const bool bShow = Alpha > 0.04f;
			if (W->IsA<UButton>() && (P.Kind == 0 || P.Kind == 3 || P.Kind == 4))
			{
				const ESlateVisibility Want = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
				if (W->GetVisibility() != Want) W->SetVisibility(Want);
			}
			else if (!W->IsA<UButton>())
			{
				const ESlateVisibility Want = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
				if (W->GetVisibility() != Want) W->SetVisibility(Want);
			}
		}
	}
}
