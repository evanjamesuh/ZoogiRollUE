# b70 part 2 (Claude): stance only at turn start; the chosen skill stays visible while aiming
import os
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def ed(rel, pairs, inc=None, after=None):
    p = os.path.join(ROOT, rel)
    raw = open(p, 'rb').read(); bom = raw[:3] == b'\xef\xbb\xbf'; s = raw.decode('utf-8-sig'); nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in pairs:
        a = a.replace('\n', nl); b = b.replace('\n', nl)
        assert s.count(a) == 1, (rel, a[:80], s.count(a)); s = s.replace(a, b)
    if inc and inc not in s:
        i = s.index(after); j = s.index(nl, i) + len(nl); s = s[:j] + inc + nl + s[j:]
    open(p, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline='').write(s)

ed('Private/ZoogiStandForms.cpp', [
 # match start: only the turn player stands
 ('			for (AZoogiBall* B : G->Balls)\n			{\n				if (!B || B->bOut) continue;\n				const FVector L = B->BallLocation();',
  '			for (AZoogiBall* B : G->Balls)\n			{\n				if (!B || B->bOut) continue;\n				if (B != G->GetActiveBall() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandAll"))) continue; // b70 (Evan): "show play stance only at turn start" - the turn player only\n				const FVector L = B->BallLocation();'),
 # dismiss: anyone standing who is not the turn player (quietly)
 ('			const bool bKnocked = B->GetBallVelocity().Size2D() > 140.f || B->bOut;\n			if (bActed || bKnocked)',
  '			const bool bKnocked = B->GetBallVelocity().Size2D() > 140.f || B->bOut || (B != G->GetActiveBall() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiStandAll"))) || (UZoogiFunSys::Get(G) && UZoogiFunSys::Get(G)->IsReplaying()); // b70: not its turn = back to the ball; replays show balls\n			if (bActed || bKnocked)'),
 # re-stand: only the turn player, at its turn start
 ('			if (Rt < 0.6f) continue;\n',
  '			if (Rt < 0.6f) continue;\n			if (B != G->GetActiveBall() || G->Phase != EZoogiPhase::Aiming) continue; // b70: the stance is the turn start only\n'),
])
ed('Private/ZoogiActionFun.cpp', [
 ('	ZoogiSkills::Draw(G, C, PC); // b63: skill target pick (arc range ring, circle / line / targets, FIRE / CANCEL)\n',
  '''	ZoogiSkills::Draw(G, C, PC); // b63: skill target pick (arc range ring, circle / line / targets, FIRE / CANCEL)
	// b70 (Evan: "skill stays visible during aiming"): the skills played this turn, on a plate while the human aims
	if (G->Phase == EZoogiPhase::Aiming && !ZoogiSkills::Picking() && !ZoogiP5Cmd::Showing() && G->CardsOn())
		if (const AZoogiBall* Hb = G->GetActiveBall())
			if (!Hb->bCPU)
				if (const UZoogiCardSubsystem* CSs = UZoogiCardSubsystem::Get(G))
					if (const FZoogiCardPlayerState* PSs = CSs->Rules().Player(Hb->PlayerIndex))
						if (PSs->Committed.Num() > 0 && PSs->Turn == G->TurnCounter)
						{
							TArray<FString> Nm;
							for (const FZoogiCardCommitted& Cm : PSs->Committed) { const FZoogiCardDef* D = CSs->Def(Cm.Card.CardId); Nm.Add(D ? D->Name.ToUpper() : Cm.Card.CardId.ToString().ToUpper()); }
							const FString Line = FString::Join(Nm, TEXT("  +  "));
							const bool bPortS = Ht > Wd;
							const float Px = (bPortS ? 30.f : 32.f) * U;
							const FVector2D Ms = ZoogiFunCanvasMeasure(Line, Px, 0);
							const float BoxW = FMath::Min(Ms.X + 200.f * U, Wd * 0.94f), BoxH = Px * 1.9f;
							const FVector2D Ctr(bPortS ? Wd * 0.5f : 40.f * U + BoxW * 0.5f, Ht * (bPortS ? 0.74f : 0.76f));
							ZoogiFunCanvasQuad(C, Ctr + FVector2D(8.f, 8.f) * U, BoxW, BoxH, -3.f, 12.f * U, FLinearColor(0.85f, 0.05f, 0.12f, 0.95f));
							ZoogiFunCanvasQuad(C, Ctr, BoxW, BoxH, -3.f, 12.f * U, FLinearColor(0.02f, 0.02f, 0.04f, 0.92f));
							ZoogiFunCanvasText(C, TEXT("SKILL"), Ctr.X - BoxW * 0.5f + 70.f * U, Ctr.Y, Px * 0.8f, FLinearColor(1.f, 0.82f, 0.25f), 1, 0);
							ZoogiFunCanvasText(C, Line, Ctr.X + 50.f * U, Ctr.Y, Px, FLinearColor::White, 1, 0);
						}
'''),
], '#include "ZoogiCardSubsystem.h"\n#include "ZoogiCards.h"\n#include "ZoogiP5Command.h"\nclass UCanvas;\nvoid ZoogiFunCanvasText(UCanvas* C, const FString& S, float X, float Y, float Px, const FLinearColor& Col, int32 Align, int32 Which);\nFVector2D ZoogiFunCanvasMeasure(const FString& S, float Px, int32 Which);\nvoid ZoogiFunCanvasQuad(UCanvas* C, const FVector2D& Ctr, float Len, float Hgt, float AngDeg, float Skew, const FLinearColor& Col);', '#include "ZoogiBanter.h"')
print('patched part 2')
