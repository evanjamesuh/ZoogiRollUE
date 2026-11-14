# b144b: -ZoogiItemTest self-test - the human's first turn: ITEM -> PRISM GRENADE row -> FIRE pressed through the real Input path
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiP5Command.cpp', '''			else Step(1, 1.2f, [&]() { if (W.E[Showtime].bOn) DoCmd(Showtime); else DoCmd(Attack); GS.TestTurns++; GS.TestStep = 0; });
		}
		return true;''', '''			else Step(1, 1.2f, [&]() { if (W.E[Showtime].bOn) DoCmd(Showtime); else DoCmd(Attack); GS.TestTurns++; GS.TestStep = 0; });
		}
		// b144b: -ZoogiItemTest - ITEM -> the PRISM GRENADE row (ZoogiSkills::Tick then presses FIRE on the throw pick)
		static const bool bItemTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiItemTest"));
		static int32 ItemStep = 0;
		if (bItemTest && ItemStep < 2)
		{
			if (ItemStep == 0 && W.Mode == 0 && W.T >= 1.0f) { UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: opens ITEM")); DoCmd(Item); ++ItemStep; }
			else if (ItemStep == 1 && W.Mode == 2 && W.ModeT >= 0.7f)
			{
				Shot(TEXT("item_list")); ++ItemStep;
				int32 Pick = -1; for (int32 r = 0; r < W.Skills.Num(); ++r) if (W.Skills[r].bOn && W.Skills[r].Name.Contains(TEXT("GRENADE"))) { Pick = r; break; }
				UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: grenade row %d of %d"), Pick, W.Skills.Num());
				if (Pick >= 0) DoSkill(Pick);
			}
		}
		return true;''')
ed1('Private/ZoogiSkills.cpp', '''	void Tick(AZoogiGameMode* G, float RealDt)
	{
''', '''	void Tick(AZoogiGameMode* G, float RealDt)
	{
		{	// b144b: -ZoogiItemTest - press FIRE on the human's grenade throw pick
			static const bool bIt = FParse::Param(FCommandLine::Get(), TEXT("ZoogiItemTest"));
			if (bIt && G && Picking() && S.bGrenade && !S.bCpu && S.FireBtn.bIsValid && FPlatformTime::Seconds() - S.OpenedAt > 1.6)
			{
				UE_LOG(LogZoogi, Log, TEXT("ITEM TEST: presses FIRE on the grenade pick"));
				G->TakeShotDelayed(TEXT("grenade_pick"), 0.f);
				Input(G, nullptr, S.FireBtn.GetCenter(), true, false, false);
				G->TakeShotDelayed(TEXT("grenade_air"), 0.35f);
				G->TakeShotDelayed(TEXT("grenade_boom"), 1.1f);
			}
		}
''')
p = os.path.expanduser('~/mnt/dev/ios/p5run.py'); b = open(p).read()
o = "            ('stands_test',"
assert b.count(o) == 1
b = b.replace(o, "            ('item_test', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiItemTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2'),\n" + o)
open(p, 'w').write(b)
print('b144b ok')
