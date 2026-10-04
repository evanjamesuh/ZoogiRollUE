#include "ZoogiMenuWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	const FLinearColor GMenuOrbBlue(0.53f, 0.81f, 0.92f, 1.f);
	const FLinearColor GGold(1.f, 0.75f, 0.2f, 1.f);
	const FLinearColor GInk(0.8f, 0.86f, 0.97f, 1.f);

	FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	FLinearColor Lighten(const FLinearColor& C, float T) { return FMath::Lerp(C, FLinearColor::White, T).CopyWithNewOpacity(1.f); }
}

void UZoogiMenuClick::Click()
{
	if (Owner) Owner->Fire(Action, Index);
}

void UZoogiMenuClick::Hover() {}

void UZoogiMenuWidget::Fire(FName Action, int32 Index)
{
	if (AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr) G->MenuAction(Action, Index);
}

TSharedRef<SWidget> UZoogiMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		BuildContents();
	}
	return Super::RebuildWidget();
}

void UZoogiMenuWidget::Refresh(float DeltaSeconds)
{
	Time += DeltaSeconds;
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (G && Root && G->MenuRevision != BuiltRevision)
	{
		Root->ClearChildren();
		Clicks.Reset();
		BuildContents();
	}
	if (Title)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 2.f);
		Title->SetColorAndOpacity(FSlateColor(FMath::Lerp(GMenuOrbBlue, FLinearColor::White, Pulse * 0.35f)));
	}
}

void UZoogiMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

UTextBlock* UZoogiMenuWidget::MakeText(UPanelWidget* Parent, const FString& S, int32 Size, const FLinearColor& C, bool bBold)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(S));
	FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	T->SetFont(F);
	T->SetColorAndOpacity(FSlateColor(C));
	T->SetShadowOffset(FVector2D(1.5f, 1.5f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	if (Parent) Parent->AddChild(T);
	return T;
}

UButton* UZoogiMenuWidget::MakeButton(UPanelWidget* Parent, const FString& Label, FName Action, int32 Index, const FLinearColor& Accent, bool bSelected, float Width, float Height, int32 FontSize)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Width > 0.f) Box->SetWidthOverride(Width);
	if (Height > 0.f) Box->SetHeightOverride(Height);
	UButton* Btn = WidgetTree->ConstructWidget<UZoogiButton>(UZoogiButton::StaticClass());
	const FLinearColor Base = bSelected ? (Accent * 0.7f).CopyWithNewOpacity(0.95f) : FLinearColor(0.07f, 0.09f, 0.17f, 0.92f);
	const FLinearColor Edge = bSelected ? Lighten(Accent, 0.35f) : (Accent * 0.65f).CopyWithNewOpacity(1.f);
	FButtonStyle Style;
	Style.SetNormal(Rounded(Base, 10.f, Edge, 2.f));
	Style.SetHovered(Rounded(Base + FLinearColor(0.12f, 0.12f, 0.14f, 0.f), 10.f, Lighten(Accent, 0.6f), 3.f));
	Style.SetPressed(Rounded(Accent * 0.5f, 10.f, FLinearColor::White, 3.f));
	Style.SetNormalPadding(FMargin(6.f, 2.f));
	Style.SetPressedPadding(FMargin(6.f, 4.f, 6.f, 0.f));
	Btn->SetStyle(Style);
	UTextBlock* T = MakeText(nullptr, Label, FontSize, FLinearColor::White);
	T->SetJustification(ETextJustify::Center);
	Btn->AddChild(T);
	Box->AddChild(Btn);
	if (Parent) Parent->AddChild(Box);

	UZoogiMenuClick* C = NewObject<UZoogiMenuClick>(this);
	C->Owner = this;
	C->Action = Action;
	C->Index = Index;
	Clicks.Add(C);
	Btn->OnClicked.AddDynamic(C, &UZoogiMenuClick::Click);
	return Btn;
}

void UZoogiMenuWidget::BuildPlayerCard(UHorizontalBox* Row, int32 i)
{
	const AZoogiGameMode* G = GetWorld()->GetAuthGameMode<AZoogiGameMode>();
	const FZoogiPlayerSetup& S = G->MenuPlayers[i];
	const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
	const FZoogiDef& D = Roster[S.ZoogiIndex % Roster.Num()];
	const FLinearColor PC = ZoogiUtil::PlayerColor(i);
	const float CardW = G->MenuNumPlayers >= 4 ? 318.f : 380.f;

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(CardW);
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrush(Rounded(FLinearColor(0.04f, 0.055f, 0.12f, 0.94f), 16.f, PC, 3.f));
	Card->SetPadding(FMargin(16.f, 12.f));
	Size->AddChild(Card);
	if (UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(Size)) HS->SetPadding(FMargin(8.f, 0.f));

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(V);

	// Header: player tag + CPU/HUMAN toggle.
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	V->AddChildToVerticalBox(Head);
	UTextBlock* Tag = MakeText(nullptr, FString::Printf(TEXT("P%d"), i + 1), 26, PC);
	if (UHorizontalBoxSlot* HS = Head->AddChildToHorizontalBox(Tag)) { HS->SetVerticalAlignment(VAlign_Center); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	MakeButton(Head, S.bCPU ? TEXT("CPU") : TEXT("HUMAN"), "ToggleCPU", i, PC, !S.bCPU, 130.f, 40.f, 18);

	// Zoogi swatch with prev / next.
	UHorizontalBox* Pick = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(Pick)) { VS->SetPadding(FMargin(0.f, 10.f)); VS->SetHorizontalAlignment(HAlign_Center); }
	UButton* Prev = MakeButton(Pick, TEXT("<"), "ZoogiPrev", i, PC, false, 46.f, 46.f, 24);
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Prev->GetParent()->Slot)) HS->SetVerticalAlignment(VAlign_Center);
	// Portrait: an in-game hero shot of the Zoogi (Art/Portraits -> /Game/UI/Portraits), framed in its colours.
	// Falls back to the coloured initial if the texture is missing.
	const float PortraitSize = 132.f;
	USizeBox* OrbBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	OrbBox->SetWidthOverride(PortraitSize);
	OrbBox->SetHeightOverride(PortraitSize);
	const FString TexPath = FString::Printf(TEXT("/Game/UI/Portraits/T_Portrait_%s.T_Portrait_%s"), *D.Id.ToString(), *D.Id.ToString());
	if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(Rounded(Lighten(D.Color, 0.15f), 22.f, Lighten(D.Secondary, 0.3f), 3.f));
		Frame->SetPadding(FMargin(4.f));
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = FVector2D(PortraitSize - 8.f, PortraitSize - 8.f);
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(18.f, 18.f, 18.f, 18.f), FSlateColor(FLinearColor::Transparent), 0.f);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Img->SetBrush(B);
		Frame->SetContent(Img);
		OrbBox->AddChild(Frame);
	}
	else
	{
		UBorder* Orb = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Orb->SetBrush(Rounded(D.Color, PortraitSize * 0.5f, Lighten(D.Secondary, 0.2f), 6.f));
		Orb->SetHorizontalAlignment(HAlign_Center);
		Orb->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Initial = MakeText(nullptr, D.Name.Left(1), 52, FLinearColor::White);
		Orb->SetContent(Initial);
		OrbBox->AddChild(Orb);
	}
	if (UHorizontalBoxSlot* HS = Pick->AddChildToHorizontalBox(OrbBox)) HS->SetPadding(FMargin(22.f, 0.f));
	UButton* Next = MakeButton(Pick, TEXT(">"), "ZoogiNext", i, PC, false, 46.f, 46.f, 24);
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Next->GetParent()->Slot)) HS->SetVerticalAlignment(VAlign_Center);

	auto Centered = [&](UTextBlock* T, float PadBottom)
	{
		T->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(T)) { VS->SetHorizontalAlignment(HAlign_Fill); VS->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom)); }
	};
	Centered(MakeText(nullptr, D.Name.ToUpper(), 30, Lighten(D.Color, 0.3f)), 0.f);
	Centered(MakeText(nullptr, FString::Printf(TEXT("%s type  -  mass %.2f"), *D.Type, D.MassRatio()), 15, GInk, false), 8.f);

	// Power.
	UBorder* PowerBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PowerBox->SetBrush(Rounded(FLinearColor(0.12f, 0.09f, 0.03f, 0.85f), 10.f, GGold * 0.6f, 1.5f));
	PowerBox->SetPadding(FMargin(10.f, 6.f));
	if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(PowerBox)) VS->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));
	UVerticalBox* PV = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PowerBox->SetContent(PV);
	MakeText(PV, FString::Printf(TEXT("POWER: %s"), *D.Ability.ToUpper()), 16, GGold);
	UTextBlock* Desc = MakeText(PV, D.AbilityDesc, 13, FLinearColor(0.92f, 0.88f, 0.8f), false);
	Desc->SetAutoWrapText(true);

	// Stat bars.
	const TPair<const TCHAR*, int32> Stats[] = { { TEXT("SPD"), D.Speed }, { TEXT("PWR"), D.Power }, { TEXT("DEF"), D.Defense }, { TEXT("CTL"), D.Control } };
	for (const TPair<const TCHAR*, int32>& St : Stats)
	{
		UHorizontalBox* SR = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UVerticalBoxSlot* VS = V->AddChildToVerticalBox(SR)) VS->SetPadding(FMargin(0.f, 2.f));
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(52.f);
		MakeText(LabelBox, St.Key, 14, GInk);
		if (UHorizontalBoxSlot* HS = SR->AddChildToHorizontalBox(LabelBox)) HS->SetVerticalAlignment(VAlign_Center);
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarBox->SetHeightOverride(12.f);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle PS;
		PS.SetBackgroundImage(Rounded(FLinearColor(0.1f, 0.12f, 0.2f, 1.f), 6.f));
		PS.SetFillImage(Rounded(FLinearColor::White, 6.f));
		Bar->SetWidgetStyle(PS);
		Bar->SetPercent(FMath::Clamp(St.Value / 100.f, 0.f, 1.f));
		Bar->SetFillColorAndOpacity(FMath::Lerp(PC, D.Color, 0.5f).CopyWithNewOpacity(1.f));
		BarBox->AddChild(Bar);
		if (UHorizontalBoxSlot* HS = SR->AddChildToHorizontalBox(BarBox)) { HS->SetVerticalAlignment(VAlign_Center); HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		UTextBlock* Num = MakeText(nullptr, FString::FromInt(St.Value), 14, FLinearColor::White);
		if (UHorizontalBoxSlot* HS = SR->AddChildToHorizontalBox(Num)) { HS->SetVerticalAlignment(VAlign_Center); HS->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f)); }
	}
}

void UZoogiMenuWidget::BuildContents()
{
	AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G || !Root) return;
	BuiltRevision = G->MenuRevision;

	// Soft vignette over the live arena behind the menu.
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Shade->SetBrushColor(FLinearColor(0.f, 0.015f, 0.05f, 0.4f));
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* OS = Root->AddChildToOverlay(Shade)) { OS->SetHorizontalAlignment(HAlign_Fill); OS->SetVerticalAlignment(VAlign_Fill); }

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* OS = Root->AddChildToOverlay(Main)) { OS->SetHorizontalAlignment(HAlign_Center); OS->SetVerticalAlignment(VAlign_Center); }

	Title = MakeText(nullptr, TEXT("ZOOGI ROLL"), 84, GMenuOrbBlue);
	{
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Black", 84);
		F.OutlineSettings.OutlineSize = 4;
		F.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.05f, 0.15f, 1.f);
		F.LetterSpacing = 120;
		Title->SetFont(F);
	}
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Title)) VS->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Sub = MakeText(nullptr, TEXT("lock on  -  fire  -  knock orbs and rivals off the edge  -  gold stars charge your power"), 18, GInk, false);
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Sub)) { VS->SetHorizontalAlignment(HAlign_Center); VS->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f)); }

	// Players + rounds.
	UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Top)) { VS->SetHorizontalAlignment(HAlign_Center); VS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f)); }
	auto Label = [&](UHorizontalBox* Box, const FString& S, float Pad)
	{
		UTextBlock* T = MakeText(nullptr, S, 22, FLinearColor(0.7f, 0.8f, 1.f));
		if (UHorizontalBoxSlot* HS = Box->AddChildToHorizontalBox(T)) { HS->SetVerticalAlignment(VAlign_Center); HS->SetPadding(FMargin(Pad, 0.f, 14.f, 0.f)); }
	};
	auto Gap = [&](UHorizontalBox* Box, float W)
	{
		USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		Sp->SetSize(FVector2D(W, 1.f));
		Box->AddChildToHorizontalBox(Sp);
	};
	Label(Top, TEXT("PLAYERS"), 0.f);
	for (int32 n = 2; n <= 4; ++n) { MakeButton(Top, FString::FromInt(n), "Players", n, GMenuOrbBlue, G->MenuNumPlayers == n, 64.f, 50.f, 24); Gap(Top, 8.f); }
	Label(Top, TEXT("ROUNDS"), 60.f);
	MakeButton(Top, TEXT("-"), "Rounds", G->MenuRounds - 1, GMenuOrbBlue, false, 50.f, 50.f, 26);
	{
		USizeBox* NB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NB->SetWidthOverride(70.f);
		UTextBlock* N = MakeText(NB, FString::FromInt(G->MenuRounds), 28, FLinearColor::White);
		N->SetJustification(ETextJustify::Center);
		if (UHorizontalBoxSlot* HS = Top->AddChildToHorizontalBox(NB)) HS->SetVerticalAlignment(VAlign_Center);
	}
	MakeButton(Top, TEXT("+"), "Rounds", G->MenuRounds + 1, GMenuOrbBlue, false, 50.f, 50.f, 26);

	// Player cards.
	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Cards)) { VS->SetHorizontalAlignment(HAlign_Center); VS->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f)); }
	for (int32 i = 0; i < G->MenuNumPlayers && G->MenuPlayers.IsValidIndex(i); ++i) BuildPlayerCard(Cards, i);

	// Arena picker.
	UHorizontalBox* ArenaRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(ArenaRow)) { VS->SetHorizontalAlignment(HAlign_Center); VS->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f)); }
	Label(ArenaRow, TEXT("ARENA"), 0.f);
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(5.f));
	ArenaRow->AddChildToHorizontalBox(Grid);
	const int32 NumArenas = (int32)EZoogiArena::MAX;
	const int32 PerRow = (NumArenas + 1) / 2;
	for (int32 a = 0; a < NumArenas; ++a)
	{
		UButton* B = MakeButton(nullptr, ZoogiUtil::ArenaName((EZoogiArena)a), "Arena", a, FLinearColor(0.4f, 0.9f, 0.45f), G->MenuArena == (EZoogiArena)a, 230.f, 48.f, 18);
		Grid->AddChildToUniformGrid(B->GetParent(), a / PerRow, a % PerRow);
	}

	// Start.
	UButton* Start = MakeButton(nullptr, TEXT("START MATCH"), "Start", 0, GGold, true, 420.f, 78.f, 34);
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Start->GetParent())) { VS->SetHorizontalAlignment(HAlign_Center); VS->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f)); }
	UTextBlock* Hint = MakeText(nullptr, TEXT("Click an enemy or orb to lock on  -  Tab: cycle targets  -  SPACE: fire  -  E / Q: Zoogi power  -  RMB / Esc: clear lock"), 16, GInk, false);
	if (UVerticalBoxSlot* VS = Main->AddChildToVerticalBox(Hint)) VS->SetHorizontalAlignment(HAlign_Center);

	// Quit, top right.
	UButton* Quit = MakeButton(nullptr, TEXT("QUIT"), "Quit", 0, FLinearColor(0.9f, 0.3f, 0.3f), false, 130.f, 50.f, 20);
	if (UOverlaySlot* OS = Root->AddChildToOverlay(Quit->GetParent())) { OS->SetHorizontalAlignment(HAlign_Right); OS->SetVerticalAlignment(VAlign_Top); OS->SetPadding(FMargin(0.f, 28.f, 30.f, 0.f)); }
}
