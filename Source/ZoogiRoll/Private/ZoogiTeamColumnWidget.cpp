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
	using namespace ZoogiUIStyle;
	const AZoogiGameMode* G = GetWorld() ? GetWorld()->GetAuthGameMode<AZoogiGameMode>() : nullptr;
	const FVector2D S = Geo.GetLocalSize();
	if (!G || S.X < 20.f || S.Y < 20.f) return Layer;
	const float u = UScale;
	const int32 L = Layer;
	const FLinearColor SC = SideColor(Side);
	auto Font = [&](float Sz) { return BoldFont(FMath::Max(7, FMath::RoundToInt(Sz * u)), FMath::Max(1.f, 2.f * u)); };
	auto Txt = [&](int32 Lay, float X, float Y, const FString& T, float Sz, const FLinearColor& C) { ZoogiUIStyle::SText(Out, Lay, Geo, FVector2D(X, Y), T, Font(Sz), C); };
	auto TxtW = [&](const FString& T, float Sz) { return FSlateApplication::IsInitialized() ? (float)FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Font(Sz)).X : T.Len() * Sz * u * 0.62f; };

	// Column panel + header
	ZoogiUIStyle::SPanel(Out, L, Geo, FVector2D::ZeroVector, S, FLinearColor(0.02f, 0.08f, 0.30f, 0.92f), SC, 16.f * u);
	FString Header;
	if (G->bComboMode) Header = AZoogiGameMode::TeamName(Side);
	else { for (const AZoogiBall* B : G->Balls) if (B && SideOf(G, B) == Side) Header += FString::Printf(TEXT("%sP%d"), Header.IsEmpty() ? TEXT("") : TEXT(" + "), B->PlayerIndex + 1); }
	Header = FString::Printf(TEXT("%s  %s"), Side == 0 ? TEXT("FRIENDLY") : TEXT("RIVAL"), *Header);
	ZoogiUIStyle::SBox(Out, L + 4, Geo, FVector2D(10.f * u, 10.f * u), FVector2D(S.X - 20.f * u, 34.f * u), FLinearColor(SC.R * 0.45f, SC.G * 0.45f, SC.B * 0.45f, 0.95f), Outline, 10.f * u, 3.f * u);
	Txt(L + 6, 22.f * u, 14.f * u, Header, 15.f, FLinearColor::White);

	const UZoogiBattleStateMachine* SM = UZoogiBattleStateMachine::Get(G);
	const float Cap = SM ? FMath::Max(1.f, SM->GetTuning().EnergyCap) : 100.f;
	const UZoogiCardSubsystem* Cards = G->CardsOn() ? UZoogiCardSubsystem::Get(G) : nullptr;
	TArray<const AZoogiBall*> Mine;
	for (const AZoogiBall* B : G->Balls) if (B && SideOf(G, B) == Side) Mine.Add(B);
	const float Top = 54.f * u;
	const float ComboH = G->bComboMode ? 70.f * u : 0.f;
	// word wrap to a pixel width (measured with the real font)
	auto Wrap = [&](const FString& T, float Sz, float MaxW, int32 MaxLines)
	{
		TArray<FString> Words, Lines; T.ParseIntoArrayWS(Words);
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && TxtW(Try, Sz) > MaxW) { Lines.Add(Cur); Cur = Wd; } else Cur = Try;
		}
		if (!Cur.IsEmpty()) Lines.Add(Cur);
		if (Lines.Num() > MaxLines) { Lines.SetNum(MaxLines); Lines.Last() += TEXT(".."); }
		return Lines;
	};
	// ACTION box content for the acting Zoogi in this column (prompt / status / confirmation + Battle prompt)
	const AZoogiBall* ActB = nullptr;
	if (G->Phase != EZoogiPhase::GameOver && G->Phase != EZoogiPhase::Forming) for (const AZoogiBall* B : Mine) if (B->PlayerIndex == G->ActivePlayer) ActB = B;
	const float BoxW = S.X - 20.f * u, BoxIn = BoxW - 28.f * u;
	TArray<FString> ALines, BLines;
	FZoogiBattleHudPrompt BP;
	bool bBP = false;
	float ActionH = 0.f;
	if (ActB)
	{
		ALines = Wrap(LastPrompt, 12.5f, BoxIn, bCompact ? 3 : 4);
		bBP = SM && SM->GetHudPrompt(BP);
		if (bBP) BLines = Wrap(BP.Body, 11.5f, BoxIn, 2);
		ActionH = 36.f * u + ALines.Num() * 19.f * u + 18.f * u; // header row + text + padding + 8 gap
		if (bBP) ActionH += 34.f * u + BLines.Num() * 17.f * u + (BP.Frac >= 0.f ? 20.f * u : 0.f);
	}
	const float CardH = FMath::Min(bCompact ? 150.f * u : 178.f * u, (S.Y - Top - ComboH - 10.f * u - ActionH) / FMath::Max(1, Mine.Num()) - 8.f * u);
	float Y = Top;
	for (const AZoogiBall* B : Mine)
	{
		const bool bActive = B->PlayerIndex == G->ActivePlayer && G->Phase != EZoogiPhase::GameOver;
		const FLinearColor PC = ZoogiUtil::PlayerColor(B->PlayerIndex);
		const float X = 10.f * u, W = S.X - 20.f * u;
		const float Slide = bActive ? (1.f - FMath::Square(1.f - TurnAnim)) : 1.f;
		const float Glow = bActive ? 0.55f + 0.45f * FMath::Sin(Time * 5.f) : 0.f;
		const float ExtraH = B == ActB ? ActionH : 0.f;
		if (bActive) ZoogiUIStyle::SBox(Out, L + 1, Geo, FVector2D(X - 5.f * u, Y - 5.f * u), FVector2D(W + 10.f * u, CardH + ExtraH + 10.f * u), FLinearColor(Gold.R, Gold.G, Gold.B, 0.35f + 0.4f * Glow), FLinearColor::Transparent, 16.f * u, 0.f);
		const FLinearColor Fill = B->bOut || B->bKnockedOut ? FLinearColor(0.10f, 0.13f, 0.30f, 0.95f) : FLinearColor(0.04f, 0.16f, 0.48f, 0.97f);
		ZoogiUIStyle::SBox(Out, L + 2, Geo, FVector2D(X + (1.f - Slide) * (Side == 0 ? -30.f : 30.f) * u, Y), FVector2D(W, CardH), Fill, bActive ? Gold : Outline, 14.f * u, 3.5f * u);
		ZoogiUIStyle::SBox(Out, L + 3, Geo, FVector2D(X + 12.f * u, Y + 4.f * u), FVector2D(W - 24.f * u, 4.f * u), FLinearColor(1.f, 1.f, 1.f, 0.15f), FLinearColor::Transparent, 2.f, 0.f);
		ZoogiUIStyle::SBox(Out, L + 3, Geo, FVector2D(X + 3.5f * u, Y + 10.f * u), FVector2D(6.f * u, CardH - 20.f * u), SC, FLinearColor::Transparent, 3.f * u, 0.f);
		// portrait
		const float PS = FMath::Min(76.f * u, CardH * 0.46f);
		const FVector2D PP(X + 16.f * u, Y + 12.f * u);
		ZoogiUIStyle::SBox(Out, L + 3, Geo, PP - FVector2D(4.f * u, 4.f * u), FVector2D(PS + 8.f * u, PS + 8.f * u), PC, Outline, 12.f * u, 3.f * u);
		if (const FSlateBrush* Br = Portrait(B->Def.Id))
			FSlateDrawElement::MakeBox(Out, L + 4, Geo.ToPaintGeometry(FVector2f(PS, PS), FSlateLayoutTransform(FVector2f(PP))), Br, ESlateDrawEffect::None, B->bOut ? FLinearColor(0.5f, 0.55f, 0.8f) : FLinearColor::White);
		else ZoogiUIStyle::SBox(Out, L + 4, Geo, PP, FVector2D(PS, PS), B->Def.Color, FLinearColor::Transparent, 9.f * u, 0.f);
		// name, tag, score
		const float TX = PP.X + PS + 12.f * u;
		Txt(L + 6, TX, Y + 10.f * u, FString::Printf(TEXT("P%d %s"), B->PlayerIndex + 1, *B->Def.Name.ToUpper()), 17.f, bActive ? FLinearColor(1.f, 0.95f, 0.6f) : FLinearColor::White);
		const FString Tag = B->bCPU ? TEXT("CPU") : TEXT("HUMAN");
		const FLinearColor TagC = B->bCPU ? FLinearColor(1.f, 0.55f, 0.25f) : Mint;
		ZoogiUIStyle::SBox(Out, L + 5, Geo, FVector2D(TX, Y + 38.f * u), FVector2D(TxtW(Tag, 12.f) + 16.f * u, 22.f * u), FLinearColor(TagC.R * 0.5f, TagC.G * 0.5f, TagC.B * 0.5f, 1.f), Outline, 8.f * u, 2.f * u);
		Txt(L + 6, TX + 8.f * u, Y + 40.f * u, Tag, 12.f, FLinearColor::White);
		if (bActive) { const float TW2 = TxtW(TEXT("TURN"), 12.f) + 16.f * u; ZoogiUIStyle::SBox(Out, L + 5, Geo, FVector2D(TX + TxtW(Tag, 12.f) + 22.f * u, Y + 38.f * u), FVector2D(TW2, 22.f * u), FLinearColor(0.85f, 0.52f, 0.04f, 1.f), Outline, 8.f * u, 2.f * u); Txt(L + 6, TX + TxtW(Tag, 12.f) + 30.f * u, Y + 40.f * u, TEXT("TURN"), 12.f, FLinearColor::White); }
		const float Pop = ScorePop.Contains(B->PlayerIndex) ? ScorePop[B->PlayerIndex] : 0.f;
		const FString Sc = FString::FromInt(B->Score);
		Txt(L + 6, X + W - 16.f * u - TxtW(Sc, 30.f + 14.f * Pop), Y + 6.f * u - 6.f * Pop * u, Sc, 30.f + 14.f * Pop, FMath::Lerp(PC + FLinearColor(0.25f, 0.25f, 0.25f, 0.f), Gold, Pop));
		// stats line (the old scoreboard data)
		int32 Hand = -1;
		if (Cards) { if (const FZoogiCardPlayerState* PS2 = Cards->Rules().Player(B->PlayerIndex)) Hand = PS2->Hand.Num(); }
		else if (G->bArtifactMode && B->Artifact) Hand = B->Artifact->Hand.Num();
		FString Stats = FString::Printf(TEXT("ORBS %d  KOS %d"), B->OrbsScored, B->KnockOuts);
		if (Hand >= 0) Stats += FString::Printf(TEXT("  HAND %d"), Hand);
		Txt(L + 6, TX, Y + 66.f * u, Stats, 11.5f, FLinearColor(0.72f, 0.9f, 1.f));
		// status effects + power indicators (chips)
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
		float CX = X + 16.f * u;
		const float CY = Y + PS + 22.f * u;
		for (const auto& C : Chips)
		{
			const float CW = TxtW(C.Key, 11.f) + 14.f * u;
			if (CX + CW > X + W - 8.f * u) break;
			ZoogiUIStyle::SBox(Out, L + 5, Geo, FVector2D(CX, CY), FVector2D(CW, 20.f * u), FLinearColor(C.Value.R * 0.45f, C.Value.G * 0.45f, C.Value.B * 0.45f, 1.f), Outline, 7.f * u, 2.f * u);
			Txt(L + 6, CX + 7.f * u, CY + 2.f * u, C.Key, 11.f, FLinearColor::White);
			CX += CW + 6.f * u;
		}
		if (G->bComboMode)
		{
			// Ring marks: the only "lives" stat (2v2). Real data, drawn as pips (no invented HP).
			for (int32 m = 0; m < 2; ++m)
			{
				const bool bOn = m < B->RingMarks && !B->bKnockedOut;
				ZoogiUIStyle::SBox(Out, L + 5, Geo, FVector2D(X + W - 60.f * u + m * 24.f * u, CY), FVector2D(18.f * u, 18.f * u), bOn ? Gold : Track, Outline, 5.f * u, 2.f * u);
			}
		}
		// Episode Energy (real value from UZoogiBattleStateMachine), juice + READY
		if (SM && SM->IsFlowOn())
		{
			const float E = SM->GetEnergy(B->PlayerIndex);
			const bool bReady = E >= Cap - 0.01f;
			const float BY = Y + CardH - 30.f * u;
			const FString EL = bReady ? TEXT("EPISODE READY!") : FString::Printf(TEXT("EPISODE %.0f/%.0f"), E, Cap);
			Txt(L + 6, X + 16.f * u, BY - 18.f * u, EL, 11.f, bReady ? FMath::Lerp(Gold, FLinearColor::White, 0.5f + 0.5f * FMath::Sin(Time * 8.f)) : EnergyA);
			const ZoogiUIStyle::FBarJuice* J = EnergyJuice.Find(B->PlayerIndex);
			if (bReady) ZoogiUIStyle::SBox(Out, L + 6, Geo, FVector2D(X + 10.f * u, BY - 6.f * u), FVector2D(W - 50.f * u, 26.f * u), FLinearColor(Gold.R, Gold.G, Gold.B, 0.3f + 0.3f * FMath::Sin(Time * 8.f)), FLinearColor::Transparent, 12.f * u, 0.f);
			ZoogiUIStyle::SBar(Out, L + 7, Geo, FVector2D(X + 16.f * u, BY), FVector2D(W - 62.f * u, 14.f * u), E / Cap, bReady ? Gold : EnergyA, bReady ? FLinearColor(1.f, 0.55f, 0.1f) : EnergyB, J, 4);
		}
		if (B == ActB)
		{
			// ACTION box: attached under the acting Zoogi's card, slides in on the turn change, flashes on a new prompt
			const float AY = Y + CardH + 4.f * u, AH = ActionH - 8.f * u;
			const float Sl2 = 1.f - FMath::Square(1.f - PromptAnim);
			const float AX = X + (1.f - Slide) * (Side == 0 ? -30.f : 30.f) * u;
			ZoogiUIStyle::SBox(Out, L + 2, Geo, FVector2D(AX, AY), FVector2D(W, AH), FLinearColor(0.02f, 0.07f, 0.28f, 0.98f), FMath::Lerp(FLinearColor::White, Gold, Sl2), 12.f * u, 3.f * u);
			ZoogiUIStyle::SBox(Out, L + 3, Geo, FVector2D(AX + 3.5f * u, AY + 8.f * u), FVector2D(5.f * u, AH - 16.f * u), SC, FLinearColor::Transparent, 2.5f * u, 0.f);
			const FString AL = UZoogiFramedHUDWidget::ActionLabel(G);
			const float LW = TxtW(AL, 11.f) + 18.f * u;
			ZoogiUIStyle::SBox(Out, L + 4, Geo, FVector2D(AX + 14.f * u, AY + 8.f * u), FVector2D(LW, 22.f * u), FLinearColor(SC.R * 0.5f, SC.G * 0.5f, SC.B * 0.5f, 1.f), Outline, 8.f * u, 2.f * u);
			Txt(L + 6, AX + 23.f * u, AY + 10.f * u, AL, 11.f, FLinearColor::White);
			Txt(L + 6, AX + 14.f * u + LW + 8.f * u, AY + 11.f * u, B->bCPU ? TEXT("CPU") : TEXT("YOUR MOVE"), 10.f, B->bCPU ? MutedText : Mint);
			float TY = AY + 36.f * u + (1.f - Sl2) * 6.f * u;
			for (const FString& Ln : ALines) { Txt(L + 6, AX + 16.f * u, TY, Ln, 12.5f, FLinearColor(1.f, 1.f, 1.f, 0.4f + 0.6f * Sl2)); TY += 19.f * u; }
			if (bBP)
			{
				TY += 6.f * u;
				ZoogiUIStyle::SBox(Out, L + 3, Geo, FVector2D(AX + 10.f * u, TY - 4.f * u), FVector2D(W - 20.f * u, AY + AH - TY - 2.f * u), FLinearColor(BP.Accent.R * 0.25f, BP.Accent.G * 0.25f, BP.Accent.B * 0.3f, 0.95f), BP.Accent, 9.f * u, 2.f * u);
				Txt(L + 6, AX + 18.f * u, TY, BP.Title.Left(34), 12.f, BP.Accent);
				TY += 22.f * u;
				for (const FString& Ln : BLines) { Txt(L + 6, AX + 18.f * u, TY, Ln, 11.5f, FLinearColor::White); TY += 17.f * u; }
				if (BP.Frac >= 0.f)
				{
					const float BX0 = AX + 18.f * u, BW0 = W - 36.f * u, BH0 = 10.f * u, BY0 = TY + 3.f * u;
					if (BP.RailEarly >= 0.f)
					{
						const float We = BW0 * BP.RailEarly, Wc = BW0 * FMath::Max(0.f, BP.RailCenter);
						ZoogiUIStyle::SBox(Out, L + 6, Geo, FVector2D(BX0, BY0), FVector2D(We, BH0), FLinearColor(1.f, 0.85f, 0.2f), FLinearColor::Transparent, 2.f, 0.f);
						ZoogiUIStyle::SBox(Out, L + 6, Geo, FVector2D(BX0 + We, BY0), FVector2D(Wc, BH0), FLinearColor(0.3f, 0.95f, 0.45f), FLinearColor::Transparent, 2.f, 0.f);
						ZoogiUIStyle::SBox(Out, L + 6, Geo, FVector2D(BX0 + We + Wc, BY0), FVector2D(FMath::Max(0.f, BW0 - We - Wc), BH0), FLinearColor(1.f, 0.42f, 0.22f), FLinearColor::Transparent, 2.f, 0.f);
						ZoogiUIStyle::SBox(Out, L + 7, Geo, FVector2D(BX0 + BW0 * BP.Frac - 3.f * u, BY0 - 4.f * u), FVector2D(6.f * u, BH0 + 8.f * u), FLinearColor::White, Outline, 2.f * u, 1.5f * u);
					}
					else ZoogiUIStyle::SBar(Out, L + 6, Geo, FVector2D(BX0, BY0), FVector2D(BW0, BH0), BP.Frac, FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(1.f, 0.45f, 0.1f), nullptr, 5);
				}
			}
			Y += ActionH;
		}
		Y += CardH + 8.f * u;
	}
	// Event feed (moved out of the arena corner): left column, under the cards, newest first, fades like the old feed
	if (Side == 0)
	{
		const float FeedBot = S.Y - ComboH - 8.f * u;
		float FY = Y + 4.f * u;
		if (FeedBot - FY > 44.f * u)
		{
			bool bTitle = false;
			for (const FZoogiFeedItem& F : G->Feed)
			{
				const float A = FMath::Clamp(1.f - (F.Age - 6.f) / 1.5f, 0.f, 1.f) * FMath::Clamp(F.Age * 6.f, 0.f, 1.f);
				if (A <= 0.f) continue;
				if (!bTitle) { Txt(L + 6, 20.f * u, FY, TEXT("EVENTS"), 11.f, MutedText); FY += 20.f * u; bTitle = true; }
				const TArray<FString> FL = Wrap(F.Text, 11.f, S.X - 52.f * u, 2);
				const float FH = 10.f * u + FL.Num() * 16.f * u;
				if (FY + FH > FeedBot) break;
				const float Sl3 = (1.f - FMath::Clamp(F.Age * 5.f, 0.f, 1.f)) * -24.f * u;
				ZoogiUIStyle::SBox(Out, L + 2, Geo, FVector2D(10.f * u + Sl3, FY), FVector2D(S.X - 20.f * u, FH), FLinearColor(0.03f, 0.10f, 0.36f, 0.9f * A), FLinearColor(Outline.R, Outline.G, Outline.B, A), 8.f * u, 2.f * u);
				ZoogiUIStyle::SBox(Out, L + 3, Geo, FVector2D(13.f * u + Sl3, FY + 4.f * u), FVector2D(5.f * u, FH - 8.f * u), FLinearColor(F.Color.R, F.Color.G, F.Color.B, A), FLinearColor::Transparent, 2.f * u, 0.f);
				float LY = FY + 5.f * u;
				for (const FString& Ln : FL) { Txt(L + 6, 24.f * u + Sl3, LY, Ln, 11.f, FLinearColor(F.Color.R * 0.4f + 0.6f, F.Color.G * 0.4f + 0.6f, F.Color.B * 0.4f + 0.6f, A)); LY += 16.f * u; }
				FY += FH + 5.f * u;
			}
		}
	}
	if (G->bComboMode)
	{
		const float BY = S.Y - ComboH;
		const float E = G->ComboEnergy[Side];
		const bool bReady = E >= ZoogiComboTune::FollowUpCost;
		Txt(L + 6, 20.f * u, BY, FString::Printf(TEXT("COMBO %.0f / %.0f%s"), E, ZoogiComboTune::GaugeMax, bReady ? TEXT("   FOLLOW-UP READY") : TEXT("")), 12.f, bReady ? Gold : FLinearColor(0.8f, 0.9f, 1.f));
		ZoogiUIStyle::SBar(Out, L + 7, Geo, FVector2D(20.f * u, BY + 24.f * u), FVector2D(S.X - 64.f * u, 16.f * u), E / FMath::Max(1.f, ZoogiComboTune::GaugeMax), SC, FLinearColor(SC.R * 0.5f, SC.G * 0.5f, SC.B, 1.f), &ComboJuice, 10);
	}
	return L + 10;
}
