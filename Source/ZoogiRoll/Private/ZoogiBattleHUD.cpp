// Oct 8: minimal in-game battle HUD for the Episode flow (replaces the raw on-screen debug overlay in normal play; the
// overlay stays behind -ZoogiBattleDebug / [ZoogiRoll.Battle] bDebugOverlay). Drawn through AHUD::OnHUDPostRender so
// ZoogiHUD stays untouched. Layout (1080p units U): Episode Energy panel on the right under the target panel; one prompt
// panel at the bottom centre above the status line (Rail Release timing window / Air Reaction / route objective / result
// flash); a big SHATTERED signal. Visual only: reads the state machine, never changes the game.

#include "ZoogiBattleStateMachine.h"
#include "ZoogiMiniGame.h"
#include "ZoogiReactionComponent.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiRoll.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/App.h"
#include "ZoogiUIStyle.h"

namespace ZoogiBattleHud
{
	static const FLinearColor Panel = ZoogiUIStyle::PanelNavy; // uistyle: saturated navy (was near-black 0.02/0.03/0.08)
	static TArray<ZoogiUIStyle::FBarJuice> EnergyJuice;         // uistyle: per-player Episode Energy bar juice (display only)
	static const FLinearColor EarlyC(1.f, 0.85f, 0.2f, 1.f), CenterC(0.3f, 0.95f, 0.45f, 1.f), LateC(1.f, 0.42f, 0.22f, 1.f);
	static const FLinearColor Cyan(0.45f, 0.9f, 1.f, 1.f);

	struct FDraw
	{
		UCanvas* C = nullptr;
		float U = 1.f;
		FSlateFontInfo Font(float Scale, bool bBig) const
		{
			UFont* F = bBig ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
			FSlateFontInfo Fi = F->GetLegacySlateFontInfo();
			Fi.Size = FMath::Max(6.f, FMath::RoundToFloat(Fi.Size * Scale * U * (bBig ? 1.75f : 2.3f)));
			if (bBig) Fi.TypefaceFontName = FName(TEXT("Bold"));
			ZoogiUIStyle::AddOutline(Fi, FMath::Max(1.f, 2.f * U)); // uistyle: navy outline on every label
			return Fi;
		}
		FVector2D Size(const FString& S, float Scale, bool bBig) const
		{
			if (FSlateApplication::IsInitialized())
				return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Font(Scale, bBig));
			return FVector2D(S.Len() * 9.f * Scale * U, 18.f * Scale * U);
		}
		void Rect(float X, float Y, float W, float H, const FLinearColor& Col) const
		{
			FCanvasTileItem T(FVector2D(X, Y), FVector2D(W, H), Col);
			T.BlendMode = SE_BLEND_Translucent;
			C->DrawItem(T);
		}
		void Text(const FString& S, float X, float Y, const FLinearColor& Col, float Scale, bool bCenter = false, bool bBig = false) const
		{
			if (bCenter) X -= Size(S, Scale, bBig).X * 0.5f;
			FCanvasTextItem I(FVector2D(X, Y), FText::FromString(S), Font(Scale, bBig), Col);
			I.EnableShadow(FLinearColor(0, 0, 0, 0.85f * Col.A), FVector2D(2.f * U, 2.f * U));
			C->DrawItem(I);
		}
	};

	static FString ChallengeTitle(EZoogiChallenge K)
	{
		switch (K)
		{
		case EZoogiChallenge::Strike: return TEXT("STRIKE");
		case EZoogiChallenge::HaloBank: return TEXT("HALO BANK");
		case EZoogiChallenge::OrbLine: return TEXT("ORB LINE");
		case EZoogiChallenge::AirReaction: return TEXT("AIR REACTION");
		case EZoogiChallenge::SkyBumper: return TEXT("SKY BUMPER");
		case EZoogiChallenge::PartnerIntercept: return TEXT("PARTNER INTERCEPT");
		case EZoogiChallenge::RailRelease: return TEXT("RAIL RELEASE");
		default: return TEXT("COMBO");
		}
	}
}

void UZoogiBattleStateMachine::OnHUDPostRender(AHUD* HUD, UCanvas* Canvas)
{
	using namespace ZoogiBattleHud;
	if (!bFlow || !Tune.bBattleHUD || !HUD || !Canvas || HUD->GetWorld() != GetWorld()) return;
	const AZoogiGameMode* G = GM();
	if (!G || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::Forming || !GEngine) return;
	FDraw D;
	D.C = Canvas;
	D.U = Canvas->ClipY / 1080.f;
	const float U = D.U, W = Canvas->ClipX, H = Canvas->ClipY;

	// ---- result flashes (latched from the counters, real time)
	if (History.Num() > HudSeenHistory)
	{
		for (int32 i = HudSeenHistory; i < History.Num(); ++i)
		{
			const FZoogiPendingSegment& S = History[i];
			if (S.Comic == EZoogiComicState::Completed || S.Result == EZoogiChallengeResult::Success)
			{
				const FZoogiEpisodeComboRow* Row = Tune.FindCombo(S.ComboId);
				HudFlashText = FString::Printf(TEXT("P%d  %s  COMPLETE"), S.Owner + 1, Row ? *Row->Label.ToUpper() : *ChallengeTitle(S.Challenge));
				HudFlashColor = CenterC;
				HudFlashUntil = RealClock + 1.6;
			}
		}
		HudSeenHistory = History.Num();
	}
	if (RailReleases > HudSeenRail)
	{
		HudSeenRail = RailReleases;
		const EZoogiRailWindow Wn = LastRailWindow;
		HudRailText = FString::Printf(TEXT("RELEASED: %s"), Wn == EZoogiRailWindow::Early ? TEXT("EARLY - HIGH ARC") : (Wn == EZoogiRailWindow::Late ? TEXT("LATE - BANK") : TEXT("CENTER - FLAT")));
		HudRailUntil = RealClock + 1.2;
	}

	// ---- Episode Energy panel (right side, under the target panel)
	if (Players.Num() > 0)
	{
		const float PW = 400.f * U, RowH = 30.f * U;
		const float X = W - 424.f * U, Y0 = 212.f * U;
		const bool bPend = Pending.Owner >= 0;
		const float PH = 38.f * U + RowH * Players.Num() + (bPend ? 28.f * U : 0.f) + 26.f * U;
		ZoogiUIStyle::CPanel(Canvas, X, Y0, PW, PH, U, Panel, Cyan);
		D.Text(TEXT("EPISODE ENERGY"), X + 12.f * U, Y0 + 8.f * U, Cyan, 0.42f, false, true);
		float Y = Y0 + 38.f * U;
		for (const FZoogiBattlePlayerState& P : Players)
		{
			const bool bActive = P.Player == G->ActivePlayer;
			const FLinearColor PC = ZoogiUtil::PlayerColor(P.Player);
			const bool bOut = G->Balls.IsValidIndex(P.Player) && G->Balls[P.Player] && G->Balls[P.Player]->bOut;
			if (bActive) ZoogiUIStyle::CRoundRect(Canvas, X + 6.f * U, Y - 2.f * U, PW - 12.f * U, RowH - 2.f * U, 8.f * U, FLinearColor(0.3f, 0.8f, 1.f, 0.16f));
			ZoogiUIStyle::CRoundRect(Canvas, X + 8.f * U, Y + 4.f * U, 18.f * U, 18.f * U, 5.f * U, ZoogiUIStyle::Outline);
			ZoogiUIStyle::CRoundRect(Canvas, X + 10.5f * U, Y + 6.5f * U, 13.f * U, 13.f * U, 3.f * U, PC);
			const FString Nm = FString::Printf(TEXT("P%d %s"), P.Player + 1, *P.Name.Left(8));
			D.Text(Nm, X + 32.f * U, Y + 3.f * U, bOut ? ZoogiUIStyle::MutedText : (bActive ? FLinearColor(1.f, 0.95f, 0.6f) : FLinearColor::White), 0.36f);
			const float E = GetEnergy(P.Player), Cap = FMath::Max(1.f, Tune.EnergyCap);
			const float BX = X + 170.f * U, BW = 150.f * U, BY = Y + 8.f * U, BH = 12.f * U;
			// uistyle: chunky cyan -> electric blue bar, navy outline, gloss, ticks, juice
			if (EnergyJuice.Num() <= P.Player) EnergyJuice.SetNum(P.Player + 1);
			ZoogiUIStyle::FBarJuice& J = EnergyJuice[P.Player];
			J.Update(FMath::Clamp(E / Cap, 0.f, 1.f), FApp::GetDeltaTime());
			ZoogiUIStyle::CBar(Canvas, BX, BY - 1.f * U, BW, BH + 2.f * U, U, FMath::Clamp(E / Cap, 0.f, 1.f), ZoogiUIStyle::EnergyA, ZoogiUIStyle::EnergyB, &J, 4, (float)RealClock);
			D.Text(FString::Printf(TEXT("%.0f"), E), BX + BW + 18.f * U, Y + 3.f * U, FLinearColor::White, 0.34f);
			const UZoogiReactionComponent* R = Reaction(P.Player);
			if (R && R->bAwakened)
			{
				ZoogiUIStyle::CRoundRect(Canvas, X + PW - 52.f * U, Y + 2.f * U, 46.f * U, 24.f * U, 7.f * U, ZoogiUIStyle::Outline);
				ZoogiUIStyle::CRoundRect(Canvas, X + PW - 49.5f * U, Y + 4.5f * U, 41.f * U, 19.f * U, 5.f * U, FLinearColor(1.f, 0.55f, 0.1f, 0.95f));
				D.Text(FString::Printf(TEXT("AWK%d"), R->ReactionsLeft), X + PW - 29.f * U, Y + 5.f * U, FLinearColor::White, 0.26f, true, true);
			}
			Y += RowH;
		}
		if (bPend)
		{
			const FZoogiEpisodeComboRow* Row = Tune.FindCombo(Pending.ComboId);
			D.Text(FString::Printf(TEXT("P%d combo: %s"), Pending.Owner + 1, Row ? *Row->Label : *Pending.ComboId.ToString()), X + 12.f * U, Y + 2.f * U, Cyan, 0.34f);
			Y += 28.f * U;
		}
		D.Text(TEXT("F5 combo  F6 commit  F7/F8 air  F9 rail"), X + 12.f * U, Y + 2.f * U, FLinearColor(0.7f, 0.88f, 1.f, 0.95f), 0.28f);
	}

	// ---- prompt panel (bottom centre above the status line: the top centre belongs to the Episode callouts)
	const float PW = 680.f * U, PX = (W - PW) * 0.5f;
	float PY = H - 300.f * U;
	auto Box = [&](float BH, const FLinearColor& Accent)
	{
		PY = H - 120.f * U - BH;
		ZoogiUIStyle::CPanel(Canvas, PX, PY, PW, BH, U, Panel, Accent);
	};
	const UZoogiMiniGameRailRelease* Rail = Cast<UZoogiMiniGameRailRelease>(ActiveMiniGame);
	const UZoogiMiniGameAirReaction* Air = Cast<UZoogiMiniGameAirReaction>(ActiveMiniGame);
	if (Rail && Rail->bInputOpen && !Rail->bReleaseRequested)
	{
		// Rail Release timing window: E / C / L segments, cursor = time on the rail
		Box(96.f * U, CenterC);
		const bool bMine = G->Balls.IsValidIndex(Rail->Body) && G->Balls[Rail->Body] && !G->Balls[Rail->Body]->bCPU;
		D.Text(FString::Printf(TEXT("RAIL RELEASE  P%d%s"), Rail->Body + 1, bMine ? TEXT("   press F9") : TEXT("   (CPU)")), PX + PW * 0.5f, PY + 8.f * U, FLinearColor::White, 0.46f, true, true);
		const float Tot = FMath::Max(0.01f, Tune.RailEarlyWindow + Tune.RailCenterWindow + Tune.RailLateWindow);
		const float BX = PX + 24.f * U, BW = PW - 48.f * U, BY = PY + 48.f * U, BH = 22.f * U;
		const float We = BW * Tune.RailEarlyWindow / Tot, Wc = BW * Tune.RailCenterWindow / Tot, Wl = BW - We - Wc;
		ZoogiUIStyle::CRoundRect(Canvas, BX - 3.5f * U, BY - 3.5f * U, BW + 7.f * U, BH + 7.f * U, 8.f * U, ZoogiUIStyle::Outline);
		D.Rect(BX, BY, We, BH, EarlyC * FLinearColor(1, 1, 1, 0.85f));
		D.Rect(BX + We, BY, Wc, BH, CenterC * FLinearColor(1, 1, 1, 0.85f));
		D.Rect(BX + We + Wc, BY, Wl, BH, LateC * FLinearColor(1, 1, 1, 0.85f));
		D.Rect(BX, BY + 2.f * U, BW, BH * 0.25f, FLinearColor(1.f, 1.f, 1.f, 0.4f)); // gloss strip
		D.Text(TEXT("EARLY / HIGH"), BX + We * 0.5f, BY + 2.f * U, FLinearColor::White, 0.28f, true, true);
		D.Text(TEXT("CENTER / FLAT"), BX + We + Wc * 0.5f, BY + 2.f * U, FLinearColor::White, 0.28f, true, true);
		D.Text(TEXT("LATE / BANK"), BX + We + Wc + Wl * 0.5f, BY + 2.f * U, FLinearColor::White, 0.28f, true, true);
		const float Cx = BX + BW * FMath::Clamp(Rail->Elapsed / Tot, 0.f, 1.f);
		D.Rect(Cx - 5.f * U, BY - 10.f * U, 10.f * U, BH + 20.f * U, ZoogiUIStyle::Outline);
		D.Rect(Cx - 2.5f * U, BY - 7.5f * U, 5.f * U, BH + 15.f * U, FLinearColor::White);
	}
	else if (Air && Air->bInputOpen && Air->Choice == EZoogiAirChoice::None)
	{
		Box(92.f * U, FLinearColor(1.f, 0.55f, 0.1f));
		const bool bMine = G->Balls.IsValidIndex(Air->Owner) && G->Balls[Air->Owner] && !G->Balls[Air->Owner]->bCPU;
		D.Text(FString::Printf(TEXT("AIR REACTION  P%d"), Air->Owner + 1), PX + PW * 0.5f, PY + 8.f * U, FLinearColor::White, 0.46f, true, true);
		D.Text(bMine ? TEXT("[F7] BRACE        [F8] REDIRECT") : TEXT("CPU is choosing..."), PX + PW * 0.5f, PY + 42.f * U, FLinearColor(1.f, 0.85f, 0.6f), 0.4f, true);
		const float Frac = FMath::Clamp(Air->InputLeft / FMath::Max(0.1f, Tune.MiniGameInputTimeout), 0.f, 1.f);
		ZoogiUIStyle::CBar(Canvas, PX + 24.f * U, PY + 76.f * U, PW - 60.f * U, 8.f * U, U, Frac, FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(1.f, 0.45f, 0.1f), nullptr, 5);
	}
	else if (RealClock < HudRailUntil)
	{
		Box(52.f * U, CenterC);
		const FLinearColor Col = LastRailWindow == EZoogiRailWindow::Early ? EarlyC : (LastRailWindow == EZoogiRailWindow::Late ? LateC : CenterC);
		D.Text(HudRailText, PX + PW * 0.5f, PY + 10.f * U, Col, 0.5f, true, true);
	}
	else if (RealClock < HudFlashUntil)
	{
		Box(52.f * U, HudFlashColor);
		D.Text(HudFlashText, PX + PW * 0.5f, PY + 10.f * U, HudFlashColor, 0.5f, true, true);
	}
	else if (Pending.Owner >= 0 && Pending.Owner == G->ActivePlayer && Pending.Challenge != EZoogiChallenge::None && G->Phase != EZoogiPhase::GameOver)
	{
		// route objective for the committed combo
		const FString Tgt = Pending.DeclaredTarget >= 0 ? BodyLabel(Pending.DeclaredTarget) : FString(TEXT("the lock-on target"));
		FString Obj;
		switch (Pending.Challenge)
		{
		case EZoogiChallenge::HaloBank: Obj = FString::Printf(TEXT("Roll through a Resonance Halo, then hit %s"), *Tgt); break;
		case EZoogiChallenge::OrbLine: Obj = Pending.CommittedOrbs.Num() > 0 ? FString::Printf(TEXT("Touch all %d orbs on the line"), Pending.CommittedOrbs.Num()) : FString(TEXT("Aim through a line of orbs")); break;
		case EZoogiChallenge::SkyBumper: Obj = TEXT("Rebound off a Sky Bumper before any hit"); break;
		case EZoogiChallenge::RailRelease: Obj = TEXT("Roll into the Launch Rail, release in a window"); break;
		case EZoogiChallenge::Strike: Obj = FString::Printf(TEXT("First hit must be %s"), *Tgt); break;
		case EZoogiChallenge::PartnerIntercept: Obj = TEXT("Hit your partner first, then a rival"); break;
		case EZoogiChallenge::AirReaction: Obj = TEXT("Get airborne, then react (F7 / F8)"); break;
		default: break;
		}
		Box(78.f * U, Cyan);
		D.Text(FString::Printf(TEXT("%s  P%d"), *ChallengeTitle(Pending.Challenge), Pending.Owner + 1), PX + PW * 0.5f, PY + 8.f * U, Cyan, 0.44f, true, true);
		D.Text(Obj, PX + PW * 0.5f, PY + 44.f * U, FLinearColor::White, 0.36f, true);
	}

	// ---- SHATTERED signal
	const double SAge = RealClock - LastShatterReal;
	if (SAge >= 0.0 && SAge < 1.6)
	{
		const float A = (float)FMath::Clamp(1.0 - FMath::Max(0.0, SAge - 0.9) / 0.7, 0.0, 1.0);
		const FLinearColor Red(1.f, 0.18f, 0.18f, A);
		const float T = 8.f * U;
		D.Rect(0, 0, W, T, Red); D.Rect(0, H - T, W, T, Red); D.Rect(0, 0, T, H, Red); D.Rect(W - T, 0, T, H, Red);
		const float Pop = 1.f + 0.25f * FMath::Max(0.f, 1.f - (float)SAge / 0.18f);
		ZoogiUIStyle::CPanel(Canvas, W * 0.5f - 300.f * U, H * 0.34f, 600.f * U, 110.f * U, U, FLinearColor(0.5f, 0.02f, 0.12f, 0.85f * A), FLinearColor(1.f, 0.85f, 0.3f, A));
		D.Text(TEXT("SHATTERED"), W * 0.5f, H * 0.34f + 8.f * U, Red, 1.1f * Pop, true, true);
		D.Text(LastShatterReason.Left(48), W * 0.5f, H * 0.34f + 78.f * U, FLinearColor(1.f, 0.85f, 0.85f, A), 0.34f, true);
	}
}
