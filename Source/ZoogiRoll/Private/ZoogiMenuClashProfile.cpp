// clash-menu (Oct 17, milestone 3 / DETAILED PASS 2): Profile (tag copy, 8 battle stats, cards found), Battle Log (real
// matches), the Replay viewer (plays a recorded match back top-down on the match's map art at 0.5/1/2/4x), Settings
// (nickname for 10 gold, Master / Sound Effects / Music sliders wired to the real audio, 5 languages, account notes,
// version) and Change Nickname. Data: UZoogiFrontProfileSubsystem (local save); replays are recorded read-only by
// UZoogiFrontRewardBridge during real matches. Nothing here is faked: email linking / log out need an account backend
// and say so; the language choice is saved, and the game has no translated strings yet (reported, not invented).
#include "ZoogiMenuRoot.h"
#include "ZoogiMenuClashStyle.h"
#include "ZoogiFrontProfile.h"
#include "ZoogiGameMode.h"
#include "ZoogiMenuWidget.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiUIStyle.h"
#include "AudioDevice.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"

namespace
{
	const TCHAR* ZPLangNames[] = { TEXT("English"), TEXT("Portugu\u00EAs"), TEXT("\u4E2D\u6587"), TEXT("\u65E5\u672C\u8A9E"), TEXT("\uD55C\uAD6D\uC5B4") };
	const TCHAR* ZPLangCodes[] = { TEXT("en"), TEXT("pt"), TEXT("zh"), TEXT("ja"), TEXT("ko") };
	const TCHAR* ZPVolKeys[] = { TEXT("ClashMaster"), TEXT("ClashSfx"), TEXT("ClashMusic") };
	FString ZPVersion()
	{
		FString V;
		if (GConfig) GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), V, GGameIni);
		if (V.IsEmpty() || V == TEXT("1.0.0.0")) V = TEXT("1.0.0");
		return V;
	}
}

// ------------------------------------------------------------------------------------------------ shared helpers
UWidget* UZoogiMenuRoot::CPopupBody(float X, float Y, float Wd, float Ht, float R)
{
	// fantasy reskin: popups share the gold-rimmed carved-stone frame of the panels (was light blue #4A9CF0 -> #2D74D6)
	return CGoldPanel(X, Y, Wd, Ht, ZCPopBot, R);
}

UWidget* UZoogiMenuRoot::CTextIntl(const FString& S, float X, float Y, float Wd, float Ht, float Size, const FLinearColor& C)
{
	// labels in other scripts (Chinese / Japanese / Korean): the engine's default composite font has the CJK fallback
	UWidget* W = CText(S, X, Y, Wd, Ht, Size, C, 1, 3.f);
	if (UBorder* B = Cast<UBorder>(W))
		if (UTextBlock* T = Cast<UTextBlock>(B->GetContent()))
		{
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(Size));
			F.OutlineSettings.OutlineSize = 3;
			F.OutlineSettings.OutlineColor = ZCDark;
			T->SetFont(F);
		}
	return W;
}

// ------------------------------------------------------------------------------------------------ audio settings
void UZoogiMenuRoot::ClashApplyAudio(bool bInit)
{
	// Master -> the engine's primary volume; Sound Effects -> the existing SFX volume (SfxVol action); Music -> the
	// music volume (MusicVol action: menu loop + match stems). Defaults 50% (DETAILED PASS 2); saved in GameUserSettings.
	const int32 Master = GetLocalSetting(ZPVolKeys[0], 50), Sfx = GetLocalSetting(ZPVolKeys[1], 50), Music = GetLocalSetting(ZPVolKeys[2], 50);
	if (GEngine)
		if (FAudioDeviceHandle Dev = GEngine->GetMainAudioDevice()) Dev->SetTransientPrimaryVolume(FMath::Clamp(Master / 100.f, 0.f, 1.f));
	FString CmdSfx;
	if (!(bInit && FParse::Value(FCommandLine::Get(), TEXT("ZoogiSfxVol="), CmdSfx))) Fire("SfxVol", Sfx); // a command-line -ZoogiSfxVol wins at start
	Fire("MusicVol", Music);
	if (bInit) UE_LOG(LogZoogi, Log, TEXT("CLASH audio: master %d%%, sfx %d%%, music %d%%"), Master, Sfx, Music);
}

void UZoogiMenuRoot::ClashTickSettings(float Dt)
{
	// sliders: polled (no dynamic delegates needed); each change goes straight to the real audio, the ini is flushed 4x/s
	if (ClashPopup == EZoogiClashPopup::Settings)
		for (int32 i = 0; i < 3; ++i)
			if (USlider* S = ClashSlider[i].Get())
			{
				const float V = S->GetValue();
				if (FMath::Abs(V - ClashSliderLast[i]) < 0.001f) continue;
				ClashSliderLast[i] = V;
				const int32 Pct = FMath::RoundToInt(V * 100.f);
				if (GConfig) GConfig->SetInt(TEXT("ZoogiRoll.MenuSettings"), ZPVolKeys[i], Pct, GGameUserSettingsIni);
				bClashSettingsDirty = true;
				if (UWidget* F = ClashSliderFill[i].Get()) if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(F->Slot)) CS->SetSize(FVector2D(FMath::Max(1.f, ClashSliderFillW * V), 28.f));
				if (UTextBlock* T = ClashSliderPct[i].Get()) T->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Pct)));
				ClashApplyAudio(false);
			}
	if (ClashNickBox.IsValid()) ClashNickDraft = ClashNickBox->GetText().ToString(); // survives rebuilds
	ClashSettingsFlushT += Dt;
	if (bClashSettingsDirty && ClashSettingsFlushT > 0.25f)
	{
		ClashSettingsFlushT = 0.f; bClashSettingsDirty = false;
		if (GConfig) GConfig->Flush(false, GGameUserSettingsIni);
	}
}

// ------------------------------------------------------------------------------------------------ replay
void UZoogiMenuRoot::ClashOpenReplay(int32 LogIdx)
{
	const UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP || !FP->Data().BattleLog.IsValidIndex(LogIdx)) return;
	const FZoogiFrontBattle& B = FP->Data().BattleLog[LogIdx];
	TSharedPtr<FZoogiReplay> R = MakeShared<FZoogiReplay>();
	if (!FP->LoadReplay(B.ReplayId, *R) || R->NumFrames() < 2)
	{
		OpenClashPopup(EZoogiClashPopup::Info, TEXT("REPLAY"), TEXT("This replay is no longer saved.\nThe last 10 matches keep their replays."));
		return;
	}
	ClashReplay = R;
	ClashReplayInfo = B;
	ClashReplayT = 0.f; ClashReplaySpeed = 1.f; bClashReplayPlaying = true;
	UE_LOG(LogZoogi, Log, TEXT("CLASH replay %s: %d frames, %d zoogis, %d orbs, %.1f s"), *B.ReplayId, R->NumFrames(), R->NumZ(), R->NumOrbs, R->Duration());
	OpenClashPopup(EZoogiClashPopup::Replay, TEXT("REPLAY"));
}

void UZoogiMenuRoot::ClashReplayApply()
{
	const FZoogiReplay* R = ClashReplay.Get();
	if (!R || R->NumFrames() < 1) return;
	// frame pair around the playhead (binary search; frames are 10 Hz world-timer samples)
	int32 Lo = 0, Hi = R->NumFrames() - 1;
	while (Lo < Hi) { const int32 Mid = (Lo + Hi + 1) / 2; if (R->Times[Mid] <= ClashReplayT) Lo = Mid; else Hi = Mid - 1; }
	const int32 F0 = Lo, F1 = FMath::Min(Lo + 1, R->NumFrames() - 1);
	const float A = F1 > F0 ? FMath::Clamp((ClashReplayT - R->Times[F0]) / FMath::Max(1e-3f, R->Times[F1] - R->Times[F0]), 0.f, 1.f) : 0.f;
	const float K = (ClashReplayS * 0.5f * 0.94f) / FMath::Max(100.f, R->Radius);
	// top-down, matching the arena's map art (P1's blue side marker on the left): screen right = world +X, screen down = world +Y
	auto ToScreen = [&](float WX, float WY) { return FVector2D(ClashReplayCX + (WX - R->Center.X) * K, ClashReplayCY + (WY - R->Center.Y) * K); };
	const int32 NZ = R->NumZ();
	for (int32 i = 0; i < NZ && i < ClashReplayZW.Num(); ++i)
	{
		const float* P0 = &R->Z[(F0 * NZ + i) * 6];
		const float* P1 = &R->Z[(F1 * NZ + i) * 6];
		const FVector2D S = ToScreen(FMath::Lerp(P0[0], P1[0], A), FMath::Lerp(P0[1], P1[1], A));
		const float Yaw = P0[4] + FMath::FindDeltaAngleDegrees(P0[4], P1[4]) * A;
		const bool bOut = R->ZFlags[F0 * NZ + i] != 0;
		if (UWidget* W = ClashReplayZW[i].Get())
		{
			if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(W->Slot)) CS->SetPosition(S - CS->GetSize() * 0.5f);
			W->SetRenderTransformAngle(Yaw); // the token's notch points along its facing
			W->SetRenderOpacity(bOut ? 0.3f : 1.f);
		}
		if (ClashReplayLW.IsValidIndex(i))
			if (UWidget* L = ClashReplayLW[i].Get())
			{
				if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(L->Slot)) CS->SetPosition(FVector2D(S.X - CS->GetSize().X * 0.5f, S.Y - 62.f));
				L->SetRenderOpacity(bOut ? 0.4f : 1.f);
			}
	}
	const int32 NO = R->NumOrbs;
	for (int32 i = 0; i < NO && i < ClashReplayOW.Num(); ++i)
		if (UWidget* W = ClashReplayOW[i].Get())
		{
			const float* P0 = &R->O[(F0 * NO + i) * 2];
			const float* P1 = &R->O[(F1 * NO + i) * 2];
			const uint8 Fl = R->OFlags[F0 * NO + i];
			const FVector2D S = ToScreen(FMath::Lerp(P0[0], P1[0], A), FMath::Lerp(P0[1], P1[1], A));
			if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(W->Slot)) CS->SetPosition(S - CS->GetSize() * 0.5f);
			W->SetRenderOpacity((Fl & 1) ? 0.f : 1.f);
			if (UImage* I = Cast<UImage>(W)) I->SetColorAndOpacity((Fl & 2) ? ZCGold : ZCHex(TEXT("DDF4FF")));
		}
	if (UWidget* Bar = ClashReplayBar.Get()) if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Bar->Slot)) CS->SetSize(FVector2D(FMath::Max(1.f, ClashReplayBarW * (R->Duration() > 0.f ? ClashReplayT / R->Duration() : 0.f)), 20.f));
}

void UZoogiMenuRoot::BuildClashReplay()
{
	const FZoogiReplay* R = ClashReplay.Get();
	const float W = DesignW, C = W * 0.5f;
	CAt(CBox(ZCHex(TEXT("0B0C0F")).CopyWithNewOpacity(0.97f), FLinearColor::Transparent, 0.f, 0.f), 0.f, -400.f, W, DesignH + 800.f);
	if (UButton* Block = CButton(CBox(FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f), []() {})) { Block->SetStyle(ZCClearStyle()); CAt(Block, 0.f, -400.f, W, DesignH + 800.f); } // eats taps behind
	ClashReplayZW.Reset(); ClashReplayLW.Reset(); ClashReplayOW.Reset();
	CChunky(TEXT("EXIT"), ZCXRed, [this]() { ClashReplay.Reset(); OpenClashPopup(EZoogiClashPopup::BattleLog); }, 30.f, 40.f, 200.f, 92.f, 36.f);
	if (!R) return;
	const FZoogiFrontBattle& B = ClashReplayInfo;
	const TCHAR* Res = B.Outcome == 0 ? TEXT("WIN") : B.Outcome == 1 ? TEXT("LOSS") : B.Outcome == 2 ? TEXT("DRAW") : TEXT("LEFT");
	// M4 portrait: header, square map (960), timeline, PAUSE, speeds; the group is centred in the column
	const float OY = FMath::Max(150.f, (DesignH - 1500.f) * 0.5f);
	CText(TEXT("REPLAY"), C - 300.f, OY, 600.f, 64.f, 46.f, FLinearColor::White, 1, 5.f);
	CText(FString::Printf(TEXT("%s  vs %s  -  %s  -  %d : %d%s"), Res, *B.Opponent, *ZoogiUtil::ArenaName((EZoogiArena)R->Arena), B.MyScore, B.OppScore, B.bDev ? TEXT("  (DEV result)") : TEXT("")),
		C - 500.f, OY + 64.f, 1000.f, 60.f, 22.f, ZCSoft, 1, 3.f, true);
	// the match's map (its own MenuArt), clipped round, with the recorded tokens on top
	const float MY = OY + 150.f;
	ClashReplayS = 960.f; ClashReplayCX = C; ClashReplayCY = MY + ClashReplayS * 0.5f;
	const float MX = C - ClashReplayS * 0.5f;
	CAt(CBox(ZCNavyDeep, ZCGold, 6.f, ClashReplayS * 0.5f + 8.f), MX - 8.f, MY - 8.f, ClashReplayS + 16.f, ClashReplayS + 16.f);
	UTexture2D* Map = Art(FString(ZCArenaSlug(R->Arena)) + TEXT("_map"));
	CImg(Map, MX, MY, ClashReplayS, ClashReplayS, ClashReplayS * 0.5f, true, FLinearColor(0.78f, 0.82f, 0.9f, 1.f));
	for (int32 i = 0; i < R->NumOrbs; ++i)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(TEXT("knob")), 22.f, 22.f, 0.f, false));
		I->SetVisibility(ESlateVisibility::HitTestInvisible);
		CAt(I, ClashReplayCX, ClashReplayCY, 22.f, 22.f);
		ClashReplayOW.Add(I);
	}
	for (int32 i = 0; i < R->NumZ(); ++i)
	{
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(TEXT("zdot")), 50.f, 50.f, 0.f, false));
		I->SetColorAndOpacity(R->Colors.IsValidIndex(i) ? R->Colors[i] : FLinearColor::White);
		I->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		I->SetVisibility(ESlateVisibility::HitTestInvisible);
		CAt(I, ClashReplayCX, ClashReplayCY, 50.f, 50.f);
		ClashReplayZW.Add(I);
		ClashReplayLW.Add(CText(FString::Printf(TEXT("P%d %s"), (R->Players.IsValidIndex(i) ? R->Players[i] : i) + 1, *R->Names[i].ToUpper()), ClashReplayCX, ClashReplayCY, 240.f, 30.f, 18.f, FLinearColor::White, 1, 3.f));
	}
	// timeline
	const float BarX = C - 480.f, BarY = MY + ClashReplayS + 46.f, BarFull = 740.f;
	ClashReplayBarW = BarFull - 8.f;
	CAt(CBox(ZCNavyDeep, ZCDark, 4.f, 14.f), BarX, BarY, BarFull, 28.f);
	{
		UBorder* Fill = CBox(ZCGold, FLinearColor::Transparent, 0.f, 10.f);
		Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		CAt(Fill, BarX + 4.f, BarY + 4.f, 1.f, 20.f); // width follows the playhead
		ClashReplayBar = Fill;
	}
	TWeakObjectPtr<UZoogiMenuRoot> WThis(this);
	CLive([WThis]() { const UZoogiMenuRoot* M = WThis.Get(); return M && M->ClashReplay.IsValid() ? FString::Printf(TEXT("%s / %s"), *ZCClock(M->ClashReplayT), *ZCClock(M->ClashReplay->Duration())) : FString(); },
		BarX + BarFull + 14.f, BarY - 8.f, 210.f, 44.f, 24.f, FLinearColor::White, 0);
	// controls: green Pause/Play, speeds 0.5 / 1 / 2 / 4x (navy; selected = olive-gold)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(ClashArt(bClashReplayPlaying ? TEXT("pause") : TEXT("play")), 40.f, 40.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f)); }
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(bClashReplayPlaying ? TEXT("PAUSE") : TEXT("PLAY"), 32.f, FLinearColor::White, 4.f))) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCGreen, [this]()
		{
			if (!ClashReplay.IsValid()) return;
			if (!bClashReplayPlaying && ClashReplayT >= ClashReplay->Duration() - 0.01f) ClashReplayT = 0.f; // play again from the start
			bClashReplayPlaying = !bClashReplayPlaying; bDirty = true;
		}, C - 150.f, BarY + 64.f, 300.f, 96.f, true);
	}
	const float Speeds[] = { 0.5f, 1.f, 2.f, 4.f };
	for (int32 i = 0; i < 4; ++i)
	{
		const float Sp = Speeds[i];
		const bool bSel = FMath::IsNearlyEqual(ClashReplaySpeed, Sp);
		CChunky(FString::Printf(TEXT("%.1fx"), Sp), bSel ? ZCOlive : ZCNavyBtn, [this, Sp]() { ClashReplaySpeed = Sp; bDirty = true; }, C - 364.f + i * 186.f, MY + ClashReplayS + 46.f + 184.f, 170.f, 84.f, 30.f);
	}
	CText(TEXT("Recorded from the real match at 10 Hz (positions + rotations, read-only). Top-down view."), C - 500.f, MY + ClashReplayS + 46.f + 290.f, 1000.f, 60.f, 18.f, ZCMuted, 1, 2.f, true);
	ClashReplayApply();
}

// ------------------------------------------------------------------------------------------------ popups
bool UZoogiMenuRoot::BuildClashProfilePopup(EZoogiClashPopup P, float PW, float PH)
{
	UZoogiFrontProfileSubsystem* FP = UZoogiFrontProfileSubsystem::Get(this);
	if (!FP) return false;
	const FZoogiFrontProfileData& D = FP->Data();
	auto Tile = [this](float X, float Y, float Wd, float Ht, UTexture2D* Icon, const FString& Label, const FString& Value, const FLinearColor& VC)
	{
		CPlate(X, Y, Wd, Ht, ZCNavyDeep.CopyWithNewOpacity(0.82f), 16.f, 4.f, false);
		CImg(Icon, X + 10.f, Y + Ht * 0.5f - 30.f, 60.f, 60.f);
		CText(Label, X + 80.f, Y, Wd - 200.f, Ht, 21.f, ZCSoft, 0, 3.f);
		CText(Value, X + Wd - 140.f, Y, 124.f, Ht, 32.f, VC, 2, 4.f);
	};
	auto Portrait = [this]() -> UTexture2D*
	{
		AZoogiGameMode* GMode = GM();
		const TArray<FZoogiDef>& Roster = ZoogiUtil::Roster();
		return (GMode && GMode->MenuPlayers.Num() > 0 && Roster.Num() > 0) ? Art(PortraitFor(Roster[GMode->MenuPlayers[0].ZoogiIndex % Roster.Num()].Id)) : nullptr;
	};

	if (P == EZoogiClashPopup::Profile)
	{
		// ---- left: avatar + level hex, name, #tag (copy), trophy pill, XP bar, No Clan (inert), Battle Log
		CAt(CBox(ZCGold, ZCDark, 6.f, 95.f), 50.f, 132.f, 190.f, 190.f);
		CImg(Portrait(), 63.f, 145.f, 164.f, 164.f, 82.f, true);
		CImg(ClashArt(TEXT("level")), 176.f, 252.f, 78.f, 78.f);
		CText(FString::FromInt(D.Level), 176.f, 252.f, 78.f, 74.f, 32.f, FLinearColor::White);
		CText(D.GuestName, 270.f, 136.f, PW - 330.f, 56.f, 38.f, FLinearColor::White, 0);
		CText(TEXT("#") + D.Tag, 270.f, 196.f, 190.f, 44.f, 28.f, ZCSoft, 0, 3.f);
		{
			UCanvasPanel* G = CGroupBegin();
			CPlate(0.f, 0.f, 56.f, 50.f, ZCNavyBtn, 12.f, 3.f, false);
			CImg(ClashArt(TEXT("copy")), 9.f, 7.f, 38.f, 36.f);
			CGroupEnd();
			const FString Tag = TEXT("#") + D.Tag;
			CAt(CButton(G, [this, Tag]() { FPlatformApplicationMisc::ClipboardCopy(*Tag); ClashCopiedT = Time; UE_LOG(LogZoogi, Log, TEXT("CLASH tag copied: %s"), *Tag); bDirty = true; }), 466.f, 193.f, 56.f, 50.f);
		}
		if (Time - ClashCopiedT < 1.5f) CText(TEXT("Copied!"), 532.f, 196.f, 150.f, 44.f, 26.f, ZCBoughtCheck, 0, 3.f);
		CPlate(270.f, 252.f, 220.f, 56.f, ZCPill.CopyWithNewOpacity(0.85f), 28.f, 4.f, false);
		CImg(ClashArt(TEXT("trophy")), 258.f, 240.f, 76.f, 76.f);
		CText(FText::AsNumber(D.Trophies).ToString(), 330.f, 252.f, 150.f, 56.f, 30.f, ZCGold, 1);
		const int32 Need = UZoogiFrontProfileSubsystem::XPForLevel(D.Level);
		CBar(50.f, 350.f, PW - 100.f, 46.f, Need > 0 ? float(D.XP) / Need : 0.f, ZCCyan, FString::Printf(TEXT("Level %d  -  %d/%d XP"), D.Level, D.XP, Need), 22.f);
		CPlate(50.f, 420.f, (PW - 120.f) * 0.5f, 92.f, ZCNavyDeep.CopyWithNewOpacity(0.7f), 16.f, 4.f, false);
		CImg(ClashArt(TEXT("shield")), 62.f, 434.f, 64.f, 64.f);
		CText(TEXT("No Clan"), 140.f, 420.f, 300.f, 92.f, 28.f, ZCMuted, 0, 3.f);
		CChunky(TEXT("BATTLE LOG"), ZCGoldBtn, [this]() { OpenClashPopup(EZoogiClashPopup::BattleLog); }, PW * 0.5f + 10.f, 414.f, (PW - 120.f) * 0.5f, 104.f, 38.f, ClashArt(TEXT("swords")), true);
		CText(TEXT("Local guest profile (this device)."), 50.f, PH - 118.f, PW - 100.f, 36.f, 19.f, ZCSoft, 1, 2.f);

		// ---- right: Battle Stats (2 columns x 4) + Cards
		// M4 portrait: stats + cards stacked under the header block
		const float RX = 50.f, RW = PW - 100.f, TW = (RW - 18.f) * 0.5f, TH = 80.f, SY = 600.f;
		CText(TEXT("BATTLE STATS"), RX, SY - 54.f, RW, 46.f, 30.f, FLinearColor::White, 0);
		const int32 Rate = D.Battles > 0 ? FMath::RoundToInt(100.f * D.Wins / D.Battles) : 0;
		struct FS { const TCHAR* Icon; const TCHAR* Label; FString V; FLinearColor C; };
		const FS Stats[] = {
			{ TEXT("crown"), TEXT("Wins"), FString::FromInt(D.Wins), ZCGold },
			{ TEXT("crown"), TEXT("Three Crown Wins"), FString::FromInt(D.ThreeCrownWins), ZCGold },
			{ TEXT("trophy"), TEXT("Highest Trophies"), FString::FromInt(FMath::Max(D.BestTrophies, D.Trophies)), ZCGold },
			{ TEXT("swords"), TEXT("Battles"), FString::FromInt(D.Battles), FLinearColor::White },
			{ TEXT("shield"), TEXT("Losses"), FString::FromInt(D.Losses), ZCHex(TEXT("FF8A80")) },
			{ TEXT("star"), TEXT("Win Rate"), FString::Printf(TEXT("%d%%"), Rate), ZCCyan },
			{ TEXT("star"), TEXT("Win Streak"), FString::FromInt(D.WinStreak), FLinearColor::White },
			{ TEXT("star"), TEXT("Best Streak"), FString::FromInt(D.BestStreak), FLinearColor::White } };
		for (int32 i = 0; i < 8; ++i)
			Tile(RX + (i % 2) * (TW + 18.f), SY + (i / 2) * (TH + 14.f), TW, TH, ClashArt(Stats[i].Icon), Stats[i].Label, Stats[i].V, Stats[i].C);
		const float CY = SY + 4.f * (TH + 14.f) + 16.f;
		CText(TEXT("CARDS"), RX, CY, RW, 46.f, 30.f, FLinearColor::White, 0);
		const int32 Found = FP->CardsFound(), Total = UZoogiFrontProfileSubsystem::CardDefs().Num();
		CBar(RX, CY + 54.f, RW, 52.f, Total > 0 ? float(Found) / Total : 0.f, ZCGreen, FString::Printf(TEXT("Cards Found  %d/%d"), Found, Total), 24.f);
		float Avg = 0.f; int32 Max = 0; FP->CardLevelStats(Avg, Max);
		Tile(RX, CY + 124.f, TW, TH, ClashArt(TEXT("cards")), TEXT("Average Level"), FString::Printf(TEXT("%.1f"), Avg), FLinearColor::White);
		Tile(RX + TW + 18.f, CY + 124.f, TW, TH, ClashArt(TEXT("cards")), TEXT("Highest Level"), FString::FromInt(Max), FLinearColor::White);
		CText(TEXT("Three Crown Wins: wins with the opponent held to 0 points."), RX, PH - 78.f, RW, 36.f, 18.f, ZCSoft, 1, 2.f);
		return true;
	}
	if (P == EZoogiClashPopup::BattleLog)
	{
		const TArray<FZoogiFrontBattle>& Log = D.BattleLog;
		if (Log.Num() == 0)
		{
			CImg(ClashArt(TEXT("swords")), PW * 0.5f - 90.f, 220.f, 180.f, 180.f);
			CText(TEXT("No battles yet."), 0.f, 420.f, PW, 60.f, 40.f, FLinearColor::White);
			CText(TEXT("Your real matches show up here with a replay."), 0.f, 480.f, PW, 40.f, 24.f, ZCSoft, 1, 3.f);
			return true;
		}
		if (UScrollBox* Old = ClashScrollW.Get()) if (ClashScrollKey == 13 && ClashPendingScroll < 0.f) ClashScrollOffset = Old->GetScrollOffset();
		ClashScrollKey = 13;
		const float RowH = 150.f, ListW = PW - 80.f; // M4 portrait: two-line rows
		UCanvasPanel* G = CGroupBegin();
		const int64 Now = UZoogiFrontProfileSubsystem::NowUtc();
		for (int32 i = 0; i < Log.Num(); ++i)
		{
			const FZoogiFrontBattle& B = Log[i];
			const float Y = i * (RowH + 12.f);
			const FLinearColor RC = B.Outcome == 0 ? ZCHex(TEXT("2E9A3C")) : B.Outcome == 2 ? ZCHex(TEXT("6E6A62")) : ZCHex(TEXT("C2362F"));
			CPlate(0.f, Y, ListW, RowH, ZCNavyDeep.CopyWithNewOpacity(0.85f), 18.f, 4.f, false);
			CPlate(12.f, Y + 12.f, 150.f, RowH - 24.f, RC, 14.f, 4.f, true);
			CText(B.Outcome == 0 ? TEXT("WIN") : B.Outcome == 1 ? TEXT("LOSS") : B.Outcome == 2 ? TEXT("DRAW") : TEXT("LEFT"), 12.f, Y + 12.f, 150.f, RowH - 24.f, 32.f, FLinearColor::White);
			const float NW = ListW - 186.f - 232.f;
			CText(TEXT("vs ") + B.Opponent, 186.f, Y + 10.f, NW, 46.f, 28.f, FLinearColor::White, 0);
			CText(ZCAgo(B.Utc, Now) + TEXT("  -  ") + ZoogiUtil::ArenaName((EZoogiArena)B.Arena), 186.f, Y + 54.f, NW, 36.f, 20.f, ZCSoft, 0, 3.f);
			if (B.bDev) { CPlate(40.f, Y + RowH - 30.f, 94.f, 30.f, ZCYellow, 10.f, 3.f, false); CText(TEXT("DEV"), 40.f, Y + RowH - 30.f, 94.f, 30.f, 18.f, ZCDark, 1, 0.f); }
			const float L2 = Y + 94.f;
			CImg(ClashArt(TEXT("trophy")), 186.f, L2, 46.f, 46.f);
			CText(FString::Printf(TEXT("%+d"), B.Trophies), 236.f, L2, 100.f, 46.f, 28.f, B.Trophies >= 0 ? ZCGold : ZCHex(TEXT("FF8A80")), 0);
			CImg(ClashArt(TEXT("crown")), 340.f, L2, 46.f, 46.f);
			CText(FString::Printf(TEXT("%d - %d"), B.MyScore, B.OppScore), 392.f, L2, 110.f, 46.f, 28.f, FLinearColor::White, 0);
			CText(ZCClock((float)B.Duration), 510.f, L2, 100.f, 46.f, 26.f, ZCSoft, 0, 3.f);
			const bool bHas = FP->HasReplay(B.ReplayId);
			if (bHas) CChunky(TEXT("Replay"), ZCBlue, [this, i]() { ClashOpenReplay(i); }, ListW - 214.f, Y + 30.f, 196.f, RowH - 60.f, 28.f, ClashArt(TEXT("play")));
			else CText(TEXT("No replay"), ListW - 214.f, Y, 196.f, RowH, 22.f, ZCMuted, 1, 3.f);
		}
		CGroupEnd();
		const float ContentH = Log.Num() * (RowH + 12.f);
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SB->SetWidthOverride(ListW); SB->SetHeightOverride(ContentH); SB->AddChild(G);
		UScrollBox* Sc = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		Sc->SetAnimateWheelScrolling(true);
		Sc->AddChild(SB);
		CAt(Sc, 40.f, 132.f, ListW + 20.f, PH - 132.f - 40.f);
		ClashScrollW = Sc;
		if (ClashPendingScroll < 0.f) ClashPendingScroll = ClashScrollOffset;
		return true;
	}
	if (P == EZoogiClashPopup::Settings)
	{
		ClashSlider[0].Reset(); ClashSlider[1].Reset(); ClashSlider[2].Reset();
		// M4 portrait: one column; Language / Account / More settings sit under Sound (RY0 shift)
		const float LX = 50.f, LW = PW - 100.f, RX = LX, RW = LW, RY0 = 524.f;
		auto Section = [this](const TCHAR* T, float X, float Y, float Wd) { CText(T, X, Y, Wd, 44.f, 28.f, FLinearColor::White, 0); };
		// ---- Profile: name field + #tag + blue Change (pencil)
		Section(TEXT("PROFILE"), LX, 126.f, LW);
		CAt(CBox(FLinearColor::White, ZCDark, 4.f, 14.f), LX, 176.f, LW - 230.f, 76.f);
		CText(D.GuestName, LX + 20.f, 176.f, LW - 270.f, 46.f, 28.f, ZCNavyDeep, 0, 0.f);
		CText(TEXT("#") + D.Tag, LX + 20.f, 218.f, LW - 270.f, 30.f, 19.f, ZCHex(TEXT("8A8478")), 0, 0.f);
		CChunky(TEXT("Change"), ZCBlue, [this]() { ClashNickDraft.Reset(); ClashNickErr.Reset(); OpenClashPopup(EZoogiClashPopup::ChangeNickname); }, LX + LW - 214.f, 172.f, 214.f, 84.f, 28.f, ClashArt(TEXT("pencil")));
		// ---- Sound: Master (gold), Sound Effects (blue), Music (green); drag the white knob
		Section(TEXT("SOUND"), LX, 280.f, LW);
		const TCHAR* Names[] = { TEXT("Master"), TEXT("Sound Effects"), TEXT("Music") };
		const FLinearColor Fills[] = { ZCMaster, ZCSfxBlue, ZCMusicGreen };
		const float TrackX = LX + 230.f, TrackW = LW - 230.f - 100.f;
		ClashSliderFillW = TrackW - 8.f;
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = 336.f + i * 96.f;
			const int32 Pct = FMath::Clamp(GetLocalSetting(ZPVolKeys[i], 50), 0, 100);
			CText(Names[i], LX, Y, 220.f, 60.f, 24.f, FLinearColor::White, 0, 3.f);
			CAt(CBox(ZCNavyDeep, ZCDark, 4.f, 18.f), TrackX, Y + 12.f, TrackW, 36.f);
			UBorder* FB = CBox(Fills[i], FLinearColor::Transparent, 0.f, 14.f);
			FB->SetVisibility(ESlateVisibility::HitTestInvisible);
			CAt(FB, TrackX + 4.f, Y + 16.f, FMath::Max(1.f, ClashSliderFillW * Pct / 100.f), 28.f); // width = value (canvas slot size)
			ClashSliderFill[i] = FB;
			USlider* S = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			FSliderStyle St;
			FSlateBrush None; None.DrawAs = ESlateBrushDrawType::NoDrawType;
			St.SetNormalBarImage(None); St.SetHoveredBarImage(None); St.SetDisabledBarImage(None);
			FSlateBrush Knob = ZCImgBrush(ClashArt(TEXT("knob")), 54.f, 54.f, 0.f, false);
			St.SetNormalThumbImage(Knob); St.SetHoveredThumbImage(Knob); St.SetDisabledThumbImage(Knob);
			St.SetBarThickness(0.f);
			S->SetWidgetStyle(St);
			S->SetValue(Pct / 100.f);
			S->SetStepSize(0.01f);
			CAt(S, TrackX - 22.f, Y + 2.f, TrackW + 44.f, 56.f);
			ClashSlider[i] = S;
			ClashSliderLast[i] = Pct / 100.f;
			UWidget* PctW = CText(FString::Printf(TEXT("%d%%"), Pct), TrackX + TrackW + 12.f, Y, 88.f, 60.f, 26.f, FLinearColor::White, 0, 3.f);
			if (UBorder* B = Cast<UBorder>(PctW)) ClashSliderPct[i] = Cast<UTextBlock>(B->GetContent());
		}
		// ---- Language: 5 buttons, selected = gold with a green check badge (saved; applies instantly)
		Section(TEXT("LANGUAGE"), RX, RY0 + 126.f, RW);
		const int32 Lang = FMath::Clamp(GetLocalSetting(TEXT("ClashLanguage"), 0), 0, 4);
		const float LBW = (RW - 24.f) / 3.f;
		for (int32 i = 0; i < 5; ++i)
		{
			const float X = RX + (i % 3) * (LBW + 12.f), Y = RY0 + 176.f + (i / 3) * 100.f;
			const bool bSel = i == Lang;
			UCanvasPanel* G = CGroupBegin();
			CChunkyVisual(nullptr, bSel ? ZCGoldBtn : ZCNavyBtn, 0.f, 0.f, LBW, 84.f);
			CTextIntl(ZPLangNames[i], 0.f, 0.f, LBW, 74.f, 26.f, FLinearColor::White);
			CGroupEnd();
			CAt(CButton(G, [this, i]()
			{
				SetLocalSetting(TEXT("ClashLanguage"), i);
				UE_LOG(LogZoogi, Log, TEXT("CLASH language -> %s (saved; no translated strings exist in the project yet)"), ZPLangCodes[i]);
			}), X, Y, LBW, 84.f);
			if (bSel) CImg(ClashArt(TEXT("check")), X + LBW - 34.f, Y - 14.f, 46.f, 46.f);
		}
		if (Lang != 0) CText(TEXT("Saved. Zoogi Roll has no translated text yet, so the menus stay in English."), RX, RY0 + 380.f, RW, 34.f, 18.f, ZCYellow, 0, 2.f);
		// ---- Account (guest): Link Email / Log Out need an account backend
		Section(TEXT("ACCOUNT"), RX, RY0 + 430.f, RW);
		CText(TEXT("Guest account. Link an email to keep your progress safe."), RX, RY0 + 480.f, RW, 60.f, 22.f, FLinearColor::White, 0, 3.f, true);
		CChunky(TEXT("Link Email"), ZCLinkBlue, [this]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("LINK EMAIL"), TEXT("Linking an email needs the account backend, which is not connected yet.\nYour progress is saved on this device.")); },
			RX, RY0 + 566.f, (RW - 20.f) * 0.5f, 92.f, 30.f);
		CChunky(TEXT("Log Out"), ZCLogoutRed, [this]() { OpenClashPopup(EZoogiClashPopup::Info, TEXT("LOG OUT"), TEXT("Guest accounts have no login, so there is nothing to log out of yet.\nAccounts need the backend. Nothing was changed.")); },
			RX + (RW + 20.f) * 0.5f, RY0 + 566.f, (RW - 20.f) * 0.5f, 92.f, 30.f);
		CChunky(TEXT("More settings"), ZCNavyBtn, [this]() { ClashPopup = EZoogiClashPopup::None; Push(EZoogiScreen::Settings); }, RX, RY0 + 682.f, (RW - 20.f) * 0.5f, 76.f, 24.f);
		CText(TEXT("Version ") + ZPVersion(), 0.f, PH - 70.f, PW, 40.f, 22.f, ZCSoft, 1, 3.f);
		return true;
	}
	if (P == EZoogiClashPopup::ChangeNickname)
	{
		CText(TEXT("Pick a new name (3-16 letters, numbers, spaces, - or _)."), 40.f, 130.f, PW - 80.f, 40.f, 22.f, ZCSoft, 1, 3.f);
		UEditableTextBox* E = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		E->SetText(FText::FromString(ClashNickDraft.IsEmpty() ? D.GuestName : ClashNickDraft));
		E->SetHintText(FText::FromString(TEXT("New name")));
		FEditableTextBoxStyle St = E->WidgetStyle;
		St.SetFont(ZCFont(32.f, 0.f));
		St.SetForegroundColor(FSlateColor(ZCNavyDeep));
		const FSlateBrush Field = ZoogiUIStyle::RoundBrush(FLinearColor::White, ZCDark, 4.f, 14.f);
		St.SetBackgroundImageNormal(Field); St.SetBackgroundImageHovered(Field); St.SetBackgroundImageFocused(Field); St.SetBackgroundImageReadOnly(Field);
		St.SetPadding(FMargin(20.f, 12.f));
		E->WidgetStyle = St;
		CAt(E, 60.f, 186.f, PW - 120.f, 86.f);
		ClashNickBox = E;
		const bool bCan = D.Gold >= UZoogiFrontProfileSubsystem::NicknameCost;
		if (!ClashNickErr.IsEmpty()) CText(ClashNickErr, 40.f, 286.f, PW - 80.f, 40.f, 22.f, ZCPriceRed, 1, 3.f);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(TEXT("Change"), 34.f, FLinearColor::White, 4.f))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f)); }
		UImage* I = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		I->SetBrush(ZCImgBrush(Art(TEXT("icon_coin")), 46.f, 46.f, 0.f, false));
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(I)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
		if (UHorizontalBoxSlot* S = H->AddChildToHorizontalBox(CTxt(FString::FromInt(UZoogiFrontProfileSubsystem::NicknameCost), 34.f, bCan ? ZCGold : ZCPriceRed, 4.f))) S->SetVerticalAlignment(VAlign_Center);
		H->SetVisibility(ESlateVisibility::HitTestInvisible);
		CChunkyW(H, ZCGreen, [this]()
		{
			UZoogiFrontProfileSubsystem* F = UZoogiFrontProfileSubsystem::Get(this);
			if (!F) return;
			const FString Want = ClashNickBox.IsValid() ? ClashNickBox->GetText().ToString() : ClashNickDraft;
			FString Why;
			if (F->ChangeNickname(Want, Why)) { ClashNickErr.Reset(); ClashNickDraft.Reset(); OpenClashPopup(EZoogiClashPopup::Settings); }
			else { ClashNickErr = Why; ClashNickDraft = Want; bDirty = true; }
		}, PW * 0.5f - 200.f, PH - 160.f, 400.f, 112.f, true);
		return true;
	}
	return false;
}
