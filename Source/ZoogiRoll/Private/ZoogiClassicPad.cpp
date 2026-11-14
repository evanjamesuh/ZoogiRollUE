// Oct 28 (Evan): classic REACTION / SUPERPOWER touch buttons (see ZoogiClassicPad.h).
#include "ZoogiClassicPad.h"
#include "ZoogiWallCine.h" // b232
#include "ZoogiHaptics.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "ZoogiSkills.h"
#include "ZoogiGuide.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ZoogiRedirect.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiRoyale.h"
#include "ZoogiFrozenRoam.h"
#include "ZoogiActionFun.h"
#include "ZoogiUIFx.h"
#include "ZoogiRoll.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/PlatformTime.h"
#include "Rendering/RenderingCommon.h"
#include "Styling/CoreStyle.h"
#include "ZoogiWolfPack.h"
namespace ZoogiModeSel { FSlateFontInfo FontInfo(float Size, float Outline); } // ZoogiModeSelect.cpp: Lilita One, dark outline

namespace
{
	TWeakObjectPtr<UWorld> GPadWorld;
	TSharedPtr<SWidget> GPadRoot;
	TWeakObjectPtr<UGameViewportClient> GPadViewport;
	FDelegateHandle GCleanup;
	double GPadPressAt = -10.0; bool GPadHeld = false; // b120: the last pad press (PointerOwned)

	AZoogiGameMode* PadGM() { UWorld* W = GPadWorld.Get(); return W ? W->GetAuthGameMode<AZoogiGameMode>() : nullptr; }
	// build 27 (Evan): REACTION is what players tap - it appears for every tap moment (power clash, timing tap / block) and the hop
	int32 PadTapMode()
	{
		AZoogiGameMode* G = PadGM(); UZoogiFunSys* F = G ? UZoogiFunSys::Get(G) : nullptr;
		const int32 M = F ? F->TapButtonMode(G) : 0;
		if (M == 0 && G) if (UZoogiWolfPackSys* W = UZoogiWolfPackSys::Get(G)) if (W->ParryOpen()) return 4; // Nov 1: parry a wolf
		if (M == 0 && G && ZoogiRedirect::Available(G)) return 6; // b66 (Evan): REACTION mid-roll = redirect (1 crystal)
		return M;
	}
	FVector2D PowerCenterPxImpl(const FVector2D& Px)
	{	// b276: the same spot SZoogiClassicPadRoot::Pad(1) puts SUPERPOWER
		if (Px.X > Px.Y && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPowerPad"))) return FVector2D(Px.X * 0.76f, Px.Y * 0.80f);
		const ZoogiRoyaleUI::FPad L = ZoogiRoyaleUI::Layout(Px);
		return FVector2D(L.JumpC.X, L.JumpC.Y + L.JumpR + L.MorphR + 76.f * L.U);
	}
	bool ReactionNeeded() { return PadTapMode() > 0; } // Oct 30 (Evan): free hop gone - REACTION only for clash / timing-tap moments

	// build 29: Persona-style starburst (white, tinted + rotated at draw time): 10 uneven hand-cut spikes
	UTexture2D* BurstTex()
	{
		static UTexture2D* Tex = nullptr;
		if (Tex) return Tex;
		ZoogiUIFx::FPixels P;
		P.W = P.H = 256;
		P.Px.SetNumZeroed(P.W * P.H);
		TArray<FVector2D> Poly;
		for (int32 k = 0; k < 20; ++k)
		{
			const float A = UE_TWO_PI * k / 20.f + 0.07f * FMath::Sin(k * 1.7f);
			const float R = (k % 2 == 0) ? 124.f * (0.84f + 0.16f * FMath::Abs(FMath::Sin(k * 2.3f + 0.4f))) : 78.f;
			Poly.Add(FVector2D(128.f + FMath::Cos(A) * R, 128.f + FMath::Sin(A) * R));
		}
		auto Inside = [&Poly](float X, float Y)
		{
			bool bIn = false;
			for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
				if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
			return bIn;
		};
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				int32 N = 0;
				for (int32 s = 0; s < 4; ++s) if (Inside(x + ((s % 2) + 0.5f) / 2.f, y + ((s / 2) + 0.5f) / 2.f)) ++N;
				if (N) P.Px[y * P.W + x] = FColor(255, 255, 255, (uint8)(N * 255 / 4));
			}
		Tex = ZoogiUIFx::ToTexture(P);
		if (Tex) Tex->AddToRoot();
		return Tex;
	}

	// white icon silhouettes (tinted at draw time): 0 = up arrow (REACTION), 1 = five-point star (SUPERPOWER)
	UTexture2D* Icon(int32 Which)
	{
		static UTexture2D* Tex[3] = { nullptr, nullptr, nullptr }; // rooted: kept for the whole session (b86: 2 = lightning bolt)
		if (Tex[Which]) return Tex[Which];
		ZoogiUIFx::FPixels P;
		P.W = P.H = 128;
		P.Px.SetNumZeroed(P.W * P.H);
		TArray<FVector2D> Poly;
		if (Which == 0)
		{
			Poly = { {64, 10}, {112, 62}, {82, 62}, {82, 104}, {46, 104}, {46, 62}, {16, 62} };
		}
		else if (Which == 2)
		{
			Poly = { {74, 6}, {28, 70}, {58, 70}, {44, 122}, {100, 50}, {70, 50}, {88, 6} }; // b86: lightning bolt
		}
		else
		{
			for (int32 k = 0; k < 10; ++k)
			{
				const float A = -UE_HALF_PI + UE_TWO_PI * k / 10.f, R = (k % 2 == 0) ? 58.f : 25.f;
				Poly.Add(FVector2D(64.f + FMath::Cos(A) * R, 68.f + FMath::Sin(A) * R));
			}
		}
		auto Inside = [&Poly](float X, float Y)
		{
			bool bIn = false;
			for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
				if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
			return bIn;
		};
		for (int32 y = 0; y < P.H; ++y)
			for (int32 x = 0; x < P.W; ++x)
			{
				int32 N = 0;
				for (int32 s = 0; s < 16; ++s) if (Inside(x + ((s % 4) + 0.5f) / 4.f, y + ((s / 4) + 0.5f) / 4.f)) ++N;
				if (N) P.Px[y * P.W + x] = FColor(255, 255, 255, (uint8)(N * 255 / 16));
			}
		Tex[Which] = ZoogiUIFx::ToTexture(P);
		if (Tex[Which]) Tex[Which]->AddToRoot();
		return Tex[Which];
	}

	UTexture2D* WolfIcon();
	UTexture2D* RapidIcon(); // b199
	bool BoltBanked(const AZoogiGameMode* G) { const AZoogiBall* B = G ? G->GetActiveBall() : nullptr; return B && !B->bCPU && ZoogiSkills::BoltCount(G, B->PlayerIndex) > 0; }
	bool WolfBanked(const AZoogiGameMode* G) { const AZoogiBall* B = G ? G->GetActiveBall() : nullptr; return B && !B->bCPU && ZoogiSkills::WolfArmed(G, B->PlayerIndex); }
	// one round button: plate + icon + label, fires on touch / click down anywhere inside its box
	class SZoogiPadButton : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiPadButton) {}
		SLATE_END_ARGS()
		bool bPower = false;
		mutable bool bHeld = false;
		double FlashAt = -10.0;
		void Construct(const FArguments&, bool bInPower) { bPower = bInPower; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64.f, 64.f); }
		virtual bool ComputeVolatility() const override { return true; }
		bool Ready() const
		{
			AZoogiGameMode* G = PadGM();
			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::ComboReady(G) || ZoogiSkills::WolfReady(G) || ZoogiSkills::BoltReady(G)) : ReactionNeeded());
		}
		FReply Press()
		{
			bHeld = true;
			FlashAt = FPlatformTime::Seconds();
			GPadPressAt = FlashAt; GPadHeld = true; // b120
			if (AZoogiGameMode* G = PadGM())
			{
				// b122b (Evan: "REACTION is the reaction, SUPERPOWER is the superpower ability - interchangeable at my own will"):
				// an open REDIRECT pick never swallows the other button - REACTION closes it, SUPERPOWER closes it and does its own job
				if (ZoogiRedirect::Picking())
				{
					ZoogiRedirect::CancelPick(bPower ? TEXT("SUPERPOWER") : TEXT("REACTION"));
					if (!bPower) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return FReply::Handled().CaptureMouse(SharedThis(this)); }
				}
				if (bPower && ZoogiSkills::DashActive()) { ZoogiSkills::DashGo(G); return FReply::Handled().CaptureMouse(SharedThis(this)); } // b223: GATTLING RAPID dash
				if (ZoogiSkills::RapidActive()) return FReply::Handled().CaptureMouse(SharedThis(this)); // b199: during GATTLING RAPID a press is just a bolt
				const int32 TapMode = bPower ? 0 : PadTapMode();
				UE_LOG(LogZoogi, Log, TEXT("CLASSIC PAD %s pressed (ready %d, tap mode %d)"), bPower ? TEXT("SUPERPOWER") : TEXT("REACTION"), Ready() ? 1 : 0, TapMode);
				UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G);
				if (TapMode == 4) { if (WP) WP->TryParry(); } // Nov 1: REACTION pops a wolf
				else if (TapMode == 6) ZoogiRedirect::Begin(G); // b66: pause + pick a target in range
				else if (TapMode > 0) { if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->OnTap(); } // clash taps / timing tap / block go to FunSys first
				else if (bPower && ZoogiSkills::ComboReady(G)) ZoogiSkills::ComboGo(G); // b120g: the next armed COMBO cast
				else if (bPower && G->Phase == EZoogiPhase::Aiming && G->GetActiveBall() && !G->GetActiveBall()->bCPU && ZoogiSkills::ComboCount(G, G->GetActiveBall()->PlayerIndex) > 0 && !ZoogiSkills::WolfReady(G) && !ZoogiSkills::BoltReady(G))
				{ if (const AZoogiBall* Bc = G->GetActiveBall()) G->PublicPopup(TEXT("LAUNCH, THEN TAP SUPERPOWER TO CAST"), Bc->BallLocation() + FVector(0, 0, 190.f), FLinearColor(1.f, 0.82f, 0.15f)); }
				else if (bPower && ZoogiSkills::WolfReady(G)) ZoogiSkills::WolfGo(G); // b77d (Evan): a banked Wolf Pack is SUPERPOWER's next press (while rolling)
				else if (bPower && ZoogiSkills::BoltReady(G)) ZoogiSkills::BoltGo(G); // b80: Bolt Balls - one straight bolt per press while rolling
				else if (bPower && (WolfBanked(G) || BoltBanked(G)) && G->Phase == EZoogiPhase::Aiming) { if (const AZoogiBall* Bw = G->GetActiveBall()) G->PublicPopup(TEXT("LAUNCH, THEN TAP SUPERPOWER FOR THE PACK"), Bw->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.55f, 1.f)); }
				else if (bPower && WP && WP->HumanPackOut()) WP->OnPowerPress(); // Nov 1: tap = recall, hold = charge (on release)
				else G->MenuAction(bPower ? TEXT("ClassicPower") : TEXT("ClassicReaction"), 0);
			}
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		FReply Release()
		{
			bHeld = false; GPadHeld = false; GPadPressAt = FPlatformTime::Seconds(); // b120: the lift belongs to the button too
			if (bPower) if (AZoogiGameMode* G = PadGM()) if (UZoogiWolfPackSys* WP = UZoogiWolfPackSys::Get(G)) WP->OnPowerRelease();
			return FReply::Handled().ReleaseMouseCapture();
		}
		virtual FReply OnTouchStarted(const FGeometry&, const FPointerEvent&) override { return Press(); }
		virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return Release(); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& E) override { return (!E.IsTouchEvent() && E.GetEffectingButton() == EKeys::LeftMouseButton) ? Press() : FReply::Unhandled(); }
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& G, const FPointerEvent& E) override { return OnMouseButtonDown(G, E); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& E) override { return !E.IsTouchEvent() ? Release() : FReply::Unhandled(); }
		virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { bHeld = false; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			if (ZoogiWallCine::Active()) { return Layer; } // b232: the wall-smash cinematic plays on a clean screen
			const FVector2D Sz = Geo.GetLocalSize();
			const float R0 = Sz.X / 2.7f;
			const bool bReady = Ready();
			const double Now = FPlatformTime::Seconds();
			const float Flash = FMath::Clamp(1.f - float(Now - FlashAt) / 0.25f, 0.f, 1.f);
			const int32 TapMode = bPower ? 0 : PadTapMode();
			const float R = R0 * (bHeld ? 0.9f : 1.f) * (1.f + 0.06f * Flash) * (TapMode > 0 ? 1.f + 0.07f * FMath::Sin(float(Now) * 16.f) : 1.f);
			const FVector2D C(Sz.X * 0.5f, 0.25f * R0 + R0);
			auto Disc = [&](const FVector2D& Ctr, float Rad, const FLinearColor& Fill, const FLinearColor& Rim, float RimW, int32 L)
			{
				const FSlateRoundedBoxBrush B(Fill, Rad, Rim, RimW);
				// MakeBox draws the fill in the tint passed here (the brush colour itself is not applied); the rim comes from the brush
				FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(FVector2D(Rad * 2.f, Rad * 2.f), FSlateLayoutTransform(1.f, Ctr - FVector2D(Rad, Rad))), &B, ESlateDrawEffect::None, Fill);
			};
			const FLinearColor Dark = FLinearColor::FromSRGBColor(FColor(14, 12, 10));
			// build 29 (Evan, Persona 5 Royal reference): REACTION is THE reaction button - red / black / white starburst, ransom-note label
			if (!bPower)
			{
				const FSlateBrush* WB = FCoreStyle::Get().GetBrush("WhiteBrush");
				const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WB);
				const FSlateRenderTransform& RT = Geo.GetAccumulatedRenderTransform();
				auto Vx = [&](const FVector2D& P, const FColor& Cc) { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.5f, 0.5f), Cc); };
				auto Quad = [&](const FVector2D& A, const FVector2D& B, const FVector2D& Cq, const FVector2D& D, const FLinearColor& Col, int32 L)
				{
					const FColor Cc = Col.ToFColor(true);
					TArray<FSlateVertex> V = { Vx(A, Cc), Vx(B, Cc), Vx(Cq, Cc), Vx(D, Cc) };
					TArray<SlateIndex> I = { 0, 1, 2, 0, 2, 3 };
					FSlateDrawElement::MakeCustomVerts(Out, L, RH, V, I, nullptr, 0, 0);
				};
				const FLinearColor P5Red = FLinearColor::FromSRGBColor(FColor(230, 0, 18)), P5Blk = FLinearColor::FromSRGBColor(FColor(10, 8, 10)), P5Wht = FLinearColor::White;
				const float Act = TapMode > 0 ? 1.f : 0.f;
				const float Beat = 0.5f + 0.5f * FMath::Sin(float(Now) * (TapMode > 0 ? 14.f : 5.f));
				const float RB = R * (1.f + 0.2f * Act);
				const float Spin = float(Now) * (TapMode > 0 ? 1.4f : 0.35f);
				auto Burst = [&](const FVector2D& Ctr, float Ro, float Ang, const FLinearColor& Col, int32 L)
				{
					UTexture2D* T = BurstTex();
					if (!T) return;
					FSlateBrush BB; BB.SetResourceObject(T); BB.ImageSize = FVector2D(256.f, 256.f);
					const float Hs = Ro * (128.f / 124.f);
					FSlateDrawElement::MakeRotatedBox(Out, L, Geo.ToPaintGeometry(FVector2D(Hs * 2.f, Hs * 2.f), FSlateLayoutTransform(1.f, Ctr - FVector2D(Hs, Hs))), &BB, ESlateDrawEffect::None, Ang, TOptional<FVector2D>(), FSlateDrawElement::RelativeToElement, Col);
				};
				Burst(C + FVector2D(R * 0.1f, R * 0.12f), RB * (1.42f + 0.08f * Beat), Spin, /* b266: smaller - it covered the MENU button */ P5Blk.CopyWithNewOpacity(0.92f), Layer);      // black burst (offset shadow)
				Burst(C, RB * (1.32f + 0.08f * Beat), Spin, TapMode > 0 ? P5Red : P5Red.CopyWithNewOpacity(0.9f), Layer + 1);             // red burst
				Disc(C, RB * 0.98f, P5Wht, FLinearColor::Transparent, 0.f, Layer + 2);                                                                  // white rim
				Disc(C, RB * 0.86f, P5Blk, FLinearColor::Transparent, 0.f, Layer + 3);                                                                  // black face
				if (TapMode > 0) Disc(C, RB * (0.86f + 0.5f * (1.f - Beat)), FLinearColor::Transparent, P5Wht.CopyWithNewOpacity(0.6f * Beat), RB * 0.05f, Layer + 3); // pulse ring
				if (UTexture2D* T = Icon(0))
				{
					FSlateBrush IB; IB.SetResourceObject(T); IB.ImageSize = FVector2D(128.f, 128.f);
					const float IS = RB * 1.0f;
					FSlateDrawElement::MakeBox(Out, Layer + 4, Geo.ToPaintGeometry(FVector2D(IS, IS), FSlateLayoutTransform(1.f, C - FVector2D(IS * 0.5f, IS * 0.55f) + FVector2D(R * 0.06f, R * 0.06f))), &IB, ESlateDrawEffect::None, P5Red);
					FSlateDrawElement::MakeBox(Out, Layer + 5, Geo.ToPaintGeometry(FVector2D(IS, IS), FSlateLayoutTransform(1.f, C - FVector2D(IS * 0.5f, IS * 0.55f))), &IB, ESlateDrawEffect::None, P5Wht);
				}
				// ransom-note label: every letter its own tilted box, alternating white-on-black / black-on-white
				const FString Word = TapMode == 3 ? TEXT("BLOCK!") : (TapMode == 5 ? TEXT("PULL!") : (TapMode == 6 ? TEXT("REACT!") : (TapMode > 0 ? TEXT("TAP!") : TEXT("REACT"))));
				const TSharedRef<FSlateFontMeasure> FMs = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				const float Base = FMath::Max(10.f, R0 * (TapMode > 0 ? 0.62f : 0.42f));
				TArray<FVector2D> Sizes; float TotW = 0.f;
				for (int32 i = 0; i < Word.Len(); ++i)
				{
					const FSlateFontInfo F = ZoogiModeSel::FontInfo(Base * ((i % 2) ? 0.84f : 1.f), 0.f);
					const FVector2D S = FMs->Measure(Word.Mid(i, 1), F) + FVector2D(Base * 0.18f, Base * 0.06f);
					Sizes.Add(S); TotW += S.X - Base * 0.04f;
				}
				// b120 (Evan 19:34: "put the react word below the button"): centred under the plate (was beside it, to the left)
				float X = C.X - TotW * 0.5f;
				const float Y0 = C.Y + RB * 1.12f;
				for (int32 i = 0; i < Word.Len(); ++i)
				{
					const bool bInv = (i % 2) == 1;
					const FVector2D S = Sizes[i];
					const float Yo = Y0 + ((i % 3) - 1) * Base * 0.08f, Sk = S.Y * 0.18f * ((i % 2) ? -1.f : 1.f);
					Quad(FVector2D(X + Sk + Base * 0.06f, Yo + Base * 0.08f), FVector2D(X + S.X + Sk + Base * 0.06f, Yo + Base * 0.08f), FVector2D(X + S.X + Base * 0.06f, Yo + S.Y + Base * 0.08f), FVector2D(X + Base * 0.06f, Yo + S.Y + Base * 0.08f), P5Red, Layer + 5);
					Quad(FVector2D(X + Sk, Yo), FVector2D(X + S.X + Sk, Yo), FVector2D(X + S.X, Yo + S.Y), FVector2D(X, Yo + S.Y), bInv ? P5Wht : P5Blk, Layer + 6);
					const FSlateFontInfo F = ZoogiModeSel::FontInfo(Base * ((i % 2) ? 0.84f : 1.f), 0.f);
					const FVector2D TS = FMs->Measure(Word.Mid(i, 1), F);
					FSlateDrawElement::MakeText(Out, Layer + 7, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, FVector2D(X + (S.X - TS.X) * 0.5f + Sk * 0.5f, Yo + (S.Y - TS.Y) * 0.5f))), Word.Mid(i, 1), F, ESlateDrawEffect::None, bInv ? P5Blk : P5Wht);
					X += S.X - Base * 0.04f;
				}
				return Layer + 8;
			}
			const FLinearColor Face = bPower ? FLinearColor::FromSRGBColor(FColor(181, 17, 95)) : FLinearColor::FromSRGBColor(FColor(31, 163, 154));
			const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(255, 213, 74));
			const FLinearColor Fill = bReady ? Face : FLinearColor::FromSRGBColor(FColor(74, 79, 92)).CopyWithNewOpacity(0.82f);
			// ready glow (soft pulse) behind the plate
			if (bReady)
			{
				const float Pulse = 0.5f + 0.5f * FMath::Sin(float(Now) * 5.f);
				Disc(C, R * (1.14f + 0.05f * Pulse), FLinearColor::Transparent, (bPower ? Gold : FLinearColor::FromSRGBColor(FColor(150, 255, 240))).CopyWithNewOpacity(0.35f + 0.35f * Pulse), R * 0.08f, Layer);
			}
			Disc(C + FVector2D(0.f, R * 0.08f), R, FLinearColor(0.f, 0.f, 0.f, 0.35f), FLinearColor::Transparent, 0.f, Layer);               // drop shadow
			Disc(C, R, Fill, Dark, R * 0.11f, Layer + 1);                                                                                     // plate + thick dark rim
			Disc(C, R * 0.84f, FLinearColor::Transparent, (bPower && bReady ? Gold : FLinearColor::White).CopyWithNewOpacity(bReady ? 0.55f : 0.18f), R * 0.05f, Layer + 2); // inner ring
			{	// lit top band (the plate catches light like the mode cards)
				const FVector2D HS(R * 1.1f, R * 0.5f);
				const FLinearColor Lit(1.f, 1.f, 1.f, bReady ? 0.2f : 0.08f);
				const FSlateRoundedBoxBrush B(Lit, HS.Y * 0.5f, FLinearColor::Transparent, 0.f);
				FSlateDrawElement::MakeBox(Out, Layer + 2, Geo.ToPaintGeometry(HS, FSlateLayoutTransform(1.f, C + FVector2D(-HS.X * 0.5f, -R * 0.72f))), &B, ESlateDrawEffect::None, Lit);
			}
			if (UTexture2D* T = Icon(bPower ? 1 : 0))
			{
				FSlateBrush IB;
				IB.SetResourceObject(T);
				IB.ImageSize = FVector2D(128.f, 128.f);
				const float IS = R * 1.05f;
				const FVector2D IP = C - FVector2D(IS * 0.5f, IS * 0.55f);
				FSlateDrawElement::MakeBox(Out, Layer + 3, Geo.ToPaintGeometry(FVector2D(IS, IS), FSlateLayoutTransform(1.f, IP + FVector2D(0.f, R * 0.06f))), &IB, ESlateDrawEffect::None, Dark.CopyWithNewOpacity(0.6f));
				FSlateDrawElement::MakeBox(Out, Layer + 4, Geo.ToPaintGeometry(FVector2D(IS, IS), FSlateLayoutTransform(1.f, IP)), &IB, ESlateDrawEffect::None,
					(bPower && bReady ? Gold : FLinearColor::White).CopyWithNewOpacity(bReady ? 1.f : 0.6f));
			}
			// label under the plate (Lilita One, dark outline)
			const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			auto Label = [&](const FString& S, float Size, const FVector2D& Center, const FLinearColor& Col, int32 L)
			{
				const FSlateFontInfo F = ZoogiModeSel::FontInfo(Size, FMath::Max(1.f, Size * 0.14f));
				const FVector2D TS = FM->Measure(S, F);
				FSlateDrawElement::MakeText(Out, L, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, Center - TS * 0.5f)), S, F, ESlateDrawEffect::None, Col);
			};
			int32 RapidN = 0; const bool bRapidNow = bPower && ZoogiSkills::RapidActive(&RapidN); // b199
			int32 DashN = 0; const bool bDashNow = bPower && ZoogiSkills::DashActive(&DashN); // b223
			if (bDashNow) Label(FString::Printf(TEXT("DASH x%d!"), DashN), FMath::Max(8.f, R0 * 0.3f), FVector2D(Sz.X * 0.5f, C.Y + R0 * 1.3f), FLinearColor::FromSRGBColor(FColor(255, 214, 74)), Layer + 5);
			else if (bRapidNow) Label(FString::Printf(TEXT("RAPID x%d!"), RapidN), FMath::Max(8.f, R0 * 0.3f), FVector2D(Sz.X * 0.5f, C.Y + R0 * 1.3f), FLinearColor::FromSRGBColor(FColor(255, 214, 74)), Layer + 5);
			else Label(bPower ? ((PadGM() && ZoogiSkills::ComboReady(PadGM()) && PadGM()->GetActiveBall()) ? FString::Printf(TEXT("CAST %d/%d!"), ZoogiSkills::ComboTotal(PadGM(), PadGM()->GetActiveBall()->PlayerIndex) - ZoogiSkills::ComboCount(PadGM(), PadGM()->GetActiveBall()->PlayerIndex) + 1, ZoogiSkills::ComboTotal(PadGM(), PadGM()->GetActiveBall()->PlayerIndex)) : (PadGM() && ZoogiSkills::WolfReady(PadGM())) ? FString(TEXT("WOLF PACK!")) : ((PadGM() && ZoogiSkills::BoltReady(PadGM()) && PadGM()->GetActiveBall()) ? FString::Printf(TEXT("BOLT x%d!"), ZoogiSkills::BoltCount(PadGM(), PadGM()->GetActiveBall()->PlayerIndex)) : FString(TEXT("SUPERPOWER")))) : (TapMode == 3 ? TEXT("BLOCK!") : (TapMode == 5 ? TEXT("PULL!") : (TapMode == 6 ? TEXT("REDIRECT") : (TapMode > 0 ? TEXT("TAP!") : TEXT("REACTION"))))), FMath::Max(8.f, R0 * 0.3f), FVector2D(Sz.X * 0.5f, C.Y + R0 * 1.3f), FLinearColor::White.CopyWithNewOpacity(bReady ? 1.f : 0.75f), Layer + 5);
			// b77d (Evan: "bring back the wolf badges - how many wolves I have left"): banked Wolf Pack summons, purple badge top-left
			if (bPower)
				if (AZoogiGameMode* G = PadGM())
					if (const AZoogiBall* Bw = G->GetActiveBall())
						if (const int32 NW = ZoogiSkills::WolfCount(G, Bw->PlayerIndex))
						{
							const FVector2D BC = C + FVector2D(-R * 0.86f, -R * 0.76f);
							const FLinearColor Wc = FLinearColor::FromSRGBColor(FColor(160, 90, 255));
							Disc(BC, R * 0.36f, Wc, FLinearColor::White, R * 0.06f, Layer + 5);
							if (UTexture2D* Wt = WolfIcon())
							{
								FSlateBrush WB; WB.SetResourceObject(Wt); WB.ImageSize = FVector2D(300.f, 250.f);
								const float Ih = R * 0.4f, Iw = Ih * 1.2f;
								FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2D(Iw, Ih), FSlateLayoutTransform(1.f, BC - FVector2D(Iw, Ih) * 0.5f - FVector2D(0.f, R * 0.06f))), &WB, ESlateDrawEffect::None, FLinearColor::White);
							}
							Label(FString::Printf(TEXT("x%d"), NW), FMath::Max(7.f, R0 * 0.24f), BC + FVector2D(R * 0.3f, R * 0.3f), FLinearColor::White, Layer + 7);
						}
			// b86 (Evan: "if both wolf and Bolt Balls are active, show the bolt counter next to the wolf's, separately round SUPERPOWER"):
			// bolts left on a yellow-ringed purple badge, top-right (the wolf badge is top-left)
			if (bPower)
				if (AZoogiGameMode* G = PadGM())
					if (const AZoogiBall* Bb = G->GetActiveBall())
						if (const int32 NB = ZoogiSkills::BoltCount(G, Bb->PlayerIndex))
						{
							const FVector2D BC = C + FVector2D(R * 0.86f, -R * 0.76f);
							const FLinearColor Yel = FLinearColor::FromSRGBColor(FColor(255, 222, 60));
							Disc(BC, R * 0.36f, FLinearColor::FromSRGBColor(FColor(90, 40, 170)), Yel, R * 0.06f, Layer + 5);
							if (UTexture2D* Bt = Icon(2))
							{
								FSlateBrush BB; BB.SetResourceObject(Bt); BB.ImageSize = FVector2D(128.f, 128.f);
								const float Is = R * 0.42f;
								FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2D(Is, Is), FSlateLayoutTransform(1.f, BC - FVector2D(Is, Is) * 0.5f - FVector2D(R * 0.08f, R * 0.06f))), &BB, ESlateDrawEffect::None, Yel);
							}
							Label(FString::Printf(TEXT("x%d"), NB), FMath::Max(7.f, R0 * 0.24f), BC + FVector2D(R * 0.3f, R * 0.3f), FLinearColor::White, Layer + 7);
						}
			// b199 GATTLING RAPID: shots so far on a gold-ringed purple badge (top centre) with the rapid art, and a ring that runs
			// down with the 6 s clock round the button
			if (bPower)
			{
				int32 Rn = 0; float Rl = 0.f;
				if (ZoogiSkills::RapidActive(&Rn, &Rl) || ZoogiSkills::DashActive(&Rn)) // b223: + the dash count
				{
					const FLinearColor Yel = FLinearColor::FromSRGBColor(FColor(255, 214, 74));
					const FVector2D BC = C + FVector2D(0.f, -R * 1.08f);
					Disc(BC, R * 0.44f, FLinearColor::FromSRGBColor(FColor(90, 40, 170)), Yel, R * 0.07f, Layer + 5);
					if (UTexture2D* Rt = RapidIcon())
					{
						FSlateBrush RB; RB.SetResourceObject(Rt); RB.ImageSize = FVector2D(300.f, 250.f);
						const float Ih = R * 0.5f, Iw = Ih * 1.2f;
						FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2D(Iw, Ih), FSlateLayoutTransform(1.f, BC - FVector2D(Iw, Ih) * 0.5f - FVector2D(0.f, R * 0.1f))), &RB, ESlateDrawEffect::None, FLinearColor::White);
					}
					Label(FString::Printf(TEXT("x%d"), Rn), FMath::Max(8.f, R0 * 0.3f), BC + FVector2D(0.f, R * 0.3f), FLinearColor::White, Layer + 7);
					// the clock: a gold arc of dots round the button, shrinking over the 6 s
					const int32 Nd = 24; const float Fr = FMath::Clamp(Rl / 6.f, 0.f, 1.f);
					for (int32 k = 0; k < FMath::RoundToInt(Nd * Fr); ++k)
					{
						const float A = -PI * 0.5f + 2.f * PI * k / Nd;
						Disc(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * 1.12f, R * 0.07f, Yel, FLinearColor::Transparent, 0.f, Layer + 5);
					}
				}
			}
			// Wolf Pack charges
			if (bPower)
				if (AZoogiGameMode* G = PadGM())
					if (G->WolfPackOnly() && FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfSuperpower"))) // b80 (Evan: "wolf heads only if Wolf Pack is selected for that player"): no charges badge otherwise
						if (const AZoogiBall* B = G->GetActiveBall())
						{
							const FVector2D BC = C + FVector2D(R * 0.82f, -R * 0.78f);
							Disc(BC, R * 0.3f, Dark, B->Charges > 0 ? Gold : FLinearColor(0.5f, 0.5f, 0.55f), R * 0.06f, Layer + 5);
							Label(FString::Printf(TEXT("%d"), B->Charges), FMath::Max(7.f, R0 * 0.26f), BC, B->Charges > 0 ? Gold : FLinearColor(0.75f, 0.75f, 0.8f), Layer + 6);
						}
			return Layer + 6;
		}
	};

	// b199 (Evan: "the rapid fire counter on the SUPERPOWER button"): the GATTLING RAPID art for the badge
	UTexture2D* RapidIcon()
	{
		static UTexture2D* T = nullptr; static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			const FString P = FPaths::ProjectContentDir() / TEXT("Data/CardArt/Icons/gattling_beat.png");
			if (FPaths::FileExists(P)) T = FImageUtils::ImportFileAsTexture2D(P);
			if (T) T->AddToRoot();
		}
		return T;
	}
	// b77 (Evan): the WOLF button - Wolf Pack skill armed -> sits in CONTROL's old slot of the mode bar, tap while rolling = the pack
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
			GPadPressAt = FlashAt; GPadHeld = true; // b120
			if (AZoogiGameMode* G = PadGM())
			{
				if (ZoogiSkills::WolfGo(G)) ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 0.7f);
				else
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f);
					if (const AZoogiBall* B = G->GetActiveBall()) G->PublicPopup(TEXT("LAUNCH, THEN TAP THE WOLF"), B->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.55f, 1.f));
				}
			}
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		FReply Release() { bHeld = false; GPadHeld = false; GPadPressAt = FPlatformTime::Seconds(); return FReply::Handled().ReleaseMouseCapture(); }
		virtual FReply OnTouchStarted(const FGeometry&, const FPointerEvent&) override { return Press(); }
		virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return Release(); }
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& E) override { return (!E.IsTouchEvent() && E.GetEffectingButton() == EKeys::LeftMouseButton) ? Press() : FReply::Unhandled(); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& E) override { return !E.IsTouchEvent() ? Release() : FReply::Unhandled(); }
		virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { bHeld = false; }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (ZoogiWallCine::Active()) { return Layer; } // b232: the wall-smash cinematic plays on a clean screen
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
			// wolves left badge (gold disc, top-right corner)
			const AZoogiBall* Bw = G ? G->GetActiveBall() : nullptr;
			const int32 NW = Bw ? ZoogiSkills::WolfCount(G, Bw->PlayerIndex) : 0;
			if (NW > 0)
			{
				const float BR = Sz.Y * 0.26f;
				const FVector2D BC = O + FVector2D(Sz.X - BR * 0.4f, BR * 0.3f);
				const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(255, 213, 74));
				Box(BC - FVector2D(BR, BR), FVector2D(BR * 2.f, BR * 2.f), Dark, Gold, FMath::Max(2.f, BR * 0.2f), BR, Layer + 5);
				const FSlateFontInfo BF = ZoogiModeSel::FontInfo(FMath::Max(8.f, BR * 1.1f), 1.f);
				const FString NS = FString::Printf(TEXT("x%d"), NW);
				const FVector2D NSz = FM->Measure(NS, BF);
				FSlateDrawElement::MakeText(Out, Layer + 6, Geo.ToPaintGeometry(NSz, FSlateLayoutTransform(1.f, BC - NSz * 0.5f)), NS, BF, ESlateDrawEffect::None, Gold);
			}
			return Layer + 7;
		}
	};

	// root: places the two buttons from ZoogiRoyaleUI::Layout every frame; only its buttons take touches
	class SZoogiClassicPadRoot : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SZoogiClassicPadRoot) {}
		SLATE_END_ARGS()
		FGeometry LastGeo;
		bool bHaveGeo = false;
		virtual void Tick(const FGeometry& G, const double T, const float Dt) override { LastGeo = G; bHaveGeo = true; SCompoundWidget::Tick(G, T, Dt); }
		static bool Shown()
		{
			AZoogiGameMode* G = PadGM();
			return G && !UGameplayStatics::IsGamePaused(G) && (G->ClassicButtonsOn() || PadTapMode() > 0) && !(G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn()); // b79: hidden under the pause menu // the Frozen bridge flow keeps its Canvas sticks
		}
		// button box in local Slate units: Which 0 = REACTION, 1 = SUPERPOWER
		FMargin Pad(int32 Which, float& OutW, float& OutH) const
		{
			OutW = OutH = 0.f;
			if (!bHaveGeo) return FMargin(0.f);
			const float Sc = FMath::Max(LastGeo.GetAccumulatedLayoutTransform().GetScale(), 0.01f);
			const FVector2D Px = LastGeo.GetLocalSize() * Sc;
			const ZoogiRoyaleUI::FPad L = ZoogiRoyaleUI::Layout(Px);
			float R = (Which == 0 ? L.JumpR : L.MorphR) / Sc;
			FVector2D C = L.JumpC / Sc;
			if (Which == 1) C.Y += (L.JumpR + L.MorphR + 76.f * L.U) / Sc; // straight under REACTION (build 12 spot)
			// Nov 1 b53 (Evan: "make the reaction button appear centre so people see it"): REACTION pops up mid-screen, a bit bigger
			// b58 (Evan, 20:32: "put it on the right but tapping anywhere acts as a press"): REACTION sits on the right edge; any tap that
			// is not on other UI already goes to FunSys / Wolf Pack OnTap from the player controller, so the whole screen counts as a press
			if (Which == 0) { R *= 1.3f; C = FVector2D(Px.X - 1.55f * R * Sc, Px.Y * 0.6f) / Sc; }
			// b77 (Evan: "superpower button moved where marked to make space for the reaction button - they overlap"): landscape = lower, left of the REACTION spot
			if (Which == 1 && Px.X > Px.Y && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldPowerPad"))) C = FVector2D(Px.X * 0.76f, Px.Y * 0.80f) / Sc;
			OutW = 2.7f * R; OutH = 2.8f * R;
			return FMargin(C.X - OutW * 0.5f, C.Y - 1.25f * R, 0.f, 0.f);
		}
		// b77: the WOLF slot = slot 2 of the classic mode bar (ZoogiAwakenWidget::BuildAim: AUTO / WOLF / ARC), in the same
		// DPI-scaled units as the bar (the bar's panel: landscape bottom-centre, 22 up; portrait bottom-left, 20 in / 40 up)
		FMargin WolfPad(float& OutW, float& OutH) const
		{
			OutW = OutH = 0.f;
			if (!bHaveGeo) return FMargin(0.f);
			const FVector2D L = LastGeo.GetLocalSize();
			const bool bPortrait = L.Y > L.X;
			const float BW = bPortrait ? 150.f : 168.f, BH = bPortrait ? 70.f : 74.f, Gap = bPortrait ? 12.f : 16.f;
			const int32 NSlots = 3; // AUTO / WOLF / ARC (b77: LAUNCH deleted)
			const float PW = NSlots * BW + (NSlots - 1) * Gap + 16.f, PH = BH + 16.f;
			const float Left = bPortrait ? 20.f : (L.X - PW) * 0.5f, Top = L.Y - (bPortrait ? 40.f : 22.f) - PH;
			OutW = BW; OutH = BH;
			return FMargin(Left + 8.f + BW + Gap, Top + 8.f, 0.f, 0.f);
		}
		void Construct(const FArguments&)
		{
			auto MakeBtn = [this](int32 Which) -> TSharedRef<SWidget>
			{
				return SNew(SBox)
					.Visibility_Lambda([Which]()
					{
						// build 27: REACTION only when it is needed; SUPERPOWER only on the human's own turn
						AZoogiGameMode* G = PadGM();
						const bool bOn = Shown() && (Which == 0 ? ReactionNeeded() : (G && G->ClassicButtonsOn()));
						return bOn ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.WidthOverride_Lambda([this, Which]() { float W, H; Pad(Which, W, H); return FOptionalSize(FMath::Max(W, 1.f)); })
					.HeightOverride_Lambda([this, Which]() { float W, H; Pad(Which, W, H); return FOptionalSize(FMath::Max(H, 1.f)); })
					[SNew(SZoogiPadButton, Which == 1)];
			};
			ChildSlot
			[
				SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return Pad(0, W, H); }))[MakeBtn(0)]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return Pad(1, W, H); }))[MakeBtn(1)]
				+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateLambda([this]() { float W, H; return WolfPad(W, H); }))
				[
					SNew(SBox)
					.Visibility_Lambda([]() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes")) || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfButton")); AZoogiGameMode* G = PadGM(); return (!bOld && ZoogiSkills::WolfSlot(G) && !ZoogiGuide::HideMenus()) ? EVisibility::Visible : EVisibility::Collapsed; })
					.WidthOverride_Lambda([this]() { float W, H; WolfPad(W, H); return FOptionalSize(FMath::Max(W, 1.f)); })
					.HeightOverride_Lambda([this]() { float W, H; WolfPad(W, H); return FOptionalSize(FMath::Max(H, 1.f)); })
					[SNew(SZoogiWolfButton)]
				]
			];
			SetVisibility(EVisibility::SelfHitTestInvisible);
		}
	};

	void RemovePad()
	{
		if (GPadRoot.IsValid())
			if (UGameViewportClient* V = GPadViewport.Get()) V->RemoveViewportWidgetContent(GPadRoot.ToSharedRef());
		GPadRoot.Reset();
		GPadViewport.Reset();
		GPadWorld.Reset();
	}
}

// b82 (Evan: "add vibration on each tap"): every touch / click press anywhere (menus, buttons, the field) gives a short buzz.
// A Slate input pre-processor sees the press before any widget, so nothing can swallow it. -ZoogiNoTapBuzz = off.
class FZoogiTapBuzz : public IInputProcessor
{
public:
	virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent& E) override
	{
		if (E.IsTouchEvent() || E.GetEffectingButton() == EKeys::LeftMouseButton)
			if (UWorld* W = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr)
				ZoogiHaptics::Kick(0.55f, W, TEXT("tap"));
		return false; // never consumes the tap
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("ZoogiTapBuzz"); }
};

bool ZoogiClassicPad::PointerOwned()
{
	const double Dt = FPlatformTime::Seconds() - GPadPressAt;
	return (GPadHeld && Dt < 1.5) || Dt < 0.15; // b125: 1.5 s cap (was 6 s) // (a lost touch-end never blocks the field for more than 6 s); b120h: 0.15 s after the lift (was 0.35)
}

void ZoogiClassicPad::Ensure(UWorld* World)
{
	{
		static TSharedPtr<FZoogiTapBuzz> GTapBuzz;
		if (!GTapBuzz.IsValid() && FSlateApplication::IsInitialized() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoTapBuzz")))
		{
			GTapBuzz = MakeShared<FZoogiTapBuzz>();
			FSlateApplication::Get().RegisterInputPreProcessor(GTapBuzz);
			UE_LOG(LogZoogi, Log, TEXT("TAP BUZZ: every press vibrates"));
		}
	}
	if (!World || !World->IsGameWorld()) return;
	UGameViewportClient* VC = World->GetGameViewport();
	if (!VC) return;
	if (GPadRoot.IsValid() && GPadWorld.Get() == World && GPadViewport.Get() == VC) return;
	RemovePad();
	if (!GCleanup.IsValid())
		GCleanup = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* W, bool, bool) { if (W && W == GPadWorld.Get()) RemovePad(); });
	GPadWorld = World;
	GPadViewport = VC;
	GPadRoot = SNew(SZoogiClassicPadRoot);
	VC->AddViewportWidgetContent(GPadRoot.ToSharedRef(), 55); // above the UMG HUD (aim bar, arena HUD), under the Dev Mode overlay (60)
	UE_LOG(LogZoogi, Log, TEXT("CLASSIC PAD: REACTION / SUPERPOWER touch buttons added"));
}

FVector2D ZoogiClassicPad::PowerCenterPx(const FVector2D& ViewportPx) { return PowerCenterPxImpl(ViewportPx); } // b276
float ZoogiClassicPad::ReactLeftPx(const FVector2D& Px)
{	// b278: same spot as SZoogiClassicPadRoot::Pad(0) - REACTION sits on the right edge at 0.6 H, R = 1.3 JumpR; the burst reaches ~1.4 R
	const ZoogiRoyaleUI::FPad L = ZoogiRoyaleUI::Layout(Px);
	const float R = L.JumpR * 1.3f;
	return Px.X - 1.55f * R - 1.4f * R;
}
