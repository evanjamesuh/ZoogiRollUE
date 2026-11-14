# b151b: wire RAID mode (menu card, hooks, results, boss attack every round + rage)
import os, re
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
def addinc(p, after, inc):
    p = R + p; b = open(p, 'rb').read(); a = after.encode()
    assert b.count(a) >= 1, (p, after)
    i = b.index(a); j = b.index(b'\n', i)
    b = b[:j+1] + inc.encode() + (b'\r\n' if b[j-1:j] == b'\r' else b'\n') + b[j+1:]
    open(p, 'wb').write(b)
ed1('Public/ZoogiGameMode.h', '''	void PublicPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }''', '''	void PublicPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }
	void PublicFinishGame() { FinishGame(); } // b151: RAID ends the match on a win / out of lives''')
GM = 'Private/ZoogiGameMode.cpp'
ed1(GM, '''	BannerTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Game over: %s"), *Banner);''', '''	ZoogiRaid::OnFinished(this); // b151: RAID CLEARED / FAILED banner
	BannerTime = 0.f;
	UE_LOG(LogZoogi, Log, TEXT("Game over: %s"), *Banner);''')
ed1(GM, '''	ZoogiRoyaleMix::OnMenu(this); // b149''', '''	ZoogiRoyaleMix::OnMenu(this); // b149
	ZoogiRaid::OnMenu(this); // b151''')
ed1(GM, '''	for (AZoogiOrb* O : Orbs) if (O && !O->bOut) Cands.Add({ FVector2D(O->OrbLocation()), 50.f, ZoogiConst::OrbMassRatio, ZoogiConst::OrbRadius, O });''', '''	for (AZoogiOrb* O : Orbs) if (O && !O->bOut) Cands.Add({ FVector2D(O->OrbLocation()), 50.f, ZoogiConst::OrbMassRatio, ZoogiConst::OrbRadius, O });
	if (AActor* RH = ZoogiRaid::HandActor()) Cands.Add({ FVector2D(RH->GetActorLocation()), 400.f, 50.f, 150.f, RH }); // b151 RAID: the titan's hand is the best target''')
addinc(GM, '#include "ZoogiRoyaleMix.h"', '#include "ZoogiRaid.h"')
ed1('Private/ZoogiCombo.cpp', '''	return bComboMode && BA && BB && BA != BB && BA->TeamIndex == BB->TeamIndex;''', '''	return (bComboMode || ZoogiRaid::Active()) && BA && BB && BA != BB && BA->TeamIndex == BB->TeamIndex; // b151: RAID = one team''')
addinc('Private/ZoogiCombo.cpp', '#include "ZoogiCombo.h"', '#include "ZoogiRaid.h"')
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''	ZoogiRoyaleMix::Tick(G, RealDt); // b149: the royale-mix action round clock''', '''	ZoogiRoyaleMix::Tick(G, RealDt); // b149: the royale-mix action round clock
	ZoogiRaid::Tick(G, RealDt); // b151: RAID mode''')
ed1(AF, '''	ZoogiSkills::Draw(G, C, PC); // b63: skill target pick (arc range ring, circle / line / targets, FIRE / CANCEL)''', '''	ZoogiSkills::Draw(G, C, PC); // b63: skill target pick (arc range ring, circle / line / targets, FIRE / CANCEL)
	ZoogiRaid::Draw(G, C); // b151: the raid boss health bar + team lives''')
addinc(AF, '#include "ZoogiRoyaleMix.h"', '#include "ZoogiRaid.h"')
SH = 'Private/ZoogiShowtime.cpp'
ed1(SH, '''	ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.7f);
	if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Aw && Aw->Name''', '''	ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 0.7f);
	ZoogiRaid::OnAwaken(G, B); // b151: SHOWTIME hits the raid titan for 20%
	if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(Aw && Aw->Name''')
addinc(SH, '#include "ZoogiShowtime.h"', '#include "ZoogiRaid.h"')
# boss attack: every raid round, rage-scaled zones
BA = 'Private/ZoogiBossAttack.cpp'
p = R + BA; s = open(p).read()
s = s.replace('		constexpr float Half = 35.f;       // sweep half-width (deg)', '		float HalfW() { return 35.f * ZoogiRaid::Rage(); } // sweep half-width (deg); b151: x1.3 when the raid titan is enraged')
s = re.sub(r'\bHalf\b(?!W)', 'HalfW()', s)
s = s.replace('SlamR[i]', '(SlamR[i] * ZoogiRaid::Rage())').replace('SlamR[2]', '(SlamR[2] * ZoogiRaid::Rage())')
s = s.replace('			if (TestMode()) return true;\n			if (G->MenuRounds >= 4)', '			if (TestMode() || ZoogiRaid::Active()) return true; // b151: the raid titan attacks every round\n			if (G->MenuRounds >= 4)')
s = s.replace('#include "ZoogiCosmosBoss.h"\n', '#include "ZoogiCosmosBoss.h"\n#include "ZoogiRaid.h"\n', 1)
open(p, 'w').write(s)
assert 'ZoogiRaid::Active()) return true' in s and 'HalfW()' in s
# menu: RAID card -> map select -> raid
MS = 'Private/ZoogiModeSelect.cpp'
ed1(MS, '''		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY") },
	};''', '''		{ TEXT("ModeSelect/ms_card_story"), TEXT("STORY") },
		{ TEXT("ModeSelect/ms_card_raid"), TEXT("RAID") }, // b151 (Evan): co-op vs the map's titan
	};
	bool GRaidMaps = false; // b151: the map cards were opened from RAID''')
ed1(MS, '''	case 4: // STORY: Oct 30 - STORY MODE (ZoogiStory.h); -ZoogiStoryAnime = the old episode 1 anime clip''', '''	case 5: // b151 RAID: pick a map (each has its titan), then you + 2 CPU allies against it
		bMapSelect = true; bTrainingMaps = false; ZoogiModeSel::GRaidMaps = true;
		bDirty = true;
		break;
	case 4: // STORY: Oct 30 - STORY MODE (ZoogiStory.h); -ZoogiStoryAnime = the old episode 1 anime clip''')
ed1(MS, '''	Fire("Mode", 0);
	Fire("Players", 4);
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;
	if (G->MenuArena != M.Arena) Fire("Arena", (int32)M.Arena);
	Fire("Start", 0);''', '''	Fire("Mode", 0);
	const bool bRaid = ZoogiModeSel::GRaidMaps; ZoogiModeSel::GRaidMaps = false;
	Fire("Players", bRaid ? 3 : 4); // b151 RAID: you + 2 CPU allies
	for (int32 i = 0; i < G->MenuPlayers.Num(); ++i) G->MenuPlayers[i].bCPU = i > 0;
	if (G->MenuArena != M.Arena) Fire("Arena", (int32)M.Arena);
	if (bRaid) { ZoogiRaid::Arm(G); UE_LOG(LogZoogi, Log, TEXT("RAID armed on %s"), M.Name); }
	Fire("Start", 0);''')
addinc(MS, '#include "ZoogiMenuRoot.h"', '#include "ZoogiRaid.h"')
# results screen
HW = 'Private/ZoogiArenaHUDWidget.cpp'
ed1(HW, '''			R.Stats.Add({ TEXT("PERFECTS"), St.Perfects });''', '''			R.Stats.Add({ TEXT("PERFECTS"), St.Perfects });
			if (ZoogiRaid::HasResult())
			{	// b151 RAID: team result, MVP, damage per Zoogi
				const bool bWin = ZoogiRaid::Cleared();
				R.Outcome = bWin ? 0 : 1;
				R.Head = bWin ? TEXT("RAID CLEARED!") : TEXT("RAID FAILED");
				R.Sub = HeroI >= 0 ? FString::Printf(TEXT("MVP  %s"), *G->Balls[HeroI]->Def.Name.ToUpper()) : FString(TEXT("TEAM EFFORT"));
				for (ZoogiP5::FResultRow& Rw : R.Rows) { Rw.Line = FString::Printf(TEXT("P%d  /  %d DAMAGE  /  %d HITS"), Rw.Player + 1, ZoogiRaid::DamageOf(Rw.Player), ZoogiRaid::HitsOf(Rw.Player)); Rw.bWin = bWin && Rw.Player == HeroI; }
				R.Stats.Reset();
				R.Stats.Add({ TEXT("BOSS HP LEFT"), ZoogiRaid::HPLeft() });
				R.Stats.Add({ TEXT("TEAM LIVES"), ZoogiRaid::LivesLeft() });
				R.Stats.Add({ TEXT("ORBS"), St.Orbs });
				R.Stats.Add({ TEXT("BEST COMBO"), St.BestCombo });
			}''')
addinc(HW, '#include "ZoogiArenaHUDWidget.h"', '#include "ZoogiRaid.h"')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B150"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B151"), TEXT("RAID MODE"), TEXT("BATTLE -> RAID -> pick a map: you and 2 CPU allies against that map\'s titan. Hit its glowing hand on the rim - roll into it or knock orbs into it (faster = more damage, gold stars hit hardest); SHOWTIME hits it for 20%. After every round it attacks (dodge the red zone) and moves; at 66% it moves after every turn, at 33% it is ENRAGED. Falls cost team lives (5). Beat it within 10 rounds - the results show the MVP.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
pr = os.path.expanduser('~/mnt/dev/ios/p5run.py'); t = open(pr).read()
o = "            ('royale_mix',"
assert t.count(o) == 1
t = t.replace(o, "            ('raid_test', 1600, 900, 360, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=90 -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=10 -ZoogiNoScreenShatter'),\n" + o)
open(pr, 'w').write(t)
print('b151b ok')
