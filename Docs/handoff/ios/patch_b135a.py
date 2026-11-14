# b135a (Evan 07:40: one AWAKEN meter, SHOWTIME at aim, no AWAKENED buff): the SUPER/ULTIMATE meter becomes the AWAKEN meter.
#  - fills from play as before (timing taps, blocks, scoring, KOs, getting knocked off) + every card cast
#  - full -> SHOWTIME lights in the command menu (anime cutscene + Awaken power) - CardsAwakenReady / CardsUseAwaken use the meter
#  - no mid-roll ultimate (human or CPU), no AWAKENED buff; crystals stay as REDIRECT fuel only
#  -ZoogiOldMeters = the b134 split (SUPER meter -> ultimate mid-roll, crystals -> SHOWTIME)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
H = 'Public/ZoogiActionFun.h'; SF = 'Private/ZoogiSuperFun.cpp'; AF = 'Private/ZoogiActionFun.cpp'; CG = 'Private/ZoogiCardGlue.cpp'
ed1(H, '''class ZOOGIROLL_API UZoogiFunSys : public UWorldSubsystem''', '''/** b135 (Evan): the SUPER meter is the AWAKEN meter (full -> SHOWTIME at aim). False with -ZoogiOldMeters. */
ZOOGIROLL_API bool ZoogiAwakenMeter();

class ZOOGIROLL_API UZoogiFunSys : public UWorldSubsystem''')
ed1(H, '''	bool UltimateReady(const AZoogiGameMode* G, int32 Player) const;
	float Meter(int32 Player) const;''', '''	bool UltimateReady(const AZoogiGameMode* G, int32 Player) const;
	float Meter(int32 Player) const;
	/** b135: AWAKEN meter - full, spend (SHOWTIME), add (card casts) */
	bool AwakenFull(int32 Player) const;
	void SpendAwaken(int32 Player);
	void AddAwakenMeter(AZoogiGameMode* G, int32 Player, float Amount, const TCHAR* Why) { AddMeter(G, Player, Amount, Why); }''')
ed1(SF, '''bool UZoogiFunSys::UltimateReady(const AZoogiGameMode* G, int32 Player) const
{''', '''bool ZoogiAwakenMeter()
{
	static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeters"));
	return !bOld;
}

bool UZoogiFunSys::AwakenFull(int32 Player) const
{
	return Players.IsValidIndex(Player) && Players[Player].Meter >= ZoogiSuperTune::MeterFull;
}

void UZoogiFunSys::SpendAwaken(int32 Player)
{
	if (!Players.IsValidIndex(Player)) return;
	Players[Player].Meter = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("FUN AWAKEN meter P%d spent (SHOWTIME)"), Player + 1);
}

bool UZoogiFunSys::UltimateReady(const AZoogiGameMode* G, int32 Player) const
{
	if (ZoogiAwakenMeter()) return false; // b135: a full meter means SHOWTIME at aim, no mid-roll ultimate''')
ed1(SF, '''		if (!B->bCPU) Callout(AwR ? TEXT("AWAKEN READY!") : TEXT("ULTIMATE READY!"), FString::Printf(TEXT("%s  -  TAP SUPERPOWER WHILE ROLLING"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);''',
'''		if (ZoogiAwakenMeter())
		{	// b135: the AWAKEN meter
			if (!B->bCPU) Callout(TEXT("AWAKEN READY!"), FString::Printf(TEXT("%s  -  TAP SHOWTIME BEFORE YOUR SHOT"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);
			else Callout(FString::Printf(TEXT("%s'S AWAKEN IS READY"), *B->Def.Name.ToUpper()), Nm, UltColor(Sig), 1.3f, 0.7f, Player, LineReady);
		}
		else if (!B->bCPU) Callout(AwR ? TEXT("AWAKEN READY!") : TEXT("ULTIMATE READY!"), FString::Printf(TEXT("%s  -  TAP SUPERPOWER WHILE ROLLING"), *Nm), UltColor(Sig), 1.8f, 0.9f, Player, LineReady);''')
ed1(SF, '''	if (B && B->bCPU && !bUltThisShot && Players.IsValidIndex(B->PlayerIndex)''', '''	if (!ZoogiAwakenMeter() && B && B->bCPU && !bUltThisShot && Players.IsValidIndex(B->PlayerIndex)''')
# no AWAKENED buff
ed1(AF, '''	if (Players.IsValidIndex(Player) && Players[Player].bFire && Players[Player].Meter >= 100.f) SetAwakened(G, Player, true);''',
        '''	if (!ZoogiAwakenMeter() && Players.IsValidIndex(Player) && Players[Player].bFire && Players[Player].Meter >= 100.f) SetAwakened(G, Player, true); // b135: no AWAKENED buff''')
# meter label + ready banner
ed1(AF, '''		FunText(C, M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER"), X + 6.f * U''', '''		FunText(C, ZoogiAwakenMeter() ? (M >= 1.f ? TEXT("AWAKEN!") : TEXT("AWAKEN")) : (M >= 1.f ? TEXT("ULTIMATE!") : TEXT("SUPER")), X + 6.f * U''')
ed1(AF, '''		if (M >= 1.f && !bUltThisShot && G->ActivePlayer == i && (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling) && !Clash.bOn)
		{
			const FString Rd = FString::Printf(TEXT("%s READY  -  TAP SUPERPOWER WHILE ROLLING"), UltName(Sig));''', '''		if (M >= 1.f && !bUltThisShot && G->ActivePlayer == i && (ZoogiAwakenMeter() ? G->Phase == EZoogiPhase::Aiming : (G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling)) && !Clash.bOn)
		{
			const FString Rd = ZoogiAwakenMeter() ? FString(TEXT("AWAKEN READY  -  TAP SHOWTIME")) : FString::Printf(TEXT("%s READY  -  TAP SUPERPOWER WHILE ROLLING"), UltName(Sig));''')
# SHOWTIME / Awaken readiness + spend from the meter; card casts feed it
ed1(CG, '#include "ZoogiSkills.h"\n', '#include "ZoogiSkills.h"\n#include "ZoogiActionFun.h"\n')
ed1(CG, '''	if (!AwakenChargeOn() || Player < 0 || Player > 3 || Pips <= 0) return;''', '''	if (!AwakenChargeOn() || Player < 0 || Player > 3 || Pips <= 0) return;
	if (ZoogiAwakenMeter()) if (UZoogiFunSys* Fm = UZoogiFunSys::Get(this)) Fm->AddAwakenMeter(this, Player, 16.f, Why); // b135: each card cast feeds the AWAKEN meter (+8 after the halving)''')
ed1(CG, '''		if (Balls.IsValidIndex(Player) && Balls[Player])
			AddPopup(TEXT("AWAKEN READY"), Balls[Player]->BallLocation() + FVector(0, 0, 210.f), FLinearColor(1.f, 0.82f, 0.35f));''',
'''		if (Balls.IsValidIndex(Player) && Balls[Player] && !ZoogiAwakenMeter()) // b135: crystals are REDIRECT fuel now
			AddPopup(TEXT("AWAKEN READY"), Balls[Player]->BallLocation() + FVector(0, 0, 210.f), FLinearColor(1.f, 0.82f, 0.35f));''')
ed1(CG, '''	return AwakenChargeOn() && Player >= 0 && Player < 4 && AwakenCharge[Player] >= AwakenChargeMax;''', '''	if (Player < 0 || Player >= 4) return false;
	if (ZoogiAwakenMeter()) if (const UZoogiFunSys* Fm = UZoogiFunSys::Get(this)) if (Fm->IsActive(this)) return Fm->AwakenFull(Player); // b135: the AWAKEN meter
	return AwakenChargeOn() && AwakenCharge[Player] >= AwakenChargeMax;''')
ed1(CG, '''	AwakenCharge[B->PlayerIndex] = 0;
	bStandbyAwaken = true;''', '''	{	// b135: SHOWTIME spends the AWAKEN meter (crystals stay for REDIRECT)
		UZoogiFunSys* Fm = ZoogiAwakenMeter() ? UZoogiFunSys::Get(this) : nullptr;
		if (Fm && Fm->IsActive(this)) Fm->SpendAwaken(B->PlayerIndex); else AwakenCharge[B->PlayerIndex] = 0;
	}
	bStandbyAwaken = true;''')
print('b135a ok')
# fix (UHT): the ZoogiAwakenMeter() declaration sits above UCLASS(), not between it and the class
