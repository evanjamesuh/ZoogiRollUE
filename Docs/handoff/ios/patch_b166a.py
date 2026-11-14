import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b166a ok', len(EDITS))
# b166a (Evan Nov 6 17:10 "I want the chest to open", 18:08 "All chest pop up types"): the three chest shows (common burst,
#  rare card fan, epic slash reveal), tap-to-skip, -ZoogiChestTest captures, NEWS B166.
F = 'Private/ZoogiFront.cpp'
NEW = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'b166_chest_new.cpp'), 'rb').read().decode('utf-8').replace('\r\n', '\n')
ed1(F, '\tvoid PaintChest(FSlateWindowElementList& Out, const FGeometry& G, int32 L, SZoogiFront* Self) const\n', NEW)
ed1(F, '\tFName RewardChest;\n', '\tFName RewardChest;\n\tfloat ChestSkipAt = 0.6f; int32 ChestSfxStep = 0; // b166: the chest show length (a tap before it skips to the end) + its next sound\n\tint32 ChestTestC = 0, ChestTestShot = -1; bool bChestTest = false; // b166: -ZoogiChestTest\n')
ed1(F, '\t\tRewards = R; RewardTitle = Title; RewardChest = Chest; Pop = EPop::Chest; PopT = T;\n', '\t\tRewards = R; RewardTitle = Title; RewardChest = Chest; Pop = EPop::Chest; PopT = T;\n\t\tChestSfxStep = 0; ChestSkipAt = 2.f;\n')
ed1(F, 'else if (Self->Pop == SZoogiFront::EPop::Chest && Self->T - Self->PopT > 0.6f) Self->Pop = SZoogiFront::EPop::None; });',
       'else if (Self->Pop == SZoogiFront::EPop::Chest) { if (Self->T - Self->PopT < Self->ChestSkipAt) { Self->PopT = Self->T - Self->ChestSkipAt; Self->ChestSfxStep = 999; } else Self->Pop = SZoogiFront::EPop::None; } });')
ed1(F, '\t\t\telse PaintChest(Out, G, L, Self);\n\t\t\tL += 40;\n', '\t\t\telse PaintChest(Out, G, L, Self);\n\t\t\tL += 90; // b166: the card fan stacks up to ~60 layers\n')
ed1(F, 'bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrontTest"));', 'bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFrontTest")); bChestTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiChestTest")); if (bChestTest) bTest = true;')
ed1(F, '\t\tif (bTest) TickTest();\n', '\t\tif (bChestTest) TickChestTest(); else if (bTest) TickTest();\n')
ed1(F, '\tvoid TickTest()\n', r"""	void TickChestTest()
	{	// b166: -ZoogiChestTest - opens a common, a rare and an epic chest and shoots each show at fixed moments
		if (T < 2.f || ChestTestC >= 3) return;
		static const float At[6] = { 0.5f, 0.85f, 1.05f, 1.4f, 1.9f, 3.0f };
		static const TCHAR* Ty[3] = { TEXT("wooden"), TEXT("royal"), TEXT("epic") };
		static const TCHAR* Nm[3] = { TEXT("COMMON CHEST"), TEXT("RARE CHEST"), TEXT("EPIC CHEST") };
		if (ChestTestShot < 0)
		{
			TArray<FZoogiFrontReward> R;
			R.Add(FZoogiFrontReward{ FName(TEXT("gold")), NAME_None, 250 + 250 * ChestTestC });
			R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("frost")), 3 });
			R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("heavy_roll")), 2 });
			if (ChestTestC >= 1) R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("ice_wall")), 1 });
			if (ChestTestC >= 2) R.Add(FZoogiFrontReward{ FName(TEXT("card")), FName(TEXT("meteor_rain")), 1 });
			ShowRewards(Nm[ChestTestC], FName(Ty[ChestTestC]), R);
			ChestTestShot = 0; return;
		}
		if (ChestTestShot < 6 && T - PopT >= At[ChestTestShot]) { Shot(FString::Printf(TEXT("chest_%d_%s_%03d"), ChestTestC, Ty[ChestTestC], FMath::RoundToInt(At[ChestTestShot] * 100.f))); ++ChestTestShot; return; }
		if (ChestTestShot >= 6 && T - PopT > 3.3f)
		{
			UE_LOG(LogZoogi, Log, TEXT("FRONT chest test %s style %d end %.2f"), Ty[ChestTestC], ChestStyle(), ChestSkipAt);
			++ChestTestC; ChestTestShot = -1;
			if (ChestTestC >= 3) { Pop = EPop::None; UE_LOG(LogZoogi, Log, TEXT("FRONT chest test done")); }
		}
	}
	void TickTest()
""")
ed1(F, '\t\t\t{ TEXT("B165"), ', '\t\t\t{ TEXT("B166"), TEXT("CHESTS NOW OPEN"), TEXT("Every chest opens with its own show: Common chests shake and burst open, Rare chests fan their cards out like a hand, Epic chests get a red slash reveal. Tap to skip to your rewards.") },\n\t\t\t{ TEXT("B165"), ')
run()
