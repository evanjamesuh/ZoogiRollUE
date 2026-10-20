// arena-hud: see ZoogiArenaHUDWidget.h
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiArtifacts.h"
#include "ZoogiBall.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuRoot.h"   // UZoogiMenuRootClick (lambda click forwarder)
#include "ZoogiMenuWidget.h" // UZoogiButton (never takes keyboard focus)
#include "ZoogiRoll.h"
#include "ZoogiUIStyle.h"
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

namespace ZoogiArenaHUD
{
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
	void SetBoardBand(float Top, float Bottom) { GBandTop = Top; GBandBottom = Bottom; GBandStamp = FPlatformTime::Seconds(); }
	bool BoardBand(const UWorld* World, float& OutTop, float& OutBottom)
	{
		if (!IsActive(World) || FPlatformTime::Seconds() - GBandStamp > 1.0) return false;
		OutTop = GBandTop; OutBottom = GBandBottom; return true;
	}

	bool IsActive(const UWorld* World)
	{
		if (!IsEnabled() || !World) return false;
		const AZoogiGameMode* G = World->GetAuthGameMode<AZoogiGameMode>();
		return G && G->Phase != EZoogiPhase::Menu && G->Balls.Num() > 0;
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

UTexture2D* UZoogiArenaHUDWidget::Tex(const FString& Key)
{
	if (TObjectPtr<UTexture2D>* Hit = TexCache.Find(Key)) return Hit->Get();
	UTexture2D* T = nullptr;
	const FLinearColor GoldA = ZCHex(TEXT("FFE66B")), GoldB = ZCHex(TEXT("E39410"));
	TArray<FZCShape> Sh;
	int32 W = 128, H = 128; float UW = 100.f, UH = 100.f;
	if (Key == TEXT("stone")) T = ZoogiUIFx::ToTexture(ZCStonePixels());
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
	if (!Canvas) return;
	Canvas->ClearChildren();
	Clicks.Reset();
	for (FUnitRefs& U : Units) U = FUnitRefs();
	GaugeSegs.Reset(); Crystals.Reset(); AwakenPips.Reset(); Cards.Reset();
	const AZoogiGameMode* G = GM();
	if (!G) return;
	for (TWeakObjectPtr<UWidget>& W : StepGlow) W = nullptr;
	if (bBoard) BuildBoard();
	else { BuildTopBar(); BuildTray(); }
	bBuiltOver = G->Phase == EZoogiPhase::GameOver;
	if (bBuiltOver) BuildResults();
	if (bPaused) BuildPause();
	bBuiltPause = bPaused;
}

void UZoogiArenaHUDWidget::BuildResults()
{
	// END OF MATCH (button map): winner banner, every Zoogi (team, score, Ring Marks), REMATCH -> new match, MENU -> BATTLE tab
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;
	At(Box(FLinearColor(0.01f, 0.008f, 0.006f, 0.6f), FLinearColor::Transparent, 0.f, 0.f), 0.f, 0.f, W, H);
	Hit(0.f, 0.f, W, H, []() {});
	const int32 N = G->Balls.Num();
	const float PW = FMath::Min(900.f, W - 48.f), RowH = bPortrait ? 112.f : 80.f, PH = 230.f + N * (RowH + 12.f) + 150.f;
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
	const float BY = PY + PH - 124.f, BW = (PW - 90.f) * 0.5f;
	Stone(PX + 30.f, BY, BW, 96.f, 34.f, ZCHex(TEXT("E9A520")), true);
	Text(TEXT("REMATCH"), PX + 30.f, BY, BW, 96.f, 36.f, FLinearColor::White, 4.f);
	Hit(PX + 30.f, BY, BW, 96.f, [this]() { if (AZoogiGameMode* GM2 = GM()) { UE_LOG(LogZoogi, Log, TEXT("ARENA HUD REMATCH")); GM2->MenuAction(TEXT("Rematch"), 0); } });
	Stone(PX + 60.f + BW, BY, BW, 96.f, 34.f, AHSapphire, true);
	Text(TEXT("MENU"), PX + 60.f + BW, BY, BW, 96.f, 36.f, FLinearColor::White, 4.f);
	Hit(PX + 60.f + BW, BY, BW, 96.f, [this]() { if (AZoogiGameMode* GM2 = GM()) { UE_LOG(LogZoogi, Log, TEXT("ARENA HUD MENU")); GM2->MenuAction(TEXT("Menu"), 0); } });
}

void UZoogiArenaHUDWidget::BuildTopBar()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X;
	const bool bL = !bPortrait;
	// side plates + Episode panel (portrait reference: 1080 wide; landscape keeps the same pieces at the corners)
	const float PlateW = 386.f, PlateH = 238.f;
	Stone(-12.f, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
	Stone(W - PlateW, -12.f, PlateW + 12.f, PlateH + 12.f, 26.f, AHStone.CopyWithNewOpacity(0.97f), true);
	const float EpW = 356.f, EpX = W * 0.5f - EpW * 0.5f;
	BuildEpisode(EpX, 0.f, EpW);
	// units: team A left (P1, P2), team B right (P3, P4); 1v1 / 3-player matches fill from the left
	const float CxA[2] = { 112.f, 287.f }, CxB[2] = { W - 287.f, W - 112.f };
	for (int32 i = 0; i < G->Balls.Num() && i < 4; ++i)
	{
		const int32 Team = G->Balls.Num() >= 4 ? AZoogiGameMode::TeamOfIndex(i) : (i == 0 ? 0 : 1);
		const int32 USlot = G->Balls.Num() >= 4 ? (i % 2) : (i == 0 ? 0 : FMath::Clamp(i - 1, 0, 1));
		BuildUnit(i, Team == 0 ? CxA[USlot] : CxB[USlot], 14.f);
	}
	// combo callout (left, under the plate): team Resonance this match
	{
		const float CY = PlateH + 34.f;
		UWidget* Streak = Img(Tex(TEXT("vgrad")), -40.f, CY + 40.f, 400.f, 70.f, 35.f, false, AHSapphire.CopyWithNewOpacity(0.55f));
		UTextBlock* Word = Text(TEXT("COMBO"), 30.f, CY, 260.f, 56.f, 40.f, ZCHex(TEXT("FFE08A")), 4.f, 0);
		ComboNum = Text(TEXT("0"), 150.f, CY + 26.f, 220.f, 120.f, 104.f, ZCHex(TEXT("FFD35C")), 6.f, 0);
		if (Word) { Word->SetRenderTransformAngle(-12.f); }
		if (ComboNum.IsValid()) ComboNum->SetRenderTransformAngle(-12.f);
		// one root to toggle: re-use the streak image as the visibility anchor
		ComboRoot = Streak;
		TurnTag = Word ? Word->GetParent() : nullptr; // the COMBO word's box (shown with the number)
	}
	// pause + match clock (right, under the plate)
	{
		const float PY = PlateH + 18.f;
		At(Box(ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.95f), ZCHex(TEXT("5C6170")), 3.f, 36.f), W - 92.f, PY, 72.f, 72.f);
		At(Box(AHParch, FLinearColor::Transparent, 0.f, 2.f), W - 67.f, PY + 20.f, 8.f, 32.f);
		At(Box(AHParch, FLinearColor::Transparent, 0.f, 2.f), W - 53.f, PY + 20.f, 8.f, 32.f);
		Hit(W - 100.f, PY - 8.f, 88.f, 88.f, [this]() { SetPaused(!bPaused); });
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
	U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 6.f, RS * 0.5f + 8.f), RX - 8.f, RY - 8.f, RS + 16.f, RS + 16.f);
	U.Ring = At(Box(ZCHex(TEXT("121418")), RingC, 8.f, RS * 0.5f), RX, RY, RS, RS);
	if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) Img(P, RX + 10.f, RY + 10.f, RS - 20.f, RS - 20.f, (RS - 20.f) * 0.5f, true);
	else Text(B->Def.Name.Left(2).ToUpper(), RX, RY, RS, RS, 48.f, FLinearColor::White, 3.f);
	// element badge: the Zoogi's affinity element (what its AWAKEN infuses)
	const EZoogiArtifact Aff = ZoogiArtifacts::Affinity(B->Def.Id);
	const FLinearColor EC = ZoogiArtifacts::Color(Aff);
	At(Box(EC, AHParch, 4.f, 23.f), Cx + 30.f, RY + RS - 46.f, 46.f, 46.f);
	Img(Tex(TEXT("star4")), Cx + 40.f, RY + RS - 36.f, 26.f, 26.f, 0.f, false);
	// score bar (team colour) + score
	U.BarW = 128.f;
	At(Box(ZCHex(TEXT("0D0F12")), ZCHex(TEXT("3B3E45")), 2.f, 9.f), Cx - 64.f, RY + RS + 8.f, U.BarW, 18.f);
	UBorder* Fill = Box(RingC, FLinearColor::Transparent, 0.f, 7.f);
	U.Bar = At(Fill, Cx - 62.f, RY + RS + 10.f, U.BarW - 4.f, 14.f);
	U.Score = Text(TEXT("0"), Cx - 64.f, RY + RS + 4.f, U.BarW, 26.f, 17.f, FLinearColor::White, 2.f);
	// Awaken pips (4): filled by committed card abilities
	const int32 N = AZoogiGameMode::AwakenChargeMax;
	const float PS = 22.f, Gap = 8.f, PW = N * PS + (N - 1) * Gap, PX = Cx - PW * 0.5f;
	for (int32 k = 0; k < N; ++k)
		U.Pips.Add(At(Box(ZCHex(TEXT("050607")), ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 34.f, PS, PS));
	(void)Team;
}

void UZoogiArenaHUDWidget::BuildEpisode(float X, float Y, float Wd)
{
	// EPISODE: the acting team's Combo gauge (Partner Follow-Up fuel), round counter, card-energy crystals
	Stone(X, Y - 14.f, Wd, 240.f, 28.f, AHStoneDeep.CopyWithNewOpacity(0.97f), true);
	Img(Tex(TEXT("crest")), X + Wd * 0.5f - 42.f, Y - 6.f, 84.f, 34.f, 0.f, false);
	Text(TEXT("EPISODE"), X, Y + 26.f, Wd, 36.f, 24.f, ZCHex(TEXT("F0C46A")), 2.f);
	const float GX = X + 30.f, GY = Y + 70.f, GW = Wd - 60.f, GH = 36.f;
	At(Box(ZCHex(TEXT("0C0E12")), ZCGoldDeep, 4.f, GH * 0.5f), GX, GY, GW, GH);
	const int32 Segs = 8;
	const float SW = (GW - 14.f - (Segs - 1) * 4.f) / Segs;
	for (int32 k = 0; k < Segs; ++k)
		GaugeSegs.Add(At(Box(AHSapphire, FLinearColor::Transparent, 0.f, 6.f), GX + 7.f + k * (SW + 4.f), GY + 7.f, SW, GH - 14.f));
	GaugeStar = Img(Tex(TEXT("star4")), GX, GY - 14.f, 56.f, 64.f, 0.f, false);
	// round + crystals
	At(Box(ZCHex(TEXT("0B0C10")), ZCHex(TEXT("2A2D34")), 2.f, 42.f), X + 24.f, Y + 122.f, Wd - 48.f, 84.f);
	for (int32 k = 0; k < 5; ++k)
		Crystals.Add(Img(Tex(TEXT("crystal")), X + 44.f + k * 26.f, Y + 136.f, 26.f, 52.f, 0.f, false));
	RoundTxt = Text(TEXT("1"), X + Wd - 150.f, Y + 124.f, 60.f, 80.f, 50.f, FLinearColor::White, 3.f, 2);
	RoundOfTxt = Text(TEXT("/ 8"), X + Wd - 86.f, Y + 134.f, 70.f, 64.f, 26.f, AHParch, 2.f, 0);
}

void UZoogiArenaHUDWidget::BuildTray()
{
	const AZoogiGameMode* G = GM();
	const float W = Design.X, H = Design.Y;
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(LocalPlayer()) : nullptr;
	const int32 NHand = PS ? PS->Hand.Num() : 0;

	// geometry (design units). Portrait: banner + card row + detail / ACTIVATE + AWAKEN across the bottom (reference
	// proportions); landscape: one row along the bottom (detail left, cards centre, ACTIVATE + AWAKEN right).
	float TrayTop, BannerY, CardY, CardW, CardH, RowX0, RowW, DetX, DetY, DetW, DetH, ActX, ActY, AwX, AwY, AwS;
	if (bPortrait)
	{
		TrayTop = H - 680.f;
		BannerY = TrayTop - 102.f;
		CardY = TrayTop + 50.f; CardW = 190.f; CardH = 262.f; RowX0 = 22.f; RowW = W - 44.f;
		DetX = 14.f; DetY = TrayTop + 360.f; DetW = 790.f; DetH = 250.f;
		ActX = DetX + DetW - 300.f; ActY = DetY + 70.f;
		AwS = 250.f; AwX = W - AwS - 8.f; AwY = DetY - 20.f;
	}
	else
	{
		TrayTop = H - 330.f;
		BannerY = TrayTop - 96.f;
		CardW = 150.f; CardH = 206.f; CardY = TrayTop + 40.f;
		DetX = 16.f; DetY = TrayTop + 50.f; DetW = 520.f; DetH = 250.f;
		AwS = 230.f; AwX = W - AwS - 12.f; AwY = H - AwS - 34.f;
		ActX = AwX - 296.f; ActY = TrayTop + 130.f;
		RowX0 = DetX + DetW + 16.f; RowW = ActX - 16.f - RowX0;
	}
	TrayY0 = TrayTop;
	TrayHidden = H - TrayTop + 24.f; // collapsed: the tray slides fully off the bottom edge

	// ---- phase banner + progress (always on screen). Over the tray while it is open; when the tray slides away (aim,
	// roll, CPU turns) the banner rises to just under the top bar so the lower screen - where the camera puts the
	// acting Zoogi - stays clear
	const float BW = bPortrait ? 470.f : 520.f, BX = W * 0.5f - BW * 0.5f;
	UCanvasPanel* Outer = Canvas;
	UCanvasPanel* BannerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BannerCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	At(BannerCanvas, 0.f, 0.f, W, H);
	BannerRoot = BannerCanvas;
	BannerShift = (bPortrait ? 300.f : 296.f) - BannerY;
	Canvas = BannerCanvas;
	{
		Img(Tex(TEXT("chevron")), BX - 54.f, BannerY + 8.f, 42.f, 56.f, 0.f, false);
		UImage* R = Img(Tex(TEXT("chevron")), BX + BW + 12.f, BannerY + 8.f, 42.f, 56.f, 0.f, false);
		if (R) R->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(-1.f, 1.f), FVector2D::ZeroVector, 0.f));
		At(Box(ZCHex(TEXT("121A4A")).CopyWithNewOpacity(0.95f), ZCHex(TEXT("6FA8FF")), 3.f, 18.f), BX, BannerY, BW, 72.f);
		BannerTxt = Text(TEXT(""), BX, BannerY, BW, 72.f, 36.f, ZCHex(TEXT("EAF3FF")), 3.f);
		At(Box(ZCHex(TEXT("07090E")), ZCHex(TEXT("2A2F3A")), 3.f, 12.f), BX + 50.f, BannerY + 78.f, BW - 100.f, 24.f);
		BannerFillW = BW - 108.f;
		BannerFill = At(Box(ZCHex(TEXT("49C3FF")), FLinearColor::Transparent, 0.f, 8.f), BX + 54.f, BannerY + 82.f, BannerFillW, 16.f);
		TurnTagTxt = Text(TEXT(""), BX, BannerY - 44.f, BW, 40.f, 22.f, AHParch, 2.f);
	}
	Canvas = Outer;

	// ---- tray body: built inside its own canvas so it can slide down as one piece
	UCanvasPanel* TrayCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	TrayCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	At(TrayCanvas, 0.f, 0.f, W, H);
	TrayRoot = TrayCanvas;
	Canvas = TrayCanvas;
	{
		// stone tray under the cards
		Stone(-12.f, TrayTop, W + 24.f, H - TrayTop + 40.f, 0.f, AHStone.CopyWithNewOpacity(0.96f), false);
		At(Box(AHRim, FLinearColor::Transparent, 0.f, 0.f), 0.f, TrayTop, W, 5.f);
		At(Box(ZCGoldDeep, FLinearColor::Transparent, 0.f, 0.f), 0.f, TrayTop + 5.f, W, 3.f);

		// card row
		const int32 NShow = FMath::Max(NHand, 1);
		const float Gap = 12.f;
		const float CW = FMath::Min(CardW, (RowW - (NShow - 1) * Gap) / NShow);
		const float CH = CardH * (CW / CardW);
		const float Used = NShow * CW + (NShow - 1) * Gap, X0 = RowX0 + (RowW - Used) * 0.5f;
		if (NHand == 0)
			Text(G->CardsOn() ? TEXT("NO CARDS IN HAND") : TEXT("CARDS OFF"), RowX0, CardY, RowW, CH, 26.f, ZCMuted, 2.f);
		for (int32 k = 0; k < NHand; ++k) BuildCard(k, X0 + k * (CW + Gap), CardY, CW, CH);

		// detail panel
		BuildDetail(DetX, DetY, DetW, DetH);

		// ACTIVATE: commit the selected cards (energy cost below)
		{
			const float AW = 270.f, AH = 88.f;
			UWidget* Rim = Stone(ActX, ActY, AW, AH, 36.f, AHSapphire, true);
			Img(Tex(TEXT("vgrad")), ActX + 7.f, ActY + 7.f, AW - 14.f, AH - 14.f, 30.f, false, ZCHex(TEXT("1545B8")));
			At(Box(FLinearColor(1.f, 1.f, 1.f, 0.25f), FLinearColor::Transparent, 0.f, 26.f), ActX + 16.f, ActY + 10.f, AW - 32.f, AH * 0.36f);
			ActivateLbl = Text(TEXT("ACTIVATE"), ActX, ActY, AW, AH, 34.f, FLinearColor::White, 4.f);
			ActivateDim = At(Box(FLinearColor(0.03f, 0.03f, 0.05f, 0.55f), FLinearColor::Transparent, 0.f, 36.f), ActX, ActY, AW, AH);
			Hit(ActX, ActY, AW, AH, [this]() { PressActivate(); });
			At(Box(ZCHex(TEXT("121419")), ZCHex(TEXT("33363E")), 2.f, 22.f), ActX + AW * 0.5f - 62.f, ActY + AH + 4.f, 124.f, 50.f);
			Img(Tex(TEXT("gem")), ActX + AW * 0.5f - 44.f, ActY + AH + 12.f, 28.f, 34.f, 0.f, false);
			ActivateCost = Text(TEXT("0"), ActX + AW * 0.5f - 8.f, ActY + AH + 4.f, 60.f, 50.f, 30.f, FLinearColor::White, 2.f, 0);
			(void)Rim;
		}

		// AWAKEN: glowing orb in a gold ring + 4 charge pips
		{
			const float C = AwS * 0.5f;
			AwakenGlow = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFC94D")).CopyWithNewOpacity(0.85f), 10.f, C + 10.f), AwX - 10.f, AwY - 10.f, AwS + 20.f, AwS + 20.f);
			At(Box(AHRim, ZCDark, 4.f, C), AwX, AwY, AwS, AwS);
			Img(Tex(TEXT("vgrad")), AwX + 4.f, AwY + 4.f, AwS - 8.f, AwS - 8.f, C - 4.f, false, ZCGoldDeep);
			At(Box(ZCHex(TEXT("3A1C8C")), ZCDark, 3.f, C - 16.f), AwX + 16.f, AwY + 16.f, AwS - 32.f, AwS - 32.f);
			Img(Tex(TEXT("vgrad")), AwX + 19.f, AwY + 19.f, AwS - 38.f, AwS - 38.f, C - 19.f, false, ZCHex(TEXT("170A40")));
			At(Box(FLinearColor(0.75f, 0.6f, 1.f, 0.22f), FLinearColor::Transparent, 0.f, C - 40.f), AwX + 40.f, AwY + 26.f, AwS - 80.f, AwS * 0.4f);
			Img(Tex(TEXT("star4")), AwX + C - 46.f, AwY + C - 70.f, 92.f, 92.f, 0.f, false, ZCHex(TEXT("FFF2B0")));
			Text(TEXT("AWAKEN"), AwX, AwY + AwS * 0.56f, AwS, 50.f, 32.f, ZCHex(TEXT("F5C75C")), 4.f);
			const int32 N = AZoogiGameMode::AwakenChargeMax;
			const float PipS = 34.f, Gap2 = 8.f, PW = N * PipS + (N - 1) * Gap2, PX = AwX + C - PW * 0.5f, PY = AwY + AwS - 16.f;
			At(Box(ZCHex(TEXT("1E1407")), ZCHex(TEXT("B9873A")), 3.f, 22.f), PX - 14.f, PY - 8.f, PW + 28.f, PipS + 16.f);
			for (int32 k = 0; k < N; ++k) AwakenPips.Add(At(Box(ZCHex(TEXT("2B2D33")), ZCHex(TEXT("4D4F55")), 2.f, PipS * 0.5f), PX + k * (PipS + Gap2), PY, PipS, PipS));
			Hit(AwX, AwY, AwS, AwS, [this]() { PressAwaken(); });
		}
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

void UZoogiArenaHUDWidget::BuildPause()
{
	const float W = Design.X, H = Design.Y;
	At(Box(FLinearColor(0.01f, 0.008f, 0.006f, 0.72f), FLinearColor::Transparent, 0.f, 0.f), 0.f, 0.f, W, H);
	Hit(0.f, 0.f, W, H, []() {}); // swallow taps behind the panel
	const float PW = FMath::Min(640.f, W - 80.f), PH = 620.f, PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - PH * 0.5f;
	Stone(PX, PY, PW, PH, 30.f, AHStone, true);
	Img(Tex(TEXT("crest")), W * 0.5f - 42.f, PY + 2.f, 84.f, 34.f, 0.f, false);
	Text(TEXT("PAUSED"), PX, PY + 34.f, PW, 70.f, 46.f, ZCHex(TEXT("F2C14E")), 4.f);
	struct FRow { const TCHAR* Label; FLinearColor Face; TFunction<void()> Fn; };
	TArray<FRow> Rows;
	Rows.Add({ TEXT("RESUME"), AHSapphire, [this]() { SetPaused(false); } });
	Rows.Add({ TEXT("RESTART MATCH"), ZCHex(TEXT("4A4D55")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Rematch"), 0); } });
	Rows.Add({ TEXT("MAIN MENU"), ZCHex(TEXT("4A4D55")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Menu"), 0); } });
	Rows.Add({ TEXT("EXIT GAME"), ZCHex(TEXT("C2362F")), [this]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("Quit"), 0); } });
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const float BY = PY + 130.f + i * 112.f, BX = PX + 60.f, BWd = PW - 120.f;
		Stone(BX, BY, BWd, 90.f, 30.f, Rows[i].Face, true);
		Text(Rows[i].Label, BX, BY, BWd, 90.f, 32.f, FLinearColor::White, 4.f);
		Hit(BX, BY, BWd, 90.f, Rows[i].Fn);
	}
}

// ------------------------------------------------------------------------------------------------ input
void UZoogiArenaHUDWidget::PressCard(int32 CardSlot)
{
	DetailSlot = CardSlot;
	BuiltHand.Reset(); // detail panel follows the tap
	AZoogiGameMode* G = GM();
	if (!G) return;
	if (G->Stage == EZoogiStage::Awaken && G->IsHumanAiming() && !G->bAwakenDone) G->MenuAction(TEXT("CardToggle"), CardSlot);
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
	return Sig;
}

bool UZoogiArenaHUDWidget::TrayWanted() const
{
	// the tray opens for the phone player's card Standby (and the short Draw beat before it); it slides away while the
	// arena needs the screen: aiming / rolling, CPU turns, Resolve
	const AZoogiGameMode* G = GM();
	if (!G || G->Phase != EZoogiPhase::Aiming) return false;
	const AZoogiBall* B = G->GetActiveBall();
	if (!B || B->bCPU || B->PlayerIndex != LocalPlayer()) return false;
	return G->Stage == EZoogiStage::Draw || (G->Stage == EZoogiStage::Awaken && !G->bAwakenDone);
}

void UZoogiArenaHUDWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	Time += Dt;
	const bool bOn = ZoogiArenaHUD::IsActive(GetWorld());
	// never collapse this widget itself: a collapsed UserWidget stops ticking and would never see the match start (menu -> match).
	// Only the content canvas hides.
	if (Canvas) Canvas->SetVisibility(bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bOn) { if (bPaused) SetPaused(false); MatchClock = 0.f; BuiltSize = FVector2D::ZeroVector; return; }
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
	}
	if (bBoard) RefreshBoard(Dt); else Refresh(Dt);
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
		if (TestT > 0.8f && Once(1)) G->MenuAction(TEXT("AimNext"), 0);
		if (TestT > 1.4f && Once(2)) TestShot(TEXT("aim"));
		if (TestT > 1.8f && Once(4)) G->MenuAction(TEXT("AimConfirm"), 0);
		if (TestT > 4.0f && Once(8)) { G->MenuAction(TEXT("AimNext"), 0); G->MenuAction(TEXT("AimConfirm"), 0); } // retry if the lock was lost
	}
}

void UZoogiArenaHUDWidget::Refresh(float Dt)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const int32 Me = LocalPlayer();
	const bool bLive = G->Phase != EZoogiPhase::GameOver && !bPaused;
	if (bLive && G->Phase != EZoogiPhase::Forming) MatchClock += Dt;
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
		const int32 Charge = G->AwakenCharge[i];
		for (int32 k = 0; k < U.Pips.Num(); ++k)
			SetFill(U.Pips[k], k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : AHArcane) : ZCHex(TEXT("050607")));
	}

	// ---- Episode: acting team's gauge, round, my card energy
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
		if (RoundTxt.IsValid()) RoundTxt->SetText(FText::AsNumber(FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds))));
		if (RoundOfTxt.IsValid()) RoundOfTxt->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), G->MenuRounds)));
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Me) : nullptr;
		const int32 Energy = PS ? PS->Energy : 0;
		for (int32 k = 0; k < Crystals.Num(); ++k)
			if (UImage* I = Cast<UImage>(Crystals[k].Get())) I->SetColorAndOpacity(k < Energy ? FLinearColor::White : FLinearColor(0.25f, 0.27f, 0.32f, 0.8f));
	}

	// ---- combo callout: the acting team's Resonance this match
	{
		const int32 Team = G->bComboMode ? FMath::Clamp(G->ComboTeam, 0, 1) : 0;
		const int32 N = G->bComboMode ? G->ComboResonance[Team] : 0;
		const bool bShow = N > 0;
		if (ComboRoot.IsValid()) ComboRoot->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (TurnTag.IsValid()) TurnTag->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (ComboNum.IsValid())
		{
			ComboNum->SetText(FText::AsNumber(N));
			ComboNum->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (UWidget* P = ComboNum->GetParent()) (void)P;
		}
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
			case EZoogiStage::Awaken: Label = bMine ? (G->bAwakenDone ? TEXT("CARDS SET") : TEXT("CHOOSE CARDS")) : TEXT("STANDBY"); Frac = FMath::Clamp(G->StageTime / 6.f, 0.f, 1.f); break;
			case EZoogiStage::Battle:
				Label = G->BattleStep == EZoogiBattleStep::PowerUp ? TEXT("POWER UP") : G->BattleStep == EZoogiBattleStep::Rolling ? TEXT("ROLL ACTIVE") : bMine ? TEXT("DRAG TO AIM") : TEXT("AIMING");
				Frac = G->BattleStep == EZoogiBattleStep::Aim ? G->AimFraction() : FMath::Clamp(G->StepTime / 1.f, 0.f, 1.f);
				break;
			case EZoogiStage::Resolve: Label = TEXT("RESOLVE"); Frac = FMath::Clamp(G->StageTime / 2.f, 0.f, 1.f); break;
			default: Label = TEXT("TURN END"); Frac = 1.f; break;
			}
		}
		if (BannerTxt.IsValid()) BannerTxt->SetText(FText::FromString(Label));
		if (TurnTagTxt.IsValid()) TurnTagTxt->SetText(FText::FromString(G->Phase == EZoogiPhase::GameOver ? FString() : (bMine ? FString::Printf(TEXT("YOUR TURN  -  %s"), *Who) : Who)));
		if (BannerFill.IsValid())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(BannerFill->Slot)) Sl->SetSize(FVector2D(FMath::Max(4.f, BannerFillW * Frac), 16.f) * S);
	}

	// ---- tray: slide + controls state
	{
		TrayT = FMath::FInterpTo(TrayT, TrayWanted() ? 1.f : 0.f, Dt, 9.f);
		if (TrayRoot.IsValid())
		{
			TrayRoot->SetRenderTranslation(FVector2D(0.f, (1.f - TrayT) * TrayHidden * S));
			TrayRoot->SetVisibility(TrayT < 0.02f ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
		if (BannerRoot.IsValid()) BannerRoot->SetRenderTranslation(FVector2D(0.f, (1.f - TrayT) * BannerShift * S));
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Me) : nullptr;
		const bool bStandby = G->Stage == EZoogiStage::Awaken && G->IsHumanAiming() && !G->bAwakenDone;
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
			Show(Cards[k].Dim, !bAfford || !bStandby);
			if (Cards[k].Root.IsValid()) Cards[k].Root->SetRenderTranslation(FVector2D(0.f, bSel ? -12.f * S : 0.f));
		}
		if (ActivateCost.IsValid()) ActivateCost->SetText(FText::AsNumber(Cost));
		Show(ActivateDim, !bStandby);
		const bool bReady = G->CardsAwakenReady(Me) && !G->bStandbyAwaken;
		if (AwakenGlow.IsValid())
		{
			AwakenGlow->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			AwakenGlow->SetRenderOpacity(0.45f + 0.55f * FMath::Abs(FMath::Sin(Time * 2.6f)));
		}
		const int32 Charge = G->AwakenCharge[FMath::Clamp(Me, 0, 3)];
		for (int32 k = 0; k < AwakenPips.Num(); ++k)
			SetFill(AwakenPips[k], k < Charge ? ZCHex(TEXT("FFD970")) : ZCHex(TEXT("2B2D33")));
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
	const float TrayRef = 1255.f;
	BoardBotY = H - (1672.f - TrayRef);
	auto BY = [this, TrayRef](float Ry) { return BoardBotY + (Ry - TrayRef); };

	// ---- top band: logo + phase stepper (reference art), live stepper states
	RefCrop(0.f, 0.f, 941.f, 172.f, 0.f, 0.f, W, 172.f);
	const TCHAR* StepNames[5] = { TEXT("DRAW"), TEXT("AWAKEN"), TEXT("BATTLE"), TEXT("RESOLVE"), TEXT("END") };
	for (int32 k = 0; k < 5; ++k)
	{
		const float* R = BRStep[k];
		StepGlow[k] = At(Box(ZCHex(TEXT("1B2342")), ZCHex(TEXT("2B3866")), 1.5f, 6.f), R[0], R[1], R[2] - R[0], R[3] - R[1]);
		StepTxt[k] = Text(StepNames[k], R[0], R[1], R[2] - R[0], R[3] - R[1], k == 2 ? 21.f : 17.f, ZCHex(TEXT("DCE6FF")), 2.f);
	}
	Hit(30.f, 12.f, 74.f, 62.f, [this]() { SetPaused(!bPaused); }); // gear -> pause / settings

	// ---- arena + round plates
	RefCrop(298.f, 176.f, 642.f, 262.f, 298.f, 176.f, 344.f, 86.f);
	At(Box(BRCover, FLinearColor::Transparent, 0.f, 4.f), 352.f, 185.f, 236.f, 27.f);
	ArenaTxt = Text(TEXT(""), 352.f, 183.f, 236.f, 30.f, 22.f, FLinearColor::White, 2.f);
	At(Box(BRCover, FLinearColor::Transparent, 0.f, 4.f), 392.f, 226.f, 156.f, 26.f);
	RoundTxt = Text(TEXT(""), 392.f, 224.f, 156.f, 30.f, 18.f, FLinearColor::White, 2.f);

	// ---- player panels
	for (int32 i = 0; i < 4; ++i)
	{
		const float OX = BRPanel[i][0], OY = BRPanel[i][1];
		FUnitRefs& U = Units[i];
		const AZoogiBall* B = G->Balls.IsValidIndex(i) ? G->Balls[i].Get() : nullptr;
		U.Active = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFD35C")), 5.f, 18.f), OX - 6.f, OY - 6.f, 282.f, 182.f);
		RefCrop(OX - 2.f, OY - 2.f, OX + 272.f, OY + 172.f, OX - 2.f, OY - 2.f, 274.f, 174.f);
		if (!B) continue;
		if (UTexture2D* P = FileTex(TEXT("Data/MenuArt"), PortraitKey(B->Def.Id))) Img(P, OX + 22.f, OY + 19.f, 88.f, 88.f, 44.f, true);
		At(Box(BRCoverDeep, FLinearColor::Transparent, 0.f, 4.f), OX + 114.f, OY + 14.f, 156.f, 38.f);
		Text(FString::Printf(TEXT("P%d %s"), i + 1, *B->Def.Name.ToUpper()), OX + 124.f, OY + 16.f, 146.f, 34.f, 22.f, FLinearColor::White, 2.f, 0);
		At(Box(ZCHex(TEXT("262B4D")), ZCHex(TEXT("8E7CFF")), 2.f, 12.f), OX + 122.f, OY + 54.f, 58.f, 25.f);
		Text(B->bCPU ? TEXT("CPU") : TEXT("YOU"), OX + 122.f, OY + 54.f, 58.f, 25.f, 15.f, ZCHex(TEXT("D8D2FF")), 1.f);
		At(Box(BRCoverDeep, FLinearColor::Transparent, 0.f, 4.f), OX + 116.f, OY + 86.f, 108.f, 26.f);
		for (int32 k = 0; k < AZoogiGameMode::AwakenChargeMax; ++k)
			U.Pips.Add(At(Box(ZCHex(TEXT("232838")), ZCHex(TEXT("555B6E")), 1.5f, 9.f), OX + 122.f + k * 24.f, OY + 90.f, 18.f, 18.f));
		At(Box(BRCoverDeep, FLinearColor::Transparent, 0.f, 6.f), OX + 210.f, OY + 46.f, 56.f, 64.f);
		U.Score = Text(TEXT("0"), OX + 206.f, OY + 44.f, 62.f, 68.f, 44.f, FLinearColor::White, 3.f);
		At(Box(BRCoverDeep, FLinearColor::Transparent, 0.f, 4.f), OX + 24.f, OY + 112.f, 150.f, 24.f);
		EpTxt[i] = Text(TEXT("EPISODE 0/100"), OX + 26.f, OY + 112.f, 160.f, 24.f, 15.f, FLinearColor::White, 1.f, 0);
		At(Box(ZCHex(TEXT("0A0D16")), ZCHex(TEXT("3B4256")), 2.f, 9.f), OX + 24.f, OY + 136.f, 222.f, 18.f);
		U.BarW = 216.f;
		U.Bar = At(Box(ZCHex(TEXT("2F7BF0")), FLinearColor::Transparent, 0.f, 7.f), OX + 27.f, OY + 139.f, 10.f, 12.f);
	}

	// ---- bottom tray (reference art pinned to the bottom edge)
	RefCrop(0.f, TrayRef, 941.f, 1672.f, 0.f, BoardBotY, W, 1672.f - TrayRef);
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(LocalPlayer()) : nullptr;
	// deck pile + discard + energy readouts
	At(Box(ZCHex(TEXT("1A2133")), FLinearColor::Transparent, 0.f, 6.f), 74.f, BY(1420.f), 62.f, 44.f);
	DeckTxt = Text(TEXT("0"), 70.f, BY(1418.f), 70.f, 48.f, 36.f, FLinearColor::White, 3.f);
	At(Box(ZCHex(TEXT("141A2A")), FLinearColor::Transparent, 0.f, 6.f), 808.f, BY(1374.f), 98.f, 46.f);
	DiscardTxt = Text(TEXT("0"), 808.f, BY(1372.f), 98.f, 50.f, 34.f, FLinearColor::White, 3.f);
	At(Box(ZCHex(TEXT("141A2A")), FLinearColor::Transparent, 0.f, 6.f), 662.f, BY(1580.f), 82.f, 62.f);
	EnergyTxt = Text(TEXT("0/5"), 660.f, BY(1578.f), 86.f, 42.f, 30.f, FLinearColor::White, 3.f);
	Text(TEXT("ENERGY"), 660.f, BY(1618.f), 86.f, 22.f, 13.f, FLinearColor::White, 1.f);
	// cards: the reference's five slots, compressed when the hand holds more
	const int32 N = PS ? PS->Hand.Num() : 0;
	At(Box(ZCHex(TEXT("10141F")).CopyWithNewOpacity(0.97f), FLinearColor::Transparent, 0.f, 8.f), 170.f, BY(1296.f), 600.f, 164.f);
	if (N == 0) Text(G->CardsOn() ? TEXT("NO CARDS IN HAND") : TEXT("CARDS OFF"), 170.f, BY(1296.f), 600.f, 164.f, 22.f, ZCMuted, 2.f);
	{
		const float X0 = 174.f, X1 = 766.f, Gap = 6.f, CH = 156.f;
		const float CW = FMath::Min(113.f, (X1 - X0 - Gap * (FMath::Max(N, 1) - 1)) / FMath::Max(N, 1));
		const float Used = N * CW + (N - 1) * Gap, XS = X0 + (X1 - X0 - Used) * 0.5f;
		for (int32 k = 0; k < N; ++k) BuildCard(k, XS + k * (CW + Gap), BY(1300.f), CW, CH);
	}
	// detail panel (selected card)
	At(Box(ZCHex(TEXT("0F1420")), FLinearColor::Transparent, 0.f, 8.f), 170.f, BY(1496.f), 294.f, 128.f);
	if (PS && PS->Hand.Num() > 0)
	{
		DetailSlot = FMath::Clamp(DetailSlot, 0, PS->Hand.Num() - 1);
		const FZoogiCardInstance& Inst = PS->Hand[DetailSlot];
		const FZoogiCardDef* D = CS->Def(Inst.CardId);
		At(Box(ZCHex(TEXT("9AA0AC")), ZCDark, 2.f, 6.f), 174.f, BY(1502.f), 86.f, 102.f);
		if (UTexture2D* Art = CardArt(Inst.CardId)) Img(Art, 177.f, BY(1505.f), 80.f, 96.f, 4.f, true);
		Text(D ? D->Name.ToUpper() : Inst.CardId.ToString().ToUpper(), 272.f, BY(1496.f), 190.f, 30.f, 22.f, FLinearColor::White, 2.f, 0);
		const FString Tag = D ? FString(ZoogiCards::TypeName(D->Type)).ToUpper() : FString();
		At(Box(ZCHex(TEXT("2B2350")), ZCHex(TEXT("9D7CFF")), 1.5f, 11.f), 274.f, BY(1528.f), FMath::Max(80.f, 11.f * Tag.Len() + 22.f), 22.f);
		Text(Tag, 274.f, BY(1528.f), FMath::Max(80.f, 11.f * Tag.Len() + 22.f), 22.f, 12.f, ZCHex(TEXT("ECE4FF")), 0.f);
		if (UTextBlock* Desc = Text(D ? D->Rule : FString(), 272.f, BY(1553.f), 190.f, 68.f, 12.5f, ZCHex(TEXT("D9DCE3")), 0.f, 0)) Desc->SetAutoWrapText(true);
	}
	// ACTIVATE (reference button) + live cost
	ActivateDim = At(Box(FLinearColor(0.02f, 0.02f, 0.04f, 0.55f), FLinearColor::Transparent, 0.f, 26.f), 468.f, BY(1526.f), 162.f, 56.f);
	Hit(466.f, BY(1522.f), 166.f, 64.f, [this]() { PressActivate(); });
	At(Box(ZCHex(TEXT("121726")), FLinearColor::Transparent, 0.f, 6.f), 540.f, BY(1588.f), 46.f, 28.f);
	ActivateCost = Text(TEXT("0"), 536.f, BY(1586.f), 54.f, 32.f, 22.f, FLinearColor::White, 2.f);
	// AWAKEN (reference orb) + live charge pips
	AwakenGlow = At(Box(FLinearColor::Transparent, ZCHex(TEXT("FFC94D")), 7.f, 90.f), 754.f, BY(1458.f), 180.f, 180.f);
	At(Box(ZCHex(TEXT("1E1407")), FLinearColor::Transparent, 0.f, 14.f), 776.f, BY(1614.f), 132.f, 30.f);
	for (int32 k = 0; k < AZoogiGameMode::AwakenChargeMax; ++k)
		AwakenPips.Add(At(Box(ZCHex(TEXT("2B2D33")), ZCHex(TEXT("4D4F55")), 1.5f, 11.f), 784.f + k * 30.f, BY(1618.f), 22.f, 22.f));
	Hit(756.f, BY(1460.f), 174.f, 190.f, [this]() { PressAwaken(); });
}

void UZoogiArenaHUDWidget::RefreshBoard(float Dt)
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	const int32 Me = LocalPlayer();
	auto Show = [](TWeakObjectPtr<UWidget> W, bool b) { if (W.IsValid()) W->SetVisibility(b ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); };
	auto SetFill = [](TWeakObjectPtr<UWidget> W, const FLinearColor& C) { if (UBorder* B = Cast<UBorder>(W.Get())) B->SetBrushColor(C); };
	// the portrait camera keeps the play focus between the arena / round plates and the tray
	ZoogiArenaHUD::SetBoardBand(270.f / FMath::Max(1.f, Design.Y), (1672.f - 1255.f + 10.f) / FMath::Max(1.f, Design.Y));

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
	if (RoundTxt.IsValid()) RoundTxt->SetText(FText::FromString(FString::Printf(TEXT("ROUND %d / %d"), FMath::Clamp(G->RoundNumber, 1, FMath::Max(1, G->MenuRounds)), G->MenuRounds)));
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
		const float E = SM ? SM->GetEnergy(i) : 0.f;
		if (EpTxt[i].IsValid()) EpTxt[i]->SetText(FText::FromString(FString::Printf(TEXT("EPISODE %d/100"), FMath::RoundToInt(E))));
		if (U.Bar.IsValid())
			if (UCanvasPanelSlot* Sl = Cast<UCanvasPanelSlot>(U.Bar->Slot)) Sl->SetSize(FVector2D(FMath::Max(10.f, U.BarW * FMath::Clamp(E / 100.f, 0.f, 1.f)), 12.f) * S);
		const int32 Charge = G->AwakenCharge[i];
		for (int32 k = 0; k < U.Pips.Num(); ++k) SetFill(U.Pips[k], k < Charge ? BRPlayer[i] : ZCHex(TEXT("232838")));
	}
	// tray readouts
	const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(this);
	const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(Me) : nullptr;
	if (DeckTxt.IsValid()) DeckTxt->SetText(FText::AsNumber(PS ? PS->DrawPile.Num() : 0));
	if (DiscardTxt.IsValid()) DiscardTxt->SetText(FText::AsNumber(PS ? PS->Discard.Num() : 0));
	if (EnergyTxt.IsValid()) EnergyTxt->SetText(FText::FromString(FString::Printf(TEXT("%d/5"), PS ? PS->Energy : 0)));
	const bool bStandby = G->Stage == EZoogiStage::Awaken && G->IsHumanAiming() && !G->bAwakenDone;
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
		Show(Cards[k].Dim, !bAfford || !bStandby);
		if (Cards[k].Root.IsValid()) Cards[k].Root->SetRenderTranslation(FVector2D(0.f, bSel ? -10.f * S : 0.f));
	}
	if (ActivateCost.IsValid()) ActivateCost->SetText(FText::AsNumber(Cost));
	Show(ActivateDim, !bStandby);
	const bool bReady = G->CardsAwakenReady(Me) && !G->bStandbyAwaken;
	if (AwakenGlow.IsValid())
	{
		AwakenGlow->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		AwakenGlow->SetRenderOpacity(0.45f + 0.55f * FMath::Abs(FMath::Sin(Time * 2.6f)));
	}
	const int32 Charge = G->AwakenCharge[FMath::Clamp(Me, 0, 3)];
	for (int32 k = 0; k < AwakenPips.Num(); ++k) SetFill(AwakenPips[k], k < Charge ? ZCHex(TEXT("FFD970")) : ZCHex(TEXT("2B2D33")));
	(void)Dt;
}
