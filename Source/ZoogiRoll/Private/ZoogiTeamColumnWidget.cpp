// framed: see ZoogiTeamColumnWidget.h
#include "ZoogiTeamColumnWidget.h"
#include "ZoogiGameMode.h"
#include "ZoogiBall.h"
#include "ZoogiTypes.h"
#include "ZoogiArtifacts.h"
#include "ZoogiArtifactComponent.h"
#include "ZoogiCardSubsystem.h"
#include "ZoogiCards.h"
#include "ZoogiCombo.h"
#include "ZoogiBattleStateMachine.h"
#include "ZoogiFramedHUDWidget.h"
#include "ZoogiHudArt.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

int32 UZoogiTeamColumnWidget::SideOf(const AZoogiGameMode* G, const AZoogiBall* B)
{
	if (!G || !B) return 0;
	return G->bComboMode ? FMath::Clamp(B->TeamIndex, 0, 1) : (B->PlayerIndex % 2);
}

FLinearColor UZoogiTeamColumnWidget::SideColor(int32 S) { return S == 0 ? ZoogiUIStyle::Friendly : ZoogiUIStyle::Enemy; }

const FSlateBrush* UZoogiTeamColumnWidget::Portrait(FName Id) const
{
	if (const FSlateBrush* B = PortraitBrush.Find(Id)) return B->GetResourceObject() ? B : nullptr;
	const FString Path = FString::Printf(TEXT("/Game/UI/Portraits/T_Portrait_%s.T_Portrait_%s"), *Id.ToString(), *Id.ToString());
	UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path);
	PortraitTex.Add(Id, T);
	FSlateBrush& B = PortraitBrush.Add(Id);
	if (T) { B.SetResourceObject(T); B.ImageSize = FVector2D(128.f, 128.f); B.DrawAs = ESlateBrushDrawType::Image; }
	return T ? &B : nullptr;
}

void UZoogiTeamColumnWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	Time += Dt;
	TurnAnim = FMath::Min(1.f, TurnAnim + Dt * 2.5f);
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	if (!G) return;
	if (G->ActivePlayer != LastActive) { LastActive = G->ActivePlayer; TurnAnim = 0.f; }
	PromptAnim = FMath::Min(1.f, PromptAnim + Dt * 4.f);
	const FString P = UZoogiFramedHUDWidget::PromptText(G);
	if (P != LastPrompt) { LastPrompt = P; PromptAnim = 0.f; }
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G);
	const float Cap = SM ? FMath::Max(1.f, SM->GetTuning().EnergyCap) : 100.f;
	for (const AZoogiBall* B : G->Balls)
	{
		if (!B || SideOf(G, B) != Side) continue;
		EnergyJuice.FindOrAdd(B->PlayerIndex).Update(SM ? SM->GetEnergy(B->PlayerIndex) / Cap : 0.f, Dt);
		int32& LS = LastScore.FindOrAdd(B->PlayerIndex, B->Score);
		float& Pop = ScorePop.FindOrAdd(B->PlayerIndex, 0.f);
		if (B->Score != LS) { LS = B->Score; Pop = 1.f; }
		Pop = FMath::Max(0.f, Pop - Dt * 2.5f);
	}
	if (G->bComboMode) ComboJuice.Update(G->ComboEnergy[Side] / FMath::Max(1.f, ZoogiComboTune::GaugeMax), Dt);
}

int32 UZoogiTeamColumnWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	// hud-v3 (Evan's 3:53 PM target): per Zoogi a carved stone group = header bar (+ gem), card panel (portrait ring,
	// name, CPU / TURN chips, score, ORBS / KOS / HAND, status chips, EPISODE rune bar), then the acting Zoogi's ACTION
	// box or a carved emblem plate; diamond connectors between groups. Art from ZoogiHudArt, data unchanged.
	using namespace ZoogiUIStyle;
	namespace HA = ZoogiHudArt;
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const FVector2D S = Geo.GetLocalSize();
	if (!G || S.X < 20.f || S.Y < 20.f) return Layer;
	const int32 L = Layer;
	const float t0 = TScale;                                  // local px per target px
	auto FontT = [&](float Sz, float tt) { return BoldFont(FMath::Max(9, FMath::RoundToInt(Sz * tt)), FMath::Max(1.f, 1.6f * tt)); };
	auto Txt = [&](int32 Lay, const FVector2D& P, const FString& T, float Sz, float tt, const FLinearColor& C, float Ax = 0.f, float Ay = 0.5f) { HA::Text(Out, Lay, Geo, P, T, FontT(Sz, tt), C, Ax, Ay); };
	auto TxtW = [&](const FString& T, float Sz, float tt) { return (float)HA::Measure(T, FontT(Sz, tt)).X; };
	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G);
	const float Cap = SM ? FMath::Max(1.f, SM->GetTuning().EnergyCap) : 100.f;
	const UZoogiCardSubsystem* Cards = G->CardsOn() ? UZoogiCardSubsystem::Get(G) : nullptr;
	TArray<const AZoogiBall*> Mine;
	for (const AZoogiBall* B : G->Balls) if (B && SideOf(G, B) == Side) Mine.Add(B);
	if (Mine.Num() == 0) return L;
	const float ComboH = G->bComboMode ? 46.f * t0 : 0.f;
	const float Gap = 6.f * t0;
	const float GH = (S.Y - ComboH - Gap * (Mine.Num() - 1)) / Mine.Num();
	const AZoogiBall* ActB = nullptr;
	if (G->Phase != EZoogiPhase::GameOver && G->Phase != EZoogiPhase::Forming) for (const AZoogiBall* B : Mine) if (B->PlayerIndex == G->ActivePlayer) ActB = B;
	const FLinearColor ScoreC = Side == 0 ? FLinearColor(0.55f, 0.85f, 1.f) : FLinearColor(1.f, 0.45f, 0.4f);
	for (int32 gi = 0; gi < Mine.Num(); ++gi)
	{
		const AZoogiBall* B = Mine[gi];
		const float t = t0 * FMath::Clamp(GH / 248.f, 0.62f, 1.f); // squeeze on short columns
		const float Y = gi * (GH + Gap), W = S.X;
		const bool bActive = B->PlayerIndex == G->ActivePlayer && G->Phase != EZoogiPhase::GameOver;
		const float Slide = bActive ? (1.f - FMath::Square(1.f - TurnAnim)) : 1.f;
		const float X = (1.f - Slide) * (Side == 0 ? -24.f : 24.f) * t;
		// group panel (+ active turn glow)
		if (bActive) ZoogiUIStyle::SBox(Out, L, Geo, FVector2D(X - 4.f * t, Y - 4.f * t), FVector2D(W + 8.f * t, GH + 8.f * t), FLinearColor(HA::Cyan.R, HA::Cyan.G, HA::Cyan.B, 0.18f + 0.14f * FMath::Sin(Time * 4.f)), FLinearColor::Transparent, 12.f * t, 0.f);
		HA::Nine(Out, L + 1, Geo, FVector2D(X, Y), FVector2D(W, GH), TEXT("group_panel"), FMargin(10.f), t);
		// header bar: team label on the first group, a side gem at the right
		HA::Nine(Out, L + 2, Geo, FVector2D(X + 8.f * t, Y + 9.f * t), FVector2D(W - 16.f * t, 23.f * t), TEXT("header_bar"), FMargin(8.f, 6.f, 8.f, 6.f), t);
		if (gi == 0)
		{
			FString Head = G->bComboMode ? AZoogiGameMode::TeamName(Side) : FString::Printf(TEXT("%s  P%d"), Side == 0 ? TEXT("FRIENDLY") : TEXT("RIVAL"), B->PlayerIndex + 1);
			Txt(L + 6, FVector2D(X + 20.f * t, Y + 20.5f * t), Head.ToUpper(), 10.5f, t, FLinearColor(0.92f, 0.92f, 0.9f));
		}
		const TCHAR* GemN = gi == 0 ? (Side == 0 ? TEXT("gem_blue") : TEXT("gem_orange")) : (Side == 1 ? TEXT("gem_teal") : nullptr);
		if (GemN) HA::Img(Out, L + 3, Geo, FVector2D(X + W - 44.f * t, Y + 3.f * t), FVector2D(34.f, 31.f) * t, GemN);
		// card panel
		const FVector2D CP(X + 10.f * t, Y + 38.f * t), CS(W - 20.f * t, 133.f * t);
		HA::Nine(Out, L + 2, Geo, CP, CS, TEXT("card_panel"), FMargin(10.f), t);
		// portrait in a status ring: cyan friendly, red rival, bronze when out / knocked out
		const FVector2D PC = CP + FVector2D(36.f, 35.f) * t;
		const float PR = 29.f * t;
		const bool bDim = B->bOut || B->bKnockedOut;
		ZoogiUIStyle::SBox(Out, L + 3, Geo, PC - FVector2D(PR, PR) * 0.8f, FVector2D(PR, PR) * 1.6f, FLinearColor(0.05f, 0.07f, 0.1f, 1.f), FLinearColor::Transparent, PR * 0.8f, 0.f);
		if (const FSlateBrush* Br = Portrait(B->Def.Id))
		{
			FSlateBrush RB = *Br; RB.DrawAs = ESlateBrushDrawType::RoundedBox; RB.OutlineSettings = FSlateBrushOutlineSettings(PR * 0.78f);
			RB.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			FSlateDrawElement::MakeBox(Out, L + 4, Geo.ToPaintGeometry(FVector2f(FVector2D(PR, PR) * 1.56f), FSlateLayoutTransform(FVector2f(PC - FVector2D(PR, PR) * 0.78f))), &RB, ESlateDrawEffect::None, bDim ? FLinearColor(0.45f, 0.45f, 0.5f) : FLinearColor::White);
		}
		else ZoogiUIStyle::SBox(Out, L + 4, Geo, PC - FVector2D(PR, PR) * 0.78f, FVector2D(PR, PR) * 1.56f, B->Def.Color, FLinearColor::Transparent, PR * 0.78f, 0.f);
		const TCHAR* Ring = bDim ? TEXT("ring_bronze") : (Side == 0 ? TEXT("ring_cyan") : TEXT("ring_red"));
		const float RP = bActive ? 1.f + 0.04f * FMath::Sin(Time * 5.f) : 1.f;
		HA::Img(Out, L + 5, Geo, PC - FVector2D(PR, PR) * RP, FVector2D(PR, PR) * 2.f * RP, Ring);
		// name, chips, score, stats
		const float TX = CP.X + 72.f * t;
		Txt(L + 6, FVector2D(TX, CP.Y + 17.f * t), FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), 13.f, t, bActive ? FLinearColor(1.f, 0.95f, 0.75f) : FLinearColor(0.95f, 0.93f, 0.88f));
		float CX = TX;
		auto Chip = [&](const TCHAR* Art, const FString& Lb, float Wd)
		{
			HA::Nine(Out, L + 5, Geo, FVector2D(CX, CP.Y + 28.f * t), FVector2D(Wd, 16.f * t), Art, FMargin(7.f, 5.f, 7.f, 5.f), t);
			Txt(L + 6, FVector2D(CX + Wd * 0.5f, CP.Y + 36.f * t), Lb, 8.f, t, FLinearColor::White, 0.5f);
			CX += Wd + 5.f * t;
		};
		Chip(TEXT("chip_purple"), B->bCPU ? TEXT("CPU") : TEXT("YOU"), FMath::Max(34.f * t, TxtW(B->bCPU ? TEXT("CPU") : TEXT("YOU"), 8.f, t) + 14.f * t));
		if (bActive) Chip(TEXT("chip_gold"), TEXT("TURN"), 41.f * t);
		const float Pop = ScorePop.Contains(B->PlayerIndex) ? ScorePop[B->PlayerIndex] : 0.f;
		Txt(L + 6, FVector2D(CP.X + CS.X - 12.f * t, CP.Y + 19.f * t - 4.f * Pop * t), FString::FromInt(B->Score), 21.f + 9.f * Pop, t, FMath::Lerp(ScoreC, HA::GoldText, Pop), 1.f);
		int32 Hand = -1;
		if (Cards) { if (const FZoogiCardPlayerState* PS2 = Cards->Rules().Player(B->PlayerIndex)) Hand = PS2->Hand.Num(); }
		else if (G->bArtifactMode && B->Artifact) Hand = B->Artifact->Hand.Num();
		FString Stats = FString::Printf(TEXT("ORBS %d  KOS %d"), B->OrbsScored, B->KnockOuts);
		if (Hand >= 0) Stats += FString::Printf(TEXT("  HAND %d"), Hand);
		Txt(L + 6, FVector2D(TX, CP.Y + 54.f * t), Stats, 8.5f, t, FLinearColor(0.82f, 0.88f, 0.95f));
		// status / power chips (row under the portrait)
		TArray<TPair<FString, FLinearColor>> Chips;
		if (B->bKnockedOut) Chips.Add({ TEXT("KO"), LossRed }); else if (B->bOut) Chips.Add({ TEXT("OUT"), LossRed });
		if (B->StunTurns > 0) Chips.Add({ FString::Printf(TEXT("STUN %d"), B->StunTurns), FLinearColor(0.85f, 0.6f, 1.f) });
		if (B->BoundShots > 0) Chips.Add({ TEXT("BOUND"), FLinearColor(0.85f, 0.6f, 1.f) });
		if (B->Artifact)
		{
			if (B->Artifact->Guard != EZoogiArtifact::None) Chips.Add({ TEXT("GUARD"), Friendly });
			if (B->Artifact->Status != EZoogiStatus::None) Chips.Add({ ZoogiArtifacts::StatusName(B->Artifact->Status), FLinearColor(1.f, 0.6f, 0.3f) });
			if (B->Artifact->Infused != EZoogiArtifact::None) Chips.Add({ TEXT("INFUSED"), Violet });
		}
		if (!G->bArtifactMode && !G->CardsOn()) Chips.Add({ FString::Printf(TEXT("POWER %d/3"), B->Charges), Violet });
		float SX = CP.X + 8.f * t;
		for (const auto& C : Chips)
		{
			const float CW = TxtW(C.Key, 7.5f, t) + 12.f * t;
			if (SX + CW > CP.X + CS.X - 8.f * t) break;
			ZoogiUIStyle::SBox(Out, L + 5, Geo, FVector2D(SX, CP.Y + 72.f * t), FVector2D(CW, 14.f * t), FLinearColor(C.Value.R * 0.35f, C.Value.G * 0.35f, C.Value.B * 0.35f, 0.95f), C.Value, 5.f * t, 1.f * t);
			Txt(L + 6, FVector2D(SX + CW * 0.5f, CP.Y + 79.f * t), C.Key, 7.5f, t, FLinearColor::White, 0.5f);
			SX += CW + 4.f * t;
		}
		if (G->bComboMode)
			for (int32 m = 0; m < 2; ++m) HA::Img(Out, L + 5, Geo, FVector2D(CP.X + CS.X - 40.f * t + m * 17.f * t, CP.Y + 68.f * t), FVector2D(16.f, 16.f) * t, m < B->RingMarks && !B->bKnockedOut ? TEXT("mark_cyan") : TEXT("mark_cracked"));
		// EPISODE rune bar (ui kit gauge): empty bar, the full bar revealed by UV up to the value
		if (SM && SM->IsFlowOn())
		{
			const float E = SM->GetEnergy(B->PlayerIndex);
			const bool bReady = E >= Cap - 0.01f;
			const ZoogiUIStyle::FBarJuice* J = EnergyJuice.Find(B->PlayerIndex);
			const float F = FMath::Clamp(J ? J->Shown : E / Cap, 0.f, 1.f);
			Txt(L + 6, FVector2D(CP.X + 9.f * t, CP.Y + 98.f * t), bReady ? FString(TEXT("EPISODE READY!")) : FString::Printf(TEXT("EPISODE %.0f/%.0f"), E, Cap), 8.f, t, bReady ? FMath::Lerp(HA::GoldText, FLinearColor::White, 0.5f + 0.5f * FMath::Sin(Time * 8.f)) : HA::Cyan);
			const FVector2D BP(CP.X + 6.f * t, CP.Y + 106.f * t), BS(CS.X - 12.f * t, 16.f * t);
			HA::Img(Out, L + 5, Geo, BP, BS, TEXT("rbar_empty"));
			if (F > 0.001f)
			{
				const FVector2D FS = HA::SizeT(TEXT("rbar_full"));
				HA::Part(Out, L + 6, Geo, BP, FVector2D(BS.X * F, BS.Y), TEXT("rbar_full"), FBox2D(FVector2D::ZeroVector, FVector2D(FS.X * F, FS.Y)), bReady ? FLinearColor(1.f, 0.9f, 0.5f) : FLinearColor::White);
			}
			if (J && J->Flash > 0.f) ZoogiUIStyle::SBox(Out, L + 7, Geo, BP, BS, FLinearColor(1.f, 1.f, 1.f, 0.4f * J->Flash), FLinearColor::Transparent, 6.f * t, 0.f);
		}
		// bottom of the group: the acting Zoogi's ACTION box, else the carved emblem plate
		const float BY = CP.Y + CS.Y + 5.f * t, BH = Y + GH - 9.f * t - BY;
		if (B == ActB && BH > 20.f * t)
		{
			const float Sl2 = 1.f - FMath::Square(1.f - PromptAnim);
			const FVector2D AP(X + 10.f * t, BY), AS(W - 20.f * t, FMath::Min(BH, 64.f * t));
			HA::Nine(Out, L + 2, Geo, AP, AS, TEXT("action_box"), FMargin(9.f, 9.f, 9.f, 9.f), t, FMath::Lerp(FLinearColor(1.4f, 1.3f, 1.f), FLinearColor::White, Sl2));
			const FString AL = UZoogiFramedHUDWidget::ActionLabel(G);
			const float LW = FMath::Max(59.f * t, TxtW(AL, 8.f, t) + 16.f * t);
			HA::Nine(Out, L + 4, Geo, AP + FVector2D(10.f, 6.f) * t, FVector2D(LW, 15.f * t), TEXT("chip_blue"), FMargin(7.f, 5.f, 7.f, 5.f), t);
			Txt(L + 6, AP + FVector2D(10.f * t + LW * 0.5f, 13.5f * t), AL, 8.f, t, FLinearColor::White, 0.5f);
			Txt(L + 6, AP + FVector2D(16.f * t + LW, 13.5f * t), B->bCPU ? TEXT("CPU") : TEXT("YOUR MOVE"), 7.5f, t, B->bCPU ? FLinearColor(0.75f, 0.78f, 0.82f) : HA::Cyan);
			// prompt (wrapped) + Battle prompt title / bar when there is one
			TArray<FString> Words; LastPrompt.ParseIntoArrayWS(Words);
			FString Line; float LY = AP.Y + 31.f * t; const float MaxW = AS.X - 20.f * t; int32 NL = 0;
			for (const FString& Wd : Words)
			{
				const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
				if (!Line.IsEmpty() && TxtW(Try, 9.f, t) > MaxW) { Txt(L + 6, FVector2D(AP.X + 10.f * t, LY), Line, 9.f, t, FLinearColor(1.f, 1.f, 1.f, 0.4f + 0.6f * Sl2)); LY += 13.f * t; Line = Wd; if (++NL >= 2) { Line += TEXT(".."); break; } }
				else Line = Try;
			}
			Txt(L + 6, FVector2D(AP.X + 10.f * t, LY), Line, 9.f, t, FLinearColor(1.f, 1.f, 1.f, 0.4f + 0.6f * Sl2));
			FZoogiBattleHudPrompt BPr;
			if (SM && SM->GetHudPrompt(BPr) && BH > 70.f * t)
			{
				const float PY = AP.Y + AS.Y + 4.f * t;
				Txt(L + 6, FVector2D(AP.X + 10.f * t, PY + 6.f * t), BPr.Title.Left(30), 8.5f, t, BPr.Accent);
				if (BPr.Frac >= 0.f && PY + 26.f * t < Y + GH - 8.f * t)
				{
					const FVector2D RBp(AP.X + 6.f * t, PY + 14.f * t), RBs(AS.X - 12.f * t, 12.f * t);
					HA::Img(Out, L + 5, Geo, RBp, RBs, TEXT("rbar_empty"));
					const FVector2D FS = HA::SizeT(TEXT("rbar_full"));
					HA::Part(Out, L + 6, Geo, RBp, FVector2D(RBs.X * BPr.Frac, RBs.Y), TEXT("rbar_full"), FBox2D(FVector2D::ZeroVector, FVector2D(FS.X * BPr.Frac, FS.Y)), BPr.Accent);
				}
			}
		}
		else if (BH > 20.f * t)
		{
			const FVector2D ES = HA::SizeT(TEXT("emblem"));
			const float EW = W - 20.f * t, EH = FMath::Min(BH, EW * ES.Y / FMath::Max(1.f, ES.X));
			HA::Img(Out, L + 2, Geo, FVector2D(X + 10.f * t, BY + (BH - EH) * 0.5f), FVector2D(EW, EH), TEXT("emblem"));
		}
		// diamond connector into the next group
		if (gi + 1 < Mine.Num()) HA::Img(Out, L + 7, Geo, FVector2D(W * 0.5f - 32.f * t, Y + GH - 18.f * t + Gap * 0.5f), FVector2D(64.f, 40.f) * t, TEXT("connector"));
	}
	if (G->bComboMode)
	{
		const float BY = S.Y - ComboH + 4.f * t0;
		const float E = G->ComboEnergy[Side];
		const bool bReady = E >= ZoogiComboTune::FollowUpCost;
		HA::Nine(Out, L + 1, Geo, FVector2D(0.f, BY), FVector2D(S.X, ComboH - 4.f * t0), TEXT("header_bar"), FMargin(8.f, 6.f, 8.f, 6.f), t0);
		Txt(L + 6, FVector2D(12.f * t0, BY + 11.f * t0), FString::Printf(TEXT("COMBO %.0f / %.0f%s"), E, ZoogiComboTune::GaugeMax, bReady ? TEXT("  READY") : TEXT("")), 8.5f, t0, bReady ? HA::GoldText : FLinearColor(0.85f, 0.9f, 1.f));
		const FVector2D RBp(8.f * t0, BY + 20.f * t0), RBs(S.X - 16.f * t0, 16.f * t0);
		HA::Img(Out, L + 5, Geo, RBp, RBs, TEXT("rbar_empty"));
		const float F = FMath::Clamp(ComboJuice.Shown, 0.f, 1.f);
		const FVector2D FS = HA::SizeT(TEXT("rbar_full"));
		if (F > 0.f) HA::Part(Out, L + 6, Geo, RBp, FVector2D(RBs.X * F, RBs.Y), TEXT("rbar_full"), FBox2D(FVector2D::ZeroVector, FVector2D(FS.X * F, FS.Y)));
	}
	return L + 10;
}
