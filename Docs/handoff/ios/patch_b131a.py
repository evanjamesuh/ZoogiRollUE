# b131a (Evan 03:32, screenshots: camera flew up to the sky with the arena small in a corner; overlapping text and banners)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
PC = 'Private/ZoogiPlayerController.cpp'
# 1. the smart roll camera framed anything moving fast - an orb / Zoogi flung high into the air or far off the map dragged the framing
#    centre into the sky. Now only bodies over the court count, and the framing stays on the court.
ed1(PC, '''			TArray<TPair<float, AActor*>> Movers;
			for (AZoogiBall* B : G->Balls) if (B && B != Shooter && !Gone(B)) { const float S = B->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, B }); }
			for (AZoogiOrb* O : G->Orbs) if (O && !Gone(O)) { const float S = O->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, O }); }''',
'''			// b131 (Evan: the camera flew up into the sky, the arena tiny in a corner): only bodies over the court (not flung high / far
			// away) are framed, and the framing centre stays on the court. -ZoogiSmartCamAll = the old framing.
			static const bool bFrameAll = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSmartCamAll"));
			const FVector ArC = G->Arena ? G->Arena->GetActorLocation() : FVector::ZeroVector;
			const float ArR = G->Arena ? G->Arena->GetKnockoffRadius() : 2000.f;
			auto OnCourt = [&](const AActor* A) { if (bFrameAll) return true; const FVector P = ActorCenter(A); return FVector::Dist2D(P, ArC) < ArR * 1.1f && P.Z < CourtZ + 450.f; };
			TArray<TPair<float, AActor*>> Movers;
			for (AZoogiBall* B : G->Balls) if (B && B != Shooter && !Gone(B) && OnCourt(B)) { const float S = B->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, B }); }
			for (AZoogiOrb* O : G->Orbs) if (O && !Gone(O) && OnCourt(O)) { const float S = O->GetVelocity().Size2D(); if (S > 220.f) Movers.Add({ S, O }); }''')
ed1(PC, '''			const FVector C = Wt > 0.f ? Sum / Wt : FVector(0.f, 0.f, CourtZ);
			float Rad = 300.f;
			for (const FVector& Pp : Pts) Rad = FMath::Max(Rad, FVector::Dist2D(Pp, C) + 220.f);''', '''			FVector C = Wt > 0.f ? Sum / Wt : FVector(0.f, 0.f, CourtZ);
			if (!bFrameAll)
			{	// b131: on the court, at court height (a shooter flying off the map no longer drags the camera up and away)
				FVector2D C2(C.X - ArC.X, C.Y - ArC.Y);
				if (C2.Size() > ArR) C2 = C2.GetSafeNormal() * ArR;
				C = FVector(ArC.X + C2.X, ArC.Y + C2.Y, FMath::Clamp((float)C.Z, CourtZ - 100.f, CourtZ + 300.f));
			}
			float Rad = 300.f;
			for (const FVector& Pp : Pts) Rad = FMath::Max(Rad, FVector::Dist2D(Pp, C) + 220.f);
			if (!bFrameAll) Rad = FMath::Min(Rad, ArR * 1.2f);''')
# 2. the "SKILL ..." plate joins the note stack (it sat on top of LOW GRAVITY / FINAL ROUNDS)
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''		if (!EventName.IsEmpty() && bPlay) Notes.Add({ EventName, FLinearColor(1.f, 0.55f, 0.88f) });''', '''		if (!EventName.IsEmpty() && bPlay) Notes.Add({ EventName, FLinearColor(1.f, 0.55f, 0.88f) });
		// b131 (Evan: "fix overlapping text and banners"): the skills played this turn are a note in this stack (was a plate on top of it)
		if (G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
			if (const AZoogiBall* Hb = G->GetActiveBall())
				if (!Hb->bCPU)
					if (const UZoogiCardSubsystem* CSs = UZoogiCardSubsystem::Get(G))
						if (const FZoogiCardPlayerState* PSs = CSs->Rules().Player(Hb->PlayerIndex))
							if (PSs->Committed.Num() > 0 && PSs->Turn == G->TurnCounter)
							{
								TArray<FString> Nm;
								for (const FZoogiCardCommitted& Cm : PSs->Committed) { const FZoogiCardDef* D = CSs->Def(Cm.Card.CardId); Nm.Add(D ? D->Name.ToUpper() : Cm.Card.CardId.ToString().ToUpper()); }
								Notes.Insert({ FString(TEXT("SKILL  ")) + FString::Join(Nm, TEXT("  +  ")), FLinearColor(0.95f, 0.1f, 0.2f) }, 0);
							}''')
ed1(AF, '''	if (G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
		if (const AZoogiBall* Hb = G->GetActiveBall())
			if (!Hb->bCPU)''', '''	static const bool bOldSkillPlate = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldSkillPlate")); // b131: the note stack shows it now
	if (bOldSkillPlate && G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
		if (const AZoogiBall* Hb = G->GetActiveBall())
			if (!Hb->bCPU)''')
# 3. SUPER / ULTIMATE! sits inside its meter (it covered the card's SP / HP line)
ed1(AF, '''		FunRect(C, X - 2.f * U, Y - 2.f * U, Bw + 4.f * U, Bh + 4.f * U, FLinearColor(0.f, 0.f, 0.05f, 0.65f));
		FunRect(C, X, Y, Bw * FMath::Clamp(M, 0.f, 1.f), Bh, Mc);
		FunText(C, M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"), X, Y + Bh + 2.f * U, 16.f * U, Mc, false);''',
'''		const float BhL = Bh + 6.f * U; // b131: a little taller, the label inside it
		FunRect(C, X - 2.f * U, Y - 2.f * U, Bw + 4.f * U, BhL + 4.f * U, FLinearColor(0.f, 0.f, 0.05f, 0.8f));
		FunRect(C, X, Y, Bw * FMath::Clamp(M, 0.f, 1.f), BhL, Mc);
		FunText(C, M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"), X + 6.f * U, Y + 1.f * U, 13.f * U, M >= 1.f ? FLinearColor(0.05f, 0.03f, 0.1f) : FLinearColor::White, false);''')
# 4. corner banner sub-line: never wider than its plate, never off the left edge
ed1(AF, '''		FVector2D SS = P5Measure(Sub, FunFontP(1, Sp));
		if (SS.X > Len - 40.f * U) { Sp *= (Len - 40.f * U) / FMath::Max(1.f, SS.X); SS = P5Measure(Sub, FunFontP(1, Sp)); }
		const float SL = SS.X + 44.f * U, SH = Sp * 1.6f;
		const FVector2D SC = Ctr + Dn * (Hgt * 0.5f + SH * 0.5f + 6.f * U) + Ax * (Len * 0.5f - SL * 0.5f - 40.f * U);''',
'''		FVector2D SS = P5Measure(Sub, FunFontP(1, Sp)) * 1.15f; // b131: the measure runs ~10% short of the drawn text
		if (SS.X > Len - 40.f * U) { Sp *= (Len - 40.f * U) / FMath::Max(1.f, SS.X); SS = P5Measure(Sub, FunFontP(1, Sp)) * 1.15f; }
		const float SL = SS.X + 44.f * U, SH = Sp * 1.6f;
		FVector2D SC = Ctr + Dn * (Hgt * 0.5f + SH * 0.5f + 6.f * U) + Ax * (Len * 0.5f - SL * 0.5f - 40.f * U);
		SC.X = FMath::Max(SC.X, SL * 0.5f + 12.f * U); // b131: never cut off on the left''')
# 5. REDIRECT combo counter below the turn / round header (it covered ROUND n / 4)
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '''			ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Ht * 0.16f), 300.f * U * Pop, 70.f * U * Pop, -6.f, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
			ZoogiFunCanvasText(C, FString::Printf(TEXT("COMBO  x%d"), ComboN), Wd * 0.5f, Ht * 0.16f, 48.f * U * Pop,''',
'''			const float Yc = Ht * (Ht > Wd ? 0.2f : 0.22f); // b131: below the turn / round header (was 0.16 - on top of ROUND n / N)
			ZoogiFunCanvasQuad(C, FVector2D(Wd * 0.5f, Yc), 300.f * U * Pop, 70.f * U * Pop, -6.f, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.85f));
			ZoogiFunCanvasText(C, FString::Printf(TEXT("COMBO  x%d"), ComboN), Wd * 0.5f, Yc, 48.f * U * Pop,''')
# 6. skill pick screen: the hint plate fits its text, "IS TARGETING..." goes under the hint (it sat on the turn header)
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		const FVector2D HS = ZoogiFunCanvasMeasure(Hint, HPx, 1);
		ZoogiFunCanvasQuad(C, TC + FVector2D(0.f, TPx * 1.25f), HS.X + 40.f * U, HPx * 1.6f, -4.f, 8.f * U, Wh);''',
'''		const FVector2D HS = ZoogiFunCanvasMeasure(Hint, HPx, 1) * 1.15f; // b131: the plate covers the whole hint
		ZoogiFunCanvasQuad(C, TC + FVector2D(0.f, TPx * 1.25f), HS.X + 60.f * U, HPx * 1.6f, -4.f, 8.f * U, Wh);''')
ed1(S, '''TC.X, TC.Y - TPx * 1.2f, 28.f * U, ZoogiUtil::PlayerColor(Cb->PlayerIndex), 1, 0);''', '''TC.X, TC.Y + TPx * 1.25f + HPx * 1.6f, 28.f * U, ZoogiUtil::PlayerColor(Cb->PlayerIndex), 1, 0); // b131: under the hint (was over the turn header)''')
# 7. the shot grade / combo callouts (GOOD! ...) wait while a target pick is open
H = 'Private/ZoogiHUD.cpp'
b = open(R + H, 'rb').read()
if b'#include "ZoogiSkills.h"' not in b: ed1(H, '#include "ZoogiHUD.h"\n', '#include "ZoogiHUD.h"\n#include "ZoogiSkills.h"\n')
b = open(R + H, 'rb').read()
if b'#include "ZoogiRedirect.h"' not in b: ed1(H, '#include "ZoogiHUD.h"\n', '#include "ZoogiHUD.h"\n#include "ZoogiRedirect.h"\n')
ed1(H, '''		for (const ZoogiScoringUI::FCallout& Call : ZoogiScoringUI::Callouts())''', '''		if (!ZoogiSkills::Picking() && !ZoogiRedirect::Picking()) for (const ZoogiScoringUI::FCallout& Call : ZoogiScoringUI::Callouts()) // b131: not over a target pick''')
print('b131a ok')
