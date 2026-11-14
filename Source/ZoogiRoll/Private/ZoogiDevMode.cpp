#include "ZoogiDevMode.h"
#include "ZoogiP5Style.h"
#include "ZoogiPlayerController.h"
#include "ZoogiDevConfig.h"
#include "ZoogiCamConfig.h"
#include "ZoogiArena.h"
#include "ZoogiAnimeCues.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiLaunchPad.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ZoogiTune.h"
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiBumpers.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"

// ------------------------------------------------------------------ layout IO

namespace
{
	static int32 GDevState = -1; // -1 = not decided yet (read the command line)
	constexpr int32 MouseFinger = 10;
}

bool ZoogiLayout::IsDevMode()
{
	if (GDevState < 0) GDevState = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDev")) ? 1 : 0;
	return GDevState == 1;
}

void ZoogiLayout::SetDevMode(bool bOn) { GDevState = bOn ? 1 : 0; }

const TCHAR* ZoogiLayout::LayoutKey() { return TEXT("layout.props"); }

FString ZoogiLayout::PathFor(const FString& Slug)
{
	FString Dir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ZoogiLayoutDir="), Dir)) Dir = TEXT("C:/dev/zoogi-layouts");
	return FPaths::Combine(Dir, Slug + TEXT(".json"));
}

TSharedPtr<FJsonValue> ZoogiLayout::ToJson(const TArray<AZoogiArena::FZoogiDevProp>& Props)
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const AZoogiArena::FZoogiDevProp& P : Props)
	{
		if (P.Type == TEXT("obstacle") && !P.IsMovedObstacle() && !P.bBumper) continue; // Oct 29: only changed obstacles are saved
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), P.Id);
		O->SetStringField(TEXT("type"), P.Type.ToString());
		O->SetNumberField(TEXT("x"), FMath::RoundToFloat(P.Location.X * 10.f) / 10.f);
		O->SetNumberField(TEXT("y"), FMath::RoundToFloat(P.Location.Y * 10.f) / 10.f);
		O->SetNumberField(TEXT("z"), FMath::RoundToFloat(P.Location.Z * 10.f) / 10.f);
		O->SetNumberField(TEXT("yaw"), FMath::RoundToFloat(P.Yaw * 10.f) / 10.f);
		O->SetNumberField(TEXT("scale"), FMath::RoundToFloat(P.Scale * 1000.f) / 1000.f);
		if (P.bBumper) { O->SetBoolField(TEXT("bumper"), true); O->SetNumberField(TEXT("bounce"), P.Bounce); }
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	return MakeShared<FJsonValueArray>(Arr);
}

bool ZoogiLayout::FromJson(const TSharedPtr<FJsonValue>& V, TArray<AZoogiArena::FZoogiDevProp>& Out)
{
	if (!V.IsValid() || V->Type != EJson::Array) return false;
	for (const TSharedPtr<FJsonValue>& E : V->AsArray())
	{
		const TSharedPtr<FJsonObject> O = E.IsValid() ? E->AsObject() : nullptr;
		if (!O) continue;
		AZoogiArena::FZoogiDevProp P;
		O->TryGetStringField(TEXT("id"), P.Id);
		P.Type = FName(O->GetStringField(TEXT("type")));
		double X = 0, Y = 0, Z = 0, Yaw = 0, Sc = 1;
		O->TryGetNumberField(TEXT("x"), X); O->TryGetNumberField(TEXT("y"), Y); O->TryGetNumberField(TEXT("z"), Z);
		O->TryGetNumberField(TEXT("yaw"), Yaw); O->TryGetNumberField(TEXT("scale"), Sc);
		P.Location = FVector(X, Y, Z);
		P.Yaw = (float)Yaw;
		P.Scale = FMath::Clamp((float)Sc, 0.25f, 4.f);
		bool bB = false; double Bo = 5;
		if (O->TryGetBoolField(TEXT("bumper"), bB)) P.bBumper = bB;
		if (O->TryGetNumberField(TEXT("bounce"), Bo)) P.Bounce = FMath::Clamp((int32)Bo, 1, 10);
		Out.Add(P);
	}
	return true;
}

bool ZoogiLayout::Load(const FString& Slug, TArray<AZoogiArena::FZoogiDevProp>& Out)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *PathFor(Slug))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { UE_LOG(LogZoogi, Warning, TEXT("LAYOUT: bad JSON %s"), *PathFor(Slug)); return false; }
	const TArray<TSharedPtr<FJsonValue>>* Props = nullptr;
	if (!Root->TryGetArrayField(TEXT("props"), Props)) return false;
	return FromJson(MakeShared<FJsonValueArray>(*Props), Out);
}

bool ZoogiLayout::Save(const AZoogiArena* Arena, FString& OutPath)
{
	if (!Arena) return false;
	const FString Slug = Arena->GetArenaSlug();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("arena"), Slug);
	Root->SetNumberField(TEXT("version"), 2);
	Root->SetStringField(TEXT("savedAt"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")) + TEXT(" local"));
	Root->SetStringField(TEXT("units"), TEXT("world cm / yaw degrees; bake with C:\\dev\\bake-layouts.ps1"));
	Root->SetField(TEXT("props"), ToJson(Arena->DevProps));
	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
	OutPath = PathFor(Slug);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
	const bool bOk = FFileHelper::SaveStringToFile(Text, *OutPath);
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: legacy layout file %s -> %s %s"), *Slug, *OutPath, bOk ? TEXT("OK") : TEXT("FAILED"));
	return bOk;
}

// ------------------------------------------------------------------ Slate overlay (touch first)

// Oct 27 (Evan: "the DEV button is difficult or impossible to activate during gameplay"): a touch that nothing handled bubbles
// up to the game viewport, which takes it for gameplay - so an SButton never got its tap. This wrapper takes the touch itself
// (it never reaches the game: no aim, move or launch from a dev press) and runs the tap / hold on release / press.
class SZoogiTouchWrap : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiTouchWrap) : _HandleMouse(false) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FSimpleDelegate, OnTap)
		SLATE_EVENT(FSimpleDelegate, OnDown)
		SLATE_EVENT(FSimpleDelegate, OnUp)
		SLATE_ARGUMENT(bool, HandleMouse)
	SLATE_END_ARGS()
	FSimpleDelegate OnTap, OnDown, OnUp;
	bool bHandleMouse = false, bDown = false;
	void Construct(const FArguments& A) { OnTap = A._OnTap; OnDown = A._OnDown; OnUp = A._OnUp; bHandleMouse = A._HandleMouse; ChildSlot[A._Content.Widget]; }
	FReply Press() { bDown = true; OnDown.ExecuteIfBound(); return FReply::Handled().CaptureMouse(SharedThis(this)); }
	FReply Release(const FGeometry& G, const FPointerEvent& E)
	{
		if (!bDown) return FReply::Handled();
		bDown = false;
		OnUp.ExecuteIfBound();
		if (G.IsUnderLocation(E.GetScreenSpacePosition())) OnTap.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override { return Press(); }
	virtual FReply OnTouchMoved(const FGeometry& G, const FPointerEvent& E) override { return FReply::Handled(); }
	virtual FReply OnTouchEnded(const FGeometry& G, const FPointerEvent& E) override { return Release(G, E); }
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { return (bHandleMouse && !E.IsTouchEvent() && E.GetEffectingButton() == EKeys::LeftMouseButton) ? Press() : FReply::Unhandled(); }
	virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override { return (bHandleMouse && !E.IsTouchEvent()) ? Release(G, E) : FReply::Unhandled(); }
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& E) override { if (bDown) { bDown = false; OnUp.ExecuteIfBound(); } }
};

// Full-screen layer UNDER the panel: it takes the touches that land on the game view (select / drag / pinch / twist).
// Being a sibling of the panel (not its parent) it never sees touches meant for the panel's buttons.
class SZoogiDevCatcher : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiDevCatcher) {}
	SLATE_END_ARGS()
	TWeakObjectPtr<AZoogiDevMode> Dev;
	void Construct(const FArguments&, AZoogiDevMode* InDev) { Dev = InDev; ChildSlot[SNew(SBox)]; }
	FVector2D ToPx(const FGeometry& G, const FPointerEvent& E) const
	{
		return G.AbsoluteToLocal(E.GetScreenSpacePosition()) * G.GetAccumulatedLayoutTransform().GetScale();
	}
	bool Editing() const { return Dev.IsValid() && Dev->IsEditing(); }
	// touches (one finger each, several at once)
	virtual FReply OnTouchStarted(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Editing()) return FReply::Unhandled();
		Dev->TouchDown((int32)E.GetPointerIndex(), ToPx(G, E));
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	virtual FReply OnTouchMoved(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Editing()) return FReply::Unhandled();
		Dev->TouchMove((int32)E.GetPointerIndex(), ToPx(G, E));
		return FReply::Handled();
	}
	virtual FReply OnTouchEnded(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Dev.IsValid()) return FReply::Unhandled();
		Dev->TouchUp((int32)E.GetPointerIndex(), ToPx(G, E));
		return FReply::Handled().ReleaseMouseCapture();
	}
	// the mouse = one finger (PC)
	virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Editing() || E.IsTouchEvent()) return FReply::Unhandled();
		if (E.GetEffectingButton() == EKeys::LeftMouseButton) { Dev->TouchDown(MouseFinger, ToPx(G, E)); return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse); }
		return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Dev.IsValid() || E.IsTouchEvent()) return FReply::Unhandled();
		Dev->TouchUp(MouseFinger, ToPx(G, E));
		return HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
	}
	virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Editing() || E.IsTouchEvent() || !HasMouseCapture()) return FReply::Unhandled();
		Dev->TouchMove(MouseFinger, ToPx(G, E));
		return FReply::Handled();
	}
	virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override
	{
		if (!Editing()) return FReply::Unhandled();
		if (Dev->IsTopView() && !Dev->HasSelection()) Dev->ZoomTop(E.GetWheelDelta() > 0 ? 0.9f : 1.1f);
		else Dev->Rotate(E.GetWheelDelta() > 0 ? 5.f : -5.f);
		return FReply::Handled();
	}
};

class SZoogiDevOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiDevOverlay) {}
	SLATE_END_ARGS()

	TWeakObjectPtr<AZoogiDevMode> Dev;
	FGeometry LastGeo;
	bool bHaveGeo = false;
	FSlateRoundedBoxBrush DevBrush = FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.14f, 0.17f, 0.95f), 60.f, FLinearColor(0.36f, 0.38f, 0.44f, 1.f), 3.f);

	static FSlateFontInfo Font(int32 Size, bool bBold = true) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }
	mutable float TacLocalSz = 0.f; // the TAC button's on-screen size (Slate units): the DEV button matches it
	float DevSize() const { return (TacLocalSz > 4.f ? TacLocalSz : 64.f) * ZoogiTune::Get(TEXT("dev.size")); }
	virtual void Tick(const FGeometry& G, const double T, const float Dt) override { LastGeo = G; bHaveGeo = true; SCompoundWidget::Tick(G, T, Dt); }
	/** DEV button spot (local Slate units): left of the TAC button when the arena HUD shows one, else bottom right; + the HUD tab nudges. */
	FMargin DevPad() const
	{
		if (!bHaveGeo) return FMargin(0.f);
		const FVector2D Size = LastGeo.GetLocalSize();
		const float Px = Size.Y / 1080.f;
		FVector2D TP, TS;
		const bool bTac = ZoogiArenaHUD::TacRect(TP, TS);
		const float Sc = FMath::Max(LastGeo.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
		TacLocalSz = bTac ? TS.Y / Sc : 0.f;
		const float Sz = DevSize();
		FVector2D P(Size.X - Sz - 70.f * Px, Size.Y - Sz - 40.f * Px);
		if (bTac)
		{
			const FVector2D L = LastGeo.AbsoluteToLocal(TP), LS = TS / Sc;
			P = FVector2D(L.X - LS.X * 0.2f - Sz, L.Y + LS.Y * 0.5f - Sz * 0.5f); // Oct 28: same gap as TAC -> settings (14 of 72)
		}
		P += FVector2D(ZoogiTune::Get(TEXT("dev.x")), -ZoogiTune::Get(TEXT("dev.y"))) * Px;
		P.X = FMath::Clamp(P.X, 8.f, FMath::Max(8.f, Size.X - Sz - 8.f));
		P.Y = FMath::Clamp(P.Y, 8.f, FMath::Max(8.f, Size.Y - Sz - 8.f));
		return FMargin(P.X, P.Y, 0.f, 0.f);
	}
	// HUD tab: every live-tunable number (sticks, DEV button, rolling, bridge launch), -/+ per value, RESET per group
	TSharedRef<SWidget> TuneTab()
	{
		const FLinearColor Red(0.7f, 0.18f, 0.15f), Blue(0.16f, 0.32f, 0.7f);
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox).Visibility_Lambda(TabVis(2));
		Box->AddSlot().AutoHeight().Padding(0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.f))
			.Text(T(TEXT("Changes show live. * = changed from the shipped value. SAVE keeps them on this device, EXPORT sends them to Claude.")))];
		const TArray<ZoogiTune::FParam>& P = ZoogiTune::Params();
		for (int32 i = 0; i < P.Num(); ++i)
		{
			if (i == 0 || FCString::Strcmp(P[i].Group, P[i - 1].Group) != 0)
			{
				const TCHAR* Group = P[i].Group;
				Box->AddSlot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(3.f).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(13)).ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.2f)).Text(FText::FromString(Group))]
					+ SHorizontalBox::Slot().FillWidth(1.6f)[Btn(T(TEXT("RESET")), [Group](AZoogiDevMode*) { ZoogiTune::ResetGroup(Group); }, Red, 11)]
				];
			}
			Box->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(3.f).VAlign(VAlign_Center).Padding(4.f, 0.f)[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Text_Lambda([i]() { return ZoogiTune::ValueText(i); })]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("-")), [i](AZoogiDevMode*) { ZoogiTune::Step(i, -1.f); }, Blue, 18)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("+")), [i](AZoogiDevMode*) { ZoogiTune::Step(i, 1.f); }, Blue, 18)]
			];
		}
		return Box;
	}
	// SPAWNS tab: Frozen Ring spawn pads + bridge launch spots (they are objects: move / turn them in OBJECTS), TEST SPAWN
	TSharedRef<SWidget> SpawnTab()
	{
		const FLinearColor Green(0.12f, 0.55f, 0.25f);
		auto Test = [this, Green](int32 k) { return Btn(FText::FromString(FString::Printf(TEXT("TEST SPAWN %d"), k + 1)), [k](AZoogiDevMode* M) { M->TestSpawn(k); }, Green, 13); };
		return SNew(SVerticalBox).Visibility_Lambda(TabVis(3))
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(12)).ColorAndOpacity(FLinearColor(0.85f, 0.92f, 1.f)).Text_Lambda([this]() { return Dev.IsValid() ? Dev->RoamText() : FText::GetEmpty(); })]
			+ SVerticalBox::Slot().AutoHeight()[Row(Test(0), Test(1))]
			+ SVerticalBox::Slot().AutoHeight()[Row(Test(2), Test(3))]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.7f, 0.75f, 0.85f))
				.Text(T(TEXT("Coloured pads = player spawns (shown only in Dev Mode). Blue rings on the bridges = launch spots, the yellow bars = the gate width. Tap one (OBJECTS tab) to drag / turn it; SIZE on a launch spot widens its gate. TEST SPAWN puts the Zoogi whose turn it is on that pad; PLAY TEST to roll it.")))];
	}

	TSharedRef<SWidget> Btn(TAttribute<FText> Label, TFunction<void(AZoogiDevMode*)> Fn, const FLinearColor& Tint = FLinearColor(0.16f, 0.32f, 0.7f, 1.f), int32 Size = 15)
	{
		return SNew(SBox).MinDesiredHeight(50.f)
			[
				SNew(SZoogiTouchWrap).OnTap_Lambda([this, Fn]() { if (Dev.IsValid()) Fn(Dev.Get()); })
				[
					SNew(SButton)
					.HAlign(HAlign_Center).VAlign(VAlign_Center)
					.ButtonColorAndOpacity(Tint)
					.OnClicked_Lambda([this, Fn]() { if (Dev.IsValid()) Fn(Dev.Get()); return FReply::Handled(); })
					[SNew(STextBlock).Text(Label).Font(Font(Size)).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)]
				]
			];
	}
	TSharedRef<SWidget> Row(TSharedRef<SWidget> A, TSharedRef<SWidget> B)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[A]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[B];
	}
	static FText T(const TCHAR* S) { return FText::FromString(S); }
	// held while the finger is down (free cam UP / DOWN)
	TSharedRef<SWidget> HoldBtn(const TCHAR* Label, int32 V)
	{
		return SNew(SBox).MinDesiredHeight(50.f).MinDesiredWidth(80.f)
			[
				SNew(SZoogiTouchWrap)
				.OnDown_Lambda([this, V]() { if (Dev.IsValid()) Dev->SetFlyVert(V); })
				.OnUp_Lambda([this]() { if (Dev.IsValid()) Dev->SetFlyVert(0); })
				[
					SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center).ButtonColorAndOpacity(FLinearColor(0.16f, 0.32f, 0.7f, 1.f))
					.OnPressed_Lambda([this, V]() { if (Dev.IsValid()) Dev->SetFlyVert(V); })
					.OnReleased_Lambda([this]() { if (Dev.IsValid()) Dev->SetFlyVert(0); })
					[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(15)).ColorAndOpacity(FLinearColor::White)]
				]
			];
	}
	TFunction<EVisibility()> TabVis(int32 Tab) { return [this, Tab]() { return Dev.IsValid() && Dev->Tab == Tab ? EVisibility::Visible : EVisibility::Collapsed; }; }
	TSharedRef<SWidget> TabBtn(int32 Tab, const TCHAR* Label)
	{
		return SNew(SBox).MinDesiredHeight(44.f)
			[
				SNew(SZoogiTouchWrap).OnTap_Lambda([this, Tab]() { if (Dev.IsValid()) Dev->Tab = Tab; })
				[
					SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center)
					.ButtonColorAndOpacity_Lambda([this, Tab]() { return FSlateColor(Dev.IsValid() && Dev->Tab == Tab ? FLinearColor(0.85f, 0.6f, 0.1f) : FLinearColor(0.2f, 0.22f, 0.3f)); })
					.OnClicked_Lambda([this, Tab]() { if (Dev.IsValid()) Dev->Tab = Tab; return FReply::Handled(); })
					[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(10)).ColorAndOpacity(FLinearColor::White)]
				]
			];
	}
	TSharedRef<SWidget> CameraTab()
	{
		const FLinearColor Gold(0.75f, 0.52f, 0.08f), Red(0.7f, 0.18f, 0.15f);
		auto Prof = [this](TCHAR P, const TCHAR* Label)
		{
			return SNew(SBox).MinDesiredHeight(50.f)
				[
					SNew(SZoogiTouchWrap).OnTap_Lambda([this, P]() { if (Dev.IsValid()) Dev->CamSetProfile(P); })
					[
						SNew(SButton).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.ButtonColorAndOpacity_Lambda([this, P]() { return FSlateColor(Dev.IsValid() && Dev->CamProfile() == P ? FLinearColor(0.12f, 0.55f, 0.25f) : FLinearColor(0.16f, 0.32f, 0.7f)); })
						.OnClicked_Lambda([this, P]() { if (Dev.IsValid()) Dev->CamSetProfile(P); return FReply::Handled(); })
						[SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Justification(ETextJustify::Center)]
					]
				];
		};
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox).Visibility_Lambda(TabVis(1));
		Box->AddSlot().AutoHeight().Padding(0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(13)).ColorAndOpacity(FLinearColor(0.75f, 0.9f, 1.f)).Text_Lambda([this]() { return Dev.IsValid() ? Dev->CamHeader() : FText::GetEmpty(); })];
		Box->AddSlot().AutoHeight()[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Prof(TEXT('A'), TEXT("A\nSTANDARD"))]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Prof(TEXT('B'), TEXT("B\nCLOSE"))]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Prof(TEXT('C'), TEXT("C\nOVERHEAD"))]];
		for (int32 i = 0; i < ZoogiCamConfig::Params().Num(); ++i)
		{
			Box->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox).Visibility_Lambda([this, i]() { return Dev.IsValid() && Dev->CamParamVisible(i) ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().FillWidth(3.f).VAlign(VAlign_Center).Padding(4.f, 0.f)[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor::White).Text_Lambda([this, i]() { return Dev.IsValid() ? Dev->CamValueText(i) : FText::GetEmpty(); })]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("-")), [i](AZoogiDevMode* M) { M->CamStep(i, -1.f); }, FLinearColor(0.16f, 0.32f, 0.7f), 18)]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("+")), [i](AZoogiDevMode* M) { M->CamStep(i, 1.f); }, FLinearColor(0.16f, 0.32f, 0.7f), 18)]
			];
		}
		Box->AddSlot().AutoHeight()[Btn(T(TEXT("RESET THIS PROFILE")), [](AZoogiDevMode* M) { M->CamReset(); }, Red, 13)];
		Box->AddSlot().AutoHeight().Padding(0.f, 4.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.7f, 0.75f, 0.85f))
			.Text(T(TEXT("Tap A / B / C to switch: the one you pick is this mode's default. Changes show live; PLAY TEST to try them, SAVE to keep them, EXPORT to send them to Claude.")))];
		(void)Gold;
		return Box;
	}

	void Construct(const FArguments&, AZoogiDevMode* InDev)
	{
		Dev = InDev;
		const FLinearColor Green(0.12f, 0.55f, 0.25f), Red(0.7f, 0.18f, 0.15f), Gold(0.75f, 0.52f, 0.08f), Grey(0.25f, 0.27f, 0.33f);
		auto Open = [this]() { return Dev.IsValid() && Dev->GetPanel() == EZoogiDevPanel::Open && !Dev->bPanelHidden ? EVisibility::Visible : EVisibility::Collapsed; };
		auto Hidden = [this]() { return Dev.IsValid() && Dev->GetPanel() == EZoogiDevPanel::Open && Dev->bPanelHidden ? EVisibility::Visible : EVisibility::Collapsed; };
		auto FlyOn = [this]() { return Dev.IsValid() && Dev->IsFreeCam() ? EVisibility::Visible : EVisibility::Collapsed; };
		auto Pin = [this]() { return Dev.IsValid() && Dev->GetPanel() == EZoogiDevPanel::Pin ? EVisibility::Visible : EVisibility::Collapsed; };
		auto Closed = [this]()
		{
			if (!Dev.IsValid() || Dev->GetPanel() != EZoogiDevPanel::Closed || !ZoogiDevGate::ButtonEnabled()) return EVisibility::Collapsed;
			const AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(Dev.Get()));
			if (G && G->Phase == EZoogiPhase::Menu) return EVisibility::Collapsed; // not on the mode select / menu screens (Evan, Oct 27)
			if (ZoogiP5::ResultsOn()) return EVisibility::Collapsed; // Oct 30: not over the P5 results screen
			// Oct 28 (Evan): in a match with the arena HUD, DEV sits in the DEV / TAC / settings row under the round counter and
			// is only there while that row is (the profiles are showing)
			FVector2D TP, TS;
			if (ZoogiArenaHUD::IsActive(Dev->GetWorld()) && !ZoogiArenaHUD::TacRect(TP, TS)) return EVisibility::Collapsed;
			return EVisibility::Visible;
		};
		auto Digit = [this](int32 D) { return Btn(FText::AsNumber(D), [D](AZoogiDevMode* M) { M->PinDigit(D); }, FLinearColor(0.16f, 0.32f, 0.7f, 1.f), 22); };

		ChildSlot
		[
			SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
			+ SOverlay::Slot()[SNew(SZoogiDevCatcher, InDev).Visibility_Lambda([this]() { return Dev.IsValid() && Dev->GetPanel() != EZoogiDevPanel::Closed ? EVisibility::Visible : EVisibility::Collapsed; })]
			+ SOverlay::Slot()[SNew(SSafeZone).Visibility(EVisibility::SelfHitTestInvisible).Padding(FMargin(6.f)) // iPhone notch / rounded corners / home bar
			[
			SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
			// ---- PIN pad
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(330.f).Visibility_Lambda(Pin)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, 0.95f)).Padding(12.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(4.f).HAlign(HAlign_Center)[SNew(STextBlock).Text(T(TEXT("DEVELOPER PIN"))).Font(Font(20)).ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.2f))]
						+ SVerticalBox::Slot().AutoHeight().Padding(4.f).HAlign(HAlign_Center)[SNew(STextBlock).Font(Font(26)).ColorAndOpacity(FLinearColor::White).Text_Lambda([this]() { return Dev.IsValid() ? Dev->PinText() : FText::GetEmpty(); })]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.f)[Row(Digit(1), Row(Digit(2), Digit(3)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.f)[Row(Digit(4), Row(Digit(5), Digit(6)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.f)[Row(Digit(7), Row(Digit(8), Digit(9)))]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.f)[Row(Btn(T(TEXT("CLEAR")), [](AZoogiDevMode* M) { M->PinClear(); }, Grey), Row(Digit(0), Btn(T(TEXT("CANCEL")), [](AZoogiDevMode* M) { M->PinCancel(); }, Red)))]
					]
				]
			]
			// ---- panel hidden: a small strip top right (bring the panel back, free cam UP / DOWN / speed)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 6.f, 6.f, 0.f))
			[
				SNew(SBorder).Visibility_Lambda(Hidden).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, 0.7f)).Padding(4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[SNew(SBox).MinDesiredWidth(96.f)[Btn(T(TEXT("PANEL")), [](AZoogiDevMode* M) { M->bPanelHidden = false; }, Green)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[SNew(SBox).Visibility_Lambda(FlyOn)[HoldBtn(TEXT("UP"), 1)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[SNew(SBox).Visibility_Lambda(FlyOn)[HoldBtn(TEXT("DOWN"), -1)]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2.f)[SNew(SBox).Visibility_Lambda(FlyOn).MinDesiredWidth(140.f)[Btn(TAttribute<FText>::CreateLambda([this]() { return Dev.IsValid() ? Dev->FlySpeedText() : FText::GetEmpty(); }), [](AZoogiDevMode* M) { M->CycleFlySpeed(); }, Grey, 13)]]
				]
			]
			// ---- the panel (right side, scrolls on short phones)
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(FMargin(0.f, 6.f, 6.f, 6.f))
			[
				SNew(SBox).WidthOverride(420.f).Visibility_Lambda(Open)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, 0.86f)).Padding(8.f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()[SNew(STextBlock).Text(T(TEXT("ZOOGI DEV"))).Font(Font(19)).ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.2f))]
						+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(12, false)).ColorAndOpacity(FLinearColor::White).Text_Lambda([this]() { return Dev.IsValid() ? Dev->StatusText() : FText::GetEmpty(); })]
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("PLAY TEST")), [](AZoogiDevMode* M) { M->ClosePanel(); }, Green), Btn(T(TEXT("HIDE PANEL")), [](AZoogiDevMode* M) { M->bPanelHidden = true; }, Grey))]
						+ SScrollBox::Slot()[Row(Btn(TAttribute<FText>::CreateLambda([]() { return T(ZoogiDevSkills::AllOpen() ? TEXT("ALL SKILLS: ON") : TEXT("ALL SKILLS: OFF")); }), [](AZoogiDevMode* M) { ZoogiDevSkills::SetAllOpen(!ZoogiDevSkills::AllOpen()); }, Gold),
							Btn(T(TEXT("(TEST - NEXT MATCH)")), [](AZoogiDevMode* M) { ZoogiDevSkills::SetAllOpen(!ZoogiDevSkills::AllOpen()); }, Grey))] // b176d
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("ANIME DIRECTOR")), [](AZoogiDevMode* M) { ZoogiAnimeCues::ToggleDirector(); }, Gold), Btn(T(TEXT("(F6 ANYWHERE)")), [](AZoogiDevMode* M) { ZoogiAnimeCues::ToggleDirector(); }, Grey))] // b164
						+ SScrollBox::Slot()[Row(Btn(TAttribute<FText>::CreateLambda([this]() { return T(Dev.IsValid() && Dev->IsTopView() ? TEXT("GAME VIEW") : TEXT("TOP VIEW")); }), [](AZoogiDevMode* M) { M->ToggleTopView(); }),
							Btn(TAttribute<FText>::CreateLambda([this]() { return T(Dev.IsValid() && Dev->IsFreeCam() ? TEXT("GAME VIEW") : TEXT("FREE CAM")); }), [](AZoogiDevMode* M) { M->ToggleFreeCam(); }, Gold))]
						// ---- free cam controls
						+ SScrollBox::Slot()[SNew(SVerticalBox).Visibility_Lambda(FlyOn)
							+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[HoldBtn(TEXT("UP"), 1)]
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[HoldBtn(TEXT("DOWN"), -1)]
								+ SHorizontalBox::Slot().FillWidth(1.4f).Padding(2.f)[Btn(TAttribute<FText>::CreateLambda([this]() { return Dev.IsValid() ? Dev->FlySpeedText() : FText::GetEmpty(); }), [](AZoogiDevMode* M) { M->CycleFlySpeed(); }, Grey, 13)]]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.f))
								.Text(T(TEXT("Left side of the screen: drag to move. Anywhere else: drag to look. Pinch: fly forward / back. Tap still selects.")))]]
						// ---- tabs
						+ SScrollBox::Slot().Padding(0.f, 4.f)[Row(Row(TabBtn(0, TEXT("OBJECTS")), TabBtn(1, TEXT("CAMERA"))), Row(TabBtn(2, TEXT("HUD")), TabBtn(3, TEXT("SPAWNS"))))]
						// ---- OBJECTS
						+ SScrollBox::Slot()[SNew(SVerticalBox).Visibility_Lambda(TabVis(0))
							+ SVerticalBox::Slot().AutoHeight()[Row(
								Btn(TAttribute<FText>::CreateLambda([this]() { return T(Dev.IsValid() && Dev->bSnap ? TEXT("SNAP: ON") : TEXT("SNAP: OFF")); }), [](AZoogiDevMode* M) { M->bSnap = !M->bSnap; }, Grey),
								Btn(TAttribute<FText>::CreateLambda([this]() { return T(Dev.IsValid() && Dev->bGrid ? TEXT("GRID: ON") : TEXT("GRID: OFF")); }), [](AZoogiDevMode* M) { M->bGrid = !M->bGrid; }, Grey))]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("+ BUMPER")), [](AZoogiDevMode* M) { M->AddBumper(); }, Green), Btn(T(TEXT("DUPLICATE")), [](AZoogiDevMode* M) { M->Duplicate(); }, Green))]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("DELETE")), [](AZoogiDevMode* M) { M->DeleteSelected(); }, Red), Btn(T(TEXT("RESET OBJECT")), [](AZoogiDevMode* M) { M->ResetSelected(); }, Gold))]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(13)).ColorAndOpacity(FLinearColor(0.75f, 0.9f, 1.f)).Text_Lambda([this]() { return Dev.IsValid() ? Dev->SelectedText() : FText::GetEmpty(); })]
							// Oct 29 (Evan): make anything a bumper + choose its rebound force
							+ SVerticalBox::Slot().AutoHeight()[Btn(TAttribute<FText>::CreateLambda([this]() { return Dev.IsValid() ? Dev->BumperText() : FText::GetEmpty(); }), [](AZoogiDevMode* M) { M->ToggleBumper(); }, FLinearColor(0.78f, 0.32f, 0.06f), 14)]
							+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("-")), [](AZoogiDevMode* M) { M->StepBounce(-1); }, FLinearColor(0.16f, 0.32f, 0.7f), 18)]
								+ SHorizontalBox::Slot().FillWidth(2.6f).Padding(2.f).VAlign(VAlign_Center).HAlign(HAlign_Center)[SNew(STextBlock).Font(Font(14)).ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.2f)).Text_Lambda([this]() { return Dev.IsValid() ? Dev->BounceText() : FText::GetEmpty(); })]
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2.f)[Btn(T(TEXT("+")), [](AZoogiDevMode* M) { M->StepBounce(1); }, FLinearColor(0.16f, 0.32f, 0.7f), 18)]]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("X -")), [](AZoogiDevMode* M) { M->Nudge(-1.f, 0.f); }), Btn(T(TEXT("X +")), [](AZoogiDevMode* M) { M->Nudge(1.f, 0.f); }))]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("Y -")), [](AZoogiDevMode* M) { M->Nudge(0.f, -1.f); }), Btn(T(TEXT("Y +")), [](AZoogiDevMode* M) { M->Nudge(0.f, 1.f); }))]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("ROTATE -")), [](AZoogiDevMode* M) { M->Rotate(M->bFine ? -1.f : -15.f); }), Btn(T(TEXT("ROTATE +")), [](AZoogiDevMode* M) { M->Rotate(M->bFine ? 1.f : 15.f); }))]
							+ SVerticalBox::Slot().AutoHeight()[Row(Btn(T(TEXT("SIZE -")), [](AZoogiDevMode* M) { M->ScaleBy(M->bFine ? 0.99f : 0.95f); }), Btn(T(TEXT("SIZE +")), [](AZoogiDevMode* M) { M->ScaleBy(M->bFine ? 1.01f : 1.05f); }))]
							+ SVerticalBox::Slot().AutoHeight()[Btn(TAttribute<FText>::CreateLambda([this]() { return T(Dev.IsValid() && Dev->bFine ? TEXT("STEPS: FINE (5 cm / 1 deg)") : TEXT("STEPS: NORMAL (25 cm / 15 deg)")); }), [](AZoogiDevMode* M) { M->bFine = !M->bFine; }, Grey, 13)]
							+ SVerticalBox::Slot().AutoHeight()[Btn(T(TEXT("RESET ALL OBJECTS")), [](AZoogiDevMode* M) { M->ResetAll(); }, Red, 13)]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[SNew(STextBlock).AutoWrapText(true).Font(Font(11, false)).ColorAndOpacity(FLinearColor(0.7f, 0.75f, 0.85f))
								.Text(T(TEXT("Long-press or tap any object: bumpers, the ship, arches, pillars, rocks, obstacles. Drag = move, pinch = size, two-finger twist = rotate. BUMPER makes it kick back whatever hits it; REBOUND sets how hard (1-10).")))]
						]
						// ---- CAMERA
						+ SScrollBox::Slot()[CameraTab()]
						// ---- HUD / SPAWNS (next builds)
						+ SScrollBox::Slot()[TuneTab()]
						+ SScrollBox::Slot()[SpawnTab()]
						// ---- always
						+ SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 0.f)[Row(Btn(T(TEXT("UNDO")), [](AZoogiDevMode* M) { M->DoUndo(); }), Btn(T(TEXT("REDO")), [](AZoogiDevMode* M) { M->DoRedo(); }))]
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("SAVE")), [](AZoogiDevMode* M) { M->DoSave(); }, Green), Btn(T(TEXT("LOAD")), [](AZoogiDevMode* M) { M->DoLoad(); }))]
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("EXPORT")), [](AZoogiDevMode* M) { M->DoExport(); }, Gold), Btn(T(TEXT("LOCK DEV")), [](AZoogiDevMode* M) { M->DoLock(); }, Grey))]
					]
				]
			]
			]]
			// ---- DEV button: a round button just left of the TAC camera button (Oct 27, Evan); its own touch handling, so a
			// press never reaches gameplay (aim / move / launch). Position / size / opacity: HUD tab (DEV BUTTON).
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { return DevPad(); }))
			[
				SNew(SBox).Visibility_Lambda(Closed)
				.WidthOverride_Lambda([this]() { return FOptionalSize(DevSize()); }).HeightOverride_Lambda([this]() { return FOptionalSize(DevSize()); })
				[
					SNew(SZoogiTouchWrap).HandleMouse(true).OnTap_Lambda([this]() { if (Dev.IsValid()) Dev->OnDevButton(); })
					[
						SNew(SBorder).BorderImage(&DevBrush).HAlign(HAlign_Center).VAlign(VAlign_Center)
						.BorderBackgroundColor_Lambda([]() { return FSlateColor(FLinearColor(1.f, 1.f, 1.f, ZoogiTune::Get(TEXT("dev.alpha")) * ZoogiArenaHUD::TacOpacity())); })
						.ColorAndOpacity_Lambda([]() { return FLinearColor(1.f, 1.f, 1.f, ZoogiTune::Get(TEXT("dev.alpha")) * ZoogiArenaHUD::TacOpacity()); }) // Oct 28: fades with the TAC row
						[SNew(STextBlock).Text(T(TEXT("DEV"))).Font_Lambda([this]() { return Font(FMath::Clamp(FMath::RoundToInt(DevSize() * 0.3f), 9, 30)); }).ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f))]
					]
				]
			]
		];
		SetVisibility(EVisibility::SelfHitTestInvisible); // only its children (buttons, panel, catcher) take touches
	}

	virtual bool SupportsKeyboardFocus() const override { return true; }

	FVector2D ToPx(const FGeometry& G, const FPointerEvent& E) const
	{
		return G.AbsoluteToLocal(E.GetScreenSpacePosition()) * G.GetAccumulatedLayoutTransform().GetScale();
	}
	bool Editing() const { return Dev.IsValid() && Dev->IsEditing(); }

	virtual FReply OnKeyDown(const FGeometry& G, const FKeyEvent& E) override
	{
		if (!Dev.IsValid()) return FReply::Unhandled();
		AZoogiDevMode* D = Dev.Get();
		const FKey K = E.GetKey();
		if (K == EKeys::F10) { D->ToggleDev(); return FReply::Handled(); }
		if (!D->IsEditing()) return FReply::Unhandled();
		if (K == EKeys::S && E.IsControlDown()) D->DoSave();
		else if (K == EKeys::Z && E.IsControlDown()) D->DoUndo();
		else if (K == EKeys::Y && E.IsControlDown()) D->DoRedo();
		else if (K == EKeys::Q) D->Rotate(E.IsShiftDown() ? -1.f : -15.f);
		else if (K == EKeys::E) D->Rotate(E.IsShiftDown() ? 1.f : 15.f);
		else if (K == EKeys::Z) D->ScaleBy(1.f / 1.05f);
		else if (K == EKeys::X) D->ScaleBy(1.05f);
		else if (K == EKeys::B) D->AddBumper();
		else if (K == EKeys::Delete || K == EKeys::BackSpace) D->DeleteSelected();
		else if (K == EKeys::T) D->ToggleTopView();
		else if (K == EKeys::Escape) D->ClosePanel();
		else if (K == EKeys::Up) D->Nudge(1.f, 0.f);
		else if (K == EKeys::Down) D->Nudge(-1.f, 0.f);
		else if (K == EKeys::Left) D->Nudge(0.f, -1.f);
		else if (K == EKeys::Right) D->Nudge(0.f, 1.f);
		else return FReply::Unhandled();
		return FReply::Handled();
	}

	// alignment grid: 100 cm lines (every 500 cm brighter) on the floor around the selection / the arena centre
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (Editing() && Dev->bGrid)
		{
			const float Sc = FMath::Max(0.01f, G.GetAccumulatedLayoutTransform().GetScale());
			const FVector C = Dev->GridCenter();
			const float Half = 2500.f, Stepcm = 100.f, Z = C.Z;
			const float X0 = FMath::GridSnap(C.X, Stepcm), Y0 = FMath::GridSnap(C.Y, Stepcm);
			for (int32 i = -25; i <= 25; ++i)
			{
				const bool bMajor = ((int32)FMath::RoundToFloat((X0 + i * Stepcm) / Stepcm)) % 5 == 0;
				const FLinearColor Col = bMajor ? FLinearColor(1.f, 0.9f, 0.3f, 0.55f) : FLinearColor(1.f, 1.f, 1.f, 0.22f);
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					const FVector A = Axis == 0 ? FVector(X0 + i * Stepcm, Y0 - Half, Z) : FVector(X0 - Half, Y0 + i * Stepcm, Z);
					const FVector B = Axis == 0 ? FVector(X0 + i * Stepcm, Y0 + Half, Z) : FVector(X0 + Half, Y0 + i * Stepcm, Z);
					// draw in short pieces so a line that passes behind the camera is clipped instead of flipped
					FVector2D Prev; bool bPrev = false;
					for (int32 k = 0; k <= 10; ++k)
					{
						FVector2D P;
						const bool bOk = Dev->Project(FMath::Lerp(A, B, k / 10.f), P);
						if (bOk && bPrev)
						{
							TArray<FVector2f> Pts = { FVector2f(Prev / Sc), FVector2f(P / Sc) };
							FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, bMajor ? 2.f : 1.f);
						}
						Prev = P; bPrev = bOk;
					}
				}
			}
		}
		// free cam move stick (left thumb)
		if (Editing() && Dev->IsFreeCam() && Dev->StickActive())
		{
			const float Sc = FMath::Max(0.01f, G.GetAccumulatedLayoutTransform().GetScale());
			const float R = Dev->StickRadiusPx();
			auto Circle = [&](const FVector2D& C, float Rad, const FLinearColor& Col, float Th)
			{
				TArray<FVector2f> Pts;
				for (int32 k = 0; k <= 36; ++k) { const float A = k / 36.f * 2.f * UE_PI; Pts.Add(FVector2f((C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rad) / Sc)); }
				FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Col, true, Th);
			};
			Circle(Dev->StickOriginPx, R, FLinearColor(1.f, 1.f, 1.f, 0.55f), 3.f);
			Circle(Dev->StickOriginPx + (Dev->StickPx - Dev->StickOriginPx).GetClampedToMaxSize(R), R * 0.38f, FLinearColor(1.f, 0.85f, 0.3f, 0.9f), 4.f);
		}
		return SCompoundWidget::OnPaint(Args, G, Cull, Out, Layer + 1, Style, bParentEnabled);
	}
};

// ------------------------------------------------------------------ actor

AZoogiDevMode::AZoogiDevMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetTickableWhenPaused(true);
	USceneComponent* R = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(R);
}

AZoogiDevMode* AZoogiDevMode::Ensure(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return nullptr;
	for (TActorIterator<AZoogiDevMode> It(World); It; ++It) return *It;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AZoogiDevMode* D = World->SpawnActor<AZoogiDevMode>(AZoogiDevMode::StaticClass(), FTransform::Identity, P);
	if (D)
	{
		if (ZoogiDevGate::ButtonEnabled()) D->ShowOverlay(true);
		if (ZoogiLayout::IsDevMode() && ZoogiDevGate::IsUnlocked()) { D->Panel = EZoogiDevPanel::Open; UE_LOG(LogZoogi, Log, TEXT("DEVMODE: panel open (-ZoogiDev)")); }
	}
	return D;
}

void AZoogiDevMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsTopView()) ToggleTopView();
	ShowOverlay(false);
	Super::EndPlay(Reason);
}

AZoogiArena* AZoogiDevMode::GetArena() const
{
	for (TActorIterator<AZoogiArena> It(GetWorld()); It; ++It) return *It;
	return nullptr;
}

APlayerController* AZoogiDevMode::PC() const { return UGameplayStatics::GetPlayerController(this, 0); }

FString AZoogiDevMode::Slug() const
{
	const AZoogiArena* A = GetArena();
	return A ? A->GetArenaSlug() : FString(TEXT("none"));
}

ZoogiDevConfig::FContext AZoogiDevMode::MakeCtx() const
{
	ZoogiDevConfig::FContext C;
	C.Level = Slug();
	if (const AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(this)))
		C.Mode = G->bRoyaleTraining ? TEXT("training") : (G->bRoyaleMode ? TEXT("royale") : (G->bComboMode ? TEXT("combo") : TEXT("classic")));
	C.Screen = TEXT("match");
	return C;
}

void AZoogiDevMode::ShowOverlay(bool bShow)
{
	if (!GEngine || !GEngine->GameViewport) return;
	if (bShow && !Overlay.IsValid())
	{
		Overlay = SNew(SZoogiDevOverlay, this);
		GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 98); // b167: above the P5 command menu (60) and banter bubbles (94/95) - they covered the PIN pad
	}
	else if (!bShow && Overlay.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
		Overlay.Reset();
	}
}

// ---- panel states

void AZoogiDevMode::OnDevButton()
{
	if (ZoogiDevGate::IsUnlocked()) OpenPanel();
	else
	{
		if (!Overlay.IsValid()) ShowOverlay(true); // b167: the PIN pad lives in the overlay
		Panel = EZoogiDevPanel::Pin; PinEntry.Reset(); PinMsg.Reset(); UGameplayStatics::SetGamePaused(this, true);
		if (APlayerController* P = PC()) P->bShowMouseCursor = true;
		if (Overlay.IsValid()) FSlateApplication::Get().SetAllUserFocus(Overlay, EFocusCause::SetDirectly);
		UE_LOG(LogZoogi, Log, TEXT("DEVMODE: PIN pad"));
	}
}

void AZoogiDevMode::ToggleFromMenu(UWorld* World)
{
	AZoogiDevMode* Dm = Ensure(World);
	if (!Dm) { UE_LOG(LogZoogi, Warning, TEXT("DEVMODE: menu toggle - no game world")); return; }
	if (Dm->Panel == EZoogiDevPanel::Closed) Dm->OnDevButton(); else Dm->ClosePanel();
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: menu toggle -> panel %d"), (int32)Dm->Panel);
}

void AZoogiDevMode::OpenPanel()
{
	if (!ZoogiDevGate::IsUnlocked()) { OnDevButton(); return; }
	if (!Overlay.IsValid()) ShowOverlay(true);
	Panel = EZoogiDevPanel::Open;
	ZoogiLayout::SetDevMode(true);
	if (AZoogiArena* MA = GetArena()) MA->SetDevMarkersVisible(true);
	ZoogiDevConfig::EnsureLoaded();
	UGameplayStatics::SetGamePaused(this, true); // the match waits while you edit
	if (AZoogiPlayerController* PCx = Cast<AZoogiPlayerController>(PC())) PCx->SetFullTickWhenPaused(true); // ... but the camera keeps updating (live CAMERA edits)
	if (APlayerController* P = PC()) P->bShowMouseCursor = true;
	if (Overlay.IsValid()) FSlateApplication::Get().SetAllUserFocus(Overlay, EFocusCause::SetDirectly);
	UpdateMarker();
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: panel OPEN (game paused) on %s"), *Slug());
}

void AZoogiDevMode::ClosePanel()
{
	Panel = EZoogiDevPanel::Closed;
	ZoogiLayout::SetDevMode(false);
	if (AZoogiArena* MA = GetArena()) MA->SetDevMarkersVisible(false);
	Fingers.Reset(); PressFinger = -1; bDragging = false;
	Selected = INDEX_NONE;
	if (IsFreeCam()) ToggleFreeCam();
	bPanelHidden = false;
	if (IsTopView()) ToggleTopView();
	UGameplayStatics::SetGamePaused(this, false);
	if (AZoogiPlayerController* PCx = Cast<AZoogiPlayerController>(PC())) PCx->SetFullTickWhenPaused(false);
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UpdateMarker();
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: panel closed (play)"));
}

void AZoogiDevMode::ToggleDev()
{
	if (Panel == EZoogiDevPanel::Open) ClosePanel(); else OnDevButton();
}

void AZoogiDevMode::PinDigit(int32 D)
{
	if (PinEntry.Len() >= 4) return;
	PinEntry.AppendInt(D);
	PinMsg.Reset();
	if (PinEntry.Len() == 4)
	{
		if (ZoogiDevGate::TryUnlock(PinEntry)) { PinEntry.Reset(); OpenPanel(); }
		else { PinEntry.Reset(); PinMsg = TEXT("WRONG PIN"); }
	}
}

void AZoogiDevMode::PinClear() { PinEntry.Reset(); PinMsg.Reset(); }

void AZoogiDevMode::PinCancel()
{
	PinEntry.Reset(); PinMsg.Reset();
	Panel = EZoogiDevPanel::Closed;
	UGameplayStatics::SetGamePaused(this, false);
}

FText AZoogiDevMode::PinText() const
{
	FString S;
	for (int32 i = 0; i < 4; ++i) S += i < PinEntry.Len() ? TEXT("*  ") : TEXT("_  ");
	if (!PinMsg.IsEmpty()) S = PinMsg;
	return FText::FromString(S.TrimEnd());
}

// ---- geometry helpers

bool AZoogiDevMode::CursorPixel(FVector2D& Out) const
{
	APlayerController* P = PC();
	float X = 0, Y = 0;
	if (!P || !P->GetMousePosition(X, Y)) return false;
	Out = FVector2D(X, Y);
	return true;
}

bool AZoogiDevMode::PixelOnPlane(const FVector2D& Px, float Z, FVector& Out) const
{
	APlayerController* P = PC();
	FVector O, D;
	if (!P || !P->DeprojectScreenPositionToWorld(Px.X, Px.Y, O, D) || FMath::Abs(D.Z) < 1e-3f) return false;
	const float T = (Z - O.Z) / D.Z;
	if (T <= 0.f) return false;
	Out = O + D * T;
	return true;
}

bool AZoogiDevMode::Project(const FVector& W, FVector2D& OutPx) const
{
	APlayerController* P = PC();
	return P && P->ProjectWorldLocationToScreen(W, OutPx, false);
}

static float PropZ(const AZoogiArena::FZoogiDevProp& P) { if (P.Type == TEXT("obstacle")) return P.Location.Z + P.PickZ; return P.Type == TEXT("ship") ? ZoogiLaunch::PadHeight : ((P.Type == TEXT("spawn") || P.Type == TEXT("launch")) ? P.Location.Z + 30.f : 70.f); }

bool AZoogiDevMode::HasSelection() const
{
	const AZoogiArena* A = GetArena();
	return A && A->DevProps.IsValidIndex(Selected);
}

FVector AZoogiDevMode::GridCenter() const
{
	const AZoogiArena* A = GetArena();
	if (A && A->DevProps.IsValidIndex(Selected)) { const auto& P = A->DevProps[Selected]; return FVector(P.Location.X, P.Location.Y, P.Type == TEXT("ship") ? ZoogiLaunch::PadHeight : 2.f); }
	return FVector(0.f, 0.f, 2.f);
}

void AZoogiDevMode::SnapProp(AZoogiArena::FZoogiDevProp& P) const
{
	if (!bSnap) return;
	P.Location.X = FMath::GridSnap(P.Location.X, Step());
	P.Location.Y = FMath::GridSnap(P.Location.Y, Step());
}

// ---- selection + gestures

bool AZoogiDevMode::SelectAtPixel(const FVector2D& Px)
{
	AZoogiArena* A = GetArena();
	APlayerController* P = PC();
	Selected = INDEX_NONE;
	if (!A || !P) { UpdateMarker(); return false; }
	int32 VX = 1920, VY = 1080;
	P->GetViewportSize(VX, VY);
	float Best = FMath::Max(110.f, 0.1f * FMath::Min(VX, VY)); // finger-sized pick radius
	for (int32 i = 0; i < A->DevProps.Num(); ++i)
	{
		FVector2D S;
		const FVector L(A->DevProps[i].Location.X, A->DevProps[i].Location.Y, PropZ(A->DevProps[i]));
		if (!P->ProjectWorldLocationToScreen(L, S)) continue;
		const float D = FVector2D::Distance(S, Px);
		if (D < Best) { Best = D; Selected = i; }
	}
	if (Selected != INDEX_NONE)
	{
		FVector Hit;
		const auto& Pr = A->DevProps[Selected];
		GrabOffset = PixelOnPlane(Px, PropZ(Pr), Hit) ? FVector(Pr.Location.X - Hit.X, Pr.Location.Y - Hit.Y, 0.f) : FVector::ZeroVector;
		LastMsg = FString::Printf(TEXT("selected %s"), *Pr.Id);
	}
	else LastMsg = TEXT("nothing there - tap a bumper, an obstacle, a spawn pad or a bridge launch spot");
	UpdateMarker();
	return Selected != INDEX_NONE;
}

void AZoogiDevMode::SelectAtCursor()
{
	FVector2D Px;
	if (CursorPixel(Px)) SelectAtPixel(Px);
}

void AZoogiDevMode::TouchDown(int32 Finger, const FVector2D& Px)
{
	// free cam: a finger landing on the left side is the move stick (kept out of the gesture fingers)
	if (IsFreeCam() && StickFinger < 0)
	{
		int32 VX = 1920, VY = 1080; if (APlayerController* P = PC()) P->GetViewportSize(VX, VY);
		if (Px.X < VX * 0.42f) { StickFinger = Finger; StickOriginPx = Px; StickPx = Px; StickDownTime = FPlatformTime::Seconds(); return; }
	}
	Fingers.Add(Finger, Px);
	AZoogiArena* A = GetArena();
	if (Fingers.Num() == 1)
	{
		PressFinger = Finger; PressPx = Px; PressTime = FPlatformTime::Seconds(); bPressMoved = false; bChanged = false; PanLast = Px;
		bDragging = false;
		// a finger that lands on the selected object drags it straight away; anywhere else waits for a long press / tap
		if (A && A->DevProps.IsValidIndex(Selected))
		{
			FVector2D S;
			const auto& Pr = A->DevProps[Selected];
			int32 VX = 1920, VY = 1080; if (APlayerController* P = PC()) P->GetViewportSize(VX, VY);
			if (Project(FVector(Pr.Location.X, Pr.Location.Y, PropZ(Pr)), S) && FVector2D::Distance(S, Px) < FMath::Max(130.f, 0.12f * FMath::Min(VX, VY)))
			{
				FVector Hit;
				GrabOffset = PixelOnPlane(Px, PropZ(Pr), Hit) ? FVector(Pr.Location.X - Hit.X, Pr.Location.Y - Hit.Y, 0.f) : FVector::ZeroVector;
				bDragging = true;
			}
		}
	}
	else if (Fingers.Num() == 2)
	{
		// two fingers: pinch = size, twist = rotate (the selection) or pan / zoom the top view
		bDragging = false; PressFinger = -1;
		TArray<FVector2D> P; Fingers.GenerateValueArray(P);
		PinchDist0 = FMath::Max(1.f, FVector2D::Distance(P[0], P[1]));
		PinchAng0 = FMath::RadiansToDegrees(FMath::Atan2(P[1].Y - P[0].Y, P[1].X - P[0].X));
		if (A && A->DevProps.IsValidIndex(Selected)) { PinchScale0 = A->DevProps[Selected].Scale; PinchYaw0 = A->DevProps[Selected].Yaw; }
	}
}

void AZoogiDevMode::TouchMove(int32 Finger, const FVector2D& Px)
{
	if (Finger == StickFinger) { StickPx = Px; return; }
	if (!Fingers.Contains(Finger)) return;
	Fingers.Add(Finger, Px);
	AZoogiArena* A = GetArena();
	if (Finger == PressFinger && FVector2D::Distance(Px, PressPx) > 16.f) bPressMoved = true;
	if (Fingers.Num() == 1)
	{
		if (bDragging && A && A->DevProps.IsValidIndex(Selected))
		{
			auto& Pr = A->DevProps[Selected];
			FVector Hit;
			if (PixelOnPlane(Px, PropZ(Pr), Hit))
			{
				Pr.Location.X = Hit.X + GrabOffset.X;
				Pr.Location.Y = Hit.Y + GrabOffset.Y;
				SnapProp(Pr);
				A->UpdateDevProp(Selected);
				bChanged = true;
				UpdateMarker();
			}
		}
		else if (IsFreeCam() && bPressMoved)
		{
			// free cam: drag anywhere else looks around
			const FVector2D D = Px - PanLast;
			FlyYaw += D.X * 0.2f;
			FlyPitch = FMath::Clamp(FlyPitch - D.Y * 0.2f, -85.f, 85.f);
		}
		else if (IsTopView() && bPressMoved)
		{
			// one finger on empty floor in the top view pans
			const FVector2D D = Px - PanLast;
			if (TopCam.IsValid()) TopCam->AddActorWorldOffset(FVector(-D.Y, D.X, 0.f) * TopHeight * 0.0011f);
		}
		PanLast = Px;
	}
	else if (Fingers.Num() == 2)
	{
		TArray<FVector2D> P; Fingers.GenerateValueArray(P);
		const float Dist = FMath::Max(1.f, FVector2D::Distance(P[0], P[1]));
		const float Ang = FMath::RadiansToDegrees(FMath::Atan2(P[1].Y - P[0].Y, P[1].X - P[0].X));
		if (A && A->DevProps.IsValidIndex(Selected))
		{
			auto& Pr = A->DevProps[Selected];
			float Sc = FMath::Clamp(PinchScale0 * Dist / PinchDist0, 0.3f, 3.f);
			float Yaw = FRotator::NormalizeAxis(PinchYaw0 + FMath::FindDeltaAngleDegrees(PinchAng0, Ang));
			if (bSnap) { Sc = FMath::GridSnap(Sc, 0.05f); Yaw = FMath::GridSnap(Yaw, 5.f); }
			Pr.Scale = Sc; Pr.Yaw = Yaw;
			A->UpdateDevProp(Selected);
			bChanged = true;
			UpdateMarker();
		}
		else if (IsTopView())
		{
			ZoomTop(PinchDist0 / Dist);
			PinchDist0 = Dist;
		}
		else if (FlyCam.IsValid())
		{
			// free cam: pinch out flies forward, pinch in flies back
			FlyCam->AddActorWorldOffset(FlyCam->GetActorForwardVector() * (Dist - PinchDist0) * 4.f);
			PinchDist0 = Dist;
		}
	}
}

void AZoogiDevMode::TouchUp(int32 Finger, const FVector2D& Px)
{
	if (Finger == StickFinger)
	{
		StickFinger = -1;
		// a quick tap on the left side still selects
		if (FVector2D::Distance(Px, StickOriginPx) < 16.f && FPlatformTime::Seconds() - StickDownTime < 0.45) SelectAtPixel(Px);
		return;
	}
	const bool bWasPress = Finger == PressFinger;
	const double Held = FPlatformTime::Seconds() - PressTime;
	Fingers.Remove(Finger);
	if (bWasPress && !bPressMoved && !bDragging && Held < 0.45) SelectAtPixel(Px); // a quick tap selects too (forgiving)
	if (Fingers.Num() == 0)
	{
		if (bChanged)
		{
			const AZoogiArena* A = GetArena();
			if (A && A->DevProps.IsValidIndex(Selected))
			{
				const auto& Pr = A->DevProps[Selected];
				CommitLayout(FString::Printf(TEXT("%s -> x %.0f y %.0f yaw %.0f size %.2f"), *Pr.Id, Pr.Location.X, Pr.Location.Y, Pr.Yaw, Pr.Scale));
			}
		}
		bChanged = false; bDragging = false; PressFinger = -1;
	}
	else if (Fingers.Num() == 1)
	{
		// back to one finger after a pinch: keep editing without a jump
		TArray<FVector2D> P; Fingers.GenerateValueArray(P);
		PanLast = P[0];
	}
}

// ---- object tools (every change goes through the store: undo / redo / export)

void AZoogiDevMode::CommitLayout(const FString& Why)
{
	const AZoogiArena* A = GetArena();
	if (!A) return;
	ZoogiDevConfig::Set(TEXT("level:") + Slug(), ZoogiLayout::LayoutKey(), ZoogiLayout::ToJson(A->DevProps), Why);
	LastMsg = Why;
}

void AZoogiDevMode::ApplyFromStore()
{
	AZoogiArena* A = GetArena();
	if (!A) return;
	const FString SelId = A->DevProps.IsValidIndex(Selected) ? A->DevProps[Selected].Id : FString(); // keep the selection by id
	TArray<AZoogiArena::FZoogiDevProp> L;
	const TSharedPtr<FJsonValue> V = ZoogiDevConfig::Get(TEXT("level:") + Slug(), ZoogiLayout::LayoutKey());
	if (V.IsValid() && ZoogiLayout::FromJson(V, L)) A->ApplyLayout(L);
	else A->ApplyLayout(A->BaselineProps);
	Selected = SelId.IsEmpty() ? INDEX_NONE : A->DevProps.IndexOfByPredicate([&SelId](const AZoogiArena::FZoogiDevProp& P) { return P.Id == SelId; });
	UpdateMarker();
}

void AZoogiDevMode::Nudge(float DX, float DY)
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.Location.X += DX * Step(); Pr.Location.Y += DY * Step();
	SnapProp(Pr);
	A->UpdateDevProp(Selected);
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("%s nudged to x %.0f y %.0f"), *Pr.Id, Pr.Location.X, Pr.Location.Y));
}

void AZoogiDevMode::Rotate(float Deg)
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.Yaw = FRotator::NormalizeAxis(Pr.Yaw + Deg);
	A->UpdateDevProp(Selected);
	CommitLayout(FString::Printf(TEXT("%s rotated to %.0f deg"), *Pr.Id, Pr.Yaw));
}

void AZoogiDevMode::ScaleBy(float F)
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.Scale = FMath::Clamp(Pr.Scale * F, 0.3f, 3.f);
	A->UpdateDevProp(Selected);
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("%s size %.2f"), *Pr.Id, Pr.Scale));
}

void AZoogiDevMode::AddBumper()
{
	AZoogiArena* A = GetArena();
	APlayerController* P = PC();
	if (!A) return;
	FVector Hit = FVector::ZeroVector;
	int32 VX = 1920, VY = 1080;
	if (P) P->GetViewportSize(VX, VY);
	if (!PixelOnPlane(FVector2D(VX * 0.4f, VY * 0.5f), 70.f, Hit)) Hit = FVector::ZeroVector; // middle of the game view (left of the panel)
	AZoogiArena::FZoogiDevProp Tmp; Tmp.Location = Hit; SnapProp(Tmp);
	Selected = A->AddStarBumperProp(FVector(Tmp.Location.X, Tmp.Location.Y, 0.f), 0.f, 1.f);
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("added %s at x %.0f y %.0f"), *A->DevProps[Selected].Id, Tmp.Location.X, Tmp.Location.Y));
}

void AZoogiDevMode::Duplicate()
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	const auto Src = A->DevProps[Selected];
	if (Src.Type != TEXT("star_bumper")) { LastMsg = TEXT("only star bumpers can be duplicated (+ BUMPER adds one; any object can be a bumper)"); return; }
	Selected = A->AddStarBumperProp(FVector(Src.Location.X + 300.f, Src.Location.Y, 0.f), Src.Yaw, Src.Scale);
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("duplicated %s as %s"), *Src.Id, *A->DevProps[Selected].Id));
}

void AZoogiDevMode::DeleteSelected()
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	if (AZoogiArena::IsFixedPropType(A->DevProps[Selected].Type)) { LastMsg = FString::Printf(TEXT("%s can move, not be deleted"), *A->DevProps[Selected].Id); return; }
	const FString Id = A->DevProps[Selected].Id;
	A->RemoveDevProp(Selected);
	Selected = INDEX_NONE;
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("deleted %s"), *Id));
}

void AZoogiDevMode::ResetSelected()
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	const FString Id = A->DevProps[Selected].Id;
	if (A->DevProps[Selected].Type == TEXT("obstacle"))
	{
		auto& Ob = A->DevProps[Selected];
		Ob.Location = Ob.Base.GetLocation(); Ob.Yaw = Ob.Base.Rotator().Yaw; Ob.Scale = 1.f; Ob.bBumper = false; Ob.Bounce = 5;
		A->UpdateDevProp(Selected);
		UpdateMarker();
		CommitLayout(FString::Printf(TEXT("reset %s to its original place"), *Id));
		return;
	}
	const AZoogiArena::FZoogiDevProp* Orig = A->BaselineProps.FindByPredicate([&Id](const AZoogiArena::FZoogiDevProp& P) { return P.Id == Id; });
	if (!Orig) { LastMsg = FString::Printf(TEXT("%s was added in dev mode - DELETE removes it"), *Id); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.Location = Orig->Location; Pr.Yaw = Orig->Yaw; Pr.Scale = Orig->Scale;
	A->UpdateDevProp(Selected);
	UpdateMarker();
	CommitLayout(FString::Printf(TEXT("reset %s to its original place"), *Id));
}

void AZoogiDevMode::ResetAll()
{
	AZoogiArena* A = GetArena();
	if (!A) return;
	A->ApplyLayout(A->BaselineProps);
	Selected = INDEX_NONE;
	UpdateMarker();
	ZoogiDevConfig::Remove(TEXT("level:") + Slug(), ZoogiLayout::LayoutKey(), FString::Printf(TEXT("reset every object on %s"), *Slug()));
	LastMsg = TEXT("everything on this map is back to the shipped layout (UNDO brings your edits back)");
}

FString AZoogiDevMode::CamMode() const { return ZoogiCamConfig::ModeName(this); }
TCHAR AZoogiDevMode::CamProfile() const { return ZoogiCamConfig::ActiveProfile(CamMode()); }

void AZoogiDevMode::CamSetProfile(TCHAR P)
{
	ZoogiCamConfig::SetActiveProfile(CamMode(), P);
	LastMsg = FString::Printf(TEXT("%s camera: profile %c"), *CamMode(), P);
}

void AZoogiDevMode::CamStep(int32 ParamIndex, float Dir)
{
	const TArray<ZoogiCamConfig::FParam>& Ps = ZoogiCamConfig::Params();
	if (!Ps.IsValidIndex(ParamIndex)) return;
	const ZoogiCamConfig::FParam& P = Ps[ParamIndex];
	const FString M = CamMode();
	const TCHAR Prof = CamProfile();
	ZoogiCamConfig::SetValue(M, Prof, P, ZoogiCamConfig::GetValue(M, Prof, P) + Dir * P.Step * (bFine ? 1.f : 2.f));
	LastMsg = FString::Printf(TEXT("%s camera %c: %s %s"), *M, Prof, P.Label, *FString::SanitizeFloat(ZoogiCamConfig::GetValue(M, Prof, P)));
}

void AZoogiDevMode::CamReset()
{
	ZoogiCamConfig::ResetProfile(CamMode(), CamProfile());
	LastMsg = FString::Printf(TEXT("%s camera %c back to shipped values (UNDO brings yours back)"), *CamMode(), CamProfile());
}

FText AZoogiDevMode::CamHeader() const
{
	const FString M = CamMode();
	return FText::FromString(FString::Printf(TEXT("Mode %s  -  %s camera  -  profile %c in use"), *M, ZoogiCamConfig::IsChaseMode(M) ? TEXT("chase") : TEXT("tactical"), CamProfile()));
}

FText AZoogiDevMode::CamValueText(int32 ParamIndex) const
{
	const TArray<ZoogiCamConfig::FParam>& Ps = ZoogiCamConfig::Params();
	if (!Ps.IsValidIndex(ParamIndex)) return FText::GetEmpty();
	const float V = ZoogiCamConfig::GetValue(CamMode(), CamProfile(), Ps[ParamIndex]);
	return FText::FromString(FString::Printf(TEXT("%s  %s"), Ps[ParamIndex].Label, *FString::SanitizeFloat(FMath::RoundToFloat(V * 100.f) / 100.f)));
}

bool AZoogiDevMode::CamParamVisible(int32 ParamIndex) const
{
	const TArray<ZoogiCamConfig::FParam>& Ps = ZoogiCamConfig::Params();
	return Ps.IsValidIndex(ParamIndex) && Ps[ParamIndex].bChase == ZoogiCamConfig::IsChaseMode(CamMode());
}

void AZoogiDevMode::DoUndo()
{
	FString Scope, Key;
	if (!ZoogiDevConfig::Undo(Scope, Key)) { LastMsg = TEXT("nothing to undo"); return; }
	if (Key == ZoogiLayout::LayoutKey() && Scope == TEXT("level:") + Slug()) ApplyFromStore();
	LastMsg = FString::Printf(TEXT("undo (%s)"), *Key);
}

void AZoogiDevMode::DoRedo()
{
	FString Scope, Key;
	if (!ZoogiDevConfig::Redo(Scope, Key)) { LastMsg = TEXT("nothing to redo"); return; }
	if (Key == ZoogiLayout::LayoutKey() && Scope == TEXT("level:") + Slug()) ApplyFromStore();
	LastMsg = FString::Printf(TEXT("redo (%s)"), *Key);
}

void AZoogiDevMode::DoSave()
{
	const bool bOk = ZoogiDevConfig::Save();
#if PLATFORM_WINDOWS
	FString Legacy;
	if (bOk) ZoogiLayout::Save(GetArena(), Legacy); // keeps C:\dev\bake-layouts.ps1 working on the PC
#endif
	LastMsg = bOk ? TEXT("SAVED on this device") : TEXT("SAVE FAILED");
}

void AZoogiDevMode::DoLoad()
{
	ZoogiDevConfig::Reload();
	ApplyFromStore();
	LastMsg = TEXT("loaded the saved dev config");
}

void AZoogiDevMode::DoExport()
{
	FString Summary, Path;
	ZoogiDevConfig::ExportPackage(MakeCtx(), Summary, Path);
	LastMsg = FString::Printf(TEXT("EXPORTED - copied to the clipboard (paste it to Claude). File: %s"), *FPaths::GetCleanFilename(Path));
}

void AZoogiDevMode::DoLock()
{
	ZoogiDevGate::Lock();
	ClosePanel();
}

void AZoogiDevMode::ToggleTopView()
{
	APlayerController* P = PC();
	if (!P) return;
	if (TopCam.IsValid())
	{
		P->SetViewTarget(PrevView.IsValid() ? PrevView.Get() : P->GetPawn());
		TopCam->Destroy();
		TopCam.Reset();
		return;
	}
	if (Panel != EZoogiDevPanel::Open) return; // free camera only inside dev mode
	if (FlyCam.IsValid()) { FlyCam->Destroy(); FlyCam.Reset(); StickFinger = -1; FlyVert = 0; } // keep PrevView: the match camera
	else PrevView = P->GetViewTarget();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* C = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(0, 0, TopHeight), FRotator(-90.f, 0.f, 0.f), SP);
	if (!C) return;
	C->SetTickableWhenPaused(true);
	C->GetCameraComponent()->SetFieldOfView(60.f);
	C->GetCameraComponent()->bConstrainAspectRatio = false;
	TopCam = C;
	P->SetViewTarget(C);
}

void AZoogiDevMode::PanTop(const FVector2D& D)
{
	if (!TopCam.IsValid()) return;
	TopCam->AddActorWorldOffset(FVector(D.X, D.Y, 0.f) * TopHeight * 0.08f);
}

void AZoogiDevMode::ZoomTop(float F)
{
	if (!TopCam.IsValid()) return;
	TopHeight = FMath::Clamp(TopHeight * F, 1500.f, 14000.f);
	FVector L = TopCam->GetActorLocation(); L.Z = TopHeight;
	TopCam->SetActorLocation(L);
}

void AZoogiDevMode::TestSpawn(int32 Index)
{
	AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(this));
	if (!G || !G->FrozenRoamOn()) { LastMsg = TEXT("TEST SPAWN works in a classic match on the Frozen Ring"); return; }
	G->FrozenTestSpawn(Index);
	LastMsg = FString::Printf(TEXT("P%d placed on spawn %d - PLAY TEST to roll it"), G->ActivePlayer + 1, Index + 1);
}

FText AZoogiDevMode::RoamText() const
{
	const AZoogiGameMode* G = Cast<AZoogiGameMode>(UGameplayStatics::GetGameMode(this));
	const AZoogiArena* A = GetArena();
	if (!G || !A || !A->IsFrozenRoam()) return FText::FromString(TEXT("Frozen Ring roam is off here (it runs in classic matches on the Frozen Ring)."));
	return FText::FromString(FString::Printf(TEXT("FROZEN RING: %d spawn pads, %d bridge launch spots\nP%d: %s%s"), A->NumRoamProps(TEXT("spawn")), A->NumRoamProps(TEXT("launch")),
		G->ActivePlayer + 1, ZoogiFrozenRoam::StateName(G->RoamState), G->FrozenNeedsEntry(G->ActivePlayer) ? TEXT(" (outside the arena)") : TEXT(" (in the arena)")));
}

void AZoogiDevMode::ToggleFreeCam()
{
	APlayerController* P = PC();
	if (!P) return;
	if (FlyCam.IsValid())
	{
		P->SetViewTarget(PrevView.IsValid() ? PrevView.Get() : P->GetPawn());
		FlyCam->Destroy();
		FlyCam.Reset();
		StickFinger = -1; FlyVert = 0; bPanelHidden = false;
		LastMsg = TEXT("Free cam off");
		return;
	}
	if (Panel != EZoogiDevPanel::Open) return; // free roam only inside dev mode
	FVector L; FRotator R;
	P->GetPlayerViewPoint(L, R);
	if (TopCam.IsValid()) { TopCam->Destroy(); TopCam.Reset(); } // keep PrevView: the match camera
	else PrevView = P->GetViewTarget();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* C = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), L, R, SP);
	if (!C) return;
	C->SetTickableWhenPaused(true);
	C->GetCameraComponent()->SetFieldOfView(90.f);
	C->GetCameraComponent()->bConstrainAspectRatio = false;
	FlyCam = C;
	FlyYaw = R.Yaw;
	FlyPitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch), -85.f, 85.f);
	StickFinger = -1; FlyVert = 0;
	P->SetViewTarget(C);
	LastMsg = TEXT("FREE CAM: left side = move, drag = look, pinch = fly forward / back");
	UE_LOG(LogZoogi, Log, TEXT("DEVMODE: free cam on at %s"), *L.ToCompactString());
}

float AZoogiDevMode::StickRadiusPx() const
{
	int32 VX = 1920, VY = 1080;
	if (APlayerController* P = PC()) P->GetViewportSize(VX, VY);
	return 0.11f * FMath::Max(64, FMath::Min(VX, VY));
}

void AZoogiDevMode::CycleFlySpeed()
{
	FlySpeed = FlySpeed < 800.f ? 1200.f : (FlySpeed < 2000.f ? 3000.f : 400.f);
}

FText AZoogiDevMode::FlySpeedText() const
{
	return FText::FromString(FlySpeed < 800.f ? TEXT("SPEED: SLOW") : (FlySpeed < 2000.f ? TEXT("SPEED: NORMAL") : TEXT("SPEED: FAST")));
}

void AZoogiDevMode::TickFreeCam()
{
	if (!FlyCam.IsValid()) return;
	const float RDt = FMath::Clamp((float)FApp::GetDeltaTime(), 0.f, 0.05f); // real time: the game is paused
	const FRotator R(FlyPitch, FlyYaw, 0.f);
	FlyCam->SetActorRotation(R);
	FVector Move = FVector::UpVector * (float)FlyVert;
	if (StickFinger >= 0)
	{
		const FVector2D S = ((StickPx - StickOriginPx) / FMath::Max(1.f, StickRadiusPx())).GetClampedToMaxSize(1.f);
		const FRotationMatrix M(R);
		Move += M.GetUnitAxis(EAxis::X) * (-S.Y) + M.GetUnitAxis(EAxis::Y) * S.X;
	}
	if (!Move.IsNearlyZero()) FlyCam->AddActorWorldOffset(Move * FlySpeed * RDt);
}

void AZoogiDevMode::UpdateMarker()
{
	AZoogiArena* A = GetArena();
	const bool bShow = IsEditing() && A && A->DevProps.IsValidIndex(Selected);
	if (!Marker && bShow)
	{
		Marker = NewObject<UStaticMeshComponent>(this);
		Marker->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cylinder")));
		Marker->SetMaterial(0, ZoogiUtil::GlowMID(this, FLinearColor(1.f, 0.85f, 0.1f), 2.5f));
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Marker->SetCastShadow(false);
		Marker->SetupAttachment(GetRootComponent());
		Marker->RegisterComponent();
	}
	if (!Marker) return;
	Marker->SetVisibility(bShow);
	if (!bShow) return;
	const auto& Pr = A->DevProps[Selected];
	const bool bRoam = Pr.Type == TEXT("spawn") || Pr.Type == TEXT("launch");
	float R = (Pr.Type == TEXT("ship") ? 300.f : (bRoam ? 90.f : 340.f)) * (bRoam ? 1.f : Pr.Scale);
	float MZ = Pr.Type == TEXT("ship") ? ZoogiLaunch::PadHeight + 260.f : (bRoam ? Pr.Location.Z + 150.f : 175.f);
	if (Pr.Type == TEXT("obstacle"))
	{
		FVector O(0.f), E(60.f);
		if (const AActor* Ac = Pr.Actor.Get()) Ac->GetActorBounds(false, O, E);
		R = FMath::Clamp((float)FMath::Max(E.X, E.Y) * 1.15f, 80.f, 900.f);
		MZ = O.Z + E.Z + 40.f;
	}
	Marker->SetWorldLocationAndRotation(FVector(Pr.Location.X, Pr.Location.Y, MZ), FRotator::ZeroRotator);
	Marker->SetWorldScale3D(FVector(R / 50.f, R / 50.f, 0.04f));
}

FText AZoogiDevMode::StatusText() const
{
	const ZoogiDevConfig::FContext C = MakeCtx();
	FString S = FString::Printf(TEXT("Map %s  |  mode %s%s\n"), *C.Level, *C.Mode, IsTopView() ? TEXT("  |  top view") : (IsFreeCam() ? TEXT("  |  free cam") : TEXT("")));
	S += FString::Printf(TEXT("%d saved override(s)%s  |  undo %s  redo %s\n"), ZoogiDevConfig::NumEntries(), ZoogiDevConfig::IsDirty() ? TEXT(" + UNSAVED edits") : TEXT(""),
		ZoogiDevConfig::CanUndo() ? TEXT("yes") : TEXT("-"), ZoogiDevConfig::CanRedo() ? TEXT("yes") : TEXT("-"));
	if (!LastMsg.IsEmpty()) S += LastMsg;
	return FText::FromString(S);
}

FText AZoogiDevMode::SelectedText() const
{
	const AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) return FText::FromString(TEXT("SELECTED: none (tap any object)"));
	const auto& P = A->DevProps[Selected];
	return FText::FromString(FString::Printf(TEXT("SELECTED: %s\nx %.0f   y %.0f   rotation %.0f   size %.2f"), *P.Id, P.Location.X, P.Location.Y, P.Yaw, P.Scale));
}

void AZoogiDevMode::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* P = PC();
	if (P)
	{
		if (P->WasInputKeyJustPressed(EKeys::F10)) ToggleDev();
		if (TopCam.IsValid() && P->GetViewTarget() != TopCam.Get()) P->SetViewTarget(TopCam.Get()); // the match camera keeps reclaiming
		if (FlyCam.IsValid() && P->GetViewTarget() != FlyCam.Get()) P->SetViewTarget(FlyCam.Get());
	}
	TickFreeCam();
	// Oct 29: obstacles become dev objects once the stage has formed (then every 2 s while editing, for late battle props)
	if (AZoogiArena* MA = GetArena())
	{
		const double NowR = FPlatformTime::Seconds();
		if ((!MA->bObstaclesRegistered || (IsEditing() && NowR >= NextObsScan)) && !MA->IsForming())
		{
			NextObsScan = NowR + 2.0;
			MA->RegisterObstacleProps();
		}
	}
	// long press (0.45 s, finger still) selects what is under it and starts dragging it
	if (IsEditing() && PressFinger >= 0 && !bPressMoved && !bDragging && Fingers.Num() == 1 && FPlatformTime::Seconds() - PressTime >= 0.45)
	{
		if (SelectAtPixel(PressPx)) bDragging = true;
		PressFinger = -1;
	}
	RunAuto(Dt);
	RunObsTest();
}

// -ZoogiDevObsTest (capture runs, with -ZoogiDev -ZoogiShots=<dir>): select an obstacle, make it a bumper at rebound 7, nudge it,
// screenshot the panel, make every star bumper an active bumper (rebound 8), then close the panel so the match plays on them.
void AZoogiDevMode::RunObsTest()
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevObsTest"));
	AZoogiArena* A = GetArena();
	if (!bOn || ObsTestStep >= 3 || !A || !A->bObstaclesRegistered || !IsEditing()) return;
	const double Now = FPlatformTime::Seconds();
	if (ObsTestAt <= 0.0) { ObsTestAt = Now + 2.0; return; }
	if (Now < ObsTestAt) return;
	FString Dir;
	FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), Dir);
	if (ObsTestStep == 0)
	{
		int32 Pick = INDEX_NONE, NObs = 0;
		for (int32 i = 0; i < A->DevProps.Num(); ++i)
			if (A->DevProps[i].Type == TEXT("obstacle"))
			{
				++NObs;
				if (Pick == INDEX_NONE || A->DevProps[i].Id.StartsWith(TEXT("arch")) || A->DevProps[i].Id.StartsWith(TEXT("pillar"))) if (Pick == INDEX_NONE || !A->DevProps[Pick].Id.StartsWith(TEXT("arch"))) Pick = i;
			}
		UE_LOG(LogZoogi, Log, TEXT("DEVOBS test: %d obstacle dev objects, picking %s"), NObs, A->DevProps.IsValidIndex(Pick) ? *A->DevProps[Pick].Id : TEXT("none"));
		if (Pick != INDEX_NONE)
		{
			Selected = Pick;
			UpdateMarker();
			ToggleBumper();
			StepBounce(2);
			Nudge(1.f, 0.f);
		}
		if (!Dir.IsEmpty()) FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, TEXT("devobs_01_selected.png")), true, false);
		ObsTestStep = 1; ObsTestAt = Now + 1.5;
	}
	else if (ObsTestStep == 1)
	{
		for (int32 i = 0; i < A->DevProps.Num(); ++i)
			if (A->DevProps[i].Type == TEXT("star_bumper")) { A->DevProps[i].bBumper = true; A->DevProps[i].Bounce = 8; ZoogiBumpers::Sync(A, i); }
		Selected = A->DevProps.IndexOfByPredicate([](const AZoogiArena::FZoogiDevProp& P) { return P.Type == TEXT("star_bumper"); });
		UpdateMarker();
		if (!Dir.IsEmpty()) FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, TEXT("devobs_02_starbumper.png")), true, false);
		UE_LOG(LogZoogi, Log, TEXT("DEVOBS test: star bumpers active (rebound 8); %s"), *SelectedText().ToString().Replace(TEXT("\n"), TEXT(" ")));
		ObsTestStep = 2; ObsTestAt = Now + 1.5;
	}
	else
	{
		ObsTestStep = 3;
		ClosePanel();
	}
}

// -ZoogiDevAuto: smoke script (PC capture runs): opens the panel, moves a bumper and the ship through the touch path,
// rotates / sizes with the buttons, undo + redo, saves, exports, closes.
void AZoogiDevMode::RunAuto(float Dt)
{
	static const bool bAuto = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevAuto"));
	if (!bAuto || (AutoStep > 6 && AutoStep < 50)) return;
	static double LastReal = FPlatformTime::Seconds();
	const double NowReal = FPlatformTime::Seconds();
	AutoT += (float)FMath::Min(NowReal - LastReal, 0.25); // real time: the game is paused while the panel is open
	LastReal = NowReal;
	(void)Dt;
	AZoogiArena* A = GetArena();
	if (!A) return;
	float Start = 8.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiDevAutoAt="), Start);
	auto Shot = [](const TCHAR* Name)
	{
		FString Dir;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), Dir)) FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, FString::Printf(TEXT("devauto_%s.png"), Name)), true, false);
	};
	if (AutoStep == 0 && AutoT > Start)
	{
		static const bool bMenuPath = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDevMenuTest"));
		if (bMenuPath) ToggleFromMenu(GetWorld()); else OpenPanel(); // b167: the pause menu path
		Shot(TEXT("0_open"));
		AutoStep = 1;
	}
	else if (AutoStep == 1 && AutoT > Start + 1.5f)
	{
		int32 Bumper = INDEX_NONE;
		for (int32 i = 0; i < A->DevProps.Num(); ++i) if (A->DevProps[i].Type == TEXT("star_bumper")) { Bumper = i; break; }
		if (Bumper == INDEX_NONE) { AddBumper(); Bumper = Selected; }
		// drive the touch path: press on the bumper, hold (long press), drag 160 px, release
		FVector2D S;
		const auto& Pr = A->DevProps[Bumper];
		if (Project(FVector(Pr.Location.X, Pr.Location.Y, 70.f), S))
		{
			Selected = INDEX_NONE;
			TouchDown(0, S);
			if (SelectAtPixel(PressPx)) bDragging = true; // what the long press in Tick does after 0.45 s
			PressFinger = -1;
			TouchMove(0, S + FVector2D(160.f, 40.f));
			TouchUp(0, S + FVector2D(160.f, 40.f));
			UE_LOG(LogZoogi, Log, TEXT("DEVMODE AUTO: touch-dragged %s -> %s"), *Pr.Id, *SelectedText().ToString().Replace(TEXT("\n"), TEXT(" ")));
		}
		Shot(TEXT("1_dragged"));
		AutoStep = 2;
	}
	else if (AutoStep == 2 && AutoT > Start + 3.f) { Rotate(30.f); ScaleBy(1.2f); bGrid = true; Shot(TEXT("2_rotated_grid")); AutoStep = 3; }
	else if (AutoStep == 3 && AutoT > Start + 4.5f) { DoUndo(); DoRedo(); UE_LOG(LogZoogi, Log, TEXT("DEVMODE AUTO: undo + redo -> %s"), *SelectedText().ToString().Replace(TEXT("\n"), TEXT(" "))); AutoStep = 4; }
	else if (AutoStep == 4 && AutoT > Start + 6.f) { DoSave(); DoExport(); Shot(TEXT("3_saved_exported")); AutoStep = 5; }
	else if (AutoStep == 5 && AutoT > Start + 7.5f) { Tab = 1; CamSetProfile(TEXT('B')); CamStep(0, -1.f); UE_LOG(LogZoogi, Log, TEXT("DEVMODE AUTO: camera %s"), *CamHeader().ToString()); AutoStep = 55; }
	else if (AutoStep == 55 && AutoT > Start + 9.f) { Shot(TEXT("4_camera_B")); CamSetProfile(TEXT('C')); AutoStep = 56; }
	else if (AutoStep == 56 && AutoT > Start + 10.5f) { Shot(TEXT("5_camera_C")); CamSetProfile(TEXT('A')); DoSave(); AutoStep = 57; }
	else if (AutoStep == 57 && AutoT > Start + 12.f)
	{
		// free cam: push the left stick forward for a second and look 30 degrees right
		Tab = 0; ToggleFreeCam();
		int32 VX = 1920, VY = 1080; if (APlayerController* P = PC()) P->GetViewportSize(VX, VY);
		TouchDown(7, FVector2D(VX * 0.2f, VY * 0.6f)); TouchMove(7, FVector2D(VX * 0.2f, VY * 0.6f - StickRadiusPx()));
		FlyYaw += 30.f; FlyPitch -= 10.f;
		AutoStep = 58;
	}
	else if (AutoStep == 58 && AutoT > Start + 13.f)
	{
		int32 VX = 1920, VY = 1080; if (APlayerController* P = PC()) P->GetViewportSize(VX, VY);
		TouchUp(7, FVector2D(VX * 0.2f, VY * 0.6f - StickRadiusPx()));
		UE_LOG(LogZoogi, Log, TEXT("DEVMODE AUTO: free cam moved to %s"), FlyCam.IsValid() ? *FlyCam->GetActorLocation().ToCompactString() : TEXT("none"));
		Shot(TEXT("6_freecam")); bPanelHidden = true; AutoStep = 59;
	}
	else if (AutoStep == 59 && AutoT > Start + 14.5f) { Shot(TEXT("7_freecam_panel_hidden")); AutoStep = 60; }
	else if (AutoStep == 60 && AutoT > Start + 16.f) { ToggleFreeCam(); bPanelHidden = false; Tab = 2; AutoStep = 61; }
	else if (AutoStep == 61 && AutoT > Start + 17.f) { ZoogiTune::Step(0, -3.f); Shot(TEXT("8_hud_tab")); AutoStep = 611; }
	else if (AutoStep == 611 && AutoT > Start + 18.f) { Tab = 3; AutoStep = 62; }
	else if (AutoStep == 62 && AutoT > Start + 18.8f) { Shot(TEXT("9_spawn_tab")); TestSpawn(0); AutoStep = 63; }
	else if (AutoStep == 63 && AutoT > Start + 20.f) { ZoogiTune::ResetGroup(TEXT("DEV BUTTON")); ClosePanel(); AutoStep = 6; }
	else if (AutoStep == 6 && AutoT > Start + 21.f) { Shot(TEXT("10_dev_button")); AutoStep = 7; }
}

// ---- Oct 29: bumpers (any solid object) + rebound level

bool AZoogiDevMode::SelectedSolid() const
{
	const AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) return false;
	const FName T = A->DevProps[Selected].Type;
	return T == TEXT("star_bumper") || T == TEXT("obstacle");
}

void AZoogiDevMode::ToggleBumper()
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	if (!SelectedSolid()) { LastMsg = TEXT("pick a solid object (a bumper or an obstacle)"); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.bBumper = !Pr.bBumper;
	Pr.Bounce = FMath::Clamp(Pr.Bounce, 1, 10);
	ZoogiBumpers::Sync(A, Selected);
	CommitLayout(Pr.bBumper ? FString::Printf(TEXT("%s is a bumper (rebound %d)"), *Pr.Id, Pr.Bounce) : FString::Printf(TEXT("%s is no longer a bumper"), *Pr.Id));
}

void AZoogiDevMode::StepBounce(int32 Dir)
{
	AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected)) { LastMsg = TEXT("select an object first"); return; }
	if (!SelectedSolid()) { LastMsg = TEXT("pick a solid object (a bumper or an obstacle)"); return; }
	auto& Pr = A->DevProps[Selected];
	Pr.Bounce = FMath::Clamp(Pr.Bounce + Dir, 1, 10);
	Pr.bBumper = true; // setting a rebound turns it on
	ZoogiBumpers::Sync(A, Selected);
	CommitLayout(FString::Printf(TEXT("%s rebound %d (x%.1f)"), *Pr.Id, Pr.Bounce, UZoogiBumperSubsystem::Mul(Pr.Bounce)));
}

FText AZoogiDevMode::BumperText() const
{
	const AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected) || !SelectedSolid()) return FText::FromString(TEXT("BUMPER: select an object"));
	return FText::FromString(A->DevProps[Selected].bBumper ? TEXT("BUMPER: ON  (tap for off)") : TEXT("BUMPER: OFF  (tap to make it a bumper)"));
}

FText AZoogiDevMode::BounceText() const
{
	const AZoogiArena* A = GetArena();
	if (!A || !A->DevProps.IsValidIndex(Selected) || !SelectedSolid()) return FText::FromString(TEXT("REBOUND"));
	const auto& Pr = A->DevProps[Selected];
	if (!Pr.bBumper) return FText::FromString(TEXT("REBOUND off"));
	return FText::FromString(FString::Printf(TEXT("REBOUND %d/10  x%.1f"), Pr.Bounce, UZoogiBumperSubsystem::Mul(Pr.Bounce)));
}
