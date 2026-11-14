# b141a (Evan 11:05 "Unlock the increase cap on the crystals energy buff gives"): Energy Buff always adds +1 crystal - past the
# 4-crystal cap and past the one-crystal-per-turn limit. Nothing else pulls a stacked count back down to 4 (turn-start +1 and
# card pips never lower it). HUD: crystals over 4 show as "+N" next to the 4 pips. -ZoogiCrystalCap = the old cap.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiSkills.cpp', '''			G->CardsAddAwakenCharge(Caster->PlayerIndex, 1, TEXT("energy buff"));''', '''			const int32 CrPi = FMath::Clamp(Caster->PlayerIndex, 0, 3), CrWas = G->AwakenCharge[CrPi];
			G->CardsAddAwakenCharge(Caster->PlayerIndex, 1, TEXT("energy buff"));
			// b141 (Evan: "Unlock the increase cap on the crystals energy buff gives"): always +1, past the cap / per-turn limit
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiCrystalCap")) && G->AwakenChargeOn() && G->AwakenCharge[CrPi] <= CrWas)
			{
				G->AwakenCharge[CrPi] = CrWas + 1;
				UE_LOG(LogZoogi, Log, TEXT("AWAKEN charge P%d %d -> %d (energy buff, no cap)"), CrPi + 1, CrWas, G->AwakenCharge[CrPi]);
			}''')
ed1('Private/ZoogiCardGlue.cpp', '''	AwakenCharge[Player] = FMath::Min(AwakenChargeMax, Was + Pips);''', '''	AwakenCharge[Player] = FMath::Max(Was, FMath::Min(AwakenChargeMax, Was + Pips)); // b141: never pulls an Energy Buff stack back to the cap''')
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '''					G->AwakenCharge[A->PlayerIndex] = FMath::Min(AZoogiGameMode::AwakenChargeMax, G->AwakenCharge[A->PlayerIndex] + 1);
					if (TestOn() && !A->bCPU) G->AwakenCharge[A->PlayerIndex] = AZoogiGameMode::AwakenChargeMax;''', '''					G->AwakenCharge[A->PlayerIndex] = FMath::Max(G->AwakenCharge[A->PlayerIndex], FMath::Min(AZoogiGameMode::AwakenChargeMax, G->AwakenCharge[A->PlayerIndex] + 1)); // b141: keeps an Energy Buff stack
					if (TestOn() && !A->bCPU) G->AwakenCharge[A->PlayerIndex] = FMath::Max(G->AwakenCharge[A->PlayerIndex], AZoogiGameMode::AwakenChargeMax);''')
H = 'Public/ZoogiArenaHUDWidget.h'
ed1(H, '''TWeakObjectPtr<UTextBlock> SP; float BarW = 0.f;''', '''TWeakObjectPtr<UTextBlock> SP; TWeakObjectPtr<UTextBlock> PipMore; float BarW = 0.f;''')
W = 'Private/ZoogiArenaHUDWidget.cpp'
ed1(W, '''	for (int32 k = 0; k < N; ++k)
		U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 90.f, PS, PS));''', '''	for (int32 k = 0; k < N; ++k)
		U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 90.f, PS, PS));
	U.PipMore = Text(TEXT(""), PX + PW + 4.f, RY + RS + 88.f, 44.f, 26.f, 15.f, ZCHex(TEXT("FFD35C")), 2.f, 0); // b141: crystals over 4''')
ed1(W, '''	for (int32 k = 0; k < NPip; ++k)
				U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("555B6E")), 1.5f, PS * 0.5f), PX + k * (PS + PGap), RY + RS + 66.f, PS, PS));''', '''	for (int32 k = 0; k < NPip; ++k)
				U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("555B6E")), 1.5f, PS * 0.5f), PX + k * (PS + PGap), RY + RS + 66.f, PS, PS));
			U.PipMore = Text(TEXT(""), PX + PW + 3.f, RY + RS + 64.f, 40.f, 20.f, 12.f, ZCHex(TEXT("FFD35C")), 1.5f, 0); // b141: crystals over 4''')
ed1(W, '''		for (int32 k = 0; k < U.Pips.Num(); ++k)
			SetFill(U.Pips[k], k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : AHArcane) : ZCHex(TEXT("050607")));''', '''		for (int32 k = 0; k < U.Pips.Num(); ++k)
			SetFill(U.Pips[k], k < Charge ? (Charge >= AZoogiGameMode::AwakenChargeMax ? ZCHex(TEXT("FFD35C")) : AHArcane) : ZCHex(TEXT("050607")));
		if (UTextBlock* Pm = U.PipMore.Get())
		{	// b141: Energy Buff stacks crystals past the 4 pips
			const FString Ms = Charge > U.Pips.Num() && U.Pips.Num() > 0 ? FString::Printf(TEXT("+%d"), Charge - U.Pips.Num()) : FString();
			if (!Pm->GetText().ToString().Equals(Ms)) Pm->SetText(FText::FromString(Ms));
		}''')
F = 'Private/ZoogiFront.cpp'
ed1(F, '''			{ TEXT("B140"), ''', '''			{ TEXT("B141"), TEXT("ENERGY BUFF: NO CRYSTAL CAP"), TEXT("Energy Buff always adds a crystal now - it can stack past 4 and works even if you already got a crystal this turn. Extra crystals show as +N next to the four circles.") },
			{ TEXT("B140"), ''')
print('b141a ok')
