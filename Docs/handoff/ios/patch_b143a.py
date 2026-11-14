# b143a (Evan 12:20 "the superpower button we have - activating an awaken should work like activating the last cast" + screenshot
# crossing out POWER; 12:21 "items shouldn't be locked if a weapon is equipped ... change weapon to item in inventory titles"):
#  - the P5 command fan has no POWER entry any more (the star SUPERPOWER button fires the armed Awaken / banked casts); -ZoogiPowerCmd = back
#  - ITEM lights up when an equipped item has a count left (or a picked-up grenade) and none was used this turn
#  - inventory: the WEAPON tab / labels say ITEM
#  - -ZoogiShowtimeTest: SHOWTIME on the human's first aim, the target is picked for it after 1.5 s
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
P = 'Private/ZoogiP5Command.cpp'
ed1(P, '''		Set(Item, TEXT("ITEM"), TEXT("Use an item"), false, 175.f, -170.f, -15.f, 240.f);''', '''		bool bAnyItem = false; // b143a (Evan): ITEM is open when an equipped item is left (one a turn)
		if (!ZoogiItems::UsedThisTurn(G, B->PlayerIndex))
		{
			for (const FName& Wn : ZoogiItems::Selected(G)) if (ZoogiItems::Count(G, B->PlayerIndex, Wn) > 0) bAnyItem = true;
			if (ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) > 0) bAnyItem = true;
		}
		Set(Item, TEXT("ITEM"), TEXT("Use an item"), bAnyItem, 175.f, -170.f, -15.f, 240.f);''')
ed1(P, '''		Set(Power, bPow ? TEXT("POWER") : TEXT(""), TEXT("Superpower"), bPow, -230.f, -40.f, -8.f, 260.f);''', '''		static const bool bPowerCmd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPowerCmd")); // b143a (Evan): the star SUPERPOWER button does it
		Set(Power, bPow && bPowerCmd ? TEXT("POWER") : TEXT(""), TEXT("Superpower"), bPow && bPowerCmd, -230.f, -40.f, -8.f, 260.f);''')
ed1(P, '''TEXT("Pick weapons in INVENTORY - WEAPON. From round 2''', '''TEXT("Pick items in INVENTORY - ITEM. From round 2''')
F = 'Private/ZoogiFront.cpp'
ed1(F, '''static const TCHAR* Tabs[] = { TEXT("BALLERS"), TEXT("ZOOGIS"), TEXT("WEAPON"), TEXT("SKILLS") };''', '''static const TCHAR* Tabs[] = { TEXT("BALLERS"), TEXT("ZOOGIS"), TEXT("ITEM"), TEXT("SKILLS") }; // b143a (Evan): WEAPON -> ITEM''')
ed1(F, '''TEXT("%s WEAPON  x%d")''', '''TEXT("%s ITEM  x%d")''')
ed1(F, '''TEXT("3 WEAPONS MAX - REMOVE ONE FIRST")''', '''TEXT("3 ITEMS MAX - REMOVE ONE FIRST")''')
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''				LastTurn = Gs->TurnCounter;
				if (++Seen < 2) return true;''', '''				LastTurn = Gs->TurnCounter;
				if (++Seen < 1) return true; // b143a: the first human aim''')
S = 'Private/ZoogiShowtime.cpp'
ed1(S, '''			if (G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
			else if (T > 13.2) { Lock(G, Nearest(G, B), TEXT("time up - nearest")); GStage = EStage::Fade; GFadeT0 = Now(); }''', '''			static const bool bStTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest"));
			if (G->Phase != EZoogiPhase::Aiming) { GStage = EStage::Fade; GFadeT0 = Now(); }
			else if (T > (bStTest ? 2.7 : 13.2)) { Lock(G, Nearest(G, B), bStTest ? TEXT("test pick") : TEXT("time up - nearest")); GStage = EStage::Fade; GFadeT0 = Now(); }''')
print('b143a ok')
