# b136g (Evan 09:02: "stop AI from using skills all at once and inappropriately - wood shield can not be used in a combo"):
#  - a CPU plays ONE skill a turn (its best one, value 30+), never a pile of them (-ZoogiCPUMultiSkill = up to 3 again)
#  - Wood Shield (it ends the turn) only when the CPU is really exposed near the edge with a rival close, and never with another skill
#  - IMPULSE (meteor_rain) values what is around the CPU (it blasts from where it is) and fires mid-roll when things are near
#  - COMBO: Wood Shield can't be ticked (players) - "WOOD SHIELD CAN'T BE IN A COMBO"
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
CS = 'Private/ZoogiCardSubsystem.cpp'
ed1(CS, '''			if (X.V < 22.f || X.C > E || Took.Contains(X.Id) || Smart.Num() >= 3) continue;''',
'''			static const bool bMulti = FParse::Param(FCommandLine::Get(), TEXT("ZoogiCPUMultiSkill"));
			if (X.V < (bMulti ? 22.f : 30.f) || X.C > E || Took.Contains(X.Id) || Smart.Num() >= (bMulti ? 3 : 1)) continue; // b136 (Evan): one skill a turn
			if (!bMulti && X.Id == FName(TEXT("wood_shield")) && Smart.Num() > 0) continue; // the shield ends the turn: never with another skill''')
CP = 'Private/ZoogiCPUSmart.cpp'
ed1(CP, '''		else if (K == TEXT("meteor_rain")) V = Ag * (S.BestBunch >= 2 ? 30.f + 16.f * FMath::Min(4, S.BestBunch) : (S.RivalsIn ? 28.f : 8.f));''',
'''		else if (K == TEXT("meteor_rain")) V = Ag * (S.Near >= 2 ? 30.f + 14.f * FMath::Min(4, S.Near) : 8.f); // b136: IMPULSE blasts from the CPU itself''')
ed1(CP, '''		else if (K == TEXT("wood_shield")) V = Df * (12.f + 50.f * Exposed + 20.f * RivalClose);''',
'''		else if (K == TEXT("wood_shield")) V = (Exposed > 0.6f && RivalClose > 0.3f) ? Df * (25.f + 50.f * Exposed + 20.f * RivalClose) : 0.f; // b136 (Evan): only when it really needs it (the shield ends the turn)''')
ed1(CP, '''		if (K == TEXT("meteor_rain") || K == TEXT("fire_meteors")) return S.BestBunch >= 2 || S.RivalsIn > 0;''',
'''		if (K == TEXT("meteor_rain")) return S.Near >= 2 || G->PhaseTime > 2.5f; // b136: IMPULSE goes off when things are close
		if (K == TEXT("fire_meteors")) return S.BestBunch >= 2 || S.RivalsIn > 0;''')
P = 'Private/ZoogiP5Command.cpp'
ed1(P, '''			if (GS.ComboSel.Contains(R)) { GS.ComboSel.Remove(R); ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.5f, 1.2f); return; }''',
'''			if (GS.ComboSel.Contains(R)) { GS.ComboSel.Remove(R); ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.5f, 1.2f); return; }
			{	// b136 (Evan): Wood Shield can not be used in a combo
				const UZoogiCardSubsystem* CSw = UZoogiCardSubsystem::Get(G);
				const FZoogiCardPlayerState* PSw = (CSw && G->CardsOn()) ? CSw->Rules().Player(W.Skills[R].Slot >= 0 && G->GetActiveBall() ? G->GetActiveBall()->PlayerIndex : 0) : nullptr;
				if (PSw && PSw->Hand.IsValidIndex(W.Skills[R].Slot) && PSw->Hand[W.Skills[R].Slot].CardId == FName(TEXT("wood_shield")))
				{
					ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f);
					if (UZoogiFunSys* Fw = UZoogiFunSys::Get(G)) Fw->PublicCallout(TEXT("NOT IN A COMBO"), TEXT("WOOD SHIELD ENDS YOUR TURN - CAST IT ON ITS OWN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.2f, 0.8f);
					return;
				}
			}''')
print('b136g ok')
