// b164 (Evan Nov 6 14:03: "Inside the PC game ... set portrait separately ... also place clips in story scenes"): the ANIME DIRECTOR
// panel - the PC editor for ZoogiAnimeCues. Dev Mode -> ANIME DIRECTOR, or F6. Pick a clip, add a cue, set when / where / how it
// plays, PREVIEW it over the live game, drag + resize its window on screen (landscape and portrait separately). Every change autosaves
// to Content/Data/Anime/AnimeCues.json. New clips: drop .mp4 files into C:\dev\anime_inbox and press IMPORT.
#include "ZoogiAnimeCuesInt.h"
#include "ZoogiRoll.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"

namespace ZoogiAnimeCues
{
	namespace
	{
		using FStr = TSharedPtr<FString>;
		FSlateFontInfo Fnt(int32 S, bool bBold = true) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", S); }
		const FLinearColor Pink(0.9f, 0.06f, 0.45f), Gold(1.f, 0.78f, 0.25f), Ink(0.05f, 0.04f, 0.08f), Panel(0.07f, 0.05f, 0.11f, 0.94f);
		FString InboxDir() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../anime_inbox")); }
		struct FScene { FString Id; TArray<FString> Lines; };
		TArray<FScene> LoadScenes()
		{
			TArray<FScene> Out; TArray<FString> Rows;
			FFileHelper::LoadFileToStringArray(Rows, *(FPaths::ProjectContentDir() / TEXT("Data/Story/Dialogue.csv")));
			bool bHeader = true;
			for (const FString& R : Rows)
			{
				if (R.IsEmpty() || R.StartsWith(TEXT("#"))) continue;
				if (bHeader) { bHeader = false; continue; }
				TArray<FString> F; R.ParseIntoArray(F, TEXT("|"), false);
				if (F.Num() < 3) continue;
				if (Out.Num() == 0 || Out.Last().Id != F[0]) { FScene S; S.Id = F[0]; Out.Add(S); }
				Out.Last().Lines.Add(F[1] + TEXT(": ") + F[2]);
			}
			return Out;
		}
	}

	class SZoogiDirector : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiDirector) {}
		SLATE_END_ARGS()
		TArray<FStr> ClipOpts, TrigOpts, ModeOpts, ArenaOpts, WhoOpts, RuleOpts, BorderOpts, InOpts, OutOpts, GameOpts, SceneOpts;
		TArray<FScene> Scenes;
		TSharedPtr<SScrollBox> ClipBox, CueBox;
		FString PickedClip, Filter;

		static TArray<FStr> Opts(const TArray<FString>& A) { TArray<FStr> O; for (const FString& S : A) O.Add(MakeShared<FString>(S)); return O; }
		static FCue* Sel() { TArray<FCue>& C = Cues(); return C.IsValidIndex(Int::GSel) ? &C[Int::GSel] : nullptr; }
		static FRect01* SelRect() { FCue* C = Sel(); return C ? (Int::GEditPortrait ? &C->Port : &C->Land) : nullptr; }
		static FText Tx(const FString& S) { return FText::FromString(S); }

		TSharedRef<SWidget> Label(const FString& S) { return SNew(STextBlock).Text(Tx(S)).Font(Fnt(11)).ColorAndOpacity(Gold); }
		TSharedRef<SWidget> Btn(TAttribute<FText> T, TFunction<void()> Fn, FLinearColor Col = FLinearColor(0.25f, 0.2f, 0.35f))
		{
			return SNew(SButton).ButtonColorAndOpacity(Col).HAlign(HAlign_Center).OnClicked_Lambda([Fn]() { Fn(); return FReply::Handled(); })
				[SNew(STextBlock).Text(T).Font(Fnt(11)).ColorAndOpacity(FLinearColor::White)];
		}
		TSharedRef<SWidget> Combo(const FString& Name, TArray<FStr>* Src, TFunction<FString()> Get, TFunction<void(const FString&)> Set)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[SNew(SBox).WidthOverride(92.f)[Label(Name)]]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SComboBox<FStr>).OptionsSource(Src)
					.OnGenerateWidget_Lambda([](FStr S) -> TSharedRef<SWidget> { return SNew(STextBlock).Text(Tx(S.IsValid() ? *S : FString())).Font(Fnt(11, false)); })
					.OnSelectionChanged_Lambda([Set](FStr S, ESelectInfo::Type) { if (S.IsValid() && Sel()) { Set(*S); Int::MarkDirty(); } })
					[SNew(STextBlock).Font(Fnt(11, false)).Text_Lambda([Get]() { return Tx(Sel() ? Get() : FString(TEXT("-"))); })]
				];
		}
		TSharedRef<SWidget> Num(const FString& Name, float Min, float Max, float Delta, TFunction<float()> Get, TFunction<void(float)> Set)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[SNew(SBox).WidthOverride(92.f)[Label(Name)]]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max).Delta(Delta).Font(Fnt(11, false))
					.Value_Lambda([Get]() { return Sel() ? Get() : 0.f; })
					.OnValueChanged_Lambda([Set](float V) { if (Sel()) { Set(V); Int::MarkDirty(); } })
				];
		}
		TSharedRef<SWidget> Row2(TSharedRef<SWidget> A, TSharedRef<SWidget> B)
		{
			return SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)[A] + SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)[B];
		}
		void RebuildClips()
		{
			ClipOpts = Opts(Clips());
			if (!ClipBox.IsValid()) return;
			ClipBox->ClearChildren();
			for (const FStr& C : ClipOpts)
			{
				if (!Filter.IsEmpty() && !C->Contains(Filter)) continue;
				const FString Id = *C;
				ClipBox->AddSlot().Padding(0.f, 1.f)[SNew(SButton).ButtonColorAndOpacity_Lambda([this, Id]() -> FSlateColor { return FSlateColor(PickedClip == Id ? Pink : FLinearColor(0.18f, 0.15f, 0.25f)); })
					.OnClicked_Lambda([this, Id]() { PickedClip = Id; return FReply::Handled(); })[SNew(STextBlock).Text(Tx(Id)).Font(Fnt(10, false))]];
			}
		}
		static FString CueLabel(int32 i)
		{
			const TArray<FCue>& C = Cues(); if (!C.IsValidIndex(i)) return FString();
			const FCue& Q = C[i];
			const FString When = Q.Trigger == TEXT("Story") ? FString::Printf(TEXT("Story %s #%d"), *Q.Scene, Q.Line) : Q.Trigger + (Q.Mode != TEXT("any") ? TEXT(" / ") + Q.Mode : FString());
			return FString::Printf(TEXT("%s%s  ->  %s"), Q.bOn ? TEXT("") : TEXT("(off) "), *When, *Q.Clip);
		}
		void RebuildCues()
		{
			if (!CueBox.IsValid()) return;
			CueBox->ClearChildren();
			for (int32 i = 0; i < Cues().Num(); ++i)
				CueBox->AddSlot().Padding(0.f, 1.f)[SNew(SButton).ButtonColorAndOpacity_Lambda([i]() -> FSlateColor { return FSlateColor(Int::GSel == i ? Pink : FLinearColor(0.18f, 0.15f, 0.25f)); })
					.OnClicked_Lambda([i]() { Int::GSel = i; return FReply::Handled(); })[SNew(STextBlock).Font(Fnt(10, false)).Text_Lambda([i]() { return Tx(CueLabel(i)); })]];
		}
		void AddCue()
		{
			FCue C; C.Clip = PickedClip.IsEmpty() ? (ClipOpts.Num() ? *ClipOpts[0] : FString()) : PickedClip;
			int32 N = Cues().Num() + 1; while (Cues().ContainsByPredicate([&](const FCue& X) { return X.Id == FString::Printf(TEXT("cue_%d"), N); })) ++N;
			C.Id = FString::Printf(TEXT("cue_%d"), N);
			C.Land.bFull = false; C.Land.W = 0.42f; C.Land.H = 0.42f; C.Land.X = 0.5f; C.Land.Y = 0.3f;
			C.Port.bFull = false; C.Port.W = 0.9f; C.Port.H = 0.28f; C.Port.X = 0.5f; C.Port.Y = 0.25f;
			Cues().Add(C); Int::GSel = Cues().Num() - 1; Int::MarkDirty(); RebuildCues();
		}
		void ImportInbox()
		{
			const FString In = InboxDir(); IFileManager::Get().MakeDirectory(*In, true);
			TArray<FString> F; IFileManager::Get().FindFiles(F, *(In / TEXT("*.mp4")), true, false);
			int32 N = 0;
			for (const FString& S : F)
			{
				FString Id = FPaths::GetBaseFilename(S).Replace(TEXT(" "), TEXT("_"));
				const FString Dst = FPaths::ProjectContentDir() / TEXT("Anime/Movies") / (Id + TEXT(".mp4"));
				if (IFileManager::Get().Copy(*Dst, *(In / S)) == COPY_OK) { IFileManager::Get().Move(*(In / TEXT("imported") / S), *(In / S)); ++N; PickedClip = Id; }
			}
			UE_LOG(LogZoogi, Log, TEXT("ANIME DIRECTOR: imported %d clip(s) from %s"), N, *In);
			RebuildClips();
		}
		int32 InboxCount() const { TArray<FString> F; IFileManager::Get().FindFiles(F, *(InboxDir() / TEXT("*.mp4")), true, false); return F.Num(); }
		FStr SceneOpt(const FString& Id) { for (const FStr& S : SceneOpts) if (*S == Id) return S; return nullptr; }

		void Construct(const FArguments&)
		{
			Scenes = LoadScenes();
			TArray<FString> Sc; for (const FScene& S : Scenes) Sc.Add(S.Id); SceneOpts = Opts(Sc);
			TrigOpts = Opts(Triggers()); InOpts = Opts(Templates(false)); OutOpts = Opts(Templates(true));
			ModeOpts = Opts({ TEXT("any"), TEXT("classic"), TEXT("battle"), TEXT("royale"), TEXT("story"), TEXT("training"), TEXT("trickshot") });
			ArenaOpts = Opts({ TEXT("any"), TEXT("meadow"), TEXT("frozen"), TEXT("volcanic"), TEXT("cosmic"), TEXT("arabian"), TEXT("tomb"), TEXT("neon"), TEXT("training"), TEXT("teaser"), TEXT("ancient") });
			WhoOpts = Opts({ TEXT("any"), TEXT("human"), TEXT("cpu") }); RuleOpts = Opts({ TEXT("first"), TEXT("always"), TEXT("chance") });
			BorderOpts = Opts({ TEXT("none"), TEXT("p5"), TEXT("thin"), TEXT("comic") }); GameOpts = Opts({ TEXT("pause"), TEXT("slow"), TEXT("play") });
			ClipOpts = Opts(Clips());
			auto LineText = [this]() -> FText
			{
				const FCue* C = Sel(); if (!C) return FText::GetEmpty();
				for (const FScene& S : Scenes) if (S.Id == C->Scene) return Tx(S.Lines.IsValidIndex(C->Line) ? S.Lines[C->Line] : FString(TEXT("(past the last line)")));
				return Tx(TEXT("(pick a scene)"));
			};
			ChildSlot.HAlign(HAlign_Right).VAlign(VAlign_Fill)
			[
				SNew(SBox).WidthOverride(560.f)
				[
					SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel).Padding(10.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Tx(TEXT("ANIME DIRECTOR"))).Font(Fnt(20)).ColorAndOpacity(Pink)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[Btn(Tx(TEXT("SAVE")), []() { Save(); }, FLinearColor(0.1f, 0.5f, 0.25f))]
							+ SHorizontalBox::Slot().AutoWidth()[Btn(Tx(TEXT("CLOSE (F6)")), []() { ToggleDirector(); })]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[SNew(STextBlock).Font(Fnt(9, false)).ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.85f)).AutoWrapText(true)
							.Text(Tx(TEXT("Every change saves itself. Claude ships this file to the phone build when you say it's set.")))]
						// ---- clips
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(TEXT("CLIPS  "))]
							+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SEditableTextBox).HintText(Tx(TEXT("filter"))).Font(Fnt(10, false)).OnTextChanged_Lambda([this](const FText& T) { Filter = T.ToString(); RebuildClips(); })]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)[Btn(TAttribute<FText>::CreateLambda([this]() { return Tx(FString::Printf(TEXT("IMPORT INBOX (%d)"), InboxCount())); }), [this]() { ImportInbox(); })]]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(150.f)[SAssignNew(ClipBox, SScrollBox)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[Row2(Btn(Tx(TEXT("+ ADD CUE FOR THIS CLIP")), [this]() { AddCue(); }, Pink),
							Btn(Tx(TEXT("PREVIEW CLIP")), [this]() { FCue P; if (FCue* S = Sel()) P = *S; P.Clip = PickedClip.IsEmpty() ? P.Clip : PickedClip; P.Id = TEXT("preview"); Play(GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr, P, true); }))]
						// ---- cues
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[Label(TEXT("CUES"))]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(130.f)[SAssignNew(CueBox, SScrollBox)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 3.f, 0.f)[Btn(Tx(TEXT("ON / OFF")), []() { if (FCue* C = Sel()) { C->bOn = !C->bOn; Int::MarkDirty(); } })]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 0.f)[Btn(Tx(TEXT("DUPLICATE")), [this]() { if (FCue* C = Sel()) { FCue D = *C; D.Id += TEXT("_copy"); Cues().Add(D); Int::GSel = Cues().Num() - 1; Int::MarkDirty(); RebuildCues(); } })]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 0.f, 0.f, 0.f)[Btn(Tx(TEXT("DELETE")), [this]() { if (Sel()) { Cues().RemoveAt(Int::GSel); Int::GSel = -1; Int::MarkDirty(); RebuildCues(); } }, FLinearColor(0.55f, 0.1f, 0.12f))]]
						// ---- the selected cue
						+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(SScrollBox).Visibility_Lambda([]() { return Sel() ? EVisibility::Visible : EVisibility::Collapsed; })
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Label(TEXT("WHEN"))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Combo(TEXT("Clip"), &ClipOpts, []() { return Sel()->Clip; }, [](const FString& V) { Sel()->Clip = V; })]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Combo(TEXT("Trigger"), &TrigOpts, []() { return Sel()->Trigger; }, [](const FString& V) { Sel()->Trigger = V; })]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(SVerticalBox).Visibility_Lambda([]() { return Sel() && Sel()->Trigger == TEXT("Story") ? EVisibility::Visible : EVisibility::Collapsed; })
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Combo(TEXT("Scene"), &SceneOpts, []() { return Sel()->Scene; }, [](const FString& V) { Sel()->Scene = V; })]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Num(TEXT("Before line"), 0.f, 60.f, 1.f, []() { return (float)Sel()->Line; }, [](float V) { Sel()->Line = FMath::RoundToInt(V); })]
								+ SVerticalBox::Slot().AutoHeight().Padding(98.f, 2.f, 0.f, 2.f)[SNew(STextBlock).Font(Fnt(9, false)).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f)).Text_Lambda(LineText)]]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(SVerticalBox).Visibility_Lambda([]() { return Sel() && Sel()->Trigger != TEXT("Story") ? EVisibility::Visible : EVisibility::Collapsed; })
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row2(Combo(TEXT("Mode"), &ModeOpts, []() { return Sel()->Mode; }, [](const FString& V) { Sel()->Mode = V; }),
									Combo(TEXT("Arena"), &ArenaOpts, []() { return Sel()->Arena; }, [](const FString& V) { Sel()->Arena = V; }))]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row2(Combo(TEXT("Whose shot"), &WhoOpts, []() { return Sel()->Who; }, [](const FString& V) { Sel()->Who = V; }),
									SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)[SNew(SBox).WidthOverride(92.f)[Label(TEXT("Zoogi id"))]]
									+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SEditableTextBox).Font(Fnt(10, false)).Text_Lambda([]() { return Tx(Sel() ? Sel()->Zoogi : FString()); })
										.OnTextCommitted_Lambda([](const FText& T, ETextCommit::Type) { if (Sel()) { Sel()->Zoogi = T.ToString().IsEmpty() ? FString(TEXT("any")) : T.ToString().ToLower(); Int::MarkDirty(); } })])]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Row2(Combo(TEXT("How often"), &RuleOpts, []() { return Sel()->Rule; }, [](const FString& V) { Sel()->Rule = V; }),
									Num(TEXT("Chance"), 0.f, 1.f, 0.05f, []() { return Sel()->Chance; }, [](float V) { Sel()->Chance = V; }))]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)[Num(TEXT("Cooldown s"), 0.f, 600.f, 1.f, []() { return Sel()->Cooldown; }, [](float V) { Sel()->Cooldown = V; })]]
							+ SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 2.f)[Label(TEXT("WINDOW"))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 3.f, 0.f)[Btn(Tx(TEXT("LANDSCAPE")), []() { Int::GEditPortrait = false; }, FLinearColor(0.2f, 0.2f, 0.3f))]
								+ SHorizontalBox::Slot().FillWidth(1.f).Padding(3.f, 0.f)[Btn(Tx(TEXT("PORTRAIT")), []() { Int::GEditPortrait = true; }, FLinearColor(0.2f, 0.2f, 0.3f))]
								+ SHorizontalBox::Slot().FillWidth(1.4f).Padding(3.f, 0.f, 0.f, 0.f)[Btn(TAttribute<FText>::CreateLambda([]() { return Tx(Int::GEditLayout ? TEXT("DRAG ON SCREEN: ON") : TEXT("DRAG ON SCREEN: OFF")); }), []() { Int::GEditLayout = !Int::GEditLayout; }, Pink)]]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(STextBlock).Font(Fnt(10)).ColorAndOpacity(FLinearColor::White).Text_Lambda([]() { return Tx(Int::GEditPortrait ? TEXT("Editing: PORTRAIT (phone frame on screen)") : TEXT("Editing: LANDSCAPE")); })]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[SNew(SCheckBox).IsChecked_Lambda([]() { return SelRect() && SelRect()->bFull ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
								.OnCheckStateChanged_Lambda([](ECheckBoxState S) { if (FRect01* R = SelRect()) { R->bFull = S == ECheckBoxState::Checked; Int::MarkDirty(); } })[SNew(STextBlock).Text(Tx(TEXT("Full screen"))).Font(Fnt(11))]]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Num(TEXT("Centre X"), 0.f, 1.f, 0.01f, []() { return SelRect()->X; }, [](float V) { SelRect()->X = V; }), Num(TEXT("Centre Y"), 0.f, 1.f, 0.01f, []() { return SelRect()->Y; }, [](float V) { SelRect()->Y = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Num(TEXT("Width"), 0.05f, 1.2f, 0.01f, []() { return SelRect()->W; }, [](float V) { SelRect()->W = V; }), Num(TEXT("Height"), 0.05f, 1.2f, 0.01f, []() { return SelRect()->H; }, [](float V) { SelRect()->H = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Combo(TEXT("Border"), &BorderOpts, []() { return Sel()->Border; }, [](const FString& V) { Sel()->Border = V; })]
							+ SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 2.f)[Label(TEXT("INTRO / EXIT"))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Combo(TEXT("Intro"), &InOpts, []() { return Sel()->Intro; }, [](const FString& V) { Sel()->Intro = V; }), Num(TEXT("Intro s"), 0.f, 3.f, 0.05f, []() { return Sel()->IntroSec; }, [](float V) { Sel()->IntroSec = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Combo(TEXT("Exit"), &OutOpts, []() { return Sel()->Outro; }, [](const FString& V) { Sel()->Outro = V; }), Num(TEXT("Exit s"), 0.f, 3.f, 0.05f, []() { return Sel()->OutroSec; }, [](float V) { Sel()->OutroSec = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 2.f)[Label(TEXT("THE GAME MEANWHILE"))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Combo(TEXT("Game"), &GameOpts, []() { return Sel()->Game; }, [](const FString& V) { Sel()->Game = V; }), Num(TEXT("Slow speed"), 0.02f, 1.f, 0.01f, []() { return Sel()->Slow; }, [](float V) { Sel()->Slow = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Num(TEXT("Music level"), 0.f, 1.f, 0.05f, []() { return Sel()->Duck; }, [](float V) { Sel()->Duck = V; }), Num(TEXT("Clip volume"), 0.f, 2.f, 0.05f, []() { return Sel()->Volume; }, [](float V) { Sel()->Volume = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Row2(Num(TEXT("Skip after s"), 0.f, 30.f, 0.1f, []() { return Sel()->SkipAfter; }, [](float V) { Sel()->SkipAfter = V; }), Num(TEXT("Trim start s"), 0.f, 60.f, 0.1f, []() { return Sel()->TrimStart; }, [](float V) { Sel()->TrimStart = V; }))]
							+ SScrollBox::Slot().Padding(0.f, 2.f)[Num(TEXT("Trim end s"), 0.f, 60.f, 0.1f, []() { return Sel()->TrimEnd; }, [](float V) { Sel()->TrimEnd = V; })]
							+ SScrollBox::Slot().Padding(0.f, 10.f, 0.f, 4.f)[SNew(SBox).HeightOverride(44.f)[Btn(Tx(TEXT("PREVIEW THIS CUE")), []() { if (FCue* C = Sel()) Play(GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr, *C, true); }, Pink)]]
						]
					]
				]
			];
			RebuildClips(); RebuildCues();
		}
	};

	namespace { TSharedPtr<SZoogiDirector> GPanel; }
	void Int::OpenPanel()
	{
		if (!GEngine || !GEngine->GameViewport) return;
		Cues(); Int::EnsureLayer();
		IFileManager::Get().MakeDirectory(*InboxDir(), true);
		if (!GPanel.IsValid()) GPanel = SNew(SZoogiDirector);
		GEngine->GameViewport->AddViewportWidgetContent(GPanel.ToSharedRef(), 99); // b167: above the dev overlay (98)
		if (UWorld* W = GEngine->GameViewport->GetWorld()) if (APlayerController* PC = W->GetFirstPlayerController()) { PC->SetShowMouseCursor(true); FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); PC->SetInputMode(M); }
		UE_LOG(LogZoogi, Log, TEXT("ANIME DIRECTOR open (%d cues, inbox %s)"), Cues().Num(), *InboxDir());
	}
	void Int::ClosePanel()
	{
		if (GPanel.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(GPanel.ToSharedRef());
		Save();
		UE_LOG(LogZoogi, Log, TEXT("ANIME DIRECTOR closed"));
	}
	void ToggleDirector()
	{
#if PLATFORM_DESKTOP
		Int::GDirector = !Int::GDirector;
		if (Int::GDirector) Int::OpenPanel(); else { Int::GEditLayout = false; Int::ClosePanel(); }
#endif
	}
	bool DirectorOpen() { return Int::GDirector; }
}
