// Oct 31: P5-style command menu for the human's turn (see ZoogiP5Command.h). Style only (own fonts / art, no Persona assets).
#include "ZoogiP5Command.h"
#include "ZoogiClassicPad.h" // b276
#include "ZoogiWallCine.h" // b232
#include "ZoogiGattling.h"
#include "ZoogiBossAttack.h" // b191
extern double GZoogiAttackJoltAt; // b193: ZoogiActionFun.cpp (global)
#include "ZoogiPlayerController.h"
#include "ZoogiSkillLab.h"
#include "ZoogiHaptics.h"
#include "ZoogiArenaHUDWidget.h"
#include "ZoogiP5Style.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiOrb.h"
#include "ZoogiRoll.h"
#include "ZoogiTypes.h"
#include "ZoogiCards.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiSkills.h"
#include "ZoogiGuide.h"
#include "ZoogiAnimeSubsystem.h"
#include "ZoogiActionFun.h"
#include "ZoogiRedirect.h"
#include "ZoogiEdgeHang.h"
#include "ZoogiShowtime.h"
#include "ZoogiCPUSmart.h"
#include "ZoogiGrenadeItem.h"
#include "ZoogiItems.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Containers/Ticker.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool ZoogiFallGuardAirborne(const AZoogiBall* B); // b285: ZoogiGameMode.cpp
namespace ZoogiP5Cmd
{
	bool On()
	{
		static const bool b = ZoogiP5::MenuOn() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldCards")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiCardHandUI")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMenu"));
		return b;
	}

	// ---- paint helpers (local pixel space)
	static const FSlateBrush* WhiteB() { return FCoreStyle::Get().GetBrush("WhiteBrush"); }
	static void CQuad(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Col)
	{
		const FSlateResourceHandle RH = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteB());
		const FSlateRenderTransform& RT = G.GetAccumulatedRenderTransform();
		const FColor Cc = Col.ToFColor(true);
		auto V = [&](const FVector2D& P) { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, FVector2f(P), FVector2f(0.5f, 0.5f), Cc); };
		TArray<FSlateVertex> Vs = { V(A), V(B), V(C), V(D) };
		TArray<SlateIndex> I = { 0, 1, 2, 0, 2, 3 };
		FSlateDrawElement::MakeCustomVerts(Out, L, RH, Vs, I, nullptr, 0, 0);
	}
	static void CBox(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& Ctr, const FVector2D& Size, float AngDeg, const FLinearColor& Col, const FSlateBrush* Brush = nullptr)
	{
		const FGeometry C = G.MakeChild(Size, FSlateLayoutTransform(Ctr - Size * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngDeg))), FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Out, L, C.ToPaintGeometry(), Brush ? Brush : WhiteB(), ESlateDrawEffect::None, Col);
	}
	static FVector2D CMeasure(const FString& S, const FSlateFontInfo& F) { return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F); }
	static void CText(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FString& S, const FSlateFontInfo& F, const FVector2D& Ctr, float AngDeg, const FLinearColor& Col)
	{
		const FVector2D Sz = CMeasure(S, F);
		const FGeometry C = G.MakeChild(Sz, FSlateLayoutTransform(Ctr - Sz * 0.5f), FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngDeg))), FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeText(Out, L, C.ToPaintGeometry(), S, F, ESlateDrawEffect::None, Col);
	}
	static void CTex(FSlateWindowElementList& Out, const FGeometry& G, int32 L, UTexture2D* T, const FVector2D& Ctr, const FVector2D& Size, float AngDeg, const FLinearColor& Col)
	{
		if (!T) return;
		FSlateBrush B; B.SetResourceObject(T); B.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
		CBox(Out, G, L, Ctr, Size, AngDeg, Col, &B);
	}
	static FVector2D Rot(const FVector2D& V, float Deg) { const float A = FMath::DegreesToRadians(Deg); return FVector2D(V.X * FMath::Cos(A) - V.Y * FMath::Sin(A), V.X * FMath::Sin(A) + V.Y * FMath::Cos(A)); }
	static float EaseOutBack(float T) { T = FMath::Clamp(T, 0.f, 1.f); const float C1 = 1.9f, C3 = C1 + 1.f; return 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f); }
	/** parallelogram plate (slanted ends) centred on C, rotated */
	static void Plate(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& C, float W, float H, float Ang, float Skew, const FLinearColor& Col)
	{
		const FVector2D P0 = C + Rot(FVector2D(-W * 0.5f + Skew, -H * 0.5f), Ang), P1 = C + Rot(FVector2D(W * 0.5f + Skew, -H * 0.5f), Ang);
		const FVector2D P2 = C + Rot(FVector2D(W * 0.5f - Skew, H * 0.5f), Ang), P3 = C + Rot(FVector2D(-W * 0.5f - Skew, H * 0.5f), Ang);
		CQuad(Out, G, L, P0, P1, P2, P3, Col);
	}
	/** jagged speech bubble (white, black outline), tail toward TailTo */
	static void Bubble(FSlateWindowElementList& Out, const FGeometry& G, int32 L, const FVector2D& C, float W, float H, const FVector2D& TailTo, float Age)
	{
		const int32 N = 22;
		TArray<FVector2D> P;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = UE_TWO_PI * k / N;
			const float J = 1.f + 0.07f * FMath::Sin(k * 2.3f + 0.7f) + ((k % 2) ? 0.06f : -0.02f);
			P.Add(C + FVector2D(FMath::Cos(A) * W * 0.5f * J, FMath::Sin(A) * H * 0.5f * J));
		}
		const float Pop = EaseOutBack(Age / 0.18f);
		auto S = [&](const FVector2D& Q, float Grow) { return C + (Q - C) * Pop * Grow; };
		// tail
		const FVector2D Dir = (TailTo - C).GetSafeNormal(), Side(-Dir.Y, Dir.X);
		const FVector2D T0 = C + Dir * H * 0.3f + Side * H * 0.14f, T1 = C + Dir * H * 0.3f - Side * H * 0.14f, T2 = C + (TailTo - C) * 0.82f;
		CQuad(Out, G, L, S(T0, 1.08f), S(T1, 1.08f), S(T2, 1.08f), S(T2, 1.08f), FLinearColor::Black);
		for (int32 k = 0; k < N; ++k) CQuad(Out, G, L, S(C, 1.f), S(P[k], 1.09f), S(P[(k + 1) % N], 1.09f), S(P[(k + 1) % N], 1.09f), FLinearColor::Black);
		CQuad(Out, G, L + 1, S(T0, 1.f), S(T1, 1.f), S(T2, 0.98f), S(T2, 0.98f), FLinearColor::White);
		for (int32 k = 0; k < N; ++k) CQuad(Out, G, L + 1, S(C, 1.f), S(P[k], 1.f), S(P[(k + 1) % N], 1.f), S(P[(k + 1) % N], 1.f), FLinearColor::White);
	}

	enum ECmd : int32 { Attack, Skill, Guard, Item, Power, Showtime, Gat, NumCmds }; // b194: Gat = the leader's SHOWTIME (Showtime = AWAKEN)
	struct FEntry { FString Label, Sub; bool bOn = true; FVector2D Off; float Ang = 0.f; float W = 300.f; };
	struct FSkillRow { FString Name, Rule; int32 Cost = 0, Slot = -1; bool bOn = false; };
	struct FParty { FString Name; int32 Score = 0, SP = -1; FLinearColor Col = FLinearColor::White; TWeakObjectPtr<UTexture2D> Face; bool bActive = false; };

	static double GP5TookAt = -10.0; static bool GP5Held = false; // b77: a press the menu took (the game ignores it to the release + 0.3 s)
	class SP5Cmd : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SP5Cmd) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) { SetCanTick(false); }
		// state written by the ticker
		bool bMenu = false;
		bool bPuppet = false;            // b67: the CPU is driving the menu (taps ignored)
		bool bMini = false;              // b73: aiming - only the SKILL plate stays up (skills until the launch)
		TFunction<void()> OnMini;        // b73: the SKILL plate tapped
		int32 Mode = 0;                  // 0 commands, 1 skills, 2 items
		bool bCombo = false; TArray<int32> ComboPicks; // b120: COMBO multi-pick (rows ticked, in pick order)
		float T = 0.f, ModeT = 0.f, ShowtimeT = -1.f;
		FString DescName, DescRule; float DescT = -1.f, DescFade = 1.f; // b280: the tapped skill's how-to at the bottom (landscape)
		FVector2D AnchorPx = FVector2D(-1, -1); // viewport pixels
		FVector2D ViewportPx = FVector2D(1, 1);
		FEntry E[NumCmds];
		TArray<FSkillRow> Skills;
		int32 SP = -1;
		FString BubbleLine, BubbleName, TargetName, TargetTag;
		TWeakObjectPtr<UTexture2D> BubbleFace;
		TArray<FParty> Party;
		FLinearColor Elem = FLinearColor(0.85f, 0.08f, 0.12f);
		int32 Hover = -1;
		int32 PrKind = -1, PrIdx = -9; float PrT = -10.f; // b156: the pressed plate squashes
		float PressK(int32 K, int32 I) const { return (PrKind == K && PrIdx == I) ? 1.f - 0.1f * FMath::Clamp(1.f - (T - PrT) / 0.22f, 0.f, 1.f) : 1.f; }
		TFunction<void(int32)> OnCmd;       // command index
		TFunction<void(int32)> OnSkill;     // skill row index, -1 = BACK
		TFunction<void()> OnOutside;        // tap outside the plates = start aiming (dismiss)
		// hit rects (local space, rotated plates approximated by their own frame)
		struct FHit { FVector2D C; FVector2D Half; float Ang; int32 Kind; int32 Index; };
		// b276 (Evan): during a REACTION pause the queued COMBO skills show as P5 plates around SUPERPOWER - tap one to cast it
		bool bReact = false; TArray<FString> ReactRows; TFunction<void(int32)> OnReact;
		mutable TArray<FHit> Hits;

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
		virtual bool ComputeVolatility() const override { return true; }

		int32 HitTest(const FGeometry& Geo, const FVector2D& Screen, int32& OutKind) const
		{
			const FVector2D P = Geo.AbsoluteToLocal(Screen);
			for (int32 i = Hits.Num() - 1; i >= 0; --i)
			{
				const FHit& H = Hits[i];
				const FVector2D Lp = Rot(P - H.C, -H.Ang);
				if (FMath::Abs(Lp.X) <= H.Half.X && FMath::Abs(Lp.Y) <= H.Half.Y) { OutKind = H.Kind; return H.Index; }
			}
			OutKind = -1;
			return -1;
		}
		FReply Press(const FGeometry& Geo, const FVector2D& Screen)
		{
			if (ZoogiArenaHUD::PressMenuAt(Screen)) return Took(); // b81: MENU works even under the command menu
			if (bReact) { int32 Kr = -1; const int32 Ir = HitTest(Geo, Screen, Kr); if (Kr == 7) { PrKind = Kr; PrIdx = Ir; PrT = T; if (OnReact) OnReact(Ir); return Took(); } return FReply::Unhandled(); } // b276
			if (!bMenu || bPuppet) return FReply::Unhandled();
			int32 Kind = -1;
			const int32 Idx = HitTest(Geo, Screen, Kind);
			if (Kind >= 0) { PrKind = Kind; PrIdx = Idx; PrT = T; if (UWorld* Wd = GWorld) ZoogiHaptics::Kick(0.25f, Wd, TEXT("ui_tap")); } // b156: squash + haptic
			if (bMini) { if (Kind == 0 && OnMini) { OnMini(); return Took(); } return FReply::Unhandled(); } // b73: anything else is the aim drag
			if (Kind == 0) { if (OnCmd) OnCmd(Idx); return Took(); }
			if (Kind == 1) { if (OnSkill) OnSkill(Idx); return Took(); }
			if (Mode != 0) { if (OnSkill) OnSkill(-1); return Took(); } // tap outside a sub list = back
			if (OnOutside) OnOutside();
			return FReply::Unhandled(); // the tap goes on to the game (starts the aim drag)
		}
		// b77: the menu keeps the whole touch (move / release never fall through to the aim drag or the target pick)
		FReply Took() { GP5TookAt = FPlatformTime::Seconds(); GP5Held = true; return FReply::Handled().CaptureMouse(SharedThis(this)); }
		FReply Let() { if (!GP5Held) return FReply::Unhandled(); GP5Held = false; GP5TookAt = FPlatformTime::Seconds(); return FReply::Handled().ReleaseMouseCapture(); }
		virtual FReply OnTouchMoved(const FGeometry&, const FPointerEvent&) override { return GP5Held ? FReply::Handled() : FReply::Unhandled(); }
		virtual FReply OnTouchEnded(const FGeometry&, const FPointerEvent&) override { return Let(); }
		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return Let(); }
		virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { GP5Held = false; GP5TookAt = FPlatformTime::Seconds(); }
		virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E2) override { return Press(Geo, E2.GetScreenSpacePosition()); }
		virtual FReply OnTouchStarted(const FGeometry& Geo, const FPointerEvent& E2) override { return Press(Geo, E2.GetScreenSpacePosition()); }
		virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E2) override { int32 K = -1; const int32 I = HitTest(Geo, E2.GetScreenSpacePosition(), K); Hover = K >= 0 ? K * 100 + I : -1; return FReply::Unhandled(); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			if (ZoogiWallCine::Active()) { Hits.Reset(); return Layer; } // b232: the wall-smash cinematic plays on a clean screen
			Hits.Reset();
			const FVector2D Sz = Geo.GetLocalSize();
			const float S = FMath::Clamp(FMath::Min(Sz.X, Sz.Y) / 1080.f, 0.45f, 2.f) * (Sz.X < Sz.Y ? 1.25f : 1.f); // portrait: a bit bigger for thumbs
			const FVector2D K(Sz.X / FMath::Max(1.f, (float)ViewportPx.X), Sz.Y / FMath::Max(1.f, (float)ViewportPx.Y));
			const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(224, 16, 32)), Blk = FLinearColor::Black, Wht = FLinearColor::White;
			int32 L = Layer;
			// ---- b280 (Evan: "in horizontal show a description at the bottom of the screen of the skill tapped so players know how to use it"):
			// a straight black band along the bottom, left of SUPERPOWER - red name tag + the card's rule text (two lines when long)
			if (DescT >= 0.f && Sz.X > Sz.Y && !DescRule.IsEmpty())
			{
				const float In = EaseOutBack(FMath::Clamp(DescT / 0.18f, 0.f, 1.f)), Al = FMath::Clamp(DescFade, 0.f, 1.f);
				const float Rx = (AnchorPx.X >= 0.f ? AnchorPx.X * K.X : Sz.X * 0.76f) - 110.f * S;
				const float X0 = 24.f * S, X1 = FMath::Max(X0 + 420.f * S, Rx), Bh = 86.f * S; // sits under the skill pick's FIRE / CANCEL row
				const float Y1 = Sz.Y - 12.f * S + (1.f - In) * 140.f * S, Y0 = Y1 - Bh;
				CQuad(Out, Geo, L, FVector2D(X0 - 6.f * S, Y0 - 6.f * S), FVector2D(X1 + 6.f * S, Y0 - 6.f * S), FVector2D(X1 + 6.f * S, Y1 + 6.f * S), FVector2D(X0 - 6.f * S, Y1 + 6.f * S), Wht.CopyWithNewOpacity(Al));
				CQuad(Out, Geo, L + 1, FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1), FLinearColor(0.02f, 0.02f, 0.02f, 0.94f * Al));
				// name tag
				FSlateFontInfo Nf = ZoogiP5::Font(0, 30.f * S);
				const float Nw = FMath::Min(CMeasure(DescName, Nf).X + 40.f * S, (X1 - X0) * 0.4f);
				if (CMeasure(DescName, Nf).X + 40.f * S > Nw) Nf = ZoogiP5::Font(0, FMath::Max(12.f, 30.f * S * (Nw - 40.f * S) / FMath::Max(1.f, CMeasure(DescName, Nf).X)));
				const FVector2D Tc(X0 + 14.f * S + Nw * 0.5f, Y0 + Bh * 0.5f);
				CQuad(Out, Geo, L + 2, Tc + FVector2D(-Nw * 0.5f, -30.f * S), Tc + FVector2D(Nw * 0.5f, -30.f * S), Tc + FVector2D(Nw * 0.5f, 30.f * S), Tc + FVector2D(-Nw * 0.5f, 30.f * S), Red.CopyWithNewOpacity(Al));
				CText(Out, Geo, L + 3, DescName, Nf, Tc, 0.f, Wht.CopyWithNewOpacity(Al));
				// rule text
				const float Tx0 = Tc.X + Nw * 0.5f + 20.f * S, Room = X1 - 18.f * S - Tx0;
				FSlateFontInfo Rf = ZoogiP5::Font(1, 24.f * S);
				TArray<FString> Lines;
				if (CMeasure(DescRule, Rf).X <= Room) Lines.Add(DescRule);
				else
				{	// split at the space nearest the middle
					int32 Best = -1, Mid = DescRule.Len() / 2;
					for (int32 c = 0; c < DescRule.Len(); ++c) if (DescRule[c] == TEXT(' ') && (Best < 0 || FMath::Abs(c - Mid) < FMath::Abs(Best - Mid))) Best = c;
					if (Best > 0) { Lines.Add(DescRule.Left(Best)); Lines.Add(DescRule.Mid(Best + 1)); } else Lines.Add(DescRule);
				}
				float Wmax = 1.f; for (const FString& Ln : Lines) Wmax = FMath::Max(Wmax, CMeasure(Ln, Rf).X);
				if (Wmax > Room) Rf = ZoogiP5::Font(1, FMath::Max(11.f, 24.f * S * Room / Wmax));
				const float Lh = (float)Rf.Size * 1.25f * (Lines.Num() > 1 ? 1.f : 0.f);
				for (int32 li = 0; li < Lines.Num(); ++li)
				{
					const FVector2D Ls = CMeasure(Lines[li], Rf);
					CText(Out, Geo, L + 3, Lines[li], Rf, FVector2D(Tx0 + Ls.X * 0.5f, Y0 + Bh * 0.5f + (li - (Lines.Num() - 1) * 0.5f) * Lh * 1.25f), 0.f, FLinearColor(1.f, 0.93f, 0.93f, Al));
				}
				L += 4;
			}
			// ---- SHOWTIME splash (also after the menu closed)
			if (ShowtimeT >= 0.f && ShowtimeT < 1.4f)
			{
				const float A = ShowtimeT;
				const float In = EaseOutBack(A / 0.22f), Outk = FMath::Clamp((A - 1.1f) / 0.3f, 0.f, 1.f);
				const float Wd = Sz.X * 1.4f * In;
				CQuad(Out, Geo, L, FVector2D(Sz.X * 0.5f - Wd * 0.5f, Sz.Y * 0.36f), FVector2D(Sz.X * 0.5f + Wd * 0.5f, Sz.Y * 0.28f), FVector2D(Sz.X * 0.5f + Wd * 0.5f, Sz.Y * 0.56f), FVector2D(Sz.X * 0.5f - Wd * 0.5f, Sz.Y * 0.66f), Red.CopyWithNewOpacity(1.f - Outk));
				CQuad(Out, Geo, L + 1, FVector2D(Sz.X * 0.5f - Wd * 0.5f, Sz.Y * 0.40f), FVector2D(Sz.X * 0.5f + Wd * 0.5f, Sz.Y * 0.33f), FVector2D(Sz.X * 0.5f + Wd * 0.5f, Sz.Y * 0.52f), FVector2D(Sz.X * 0.5f - Wd * 0.5f, Sz.Y * 0.61f), Blk.CopyWithNewOpacity(1.f - Outk));
				const FString W = TEXT("AWAKEN!"); // b186: was SHOWTIME!
				const float Base = 120.f * S;
				float X = Sz.X * 0.5f - Base * 2.7f;
				for (int32 i = 0; i < W.Len(); ++i)
				{
					const float Li = FMath::Clamp((A - 0.05f * i) / 0.14f, 0.f, 1.f);
					if (Li <= 0.f) continue;
					const float Sl = 1.f + 1.2f * (1.f - EaseOutBack(Li));
					const bool bW = i % 3 != 1;
					const FVector2D Cc(X, Sz.Y * 0.47f - (i % 2) * 10.f * S);
					CBox(Out, Geo, L + 2, Cc, FVector2D(Base * 0.62f, Base * 0.86f) * Sl, -8.f + (i % 3) * 5.f, bW ? Wht : Red);
					CText(Out, Geo, L + 3, W.Mid(i, 1), ZoogiP5::Font(i % 2 ? 3 : 0, Base * 0.66f * Sl), Cc, -8.f + (i % 3) * 5.f, bW ? Blk : Wht);
					X += Base * 0.6f;
				}
				L += 5;
			}
			if (bReact && AnchorPx.X >= 0.f)
			{	// b276: the combo skills, P5 plates like ITEM / SKILL / GUARD / ATTACK, stacked up from the SUPERPOWER button
				const FVector2D A = AnchorPx * K;
				for (int32 i = 0; i < ReactRows.Num(); ++i)
				{
					const float Ai = EaseOutBack((T - 0.05f * i) / 0.2f);
					if (Ai <= 0.f) continue;
					const float Ang = (i % 2) ? 6.f : -8.f;
					const float W = 285.f * S * PressK(7, i), H = 70.f * S * PressK(7, i); // b278: a bit narrower to fit between SUPERPOWER and REACTION
					// b277 (Evan: "more to the right of super power button"): plates stack up on the RIGHT side of SUPERPOWER, kept on screen
					// b278 (B277 preview: the plates covered REACTION): stack UP from the top-right of SUPERPOWER, and never past REACTION's left edge -
					// REACTION trumps the plates, they share the space beside it
					FVector2D C = A + FVector2D((165.f + (i % 2) * 18.f) * S, (-150.f - 88.f * i) * S) * Ai;
					const float RL = ZoogiClassicPad::ReactLeftPx(ViewportPx) * K.X;
					C.X = FMath::Max(W * 0.5f + 20.f * S, FMath::Min(C.X, FMath::Min(RL - 10.f * S, Sz.X - 24.f * S) - W * 0.5f));
					const bool bHov = Hover == 700 + i;
					Plate(Out, Geo, L, C + FVector2D(7.f, 8.f) * S, W, H, Ang, 14.f * S, i == 0 ? Blk : Red);
					Plate(Out, Geo, L + 1, C, W + 8.f * S, H + 8.f * S, Ang, 14.f * S, Wht);
					Plate(Out, Geo, L + 2, C, W, H, Ang, 14.f * S, (i == 0 || bHov) ? Red : Blk);
					const FVector2D Tab = C + Rot(FVector2D(-W * 0.5f + 26.f * S, 0.f), Ang);
					CBox(Out, Geo, L + 3, Tab, FVector2D(34.f, 34.f) * S, Ang + 45.f, Wht);
					CText(Out, Geo, L + 4, FString::FromInt(i + 1), ZoogiP5::Font(1, 18.f * S), Tab, Ang, Blk);
					const FString& Nm = ReactRows[i];
					FSlateFontInfo Big = ZoogiP5::Font(0, 40.f * S);
					const float Room = W - 80.f * S; const FVector2D Ms = CMeasure(Nm, Big); if (Ms.X > Room && Ms.X > 1.f) Big = ZoogiP5::Font(0, FMath::Max(14.f, 40.f * S * Room / Ms.X));
					const FVector2D LabC = C + Rot(FVector2D(18.f * S, -8.f * S), Ang);
					CText(Out, Geo, L + 4, Nm, Big, LabC, Ang, Wht);
					CText(Out, Geo, L + 4, TEXT("Tap to cast"), ZoogiP5::Font(1, 15.f * S), LabC + Rot(FVector2D(0.f, 30.f * S), Ang), Ang, FLinearColor(1.f, 0.82f, 0.86f));
					Hits.Add({ C, FVector2D(W * 0.5f + 8.f * S, H * 0.5f + 6.f * S), Ang, 7, i });
				}
				return L + 6;
			}
			if (bMenu && bMini)
			{	// b73 (Evan: "don't hide skills until the player has launched - no exceptions, they must be able to deplete SP"): a SKILL plate
				// left of centre while aiming; tap = the skill list again
				const FEntry& En = E[Skill];
				const float W = 250.f * S * PressK(0, Skill), H = 70.f * S * PressK(0, Skill), Ang = -8.f;
				FVector2D C(W * 0.5f + 40.f * S, Sz.Y * (Sz.X < Sz.Y ? 0.6f : 0.55f));
				if (Sz.X >= Sz.Y && AnchorPx.X >= 0.f) { const FVector2D Az = AnchorPx * K; C = FVector2D(FMath::Clamp(Az.X - 230.f * S, W * 0.5f + 20.f * S, Sz.X - W * 0.5f - 20.f * S), FMath::Clamp(Az.Y + 30.f * S, H + 20.f * S, Sz.Y - H - 20.f * S)); } // b76 (Evan): landscape - right beside the Zoogi
				const float Pulse = 1.f + 0.03f * FMath::Sin(T * 6.f);
				Plate(Out, Geo, L, C + FVector2D(7.f, 8.f) * S, W * Pulse, H, Ang, 14.f * S, Blk);
				Plate(Out, Geo, L + 1, C, (W + 8.f * S) * Pulse, H + 8.f * S, Ang, 14.f * S, Wht);
				Plate(Out, Geo, L + 2, C, W * Pulse, H, Ang, 14.f * S, Red);
				CText(Out, Geo, L + 3, TEXT("SKILL"), ZoogiP5::Font(0, 42.f * S), C + Rot(FVector2D(-20.f * S, -6.f * S), Ang), Ang, Wht);
				CText(Out, Geo, L + 3, SP >= 0 ? FString::Printf(TEXT("SP %d"), SP) : FString(), ZoogiP5::Font(1, 16.f * S), C + Rot(FVector2D(70.f * S, 20.f * S), Ang), Ang, FLinearColor(1.f, 0.85f, 0.88f));
				(void)En;
				Hits.Add({ C, FVector2D(W * 0.5f + 8.f * S, H * 0.5f + 6.f * S), Ang, 0, Skill });
				return L + 4;
			}
			if (!bMenu || AnchorPx.X < 0.f) return L;
			const FVector2D A = AnchorPx * K;
			const float Open = EaseOutBack(T / 0.22f);
			// ---- red slash burst behind the Zoogi (the P5 "command" splash) - Oct 31 b42 (Evan): removed, blank bars covered the Zoogi
			if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CmdSlash")))
			{
				const float R = 230.f * S * Open;
				CQuad(Out, Geo, L, A + FVector2D(-R * 0.9f, R * 0.55f), A + FVector2D(R * 0.15f, -R * 0.95f), A + FVector2D(R * 0.38f, -R * 0.82f), A + FVector2D(-R * 0.55f, R * 0.78f), Red);
				CQuad(Out, Geo, L, A + FVector2D(-R * 0.35f, R * 0.95f), A + FVector2D(R * 0.85f, -R * 0.2f), A + FVector2D(R * 0.95f, -R * 0.02f), A + FVector2D(-R * 0.12f, R * 1.05f), Blk);
				L += 1;
			}
			if (Mode == 0)
			{
				for (int32 i = 0; i < NumCmds; ++i)
				{
					const FEntry& En = E[i];
					if (En.Label.IsEmpty()) continue;
					const float Ai = EaseOutBack((T - 0.04f * i) / 0.2f);
					if (Ai <= 0.f) continue;
					const bool bHov = Hover == i;
					const float J = bHov ? 3.f * FMath::Sin(T * 31.f) : 0.f;
					const FVector2D C = A + En.Off * S * Ai + FVector2D(J, 0.f);
					const float W = En.W * S * PressK(0, i), H = 74.f * S * PressK(0, i);
					const FLinearColor GoldF(1.f, 0.78f, 0.18f); // b194: the SHOWTIME plate is gold
					const FLinearColor Face = !En.bOn ? FLinearColor(0.08f, 0.08f, 0.08f, 0.85f) : (i == Gat ? GoldF : (bHov || i == Attack ? Red : Blk));
					Plate(Out, Geo, L, C + FVector2D(7.f, 8.f) * S, W, H, En.Ang, 14.f * S, En.bOn ? (i == Attack ? Blk : Red) : FLinearColor(0.f, 0.f, 0.f, 0.5f)); // offset block
					Plate(Out, Geo, L + 1, C, W + 8.f * S, H + 8.f * S, En.Ang, 14.f * S, En.bOn ? Wht : FLinearColor(0.5f, 0.5f, 0.5f, 0.6f));
					Plate(Out, Geo, L + 2, C, W, H, En.Ang, 14.f * S, Face);
					// key tab
					const FVector2D Tab = C + Rot(FVector2D(-W * 0.5f + 26.f * S, 0.f), En.Ang);
					CBox(Out, Geo, L + 3, Tab, FVector2D(34.f, 34.f) * S, En.Ang + 45.f, En.bOn ? Wht : FLinearColor(0.4f, 0.4f, 0.4f));
					CText(Out, Geo, L + 4, FString::FromInt(i + 1), ZoogiP5::Font(1, 18.f * S), Tab, En.Ang, Blk);
					// label: first letter big (ransom accent), rest Anton
					const FSlateFontInfo Big = ZoogiP5::Font(0, 46.f * S), Small = ZoogiP5::Font(1, 15.f * S);
					const FVector2D LabSz = CMeasure(En.Label, Big);
					const FVector2D LabC = C + Rot(FVector2D(18.f * S, -8.f * S), En.Ang);
					CText(Out, Geo, L + 4, En.Label, Big, LabC, En.Ang, En.bOn ? (i == Gat ? Blk : Wht) : FLinearColor(0.55f, 0.55f, 0.55f));
					CText(Out, Geo, L + 4, En.Sub, Small, LabC + Rot(FVector2D(LabSz.X * 0.15f, 30.f * S), En.Ang), En.Ang, En.bOn ? (i == Attack || bHov ? Wht : FLinearColor(1.f, 0.82f, 0.86f)) : FLinearColor(0.45f, 0.45f, 0.45f));
					Hits.Add({ C, FVector2D(W * 0.5f + 8.f * S, H * 0.5f + 6.f * S), En.Ang, 0, i });
				}
				L += 6;
			}
			else
			{
				// ---- skill / item list: a tilted black panel right of the Zoogi, rows of name + SP cost, rule text under the hovered row
				const float Rows = FMath::Max(1, Skills.Num()) + 1;
				const float RowH = 64.f * S, PW = 560.f * S;
				FVector2D P0 = A + FVector2D(90.f * S, -RowH * Rows * 0.5f);
				P0.X = FMath::Min(P0.X, Sz.X - PW - 20.f * S); P0.Y = FMath::Clamp(P0.Y, 70.f * S, Sz.Y - RowH * (Rows + 1.5f));
				const float Ai = EaseOutBack(ModeT / 0.2f);
				CQuad(Out, Geo, L, P0 + FVector2D(-20.f * S, -46.f * S), P0 + FVector2D(PW * Ai + 30.f * S, -64.f * S), P0 + FVector2D(PW * Ai + 10.f * S, RowH * Rows + 30.f * S), P0 + FVector2D(-34.f * S, RowH * Rows + 46.f * S), Blk);
				int32 ComboCost = 0; for (int32 Ci : ComboPicks) if (Skills.IsValidIndex(Ci)) ComboCost += Skills[Ci].Cost;
				const FString Hdr = Mode != 1 ? FString(TEXT("ITEM")) : bCombo ? FString::Printf(TEXT("COMBO   SP %d / %d"), ComboCost, FMath::Max(SP, 0)) : FString::Printf(TEXT("SKILL   SP %d"), FMath::Max(SP, 0));
				CText(Out, Geo, L + 1, Hdr, ZoogiP5::Font(0, 34.f * S), P0 + FVector2D((bCombo ? 190.f : 150.f) * S, -28.f * S), -3.f, Red);
				for (int32 r = 0; r < Skills.Num(); ++r)
				{
					const FSkillRow& Sr = Skills[r];
					const float Rr = EaseOutBack((ModeT - 0.04f * r) / 0.18f);
					if (Rr <= 0.f) continue;
					const bool bHov = Hover == 100 + r;
					const FVector2D C = P0 + FVector2D(PW * 0.5f + (1.f - Rr) * 300.f * S, RowH * (r + 0.6f));
					Plate(Out, Geo, L + 1, C, (PW - 30.f * S) * PressK(1, r), (RowH - 10.f * S) * PressK(1, r), -2.f, 10.f * S, bHov ? Red : (Sr.bOn ? FLinearColor(0.12f, 0.12f, 0.12f) : FLinearColor(0.06f, 0.06f, 0.06f)));
					CText(Out, Geo, L + 2, Sr.Name, ZoogiP5::Font(0, 30.f * S), C + FVector2D(-PW * 0.5f + 30.f * S + CMeasure(Sr.Name, ZoogiP5::Font(0, 30.f * S)).X * 0.5f, 0.f), -2.f, Sr.bOn ? Wht : FLinearColor(0.45f, 0.45f, 0.45f));
					if (Sr.Cost > 0) CText(Out, Geo, L + 2, FString::Printf(TEXT("%d SP"), Sr.Cost), ZoogiP5::Font(1, 22.f * S), C + FVector2D(PW * 0.5f - 70.f * S, 0.f), -2.f, Sr.bOn ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(0.45f, 0.45f, 0.45f));
					Hits.Add({ C, FVector2D((PW - 30.f * S) * 0.5f, (RowH - 10.f * S) * 0.5f), -2.f, 1, r });
					const int32 PickN = bCombo ? ComboPicks.IndexOfByKey(r) : INDEX_NONE;
					if (PickN != INDEX_NONE)
					{	// b120: COMBO order badge on the ticked row
						const FVector2D Bd = C + FVector2D(-PW * 0.5f + 6.f * S, 0.f);
						Plate(Out, Geo, L + 3, Bd, 44.f * S, 44.f * S, 6.f, 6.f * S, Red);
						CText(Out, Geo, L + 4, FString::FromInt(PickN + 1), ZoogiP5::Font(0, 30.f * S), Bd, 6.f, Wht);
					}
					if (bHov && !Sr.Rule.IsEmpty()) CText(Out, Geo, L + 3, Sr.Rule, ZoogiP5::Font(1, 16.f * S), P0 + FVector2D(PW * 0.5f, RowH * Rows + 8.f * S), -2.f, FLinearColor(1.f, 0.9f, 0.9f));
				}
				// BACK
				const FVector2D Bc = P0 + FVector2D(PW - 90.f * S, RowH * (Skills.Num() + 0.6f));
				Plate(Out, Geo, L + 1, Bc, 150.f * S, RowH - 14.f * S, -2.f, 10.f * S, Hover == 99 ? Red : Wht);
				CText(Out, Geo, L + 2, TEXT("BACK"), ZoogiP5::Font(0, 28.f * S), Bc, -2.f, Hover == 99 ? Wht : Blk);
				Hits.Add({ Bc, FVector2D(75.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -1 });
				static const bool bNoCombo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCombo"));
				if (Mode == 1 && !bNoCombo)
				{	// b120 (Evan: "CPUs use multiple skills at once - how can a player do the same"): COMBO toggle + CAST
					const FVector2D Cc = P0 + FVector2D(105.f * S, RowH * (Skills.Num() + 0.6f));
					Plate(Out, Geo, L + 1, Cc, 180.f * S, RowH - 14.f * S, -2.f, 10.f * S, bCombo ? Red : FLinearColor(0.12f, 0.12f, 0.12f));
					CText(Out, Geo, L + 2, bCombo ? TEXT("COMBO ON") : TEXT("COMBO"), ZoogiP5::Font(0, 26.f * S), Cc, -2.f, Wht);
					Hits.Add({ Cc, FVector2D(90.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -2 });
					if (bCombo && ComboPicks.Num() > 0)
					{
						const FVector2D Kc = P0 + FVector2D(310.f * S, RowH * (Skills.Num() + 0.6f));
						const float Pu = 1.f + 0.05f * FMath::Sin(T * 9.f);
						Plate(Out, Geo, L + 1, Kc, 200.f * S * Pu, (RowH - 14.f * S) * Pu, -2.f, 10.f * S, FLinearColor(1.f, 0.82f, 0.15f));
						CText(Out, Geo, L + 2, FString::Printf(TEXT("CAST %d!"), ComboPicks.Num()), ZoogiP5::Font(0, 28.f * S), Kc, -2.f, Blk);
						Hits.Add({ Kc, FVector2D(100.f * S, (RowH - 14.f * S) * 0.5f), -2.f, 1, -3 });
					}
				}
				L += 4;
			}
			// ---- speech bubble from a portrait (top right)
			if (!BubbleLine.IsEmpty() && T < 4.f)
			{
				const float By = Sz.Y * 0.25f + 78.f * S; // b78 (Evan): never over the profile avatars - below their band
				const FVector2D Face(Sz.X - 340.f * S, By); // b275 (Evan: nothing covered): left of the MENU button / timer column (it sat on them)
				const FVector2D BC(Sz.X - 750.f * S, By - 2.f * S);
				Bubble(Out, Geo, L, BC, 640.f * S, 120.f * S, Face, T);
				const float Pop = EaseOutBack(T / 0.2f);
				CText(Out, Geo, L + 2, BubbleLine, ZoogiP5::Font(1, 24.f * S * Pop), BC, -2.f, Blk);
				CBox(Out, Geo, L + 1, Face, FVector2D(150.f, 150.f) * S * Pop, 8.f, Blk);
				CTex(Out, Geo, L + 2, BubbleFace.Get(), Face, FVector2D(136.f, 136.f) * S * Pop, 8.f, Wht);
				CText(Out, Geo, L + 3, BubbleName, ZoogiP5::Font(0, 22.f * S), Face + FVector2D(0.f, 78.f * S), 8.f, Wht);
				L += 4;
			}
			// ---- target name tag (top left, tilted)
			if (!TargetName.IsEmpty())
			{
				const FVector2D C(260.f * S, Sz.Y * 0.25f + 60.f * S); // b78: below the profile band
				CQuad(Out, Geo, L, C + FVector2D(-230.f, 10.f) * S, C + FVector2D(250.f, -48.f) * S, C + FVector2D(258.f, -14.f) * S, C + FVector2D(-224.f, 44.f) * S, Blk);
				CQuad(Out, Geo, L + 1, C + FVector2D(-220.f, 14.f) * S, C + FVector2D(244.f, -42.f) * S, C + FVector2D(248.f, -18.f) * S, C + FVector2D(-216.f, 38.f) * S, Wht);
				CText(Out, Geo, L + 2, TargetName, ZoogiP5::Font(2, 30.f * S), C + FVector2D(20.f, -2.f) * S, -7.f, Blk);
				CBox(Out, Geo, L + 2, C + FVector2D(-190.f, 26.f) * S, FVector2D(70.f, 30.f) * S, -7.f, Blk);
				CText(Out, Geo, L + 3, TargetTag, ZoogiP5::Font(0, 20.f * S), C + FVector2D(-190.f, 26.f) * S, -7.f, Wht);
				L += 4;
			}
			// ---- party strip (bottom right): portrait + score + SP in slanted cells - Oct 31 b42 (Evan): removed, it sat over the bottom buttons
			if (Party.Num() > 0 && FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CmdParty")))
			{
				const float CW = 190.f * S, CH = 96.f * S;
				for (int32 p = 0; p < Party.Num(); ++p)
				{
					const FParty& Pa = Party[p];
					const FVector2D C(Sz.X - (Party.Num() - p) * (CW + 6.f * S) + CW * 0.5f - 16.f * S, Sz.Y - CH * 0.62f - 18.f * S + (Pa.bActive ? -10.f * S : 0.f));
					Plate(Out, Geo, L, C, CW, CH, -6.f, 18.f * S, Blk);
					CBox(Out, Geo, L + 1, C + FVector2D(-CW * 0.3f, -8.f * S), FVector2D(76.f, 76.f) * S, -6.f, Pa.bActive ? Red : Pa.Col);
					CTex(Out, Geo, L + 2, Pa.Face.Get(), C + FVector2D(-CW * 0.3f, -8.f * S), FVector2D(70.f, 70.f) * S, -6.f, Wht);
					CText(Out, Geo, L + 2, FString::FromInt(Pa.Score), ZoogiP5::Font(0, 34.f * S), C + FVector2D(CW * 0.18f, -14.f * S), -6.f, Wht);
					CQuad(Out, Geo, L + 2, C + FVector2D(-CW * 0.02f, 16.f * S), C + FVector2D(CW * 0.42f, 10.f * S), C + FVector2D(CW * 0.42f, 18.f * S), C + FVector2D(-CW * 0.02f, 24.f * S), FLinearColor(0.2f, 1.f, 0.6f));
					if (Pa.SP >= 0) CText(Out, Geo, L + 2, FString::Printf(TEXT("SP %d"), Pa.SP), ZoogiP5::Font(1, 16.f * S), C + FVector2D(CW * 0.2f, 34.f * S), -6.f, FLinearColor(1.f, 0.35f, 0.75f));
				}
				L += 3;
			}
			return L;
		}
	};

	struct FState
	{
		TSharedPtr<SP5Cmd> W;
		TWeakObjectPtr<UGameViewportClient> VC;
		TWeakObjectPtr<AZoogiGameMode> G;
		int32 DismissTurn = -1, DismissPlayer = -1, OpenTurn = -1, OpenPlayer = -1;
		double OpenAt = 0.0, ModeAt = 0.0, ShowAt = -100.0;
		int32 LineIdx = 0;
		bool bOpenSkills = false; // b67: next open lands on the skill list
		int32 SkillsTurn = -1;    // b76: the skill list was in use this turn - every re-show lands on it until BACK
		// b67: the CPU goes through the menu like a person
		int32 CpuTurn = -1, CpuPlayer = -1, CpuStep = 0; double CpuAt = 0.0; bool bCpuMenu = false;
		struct FCpuStep { float At = 0.f; int32 Hover = -1; int32 Act = 0; }; // Act: 0 hover, 1 open SKILL list, 2 confirm a card, 3 back, 4 ATTACK (close)
		TArray<FCpuStep> CpuSteps; TArray<FSkillRow> CpuRows; TArray<FName> CpuCasts;
		TArray<FName> CpuRollCasts; double CpuRollNextAt = 0.0; // b124: skills the CPU casts while rolling
		// b120 COMBO: rows ticked in the skill list, then the queue of card ids played back to back
		bool bCombo = false; TArray<int32> ComboSel; TArray<FName> ComboQueue; int32 ComboTurn = -1; double ComboNextAt = 0.0;
		TArray<int32> SpecialQueue; int32 SpecialTurn = -1; double SpecialNextAt = 0.0; // b194: combo AWAKEN (-301) / SHOWTIME (-302)
		// -ZoogiP5CmdTest
		int32 TestTurns = 0, TestStep = 0;
		FString ShotDir;
	};
	static FState GS;
	static FString GDescName, GDescRule; static double GDescAt = 0.0; // b280
	static void ShowDesc(const FString& N, const FString& R) { if (R.IsEmpty()) return; GDescName = N; GDescRule = R; GDescAt = FPlatformTime::Seconds(); }
	static TWeakObjectPtr<AZoogiBall> GKeepBall; static float GKeepSpd = 0.f; static FVector GKeepDir = FVector::ZeroVector; static double GKeepAt = 0.0; // b279: roll kept across a mid-roll skill
	static float P5Snap() { static const float K = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSlowFx")) ? 1.f : 1.8f; return K; } // b190: menu animations ~1.8x faster
	static bool GShowing = false;
	bool Showing() { return GShowing; }
	bool InputBlocked() { return (GP5Held && FPlatformTime::Seconds() - GP5TookAt < 1.5) || FPlatformTime::Seconds() - GP5TookAt < 0.3; } // b125: a lost touch-end never blocks taps for good
	void Reopen() { GS.DismissTurn = -1; GS.DismissPlayer = -1; GS.OpenTurn = -1; GS.OpenPlayer = -1; UE_LOG(LogZoogi, Log, TEXT("P5CMD reopen (target tapped again)")); }
	// b119b (Evan 16:42: "I need skills to always open this menu, not straight to the list"): every re-show lands on the main
	// ATTACK / SKILL / GUARD / ITEM menu (-ZoogiSkillListDirect = the b76 straight-to-the-list)
	static bool ListDirect() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSkillListDirect")); return b; }
	void BackToSkills()
	{
		if (GS.ComboQueue.Num()) UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO queue dropped (%d left)"), GS.ComboQueue.Num());
		GS.ComboQueue.Reset(); // b120
		Reopen();
		if (ListDirect()) { GS.bOpenSkills = true; if (AZoogiGameMode* Gk = GS.G.Get()) GS.SkillsTurn = Gk->TurnCounter; }
		else { GS.bOpenSkills = false; GS.SkillsTurn = -1; }
		UE_LOG(LogZoogi, Log, TEXT("P5CMD back to the %s"), ListDirect() ? TEXT("skill list") : TEXT("command menu"));
	}

	static AZoogiBall* Human(AZoogiGameMode* G)
	{
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		return (B && !B->bCPU) ? B : nullptr;
	}
	static void Shot(const FString& Name)
	{
		if (GS.ShotDir.IsEmpty()) return;
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(GS.ShotDir, FString::Printf(TEXT("p5cmd_%s.png"), *Name)), true, false); // with UI: the menu is a Slate overlay
		UE_LOG(LogZoogi, Log, TEXT("P5CMD capture %s"), *Name);
	}
	static void Dismiss(AZoogiGameMode* G)
	{
		if (!G) return;
		GS.DismissTurn = G->TurnCounter; GS.DismissPlayer = G->ActivePlayer;
		if (GS.W.IsValid()) { GS.W->Mode = 0; GS.W->bMenu = false; }
		// b187 (Evan: "after selecting the skill remove the wait to aim"): the menu folds before the finger lifts, so the release never
		// reached it and taps on the field were blocked for 1.5 s - a dismissed menu lets go of the press now (0.1 s guard)
		if (GP5Held) { GP5Held = false; GP5TookAt = FPlatformTime::Seconds() - 0.2; }
	}
	void ToSkillButton()
	{	// b119b (Evan 16:40: "forget what I said about the skills menu"): the b79 fold is back (-ZoogiMenuStays = the b119 stay-open)
		static const bool bFold = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiMenuStays"));
		if (!bFold) { BackToSkills(); return; }
		if (AZoogiGameMode* G = GS.G.Get()) { Dismiss(G); UE_LOG(LogZoogi, Log, TEXT("P5CMD folded to the SKILL button (skill played)")); }
	}
	static int32 GuardSlot(AZoogiGameMode* G, AZoogiBall* B)
	{
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn() && B) ? CS->Rules().Player(B->PlayerIndex) : nullptr;
		if (!PS) return -1;
		for (int32 k = 0; k < PS->Hand.Num(); ++k)
			if (PS->Hand[k].CardId.ToString().Contains(TEXT("guard")) && G->CardsSlotPlayableNow(k))
				if (const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId)) if (D->Cost <= PS->Energy) return k;
		return -1;
	}
	static void DoCmd(int32 I)
	{
		AZoogiGameMode* G = GS.G.Get();
		AZoogiBall* B = Human(G);
		if (!G || !B || !GS.W.IsValid()) return;
		SP5Cmd& W = *GS.W;
		if (I < 0 || I >= NumCmds || !W.E[I].bOn) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 1.f + 0.05f * I);
		UE_LOG(LogZoogi, Log, TEXT("P5CMD P%d %s"), B->PlayerIndex + 1, *W.E[I].Label);
		switch (I)
		{
		case Attack:
		{	// b193 (Evan: "ATTACK sends an instant jolt launch forward - a tunnel vision effect - then it's REACTION only"): launch at
			// once along the current aim (strong), the tunnel-vision rush plays; the roll is steered with REACTION as usual
			// (-ZoogiAttackAims = the old ATTACK, which only closes the menu so you aim yourself)
			static const bool bOldAttack = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAttackAims"));
			Dismiss(G);
			if (bOldAttack || G->Phase != EZoogiPhase::Aiming || B->bCPU) break;
			FVector Dir = FRotator(0.f, B->AimYaw, 0.f).Vector();
			if (G->bFreeAim) { const FVector Fd = (G->FreeAimPoint - B->BallLocation()).GetSafeNormal2D(); if (!Fd.IsNearlyZero()) Dir = Fd; } // the free-aim point is the aim
			G->SetAim(true, Dir * (ZoogiConst::FullPull * 0.85f / ZoogiConst::DragScale));
			if (G->ReleaseAim())
			{
				::GZoogiAttackJoltAt = FPlatformTime::Seconds();
				if (AZoogiPlayerController* Zp = Cast<AZoogiPlayerController>(G->GetWorld()->GetFirstPlayerController())) Zp->OnImpact(0.7f);
				ZoogiHaptics::Kick(0.8f, G->GetWorld(), TEXT("attack_jolt"));
				ZoogiUtil::PlaySfx(G, TEXT("ArcWhoosh"), 1.f, 1.2f);
				UE_LOG(LogZoogi, Log, TEXT("P5CMD ATTACK jolt launch P%d along yaw %.0f"), B->PlayerIndex + 1, B->AimYaw);
			}
			break;
		}
		case Skill: W.Mode = 1; GS.SkillsTurn = G->TurnCounter; GS.ModeAt = FPlatformTime::Seconds(); break;
		case Item: W.Mode = 2; GS.ModeAt = FPlatformTime::Seconds(); break;
		case Guard: { const int32 Gs = GuardSlot(G, B); if (Gs >= 0 && !ZoogiSkills::GuardBusy(G, B->PlayerIndex)) { G->MenuAction(TEXT("CardPlay"), Gs); ZoogiSkills::GuardArm(G, B->PlayerIndex); } Dismiss(G); break; } // b83: once, then the barrier
		case Power: G->MenuAction(TEXT("ClassicPower"), 0); Dismiss(G); break;
		case Gat: Dismiss(G); if (!ZoogiGattling::StartFromMenu(G)) ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); break; // b194
		case Showtime: G->MenuAction(TEXT("AwakenUse"), 0); ZoogiShowtime::Begin(G, B); // b122: rumble, rune seals, lights dim, energy buff, then the target pick
			 GS.ShowAt = FPlatformTime::Seconds(); ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.6f, 1.1f); Dismiss(G); break;
		default: break;
		}
	}
	static void DoSkill(int32 R)
	{
		AZoogiGameMode* G = GS.G.Get();
		if (!G || !GS.W.IsValid()) return;
		SP5Cmd& W = *GS.W;
		if (R >= 0 && W.Skills.IsValidIndex(R)) ShowDesc(W.Skills[R].Name, W.Skills[R].Rule); // b280: how to use what was tapped
		if (W.Mode == 2 && R >= 0)
		{	// b130: ITEM list - the grenade opens its throw pick
			if (W.Skills.IsValidIndex(R) && W.Skills[R].Slot <= -100 && W.Skills[R].bOn)
			{	// b137: every item row: -100 - index into ZoogiItems::WeaponIds()
				const int32 Wi = -100 - W.Skills[R].Slot;
				if (AZoogiBall* Hb = Human(G)) if (ZoogiItems::WeaponIds().IsValidIndex(Wi) && ZoogiSkills::BeginWeapon(G, Hb, ZoogiItems::WeaponIds()[Wi]))
				{
					if (ZoogiSkills::Picking()) Dismiss(G); else ToSkillButton();
					return;
				}
			}
			ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f);
			return;
		}
		if (R == -2 && W.Mode == 1)
		{	// b120: COMBO on / off
			GS.bCombo = !GS.bCombo; GS.ComboSel.Reset();
			ZoogiUtil::PlaySfx(G, GS.bCombo ? TEXT("UiConfirm") : TEXT("UiCancel"), 0.7f, GS.bCombo ? 1.3f : 1.f);
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO %s"), GS.bCombo ? TEXT("on") : TEXT("off"));
			return;
		}
		if (R == -3 && W.Mode == 1)
		{	// b120: CAST - the ticked skills play back to back (Tick works the queue)
			const UZoogiCardSubsystem* CSq = UZoogiCardSubsystem::Get(G);
			const AZoogiBall* Hq = Human(G);
			const FZoogiCardPlayerState* PSq = (CSq && Hq) ? CSq->Rules().Player(Hq->PlayerIndex) : nullptr;
			// b120g (Evan: "don't start the skills or their effects until after launch and SUPERPOWER is tapped"): CAST arms them -
			// the cards are committed now, each SUPERPOWER tap after the launch casts the next one from the Zoogi
			// b195 (Evan: "AWAKEN and SHOWTIME go off in the order they were cast"): one ordered list - each ticked row (skill, AWAKEN,
			// SHOWTIME) goes into the SUPERPOWER queue in tick order; AWAKEN's target pick happens now, before the launch
			TArray<FName> Ids;
			if (PSq) for (int32 Ri : GS.ComboSel) if (W.Skills.IsValidIndex(Ri))
			{
				const int32 Sl = W.Skills[Ri].Slot;
				if (Sl == -301) Ids.Add(FName(TEXT("__awaken")));
				else if (Sl == -302) Ids.Add(FName(TEXT("__showtime")));
				else if (PSq->Hand.IsValidIndex(Sl)) Ids.Add(PSq->Hand[Sl].CardId);
			}
			GS.SpecialQueue.Reset();
			for (int32 Ri : GS.ComboSel) if (W.Skills.IsValidIndex(Ri) && W.Skills[Ri].Slot == -301) GS.SpecialQueue.Add(-301); // the Awaken pick (armed for its SUPERPOWER tap)
			GS.SpecialTurn = G->TurnCounter; GS.SpecialNextAt = FPlatformTime::Seconds() + 0.3;
			GS.ComboQueue.Reset(); GS.bCombo = false; GS.ComboSel.Reset();
			int32 Armed = 0;
			for (const FName& Id : Ids)
			{
				if (Id == FName(TEXT("__awaken")) || Id == FName(TEXT("__showtime"))) { ZoogiSkills::ComboArm(G, Hq->PlayerIndex, Id); ++Armed; continue; }
				const FZoogiCardPlayerState* PSn = (CSq && Hq) ? CSq->Rules().Player(Hq->PlayerIndex) : nullptr;
				int32 Slot = INDEX_NONE;
				if (PSn) for (int32 k = 0; k < PSn->Hand.Num(); ++k) if (PSn->Hand[k].CardId == Id) { Slot = k; break; }
				FName Got;
				if (Slot == INDEX_NONE || !G->CardsArmCombo(Slot, Got)) { UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO could not arm %s"), *Id.ToString()); continue; }
				if (ZoogiSkills::IsSkill(Got) || Got == FName(TEXT("precision"))) { ZoogiSkills::ComboArm(G, Hq->PlayerIndex, Got); ++Armed; } // classic launch cards apply at once
			}
			UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO armed %d of %d"), Armed, Ids.Num());
			ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 1.f, 1.35f);
			if (UZoogiFunSys* Fc = UZoogiFunSys::Get(G)) Fc->PublicCallout(FString::Printf(TEXT("COMBO x%d ARMED!"), Armed), TEXT("LAUNCH, THEN TAP SUPERPOWER - ONE CAST PER TAP"), FLinearColor(1.f, 0.82f, 0.15f), 2.f, 1.f);
			Dismiss(G);
			return;
		}
		if (R >= 0 && GS.bCombo && W.Mode == 1)
		{	// b120: tick / untick a row (only what the SP covers)
			if (!W.Skills.IsValidIndex(R)) return;
			if (GS.ComboSel.Contains(R)) { GS.ComboSel.Remove(R); ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.5f, 1.2f); return; }
			{	// b136 (Evan): Wood Shield can not be used in a combo
				const UZoogiCardSubsystem* CSw = UZoogiCardSubsystem::Get(G);
				const FZoogiCardPlayerState* PSw = (CSw && G->CardsOn()) ? CSw->Rules().Player(W.Skills[R].Slot >= 0 && G->GetActiveBall() ? G->GetActiveBall()->PlayerIndex : 0) : nullptr;
				const FName Cw = PSw && PSw->Hand.IsValidIndex(W.Skills[R].Slot) ? PSw->Hand[W.Skills[R].Slot].CardId : NAME_None;
				if (Cw == FName(TEXT("wood_shield")) || Cw == FName(TEXT("energy_buff"))) // b137d (Evan): Energy Buff can't be in a combo either
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f);
					if (UZoogiFunSys* Fw = UZoogiFunSys::Get(G)) Fw->PublicCallout(TEXT("NOT IN A COMBO"), Cw == FName(TEXT("wood_shield")) ? TEXT("WOOD SHIELD ENDS YOUR TURN - CAST IT ON ITS OWN") : TEXT("ENERGY BUFF IS CAST ON ITS OWN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.2f, 0.8f);
					return;
				}
			}
			int32 Sum = W.Skills[R].Cost; for (int32 Ci : GS.ComboSel) if (W.Skills.IsValidIndex(Ci)) Sum += W.Skills[Ci].Cost;
			if (!W.Skills[R].bOn || Sum > W.SP) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			GS.ComboSel.Add(R); ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.0f + 0.1f * GS.ComboSel.Num());
			return;
		}
		if (R < 0 || W.Mode == 2) { GS.bCombo = false; GS.ComboSel.Reset(); W.Mode = 0; GS.SkillsTurn = -1; GS.ModeAt = FPlatformTime::Seconds(); ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; } // b76: BACK leaves the skill list for good
		if (!W.Skills.IsValidIndex(R) || !W.Skills[R].bOn) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
		UE_LOG(LogZoogi, Log, TEXT("P5CMD skill %s (slot %d, %d SP)"), *W.Skills[R].Name, W.Skills[R].Slot, W.Skills[R].Cost);
		G->MenuAction(TEXT("CardPlay"), W.Skills[R].Slot);
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.2f);
		if (ZoogiSkills::Picking()) Dismiss(G); // the target pick takes the screen (FIRE brings the list back)
		else ToSkillButton(); // b79 (Evan): played - fold to the SKILL button so the effect shows (tap it for the list again)
	}

	static void Refresh(AZoogiGameMode* G, AZoogiBall* B, SP5Cmd& W)
	{
		// commands
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
		const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(B->PlayerIndex) : nullptr;
		W.SP = PS ? PS->Energy : -1;
		W.Skills.Reset();
		W.bCombo = GS.bCombo; W.ComboPicks = GS.ComboSel; // b120
		if (W.Mode == 1 && PS)
			for (int32 k = 0; k < PS->Hand.Num(); ++k)
			{
				const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId);
				FSkillRow R; R.Slot = k; R.Name = D ? D->Name.ToUpper() : PS->Hand[k].CardId.ToString().ToUpper(); R.Cost = D ? D->Cost : 0; R.Rule = D ? D->Rule : FString();
				R.bOn = G->CardsSlotPlayableNow(k) && (!D || D->Cost <= PS->Energy);
				{	// b136 (Evan): cards with no way to be played in classic are greyed and locked
					const FName Ck = PS->Hand[k].CardId;
					if (Ck == FName(TEXT("bank_strike")) || Ck == FName(TEXT("orb_relay")) || Ck == FName(TEXT("recovery_roll")) || Ck == FName(TEXT("fire_meteors"))) { R.bOn = false; R.Name += TEXT("  - LOCKED"); }
				}
				W.Skills.Add(R);
			}
		if (W.Mode == 1 && GS.bCombo)
		{	// b194 (Evan: "if COMBO is pressed they appear on the list to be cast in deliberate order"): AWAKEN / SHOWTIME rows
			if (G->CardsAwakenReady(B->PlayerIndex)) { FSkillRow R; R.Slot = -301; R.Name = TEXT("AWAKEN"); R.Cost = 0; R.Rule = TEXT("Your Awaken - in the order you tick it."); R.bOn = true; W.Skills.Add(R); }
			if (ZoogiGattling::Ready(G)) { FSkillRow R; R.Slot = -302; R.Name = TEXT("SHOWTIME"); R.Cost = 0; R.Rule = TEXT("Your SHOWTIME - in the order you tick it."); R.bOn = true; W.Skills.Add(R); }
		}
		if (W.Mode == 2)
		{	// b130: PRISM GRENADE (from the orb generator)
			// b137 (Evan): the selected weapons from the collection (+ grenades picked up), one item per turn
			TArray<FName> Rows = ZoogiItems::Selected(G);
			if (ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) > 0) Rows.AddUnique(FName(TEXT("grenade")));
			if (ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("apple"))) > 0) Rows.AddUnique(FName(TEXT("apple"))); // b188: the Golden Apple is always in ITEM
			const bool bUsed = ZoogiItems::UsedThisTurn(G, B->PlayerIndex);
			for (const FName& Wn : Rows)
			{
				const int32 Ng = ZoogiItems::Count(G, B->PlayerIndex, Wn);
				FSkillRow R; R.Name = FString::Printf(TEXT("%s  x%d"), *ZoogiItems::WeaponName(Wn), Ng); R.Rule = bUsed ? FString(TEXT("One item a turn - used this turn.")) : ZoogiItems::WeaponRule(Wn);
				R.Slot = -100 - ZoogiItems::WeaponIds().IndexOfByKey(Wn); R.bOn = Ng > 0 && !bUsed && G->Phase == EZoogiPhase::Aiming;
				W.Skills.Add(R);
			}
			if (Rows.Num() == 0) { FSkillRow R; R.Name = TEXT("NO ITEMS YET"); R.Rule = TEXT("Pick items in INVENTORY - ITEM. From round 2 the orb generator drops a PRISM GRENADE."); R.bOn = false; W.Skills.Add(R); }
		}
		bool bAnySkill = false;
		if (PS) for (int32 k = 0; k < PS->Hand.Num(); ++k) if (G->CardsSlotPlayableNow(k)) if (const FZoogiCardDef* D = CS->Def(PS->Hand[k].CardId)) if (D->Cost <= PS->Energy) bAnySkill = true;
		// placement around the Zoogi (P5: a loose fan to the right, the big ATTACK low right)
		auto Set = [&](int32 I, const TCHAR* Lab, const TCHAR* Sub, bool bOn, float X, float Y, float Ang, float Wd) { FEntry& En = W.E[I]; En.Label = Lab; En.Sub = Sub; En.bOn = bOn; En.Off = FVector2D(X, Y); En.Ang = Ang; En.W = Wd; };
		Set(Attack, TEXT("ATTACK"), TEXT("Roll & strike"), true, 250.f, 95.f, 14.f, 330.f);
		Set(Guard, TEXT("GUARD"), ZoogiSkills::GuardBusy(G, B->PlayerIndex) ? TEXT("Barrier set") : TEXT("Defend yourself"), GuardSlot(G, B) >= 0 && !ZoogiSkills::GuardBusy(G, B->PlayerIndex), 330.f, 5.f, 5.f, 280.f); // b83: one GUARD at a time
		Set(Skill, TEXT("SKILL"), PS ? *FString::Printf(TEXT("Use a skill  SP %d"), PS->Energy) : TEXT("Use a skill"), bAnySkill, 290.f, -90.f, -9.f, 300.f);
		bool bAnyItem = false; // b143a (Evan): ITEM is open when an equipped item is left (one a turn)
		if (!ZoogiItems::UsedThisTurn(G, B->PlayerIndex))
		{
			for (const FName& Wn : ZoogiItems::Selected(G)) if (ZoogiItems::Count(G, B->PlayerIndex, Wn) > 0) bAnyItem = true;
			if (ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) > 0 || ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("apple"))) > 0) bAnyItem = true;
		}
		Set(Item, TEXT("ITEM"), TEXT("Use an item"), bAnyItem, 175.f, -170.f, -15.f, 240.f);
		const bool bPow = G->ClassicSuperpowerReady();
		static const bool bPowerCmd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerCmd")); // b143a (Evan): the star SUPERPOWER button does it
		Set(Power, bPow && bPowerCmd ? TEXT("POWER") : TEXT(""), TEXT("Superpower"), bPow && bPowerCmd, -230.f, -40.f, -8.f, 260.f);
		const bool bShow = G->CardsAwakenReady(B->PlayerIndex);
		Set(Showtime, bShow ? TEXT("AWAKEN") : TEXT(""), TEXT("Awaken!"), /* b186: was SHOWTIME - that name is the leader event now */ bShow, -200.f, -150.f, -16.f, 320.f);
		{	// b194 (Evan: "a P5-style SHOWTIME button like before, under AWAKEN if both are available"): only when it's this player's show
			const bool bGat = ZoogiGattling::Ready(G);
			static FString GatSub; GatSub = FString::Printf(TEXT("P%d %s's show!"), B->PlayerIndex + 1, *B->Def.Name.ToUpper());
			Set(Gat, bGat ? TEXT("SHOWTIME") : TEXT(""), *GatSub, bGat, -200.f, bShow ? -55.f : -150.f, bShow ? -6.f : -16.f, 340.f);
		}
		// target tag
		W.TargetName.Reset(); W.TargetTag.Reset();
		if (AActor* T = G->GetLockTarget())
		{
			if (const AZoogiBall* TB = Cast<AZoogiBall>(T)) { W.TargetName = TB->Def.Name.ToUpper(); W.TargetTag = FString::Printf(TEXT("P%d"), TB->PlayerIndex + 1); }
			else if (Cast<AZoogiOrb>(T)) { W.TargetName = TEXT("ORB"); W.TargetTag = TEXT("+50"); }
		}
		// party: 2v2 = your team, else everyone (you first)
		W.Party.Reset();
		for (int32 i = 0; i < G->Balls.Num(); ++i)
		{
			const AZoogiBall* P = G->Balls[i];
			if (!P) continue;
			if (G->bComboMode && AZoogiGameMode::TeamOfIndex(i) != AZoogiGameMode::TeamOfIndex(B->PlayerIndex)) continue;
			FParty Pa; Pa.Name = P->Def.Name; Pa.Score = P->Score; Pa.Col = ZoogiUtil::PlayerColor(i); Pa.Face = ZoogiP5::Portrait(P->Def.Name); Pa.bActive = P == B;
			if (CS && G->CardsOn()) if (const FZoogiCardPlayerState* Ps = CS->Rules().Player(i)) Pa.SP = Ps->Energy;
			if (P == B) W.Party.Insert(Pa, 0); else W.Party.Add(Pa);
		}
	}

	// anchor = the Zoogi on screen (a little left/up so the fan sits beside it)
	static void PlaceAnchor(UWorld* World, UGameViewportClient* VC, AZoogiBall* B, SP5Cmd& W)
	{
		W.AnchorPx = FVector2D(-1, -1);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		FVector2D Scr;
		static const bool bAtZoogi = FParse::Param(FCommandLine::Get(), TEXT("ZoogiMenuAtZoogi"));
		if (!bAtZoogi && VC)
		{	// b276 (Evan: "move the centre of the awaken, showtime & skills menu from the player to the super power button")
			FVector2D VS0; VC->GetViewportSize(VS0);
			if (VS0.X > 1.f && VS0.Y > 1.f && VS0.X > VS0.Y) // b281 (Evan: "in portrait leave how it was before"): portrait keeps the menu at the Zoogi
			{
				W.ViewportPx = VS0;
				const float Sc0 = FMath::Clamp(FMath::Min(VS0.X, VS0.Y) / 1080.f, 0.45f, 2.f) * (VS0.X < VS0.Y ? 1.25f : 1.f);
				FVector2D Pc = ZoogiClassicPad::PowerCenterPx(VS0);
				const float MaxX0 = VS0.X - 16.f - 495.f * Sc0, MinX0 = 16.f + 390.f * Sc0;
				Pc.X = MaxX0 >= MinX0 ? FMath::Clamp(Pc.X, MinX0, MaxX0) : MaxX0;
				Pc.Y = FMath::Clamp(Pc.Y, 215.f * Sc0, VS0.Y - 140.f * Sc0);
				W.AnchorPx = Pc;
				return;
			}
		}
		if (!PC || !B || !UGameplayStatics::ProjectWorldToScreen(PC, B->BallLocation() + FVector(0, 0, 40.f), Scr, false)) return;
		FVector2D VS; VC->GetViewportSize(VS);
		W.ViewportPx = VS;
		Scr.X = FMath::Clamp(Scr.X, VS.X * 0.28f, VS.X * 0.62f);
		const float Sc = FMath::Clamp(FMath::Min(VS.X, VS.Y) / 1080.f, 0.45f, 2.f) * (VS.X < VS.Y ? 1.25f : 1.f);
		const float MaxX = VS.X - 16.f - 475.f * Sc, MinX = 16.f + 365.f * Sc;
		Scr.X = MaxX >= MinX ? FMath::Clamp(Scr.X, MinX, MaxX) : MaxX;
		Scr.Y = FMath::Clamp(Scr.Y, VS.Y * 0.30f, VS.Y * 0.70f);
		W.AnchorPx = Scr;
	}

	static void CpuStart(AZoogiGameMode* G, AZoogiBall* B)
	{
		GS.CpuTurn = G->TurnCounter; GS.CpuPlayer = G->ActivePlayer; GS.CpuStep = 0;
		GS.CpuRows.Reset(); GS.CpuCasts.Reset(); GS.CpuSteps.Reset(); GS.CpuRollCasts.Reset();
		const UZoogiCardSubsystem* CS = UZoogiCardSubsystem::Get(G);
		if (const FZoogiCardPlayerState* PS = (CS && G->CardsOn()) ? CS->Rules().Player(B->PlayerIndex) : nullptr)
			for (const FZoogiCardCommitted& Cm : PS->Committed)
			{
				const FZoogiCardDef* D = CS->Def(Cm.Card.CardId);
				FSkillRow R; R.Name = D ? D->Name.ToUpper() : Cm.Card.CardId.ToString().ToUpper(); R.Cost = D ? D->Cost : 0; R.Rule = D ? D->Rule : FString(); R.bOn = true;
				GS.CpuRows.Add(R);
				// b124: aiming skills wait for the roll (cast from where the Zoogi is then, like the player's COMBO); buffs before launch
				if (ZoogiSkills::IsSkill(Cm.Card.CardId)) (ZoogiCPUSmart::On() && ZoogiCPUSmart::CastMidRoll(Cm.Card.CardId) ? GS.CpuRollCasts : GS.CpuCasts).Add(Cm.Card.CardId);
			}
		// the script: a person's cursor - a look around, then the choice (small random hesitations)
		auto J = []() { return FMath::FRandRange(-0.08f, 0.15f); };
		float T = 0.55f + J();
		if (GS.CpuRows.Num())
		{
			GS.CpuSteps.Add({ T, Attack, 0 }); T += 0.35f + J();
			GS.CpuSteps.Add({ T, Skill, 0 }); T += 0.45f + J();
			GS.CpuSteps.Add({ T, -1, 1 }); T += 0.45f + J();
			for (int32 r = 0; r < GS.CpuRows.Num() && r < 5; ++r) { GS.CpuSteps.Add({ T, 100 + r, 0 }); T += 0.28f + J() * 0.5f; GS.CpuSteps.Add({ T, 100 + r, 2 }); T += 0.22f + J() * 0.5f; }
			GS.CpuSteps.Add({ T, -1, 3 }); T += 0.35f + J();
		}
		else
		{
			GS.CpuSteps.Add({ T, FMath::RandBool() ? Skill : Guard, 0 }); T += 0.4f + J();
		}
		GS.CpuSteps.Add({ T, Attack, 0 }); T += 0.45f + J();
		GS.CpuSteps.Add({ T, Attack, 4 });
		GS.CpuAt = GS.OpenAt = GS.ModeAt = FPlatformTime::Seconds();
		GS.bCpuMenu = true;
		if (GS.W.IsValid())
		{
			SP5Cmd& W = *GS.W;
			W.Mode = 0; W.Hover = -1;
			static const TCHAR* Lines[] = { TEXT("My move. Watch closely."), TEXT("Let's see... there!"), TEXT("You're wide open!"), TEXT("Time to roll!"), TEXT("Hmm... got it.") };
			W.BubbleLine = Lines[GS.LineIdx++ % (int32)UE_ARRAY_COUNT(Lines)];
			W.BubbleName = B->Def.Name.ToUpper(); W.BubbleFace = ZoogiP5::Portrait(B->Def.Name);
		}
		ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.6f, 1.1f);
		UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu P%d goes through the menu (%d cards committed, %d skills to aim)"), B->PlayerIndex + 1, GS.CpuRows.Num(), GS.CpuCasts.Num());
	}

	bool CpuHold(AZoogiGameMode* G)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCpuInstant"));
		AZoogiBall* B = G ? G->GetActiveBall() : nullptr;
		if (bOff || !B || !B->bCPU || G->bRoyaleMode || !On() || !GS.W.IsValid()) return false;
		if (GS.CpuTurn != G->TurnCounter || GS.CpuPlayer != G->ActivePlayer) CpuStart(G, B);
		if (GS.bCpuMenu) return true;
		if (ZoogiSkills::Picking()) return true;
		if (GS.CpuCasts.Num()) { const FName Id = GS.CpuCasts[0]; GS.CpuCasts.RemoveAt(0); ZoogiSkills::CpuCast(G, Id); return ZoogiSkills::Picking(); }
		return false;
	}

	static void CpuTick(AZoogiGameMode* G, AZoogiBall* B, SP5Cmd& W, UWorld* World, UGameViewportClient* VC, double Now)
	{
		W.bMenu = true; W.bPuppet = true; GShowing = true;
		W.SetVisibility(EVisibility::HitTestInvisible);
		W.T = (float)(Now - GS.OpenAt) * P5Snap(); W.ModeT = (float)(Now - GS.ModeAt) * P5Snap(); // b190 FAST FX
		PlaceAnchor(World, VC, B, W);
		Refresh(G, B, W);
		W.E[Attack].bOn = true; W.E[Skill].bOn = GS.CpuRows.Num() > 0; W.E[Guard].bOn = false;
		if (W.Mode == 1) W.Skills = GS.CpuRows;
		const float T = (float)(Now - GS.CpuAt);
		while (GS.CpuStep < GS.CpuSteps.Num() && T >= GS.CpuSteps[GS.CpuStep].At)
		{
			const FState::FCpuStep& St = GS.CpuSteps[GS.CpuStep++];
			W.Hover = St.Hover;
			switch (St.Act)
			{
			case 0: ZoogiUtil::PlaySfx(G, TEXT("UiClick"), 0.35f, 1.3f); break;
			case 1: W.Mode = 1; GS.ModeAt = Now; ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.6f, 1.1f); break;
			case 2: ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.8f, 1.25f); break;
			case 3: W.Mode = 0; GS.ModeAt = Now; break;
			default:
				ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.2f);
				GS.bCpuMenu = false; W.bPuppet = false; W.bMenu = false; W.Mode = 0; W.Hover = -1; GShowing = false;
				UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu P%d chose ATTACK"), B->PlayerIndex + 1);
				break;
			}
		}
	}

	static bool Tick(float Dt)
	{
		UGameViewportClient* VC = GEngine ? GEngine->GameViewport : nullptr;
		UWorld* World = VC ? VC->GetWorld() : nullptr;
		AZoogiGameMode* G = World ? World->GetAuthGameMode<AZoogiGameMode>() : nullptr;
		if (!G || !On()) { GShowing = false; if (GS.W.IsValid() && GS.VC.IsValid()) { GS.VC->RemoveViewportWidgetContent(GS.W.ToSharedRef()); GS.W.Reset(); } return true; }
		if (!GS.W.IsValid() || GS.VC.Get() != VC)
		{
			if (GS.W.IsValid() && GS.VC.IsValid()) GS.VC->RemoveViewportWidgetContent(GS.W.ToSharedRef());
			GS.W = SNew(SP5Cmd);
			GS.W->OnCmd = [](int32 I) { DoCmd(I); };
			GS.W->OnSkill = [](int32 R) { DoSkill(R); };
			GS.W->OnOutside = []() { if (AZoogiGameMode* Gm = GS.G.Get()) Dismiss(Gm); };
			GS.W->OnMini = []() { ZoogiUtil::PlaySfx(GS.G.Get(), TEXT("UiConfirm"), 0.8f, 1.1f); BackToSkills(); if (GS.W.IsValid()) { GS.W->Mode = ListDirect() ? 1 : 0; GS.W->bMini = false; GS.ModeAt = FPlatformTime::Seconds(); } }; // b76: straight to the skill list; b119b: the command menu
			GS.VC = VC;
			VC->AddViewportWidgetContent(GS.W.ToSharedRef(), 60);
			FParse::Value(FCommandLine::Get(), TEXT("ZoogiShots="), GS.ShotDir);
			UE_LOG(LogZoogi, Log, TEXT("P5CMD command menu overlay on (cards hidden; -ZoogiOldCards = old card UI)"));
		}
		GS.G = G;
		SP5Cmd& W = *GS.W;
		if (UGameplayStatics::IsGamePaused(World) || ZoogiSkillLab::Open() || ZoogiGattling::Active() || ZoogiBossAttack::DuelActive()) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b182: + SHOWTIME // b79: nothing over the pause menu; b108: or the lab
		const double Now = FPlatformTime::Seconds();
		if (GS.CpuRollCasts.Num())
		{	// b124 (Evan: "CPUs should play like a person"): the CPU casts its aiming skills mid-roll when a target is in reach
			AZoogiBall* Rb = G->GetActiveBall();
			if (!Rb || !Rb->bCPU || Rb->bOut || G->TurnCounter != GS.CpuTurn || (G->Phase != EZoogiPhase::Aiming && G->Phase != EZoogiPhase::Rolling))
			{
				UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu roll casts: %d left unused (the roll ended)"), GS.CpuRollCasts.Num());
				GS.CpuRollCasts.Reset();
			}
			else if (G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.45f && Now >= GS.CpuRollNextAt && !ZoogiSkills::Picking() && !ZoogiRedirect::Picking()
				&& (FParse::Param(FCommandLine::Get(), TEXT("ZoogiComboFirst")) || !ZoogiSkills::PowerLeft(G, Rb->PlayerIndex))) // b128: the CPU uses up its bolts / wolves first too
			{
				bool bLast = false;
				const FName Id = GS.CpuRollCasts[0];
				if (ZoogiCPUSmart::WantCastNow(G, Rb, Id, bLast))
				{
					GS.CpuRollCasts.RemoveAt(0);
					GS.CpuRollNextAt = Now + 0.9 + FMath::FRandRange(0.f, 0.4f);
					UE_LOG(LogZoogi, Log, TEXT("P5CMD cpu P%d casts %s mid-roll (%s, %.0f cm/s)"), Rb->PlayerIndex + 1, *Id.ToString(), bLast ? TEXT("last chance") : TEXT("target in reach"), Rb->GetBallVelocity().Size2D());
					ZoogiSkills::CpuCast(G, Id);
				}
			}
		}
		W.ShowtimeT = (float)(Now - GS.ShowAt);
		{	// b280: the how-to stays 6 s, and as long as the skill's target pick is open
			if (GDescAt > 0.0 && ZoogiSkills::Picking() && Now - GDescAt > 5.0) GDescAt = Now - 5.0;
			static const bool bDescTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiDescTest")); // preview: the first skill row's how-to as the list opens
			if (bDescTest && W.Mode == 1 && W.Skills.Num() > 0 && Now - GDescAt > 8.0) ShowDesc(W.Skills[0].Name, W.Skills[0].Rule);
			const float Dt0 = GDescAt > 0.0 ? (float)(Now - GDescAt) : -1.f;
			if (Dt0 >= 0.f && Dt0 < 6.f) { W.DescT = Dt0; W.DescFade = FMath::Clamp((6.f - Dt0) / 0.4f, 0.f, 1.f); W.DescName = GDescName; W.DescRule = GDescRule; }
			else W.DescT = -1.f;
		}
		{	// b74 (Evan: "skills menu slowly fades as the launch drag is pulled back, but reappears if dragged back to release"): opacity follows the pull
			static float MenuA = 1.f;
			const float Want = G->HumanDragFrac > 0.f ? FMath::Clamp(1.f - G->HumanDragFrac / 0.55f, 0.f, 1.f) : 1.f;
			MenuA = FMath::FInterpTo(MenuA, Want, FMath::Min(Dt, 0.05f), 10.f);
			W.SetRenderOpacity(MenuA);
		}
		if (GS.bCpuMenu)
		{	// b67: the CPU's turn - it drives the same menu
			AZoogiBall* Cb = G->GetActiveBall();
			const bool bCpuStuck = Now - GS.CpuAt > 12.0; // b95c watchdog: a CPU menu never runs longer than 12 s
			if (Cb && Cb->bCPU && G->TurnCounter == GS.CpuTurn && G->Phase == EZoogiPhase::Aiming && !ZoogiP5::ResultsOn() && !bCpuStuck) { CpuTick(G, Cb, W, World, VC, Now); return true; }
			if (bCpuStuck) UE_LOG(LogZoogi, Warning, TEXT("P5CMD watchdog: CPU menu open %.0f s -> closed"), Now - GS.CpuAt);
			GS.bCpuMenu = false; W.bPuppet = false;
			W.bMenu = false; W.Mode = 0; W.Hover = -1; GShowing = false; // b95c: don't leave the CPU's menu drawn over a turn that moved on
		}
		AZoogiBall* B = Human(G);
		{	// b276 (Evan): a REACTION pause with combo skills queued = tap the skill you want; otherwise only the REACTION button
			static const bool bNoReactList = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactCombo"));
			AZoogiBall* Ab = G->GetActiveBall();
			// b279 (Evan: "allow reaction after using a skill mid roll, it's like I lose momentum - use them interchangeably"): once the
			// skill picked in a REACTION pause is done (aimed + fired, or a self skill at once) the Zoogi gets its roll speed back and the
			// REACTION pause opens again, so skill -> REACTION -> skill... flows without stopping. Off: -ZoogiNoReactKeep
			if (GKeepAt > 0.0)
			{
				AZoogiBall* Kb = GKeepBall.Get();
				if (!Kb || Kb->bOut || G->Phase != EZoogiPhase::Rolling || Now - GKeepAt > 30.0) { GKeepAt = 0.0; GKeepBall.Reset(); }
				else if (!ZoogiSkills::Picking() && Now - GKeepAt > 0.12)
				{
					const FVector V = Kb->GetBallVelocity(); const float Cur = V.Size2D();
					if (Cur < GKeepSpd)
					{
						FVector D = FVector(V.X, V.Y, 0.f).GetSafeNormal(); if (D.IsNearlyZero()) D = GKeepDir;
						Kb->SetPlanarVelocity(D * GKeepSpd);
					}
					const bool bRe = ZoogiRedirect::Begin(G);
					UE_LOG(LogZoogi, Log, TEXT("P5CMD skill done mid-roll: speed %.0f -> %.0f, REACTION pause %s"), Cur, FMath::Max(Cur, GKeepSpd), bRe ? TEXT("reopened") : TEXT("not available (REACTION button stays)"));
					GKeepAt = 0.0; GKeepBall.Reset();
				}
			}
			TArray<FName> Q = (Ab && !Ab->bCPU) ? ZoogiSkills::ComboQueue(G, Ab->PlayerIndex) : TArray<FName>();
			static const bool bReactTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiReactComboTest")); // preview: the plates on every human roll (0.5 - 3 s)
			const bool bTestShow = bReactTest && Ab && !Ab->bCPU && G->Phase == EZoogiPhase::Rolling && G->PhaseTime > 0.5f && G->PhaseTime < 3.f;
			if (bTestShow && Q.Num() == 0) { Q.Add(FName(TEXT("water_cannon"))); Q.Add(FName(TEXT("ice_wall"))); Q.Add(FName(TEXT("__awaken"))); }
			const bool bReactNow = bTestShow || (!bNoReactList && Ab && !Ab->bCPU && G->Phase == EZoogiPhase::Rolling && ZoogiRedirect::Picking() && Q.Num() > 0 && !ZoogiSkills::Picking());
			if (bReactNow)
			{
				if (!W.bReact) { GS.OpenAt = Now; UE_LOG(LogZoogi, Log, TEXT("P5CMD reaction pause: %d combo skills shown at SUPERPOWER"), Q.Num()); }
				W.bReact = true; W.bMenu = false; W.bMini = false; GShowing = false;
				W.ReactRows.Reset();
				const UZoogiCardSubsystem* CSr = UZoogiCardSubsystem::Get(G);
				for (const FName& Id : Q)
				{
					FString Nm = Id == FName(TEXT("__awaken")) ? FString(TEXT("AWAKEN")) : Id == FName(TEXT("__showtime")) ? FString(TEXT("SHOWTIME")) : FString();
					if (Nm.IsEmpty()) { const FZoogiCardDef* D = CSr ? CSr->Def(Id) : nullptr; Nm = D ? D->Name.ToUpper() : Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(); }
					W.ReactRows.Add(Nm);
				}
				W.OnReact = [](int32 I)
				{
					AZoogiGameMode* Gr = GS.G.Get(); if (!Gr) return;
					static const bool bNoKeep = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactKeep"));
					if (AZoogiBall* Kb = Gr->GetActiveBall(); Kb && !bNoKeep)
					{	// b279: remember the roll so it carries on after the skill
						const FVector V = Kb->GetBallVelocity(); GKeepBall = Kb; GKeepSpd = FMath::Max(V.Size2D(), 160.f); GKeepDir = FVector(V.X, V.Y, 0.f).GetSafeNormal(); GKeepAt = FPlatformTime::Seconds();
					}
					{	// b280: the plate's how-to at the bottom
						AZoogiBall* Db = Gr->GetActiveBall();
						const TArray<FName> Dq = Db ? ZoogiSkills::ComboQueue(Gr, Db->PlayerIndex) : TArray<FName>();
						if (Dq.IsValidIndex(I))
						{
							const FName Id = Dq[I];
							const UZoogiCardSubsystem* CSd = UZoogiCardSubsystem::Get(Gr);
							const FZoogiCardDef* D = CSd ? CSd->Def(Id) : nullptr;
							if (Id == FName(TEXT("__awaken"))) ShowDesc(TEXT("AWAKEN"), TEXT("Your Awaken power fires from where your Zoogi is now."));
							else if (Id == FName(TEXT("__showtime"))) ShowDesc(TEXT("SHOWTIME"), TEXT("Your SHOWTIME starts now - keep the beat to score."));
							else if (D) ShowDesc(D->Name.ToUpper(), D->Rule);
						}
					}
					ZoogiRedirect::CancelPick(TEXT("combo skill picked")); // nothing spent - REACTION is still there after the skill
					const bool bOk = ZoogiSkills::ComboGoAt(Gr, I);
					ZoogiUtil::PlaySfx(Gr, TEXT("UiConfirm"), 0.8f, 1.2f);
					UE_LOG(LogZoogi, Log, TEXT("P5CMD reaction pause: combo skill %d cast (%s)"), I + 1, bOk ? TEXT("ok") : TEXT("not ready"));
					if (GS.W.IsValid()) GS.W->bReact = false;
				};
				static int32 GTestTapTurn = -1; // b279 preview: tap plate 1 once per roll at 2 s to check the roll keeps going and REACTION comes back
				if (bTestShow && G->PhaseTime > 2.f && GTestTapTurn != G->TurnCounter) { GTestTapTurn = G->TurnCounter; if (W.OnReact) W.OnReact(0); }
				W.T = (float)(Now - GS.OpenAt) * P5Snap();
				PlaceAnchor(World, VC, Ab, W);
				W.SetRenderOpacity(1.f);
				W.SetVisibility(EVisibility::Visible);
				return true;
			}
			if (W.bReact) { W.bReact = false; W.ReactRows.Reset(); }
		}
		if (GS.ComboQueue.Num())
		{	// b120 COMBO: one skill every 0.12 s; a target pick holds the queue until FIRE
			if (!B || G->TurnCounter != GS.ComboTurn || G->Phase != EZoogiPhase::Aiming) GS.ComboQueue.Reset();
			else if (!ZoogiSkills::Picking() && Now >= GS.ComboNextAt)
			{
				const FName Id = GS.ComboQueue[0]; GS.ComboQueue.RemoveAt(0);
				const UZoogiCardSubsystem* CSc = UZoogiCardSubsystem::Get(G);
				const FZoogiCardPlayerState* PSc = CSc ? CSc->Rules().Player(B->PlayerIndex) : nullptr;
				int32 Slot = INDEX_NONE;
				if (PSc) for (int32 k = 0; k < PSc->Hand.Num(); ++k) if (PSc->Hand[k].CardId == Id) { Slot = k; break; }
				if (Slot != INDEX_NONE && G->CardsSlotPlayableNow(Slot)) { G->MenuAction(TEXT("CardPlay"), Slot); UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO plays %s (slot %d, %d left)"), *Id.ToString(), Slot, GS.ComboQueue.Num()); }
				else UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO skips %s (not playable now)"), *Id.ToString());
				GS.ComboNextAt = Now + 0.12;
			}
		}
		if (GS.SpecialQueue.Num())
		{	// b194: one at a time - the next waits until the last one's pick / show is over
			if (!B || G->TurnCounter != GS.SpecialTurn || G->Phase != EZoogiPhase::Aiming) GS.SpecialQueue.Reset();
			else if (Now >= GS.SpecialNextAt && !ZoogiSkills::Picking() && !ZoogiShowtime::Picking() && !ZoogiGattling::Active() && !ZoogiP5::CutInBusy() && !ZoogiP5::WipeBusy())
			{
				const int32 Sp = GS.SpecialQueue[0]; GS.SpecialQueue.RemoveAt(0);
				if (Sp == -302) { const bool bOk = ZoogiGattling::StartFromMenu(G); UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO SHOWTIME %s"), bOk ? TEXT("starts") : TEXT("not ready")); }
				else if (G->CardsAwakenReady(B->PlayerIndex)) { G->MenuAction(TEXT("AwakenUse"), 0); ZoogiShowtime::Begin(G, B); GS.ShowAt = FPlatformTime::Seconds(); UE_LOG(LogZoogi, Log, TEXT("P5CMD COMBO AWAKEN")); }
				GS.SpecialNextAt = Now + 1.0;
			}
		}
		const bool bTurn = B && !G->bRoyaleMode && G->Phase == EZoogiPhase::Aiming && (G->Stage == EZoogiStage::Awaken || G->IsAimStep());
		const bool bDismissed = GS.DismissTurn == G->TurnCounter && GS.DismissPlayer == G->ActivePlayer;
		// b76 (Evan: "hide when it makes sense - while the effect is happening; not for the banners sliding in from the top"): anime clips,
		// a skill's effect, a clash / duel, a redirect pick, the edge hang
		// b119b (Evan 16:40: "forget what I said about the skills menu"): hidden again for a skill's effect / anime clip (-ZoogiMenuStays = off)
		static const bool bOldHides = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiMenuStays"));
		bool bBusyHide = (bOldHides && ZoogiSkills::EffectBusy()) || ZoogiRedirect::Picking() || ZoogiEdgeHang::Active();
		if (bOldHides) if (UZoogiAnimeSubsystem* AnH = World->GetSubsystem<UZoogiAnimeSubsystem>()) bBusyHide = bBusyHide || AnH->IsBusy();
		if (UZoogiFunSys* FnH = UZoogiFunSys::Get(G)) bBusyHide = bBusyHide || FnH->ClashOn();
		const bool bWant = bTurn && !ZoogiFallGuardAirborne(B) && !bDismissed && !bBusyHide && !ZoogiGuide::HideMenus() && !ZoogiP5::ResultsOn() && !ZoogiP5::CutInBusy() && !ZoogiP5::WipeBusy() && !ZoogiSkills::Picking(); // b63: never over a skill pick (b73: shown on the guided turn too - no exceptions)
		if (bWant && !(GS.OpenTurn == G->TurnCounter && GS.OpenPlayer == G->ActivePlayer))
		{
			GS.OpenTurn = G->TurnCounter; GS.OpenPlayer = G->ActivePlayer; GS.OpenAt = Now; W.Mode = GS.bOpenSkills ? 1 : 0; W.Hover = -1; if (GS.bOpenSkills) GS.ModeAt = Now; GS.bOpenSkills = false;
			static const TCHAR* Lines[] = { TEXT("Now's our chance to strike! Go!"), TEXT("They're wide open - hit 'em!"), TEXT("Line it up. Knock them off!"),
				TEXT("Those orbs are ripe for the taking!"), TEXT("Show them what a Zoogi can do!"), TEXT("Don't hold back - roll!") };
			W.BubbleLine = Lines[GS.LineIdx++ % (int32)UE_ARRAY_COUNT(Lines)];
			// the speaker: your partner in 2v2, else your own Zoogi
			const AZoogiBall* Sp = B;
			if (G->bComboMode) for (const AZoogiBall* P : G->Balls) if (P && P != B && AZoogiGameMode::TeamOfIndex(P->PlayerIndex) == AZoogiGameMode::TeamOfIndex(B->PlayerIndex)) Sp = P;
			W.BubbleName = Sp->Def.Name.ToUpper(); W.BubbleFace = ZoogiP5::Portrait(Sp->Def.Name);
			ZoogiUtil::PlaySfx(G, TEXT("Dash"), 0.7f, 1.2f);
			UE_LOG(LogZoogi, Log, TEXT("P5CMD open P%d turn %d"), B->PlayerIndex + 1, G->TurnCounter);
		}
		W.bMini = false;
		if (!bWant)
		{	// b73: aiming after ATTACK / an outside tap - the SKILL plate stays until the launch while a skill is still playable
			const bool bMiniWant = bTurn && bDismissed && !bBusyHide && G->Phase == EZoogiPhase::Aiming && !ZoogiP5::ResultsOn() && !ZoogiP5::CutInBusy() && !ZoogiP5::WipeBusy() && !ZoogiSkills::Picking() && G->CardsOn();
			if (bMiniWant)
			{
				W.T = (float)(Now - GS.OpenAt) * P5Snap(); // b190 FAST FX
				PlaceAnchor(World, VC, B, W); // b76: the SKILL button follows the Zoogi (landscape)
				Refresh(G, B, W);
				if (W.E[Skill].bOn)
				{
					W.bMenu = true; W.bMini = true; GShowing = false;
					W.SetVisibility(EVisibility::Visible);
					return true;
				}
			}
		}
		if (bWant && ListDirect() && GS.SkillsTurn == G->TurnCounter && W.Mode == 0) { W.Mode = 1; GS.ModeAt = Now; } // b76 (Evan: "skill button opens the main command menu"): back on the skill list
		W.bMenu = bWant;
		GShowing = bWant;
		W.SetVisibility(bWant ? EVisibility::Visible : EVisibility::HitTestInvisible);
		if (!bWant) return true;
		W.T = (float)(Now - GS.OpenAt) * P5Snap(); // b190 FAST FX
		W.ModeT = (float)(Now - GS.ModeAt) * P5Snap(); // b190 FAST FX
		PlaceAnchor(World, VC, B, W);
		Refresh(G, B, W);
		// keys
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			static const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
			for (int32 k = 0; k < 6; ++k) if (PC->WasInputKeyJustPressed(Keys[k])) { if (W.Mode == 0) DoCmd(k); else DoSkill(k); }
			if (PC->WasInputKeyJustPressed(EKeys::Escape) && W.Mode != 0) DoSkill(-1);
		}
		// -ZoogiP5CmdTest: captures of each state on the human's first turns (turn 1: open -> SKILL list -> first skill; turn 2: open -> ATTACK)
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiP5CmdTest"));
		if (bTest && GS.TestTurns < 3)
		{
			const float Tt = W.T;
			const int32 Tn = GS.TestTurns;
			auto Step = [&](int32 S2, float At, TFunction<void()> Fn) { if (GS.TestStep == S2 && Tt >= At) { Fn(); ++GS.TestStep; } };
			Step(0, 0.7f, [&]() { Shot(FString::Printf(TEXT("t%d_open"), Tn + 1)); if (Tn >= 2 && FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"))) if (UZoogiFunSys* Ft = UZoogiFunSys::Get(G)) Ft->FillMeterForTest(G, B->PlayerIndex); });
			if (Tn == 0)
			{
				Step(1, 1.1f, [&]() { DoCmd(Skill); });
				Step(2, 1.6f, [&]() { Shot(TEXT("t1_skills")); });
				Step(3, 2.0f, [&]() { int32 Pick = -1; for (int32 r = 0; r < W.Skills.Num(); ++r) if (W.Skills[r].bOn) { Pick = r; break; } if (Pick >= 0) DoSkill(Pick); else { DoSkill(-1); DoCmd(Attack); } GS.TestTurns++; GS.TestStep = 0; });
			}
			else if (Tn == 1) Step(1, 1.2f, [&]() { DoCmd(Attack); GS.TestTurns++; GS.TestStep = 0; });
			else Step(1, 1.2f, [&]() { if (W.E[Showtime].bOn) DoCmd(Showtime); else DoCmd(Attack); GS.TestTurns++; GS.TestStep = 0; });
		}
		// b144b: -ZoogiItemTest - ITEM -> the PRISM GRENADE row (ZoogiSkills::Tick then presses FIRE on the throw pick)
		static const bool bItemTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiItemTest"));
		static int32 ItemStep = 0;
		if (bItemTest && ItemStep < 2)
		{
			if (ItemStep == 0 && W.Mode == 0 && W.T >= 1.0f) { UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: opens ITEM")); DoCmd(Item); ++ItemStep; }
			else if (ItemStep == 1 && W.Mode == 2 && W.ModeT >= 0.7f)
			{
				Shot(TEXT("item_list")); ++ItemStep;
				int32 Pick = -1; for (int32 r = 0; r < W.Skills.Num(); ++r) if (W.Skills[r].bOn && W.Skills[r].Name.Contains(TEXT("GRENADE"))) { Pick = r; break; }
				UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: grenade row %d of %d"), Pick, W.Skills.Num());
				if (Pick >= 0) DoSkill(Pick);
			}
		}
		return true;
	}

	static FDelayedAutoRegisterHelper GRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&ZoogiP5Cmd::Tick), 0.f);
	});
}
