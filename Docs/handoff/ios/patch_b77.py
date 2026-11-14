# b77 (Claude): wolf icon (replaces CONTROL), CONTROL / DRIVE gone (AUTO = DRIVE force), no pizza on Arabian, spire hard fade,
# all anime at the top (cut-ins + crash / skill-summon clips), P5 menu taps no longer reach the game
import os
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style')
def ed(rel, pairs, inc=None, after=None):
    p = os.path.join(ROOT, rel)
    raw = open(p, 'rb').read(); bom = raw[:3] == b'\xef\xbb\xbf'; s = raw.decode('utf-8-sig'); nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in pairs:
        a = a.replace('\r\n', '\n').replace('\n', nl); b = b.replace('\r\n', '\n').replace('\n', nl)
        assert s.count(a) == 1, (rel, a[:90], s.count(a)); s = s.replace(a, b)
    if inc and inc not in s:
        i = s.index(after); j = s.index(nl, i) + len(nl); s = s[:j] + inc + nl + s[j:]
    open(p, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='').write(s)
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'

# ---------------- skills: wolf state
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API bool EffectBusy();\n',
  '''	ZOOGIROLL_API bool EffectBusy();
	/** b77 (Evan: "when Wolf Pack is selected you get a separate wolf icon that replaces the CONTROL button and activates the
	 *  effect during the roll"): casting wolf_pack arms the WOLF button for that player; tapping it while their shot rolls
	 *  summons the old Wolf Pack (howl, wolves). -ZoogiWolfSkillRedirects = the b66 skill (3 free redirects). */
	ZOOGIROLL_API void ArmWolf(AZoogiGameMode* G, int32 Player);
	ZOOGIROLL_API bool WolfArmed(const AZoogiGameMode* G, int32 Player);
	/** the WOLF slot shows for the human on the turn ball (aiming: in the mode bar, rolling: same spot) */
	ZOOGIROLL_API bool WolfSlot(const AZoogiGameMode* G);
	/** the turn ball is rolling and its player has the wolf armed */
	ZOOGIROLL_API bool WolfReady(const AZoogiGameMode* G);
	ZOOGIROLL_API bool WolfGo(AZoogiGameMode* G);
	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);
'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''		if (Id == FName(TEXT("wolf_pack")))
		{
			// the Wolf Pack intro''',
  '''		static const bool bWolfRedirects = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSkillRedirects"));
		if (Id == FName(TEXT("wolf_pack")) && !bWolfRedirects)
		{
			// b77 (Evan): the skill arms the WOLF button (in CONTROL's old slot); tapping it during the roll summons the pack
			ArmWolf(G, Pi);
			AZoogiFX::Spawn(W, MeFloor, FLinearColor(0.75f, 0.45f, 1.f), 300.f, 0.6f, true, 8.f);
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 0.8f);
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("WOLF PACK READY!"), Caster->bCPU ? TEXT("THE PACK IS COMING") : TEXT("TAP THE WOLF WHILE YOU ROLL"), FLinearColor(0.75f, 0.45f, 1.f), 1.6f, 1.f);
		}
		else if (Id == FName(TEXT("wolf_pack")))
		{
			// the Wolf Pack intro'''),
 ('''	void Tick(AZoogiGameMode* G, float RealDt)
	{
''', '''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		WolfTick(G);
'''),
 ('	bool AutoCast(AZoogiGameMode* G, FName Id)\n',
  '''	// ---- b77 wolf button
	static TWeakObjectPtr<const AZoogiGameMode> GWolfG;
	static TSet<int32> GWolfArmed;
	void ArmWolf(AZoogiGameMode* G, int32 Player)
	{
		if (GWolfG.Get() != G) { GWolfArmed.Reset(); GWolfG = G; }
		GWolfArmed.Add(Player);
		UE_LOG(LogZoogi, Log, TEXT("WOLF button armed for P%d"), Player + 1);
	}
	bool WolfArmed(const AZoogiGameMode* G, int32 Player) { return G && GWolfG.Get() == G && GWolfArmed.Contains(Player); }
	bool WolfSlot(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (!B || B->bCPU || B->bOut || !WolfArmed(G, B->PlayerIndex)) return false;
		return (G->Phase == EZoogiPhase::Aiming && G->Stage == EZoogiStage::Battle) || G->Phase == EZoogiPhase::Rolling;
	}
	bool WolfReady(const AZoogiGameMode* G)
	{
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return B && !B->bOut && G->Phase == EZoogiPhase::Rolling && WolfArmed(G, B->PlayerIndex);
	}
	bool WolfGo(AZoogiGameMode* G)
	{
		if (!WolfReady(G)) return false;
		AZoogiBall* B = G->GetActiveBall();
		GWolfArmed.Remove(B->PlayerIndex);
		UE_LOG(LogZoogi, Log, TEXT("WOLF button: P%d summons the pack mid-roll"), B->PlayerIndex + 1);
		G->FireWolfSkill(B);
		return true;
	}
	void WolfTick(AZoogiGameMode* G)
	{
		// the CPU taps its wolf a moment into its own roll, like a player would
		const AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (B && B->bCPU && WolfReady(G) && G->PhaseTime > 0.75f) WolfGo(G);
	}

	bool AutoCast(AZoogiGameMode* G, FName Id)
'''),
])
ed(H + 'ZoogiGameMode.h', [
 ('	void MenuAction(FName Action, int32 Index);\n	void StartMatch();\n',
  '	void MenuAction(FName Action, int32 Index);\n	/** b77: the WOLF button (Wolf Pack skill) - the old Wolf Pack superpower for this ball, no charge spent */\n	void FireWolfSkill(AZoogiBall* Ball);\n	void StartMatch();\n'),
])
ed(P + 'ZoogiPowers.cpp', [
 ('''void AZoogiGameMode::FireInstantPower(AZoogiBall* Ball)
{
	const EZoogiPower P = Ball->Def.PowerKind;
	const FLinearColor C = PowerColor(P);
	const FVector L = Ball->BallLocation();
	Ball->Charges = FMath::Max(0, Ball->Charges - 1);
	ZoogiProgressHooks::PowerUsed().Broadcast(Ball->PlayerIndex); // read-only (progression award hooks)
	bPowerUsed = true;
	AddPopup(Ball->Def.Ability.ToUpper() + TEXT("!"), L + FVector(0, 0, 140.f), C);''',
  '''namespace { bool GWolfSkillFire = false; } // b77: FireWolfSkill -> the Wolf Pack branch for any Zoogi, no charge spent

void AZoogiGameMode::FireWolfSkill(AZoogiBall* Ball)
{
	if (!Ball) return;
	GWolfSkillFire = true;
	FireInstantPower(Ball);
	GWolfSkillFire = false;
}

void AZoogiGameMode::FireInstantPower(AZoogiBall* Ball)
{
	const EZoogiPower P = GWolfSkillFire ? EZoogiPower::WolfPack : Ball->Def.PowerKind;
	const FLinearColor C = PowerColor(P);
	const FVector L = Ball->BallLocation();
	if (!GWolfSkillFire)
	{
		Ball->Charges = FMath::Max(0, Ball->Charges - 1);
		ZoogiProgressHooks::PowerUsed().Broadcast(Ball->PlayerIndex); // read-only (progression award hooks)
		bPowerUsed = true;
	}
	AddPopup(GWolfSkillFire ? FString(TEXT("WOLF PACK!")) : Ball->Def.Ability.ToUpper() + TEXT("!"), L + FVector(0, 0, 140.f), C);'''),
 ('		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSuperpower"))) { UE_LOG',
  '		if (!GWolfSkillFire && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSuperpower"))) { UE_LOG'),
])

# ---------------- mode bar: AUTO / [WOLF] / LAUNCH / ARC
ed(P + 'ZoogiAwakenWidget.cpp', [
 ('''		const float PW = 5.f * BW + 4.f * Gap + 16.f, PH = BH + 16.f; // Oct 30: + ARC''',
  '''		// b77 (Evan: "remove the CONTROL button, AUTO now gives the same force as DRIVE, remove the DRIVE button"): AUTO / LAUNCH / ARC,
		// with the WOLF slot (drawn by ZoogiClassicPad, the same spot while rolling) after AUTO while Wolf Pack is armed. -ZoogiOldModes = all four
		static const bool bOldModes = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes"));
		const bool bWolfSlot = !bOldModes && ZoogiSkills::WolfSlot(G);
		TArray<int32> Steps; if (bOldModes) Steps = { 0, 1, 2, 3 }; else Steps = { 0, 3 };
		const int32 NSlots = Steps.Num() + 1 + (bWolfSlot ? 1 : 0);
		const float PW = NSlots * BW + (NSlots - 1) * Gap + 16.f, PH = BH + 16.f; // Oct 30: + ARC'''),
 ('''		for (int32 i = 0; i < 4; ++i)
		{
			MakePlateButton(P, ZoogiCards::ForceLabel(i, true), FVector2D(X, 8.f), FVector2D(BW, BH), TEXT("AimPreset"), i, FLinearColor::FromSRGBColor(Cols[i]), G->AimPowerStep == i);
			X += BW + Gap;
		}''',
  '''		for (int32 k = 0; k < Steps.Num(); ++k)
		{
			const int32 i = Steps[k];
			MakePlateButton(P, ZoogiCards::ForceLabel(i, true), FVector2D(X, 8.f), FVector2D(BW, BH), TEXT("AimPreset"), i, FLinearColor::FromSRGBColor(Cols[i]), G->AimPowerStep == i);
			X += BW + Gap;
			if (k == 0 && bWolfSlot) X += BW + Gap; // the WOLF button sits here (Slate, ZoogiClassicPad)
		}'''),
 ('''	return FString::Printf(TEXT("%d|%d|%d|%d"), G->AimPowerStep, G->HasAim() ? 1 : 0, IsPortraitView() ? 1 : 0, G->bArcShot ? 1 : 0) + (G->ActiveInOuterRing() ? TEXT("|O") : TEXT(""));''',
  '''	return FString::Printf(TEXT("%d|%d|%d|%d"), G->AimPowerStep, G->HasAim() ? 1 : 0, IsPortraitView() ? 1 : 0, G->bArcShot ? 1 : 0) + (G->ActiveInOuterRing() ? TEXT("|O") : TEXT("")) + (ZoogiSkills::WolfSlot(G) ? TEXT("|W") : TEXT(""));'''),
], inc='#include "ZoogiSkills.h"', after='#include "ZoogiAwakenWidget.h"')

# AUTO = DRIVE force (human)
ed(P + 'ZoogiTargeting.cpp', [
 ('''	static const float Steps[4] = { -1.f, 0.42f, 0.66f, 1.f };
	OutFraction = AimPowerStep > 0 ? Steps[FMath::Clamp(AimPowerStep, 1, 3)] : BaseFraction;''',
  '''	static const float Steps[4] = { -1.f, 0.42f, 0.66f, 1.f };
	int32 Step = AimPowerStep;
	static const bool bOldModes = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes"));
	if (Step == 0 && !bOldModes) if (const AZoogiBall* Hb = GetActiveBall()) if (!Hb->bCPU) Step = 2; // b77 (Evan): AUTO now gives DRIVE's force
	OutFraction = Step > 0 ? Steps[FMath::Clamp(Step, 1, 3)] : BaseFraction;'''),
 ('	if ((AimPowerStep > 0 || HumanDragFrac >= 0.f) && !bFreeAim && TargetLock.IsValid())',
  '	if ((AimPowerStep > 0 || HumanDragFrac >= 0.f || (GetActiveBall() && !GetActiveBall()->bCPU)) && !bFreeAim && TargetLock.IsValid())'),
])

# ---------------- WOLF button (Slate, CONTROL's old slot in the bar; stays there while rolling)
ed(P + 'ZoogiClassicPad.cpp', [
 ('''	// root: places the two buttons from ZoogiRoyaleUI::Layout every frame; only its buttons take touches''',
  '''	// b77 (Evan): the WOLF button - Wolf Pack skill armed -> sits in CONTROL's old slot of the mode bar, tap while rolling = the pack
	UTexture2D* WolfIcon()
	{
		static UTexture2D* T = nullptr; static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			const FString P = FPaths::ProjectContentDir() / TEXT("Data/CardArt/Icons/wolf_pack.png");
			if (FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
			if (T) T->AddToRoot();
		}
		return T;
	}
	class SZoogiWolfButton : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiWolfButton) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		mutable bool bHeld = false;
		double FlashAt = -10.0;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.f, 32.f); }
		virtual bool ComputeVolatility() const override { return true; }
		FReply Press()
		{
			bHeld = true; FlashAt = FPlatformTime::Seconds();
			if (AZoogiGameMode* G = PadGM())
			{
				if (ZoogiSkills::WolfGo(G)) ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 0.7f);
				else
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f);
					if (const AZoogiBall* B = G->GetActiveBall()) G->AddPopup(TEXT("LAUNCH, THEN TAP THE WOLF"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.55f, 1.f));
				}
			}
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		FReply Release() { bHeld = false; return FReply::Handled().ReleaseMouseCapture(); }
		virtual FReply OnTouchStarted(const FGeometry&, const FPointerEvent&) override { return Press(); }
		virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return Release(); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& E) override { return (!E.IsTouchEvent() && E.GetEffectingButton() == EKeys::LeftMouseButton) ? Press() : FReply::Unhandled(); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& E) override { return !E.IsTouchEvent() ? Release() : FReply::Unhandled(); }
		virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { bHeld = false; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			AZoogiGameMode* G = PadGM();
			const bool bReady = ZoogiSkills::WolfReady(G);
			const double Now = FPlatformTime::Seconds();
			const float Pulse = 0.5f + 0.5f * FMath::Sin(float(Now) * (bReady ? 9.f : 3.f));
			const float Flash = FMath::Clamp(1.f - float(Now - FlashAt) / 0.25f, 0.f, 1.f);
			const FVector2D Sz = Geo.GetLocalSize() * (bHeld ? 0.94f : 1.f) * (1.f + (bReady ? 0.04f * Pulse : 0.f) + 0.05f * Flash);
			const FVector2D O = (Geo.GetLocalSize() - Sz) * 0.5f;
			const FLinearColor Dark = FLinearColor::FromSRGBColor(FColor(14, 12, 10));
			const FLinearColor Face = FLinearColor::FromSRGBColor(bReady ? FColor(140, 70, 230) : FColor(86, 52, 128));
			const FLinearColor Glow = FLinearColor(0.85f, 0.6f, 1.f, bReady ? 0.35f + 0.45f * Pulse : 0.15f);
			auto Box = [&](const FVector2D& P0, const FVector2D& S, const FLinearColor& Fill, const FLinearColor& Rim, float RimW, float Rad, int32 L)
			{
				const FSlateRoundedBoxBrush B(Fill, Rad, Rim, RimW);
				FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(S, FSlateLayoutTransform(1.f, P0)), &B, ESlateDrawEffect::None, Fill);
			};
			const float Rad = Sz.Y * 0.22f;
			Box(O - FVector2D(4.f, 4.f), Sz + FVector2D(8.f, 8.f), FLinearColor::Transparent, Glow, 4.f, Rad + 4.f, Layer);
			Box(O + FVector2D(0.f, Sz.Y * 0.06f), Sz, FLinearColor(0.f, 0.f, 0.f, 0.35f), FLinearColor::Transparent, 0.f, Rad, Layer);
			Box(O, Sz, Face, Dark, FMath::Max(3.f, Sz.Y * 0.08f), Rad, Layer + 1);
			// the wolf art (Content/Data/CardArt/Icons/wolf_pack.png, 300 x 250) on the left, WOLF on the right
			const float IH = Sz.Y * 0.78f, IW = IH * 1.2f;
			const FVector2D IP = O + FVector2D(Sz.Y * 0.14f, (Sz.Y - IH) * 0.5f);
			if (UTexture2D* T = WolfIcon())
			{
				FSlateBrush IB; IB.SetResourceObject(T); IB.ImageSize = FVector2D(300.f, 250.f);
				const FSlateRoundedBoxBrush Frame(FLinearColor::White, IH * 0.14f, FLinearColor::White, 3.f);
				FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(FVector2D(IW + 6.f, IH + 6.f), FSlateLayoutTransform(1.f, IP - FVector2D(3.f, 3.f))), &Frame, ESlateDrawEffect::None, FLinearColor::White);
				FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(IW, IH), FSlateLayoutTransform(1.f, IP)), &IB, ESlateDrawEffect::None, FLinearColor::White.CopyWithNewOpacity(bReady ? 1.f : 0.75f));
			}
			const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const float FS = FMath::Max(9.f, Sz.Y * 0.36f);
			const FSlateFontInfo F = ZoogiModeSel::FontInfo(FS, FMath::Max(1.f, FS * 0.14f));
			const FString S = bReady ? TEXT("WOLF!") : TEXT("WOLF");
			const FVector2D TS = FM->Measure(S, F);
			const float TX = IP.X + IW + (O.X + Sz.X - (IP.X + IW)) * 0.5f - TS.X * 0.5f;
			FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, FVector2D(TX, O.Y + (Sz.Y - TS.Y) * 0.5f))), S, F, ESlateDrawEffect::None, FLinearColor::White);
			return Layer + 5;
		}
	};

	// root: places the two buttons from ZoogiRoyaleUI::Layout every frame; only its buttons take touches'''),
 ('''		void Construct(const FArguments&)
		{
			auto MakeBtn = [this](int32 Which) -> TSharedRef<SWidget>''',
  '''		// b77: the WOLF slot = slot 2 of the classic mode bar (ZoogiAwakenWidget::BuildAim: AUTO / WOLF / LAUNCH / ARC), in the same
		// DPI-scaled units as the bar (the bar's panel: landscape bottom-centre, 22 up; portrait bottom-left, 20 in / 40 up)
		FMargin WolfPad(float& OutW, float& OutH) const
		{
			OutW = OutH = 0.f;
			if (!bHaveGeo) return FMargin(0.f);
			const FVector2D L = LastGeo.GetLocalSize();
			const bool bPortrait = L.Y > L.X;
			const float BW = bPortrait ? 150.f : 168.f, BH = bPortrait ? 70.f : 74.f, Gap = bPortrait ? 12.f : 16.f;
			const int32 NSlots = 4;
			const float PW = NSlots * BW + (NSlots - 1) * Gap + 16.f, PH = BH + 16.f;
			const float Left = bPortrait ? 20.f : (L.X - PW) * 0.5f, Top = L.Y - (bPortrait ? 40.f : 22.f) - PH;
			OutW = BW; OutH = BH;
			return FMargin(Left + 8.f + BW + Gap, Top + 8.f, 0.f, 0.f);
		}
		void Construct(const FArguments&)
		{
			auto MakeBtn = [this](int32 Which) -> TSharedRef<SWidget>'''),
 ('''				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return Pad(1, W, H); }))[MakeBtn(1)]
			];''',
  '''				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return Pad(1, W, H); }))[MakeBtn(1)]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return WolfPad(W, H); }))
				[
					SNew(SBox)
					.Visibility_Lambda([]() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes")); AZoogiGameMode* G = PadGM(); return (!bOld && ZoogiSkills::WolfSlot(G) && !ZoogiGuide::HideMenus()) ? EVisibility::Visible : EVisibility::Collapsed; })
					.WidthOverride_Lambda([this]() { float W, H; WolfPad(W, H); return FOptionalSize(FMath::Max(W, 1.f)); })
					.HeightOverride_Lambda([this]() { float W, H; WolfPad(W, H); return FOptionalSize(FMath::Max(H, 1.f)); })
					[SNew(SZoogiWolfButton)]
				]
			];'''),
], inc='#include "ZoogiSkills.h"\n#include "ZoogiGuide.h"\n#include "ImageUtils.h"\n#include "Misc/Paths.h"', after='#include "ZoogiClassicPad.h"')

# ---------------- Arabian: no pizza dressing
ed(P + 'ZoogiArena.cpp', [
 ('''	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	{
		const float KR = ZoogiConst::KnockoffRadius;
		UStaticMesh* PizzaSph''',
  '''	RockPM = ZoogiPhysics::MakeMaterial(this, 0.3f, 0.5f, false);
	static const bool bPizza = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPizza")); // b77 (Evan: "remove the pizza like toppings from Arabian Nights")
	if (bPizza)
	{
		const float KR = ZoogiConst::KnockoffRadius;
		UStaticMesh* PizzaSph'''),
])

# ---------------- spire: every part fades out with the scoreboard
ed(P + 'ZoogiHoloSpire.cpp', [
 ('''			Sp->SetActorHiddenInGame(FadeK < 0.01f);''',
  '''			Sp->SetActorHiddenInGame(FadeK < 0.01f);
			// b77 (Evan: "the white clear pole spire in the centre should fade away with the scoreboard too"): every part, beam / sheath included
			TInlineComponentArray<UPrimitiveComponent*> Parts(Sp);
			for (UPrimitiveComponent* Pc : Parts) if (Pc) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f);'''),
])

# ---------------- anime at the top
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('''	bPanelMode = bMovie && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeCentreCard"));''',
  '''	bPanelMode = bMovie && FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimePanels")); // b77 (Evan: "all anime animation play at the top"): the top card again; -ZoogiAnimePanels = the b72 side windows
	if (false) bPanelMode = bMovie && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeCentreCard"));'''),
 ('''void UZoogiAnimeSubsystem::DrawCrash(UCanvas* C)
{
	if (CrashState == 0 || !C) return;
	const float W = C->ClipX, H = C->ClipY;''',
  '''void UZoogiAnimeSubsystem::DrawCrash(UCanvas* C)
{
	if (CrashState == 0 || !C) return;
	const float W = C->ClipX, H = C->ClipY;
	// b77 (Evan: "skill summon animation still needs to play at the top" / "all anime animation play at the top"): the crash and
	// skill-summon clips drop into the top card over the frozen game, no smoke cover. -ZoogiCrashFull = the full-screen smoke version
	static const bool bCrashFull = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrashFull"));
	if (!bCrashFull)
	{
		if (!MovieTexture || !MovieTexture->GetResource()) return;
		float K = 0.f;
		if (CrashState == 2 && bCrashFrames) K = FMath::Clamp((CrashAge - CrashWait) / 0.2f, 0.f, 1.f);
		else if (CrashState == 3 && bCrashFrames) K = FMath::Clamp(1.f - CrashAge / 0.25f, 0.f, 1.f);
		if (K <= 0.f) return;
		const float E = 1.f - FMath::Pow(1.f - K, 3.f);
		const FBox2D VR = ZoogiArenaHUD::AnimeVideoRect(FVector2D(W, H));
		const float MW = VR.GetSize().X, MH = VR.GetSize().Y, MX = VR.Min.X, MY = VR.Min.Y - (1.f - E) * (VR.Max.Y + 20.f);
		const float Bd = FMath::Max(3.f, MH * 0.03f);
		FCanvasTileItem Sh(FVector2D(MX - Bd * 2.f + 6.f, MY - Bd * 2.f + 8.f), FVector2D(MW + Bd * 4.f, MH + Bd * 4.f), FLinearColor(0.f, 0.f, 0.f, 0.45f)); Sh.BlendMode = SE_BLEND_Translucent; C->DrawItem(Sh);
		FCanvasTileItem Ot(FVector2D(MX - Bd * 2.f, MY - Bd * 2.f), FVector2D(MW + Bd * 4.f, MH + Bd * 4.f), FLinearColor(0.04f, 0.03f, 0.04f, 1.f)); Ot.BlendMode = SE_BLEND_Opaque; C->DrawItem(Ot);
		FCanvasTileItem Rm(FVector2D(MX - Bd, MY - Bd), FVector2D(MW + Bd * 2.f, MH + Bd * 2.f), FMath::Lerp(FLinearColor::White, CrashTint, 0.55f)); Rm.BlendMode = SE_BLEND_Opaque; C->DrawItem(Rm);
		FCanvasTileItem T(FVector2D(MX, MY), MovieTexture->GetResource(), FVector2D(MW, MH), FVector2D(0, 0), FVector2D(1, 1), FMath::Lerp(FLinearColor::White, CrashTint, 0.12f));
		T.BlendMode = SE_BLEND_Opaque;
		C->DrawItem(T);
		return;
	}'''),
], inc='#include "ZoogiArenaHUDWidget.h"', after='#include "ZoogiAnimeSubsystem.h"')

ed(P + 'ZoogiHUD.cpp', [
 ('''		static const bool bOldCard = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeCard"));
		if (bOldCard)
		{
			Panel(MX - 6.f * U, MY - 6.f * U, MW + 12.f * U, MH + 12.f * U, FLinearColor(C.R, C.G, C.B, 0.9f * A));
			DrawTexture(Anime->MovieTexture, MX, MY, MW, MH, 0.f, 0.f, 1.f, 1.f, FLinearColor(1, 1, 1, A));
		}''',
  '''		// b77 (Evan: "all anime animation play at the top"): the top-centre card is the default again (drops in from the top edge);
		// -ZoogiAnimeTriangle = the b65 side triangle, -ZoogiAnimeFull = full screen
		static const bool bOldCard = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeCard")) || !(FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeTriangle")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAnimeFull")));
		if (bOldCard)
		{
			const float Dn = (1.f - FMath::Clamp(Age / 0.18f, 0.f, 1.f)) * (MY + MH + 16.f * U) + (1.f - Out) * (MY + MH + 16.f * U) * 0.6f;
			Panel(MX - 9.f * U, MY - 9.f * U - Dn, MW + 18.f * U, MH + 18.f * U, FLinearColor(0.04f, 0.03f, 0.04f, 0.95f));
			Panel(MX - 5.f * U, MY - 5.f * U - Dn, MW + 10.f * U, MH + 10.f * U, FLinearColor(FMath::Lerp(1.f, C.R, 0.55f), FMath::Lerp(1.f, C.G, 0.55f), FMath::Lerp(1.f, C.B, 0.55f), 1.f));
			DrawTexture(Anime->MovieTexture, MX, MY - Dn, MW, MH, 0.f, 0.f, 1.f, 1.f, FLinearColor::White);
		}'''),
 ('''	return (AwakenWidget && AwakenWidget->IsOverPanels(ViewportPos)) || ZoogiFramed::IsOutsideArena(GetWorld(), ViewportPos);''',
  '''	return (AwakenWidget && AwakenWidget->IsOverPanels(ViewportPos)) || ZoogiFramed::IsOutsideArena(GetWorld(), ViewportPos) || ZoogiP5Cmd::InputBlocked(); // b77: a P5 menu tap never reaches the game'''),
], inc='#include "ZoogiP5Command.h"', after='#include "ZoogiHUD.h"')

# ---------------- P5 menu: taps on it stay on it
ed(H + 'ZoogiP5Command.h', [
 ('	ZOOGIROLL_API bool Showing();\n',
  '	ZOOGIROLL_API bool Showing();\n	/** b77 (Evan: "selecting skill buttons should not move the player camera - taps pass through the menu"): a menu press is\n	 *  held (or just ended) - the game ignores that touch */\n	ZOOGIROLL_API bool InputBlocked();\n'),
])
ed(P + 'ZoogiP5Command.cpp', [
 ('''	class SP5Cmd : public SLeafWidget
	{''',
  '''	static double GP5TookAt = -10.0; static bool GP5Held = false; // b77: a press the menu took (the game ignores it to the release + 0.3 s)
	class SP5Cmd : public SLeafWidget
	{'''),
 ('''			if (bMini) { if (Kind == 0 && OnMini) { OnMini(); return FReply::Handled(); } return FReply::Unhandled(); } // b73: anything else is the aim drag
			if (Kind == 0) { if (OnCmd) OnCmd(Idx); return FReply::Handled(); }
			if (Kind == 1) { if (OnSkill) OnSkill(Idx); return FReply::Handled(); }
			if (Mode != 0) { if (OnSkill) OnSkill(-1); return FReply::Handled(); } // tap outside a sub list = back''',
  '''			if (bMini) { if (Kind == 0 && OnMini) { OnMini(); return Took(); } return FReply::Unhandled(); } // b73: anything else is the aim drag
			if (Kind == 0) { if (OnCmd) OnCmd(Idx); return Took(); }
			if (Kind == 1) { if (OnSkill) OnSkill(Idx); return Took(); }
			if (Mode != 0) { if (OnSkill) OnSkill(-1); return Took(); } // tap outside a sub list = back'''),
 ('''		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E2) override { return Press(Geo, E2.GetScreenSpacePosition()); }''',
  '''		// b77: the menu keeps the whole touch (move / release never fall through to the aim drag or the target pick)
		FReply Took() { GP5TookAt = FPlatformTime::Seconds(); GP5Held = true; return FReply::Handled().CaptureMouse(SharedThis(this)); }
		FReply Let() { if (!GP5Held) return FReply::Unhandled(); GP5Held = false; GP5TookAt = FPlatformTime::Seconds(); return FReply::Handled().ReleaseMouseCapture(); }
		virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return GP5Held ? FReply::Handled() : FReply::Unhandled(); }
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return Let(); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return Let(); }
		virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { GP5Held = false; GP5TookAt = FPlatformTime::Seconds(); }
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E2) override { return Press(Geo, E2.GetScreenSpacePosition()); }'''),
 ('''	bool Showing() { return GShowing; }''',
  '''	bool Showing() { return GShowing; }
	bool InputBlocked() { return GP5Held || FPlatformTime::Seconds() - GP5TookAt < 0.3; }'''),
])
print('b77 patch ok')
