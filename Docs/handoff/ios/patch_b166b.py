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
    print('b166b ok', len(EDITS))
# b166b (Evan Nov 6 18:18 "Update all story mode level some look outdated", 18:21 "It's the old launch mechanic"):
#  the 2v2 story fights (mode=combo: m1_final, m1_escape, m2_dunes, m2_boss, m4_gate, m4_final) ran the old COMBO mode -
#  its own old arenas and the old SKILL / % LAUNCH bar. They now run on the CLASSIC turn flow (today's arenas, the P5
#  ITEM/SKILL/GUARD/ATTACK menu, drag-back launch) with teams kept: P1+P2 vs P3+P4, allies are never targeted, the team with
#  more points wins. -ZoogiStoryOldCombo = the old combo matches.
GM = 'Private/ZoogiGameMode.cpp'; GH = 'Public/ZoogiGameMode.h'; CB = 'Private/ZoogiCombo.cpp'; ST = 'Private/ZoogiStoryMode.inl'; FR = 'Private/ZoogiFront.cpp'
ed1(GH, '\tbool bComboMode = false;\n', '\tbool bComboMode = false;\n\tbool bClassicTeams = false; // b166: story 2v2 on the classic turn flow (P1+P2 vs P3+P4); cleared by MenuAction Mode\n')
ed1(CB, '\treturn (bComboMode || ZoogiRaid::Active()) && BA && BB && BA != BB && BA->TeamIndex == BB->TeamIndex; // b151: RAID = one team\n',
        '\tif (bClassicTeams && !bComboMode && BA && BB && BA != BB) return TeamOfIndex(BA->PlayerIndex) == TeamOfIndex(BB->PlayerIndex); // b166: story teams on classic\n\treturn (bComboMode || ZoogiRaid::Active()) && BA && BB && BA != BB && BA->TeamIndex == BB->TeamIndex; // b151: RAID = one team\n')
ed1(GM, 'else if (Action == "Mode") { if (Index == 2)', 'else if (Action == "Mode") { bClassicTeams = false; if (Index == 2)')
ed1(GM, '(bComboMode && O->TeamIndex == Me->TeamIndex)) continue;', '(bComboMode && O->TeamIndex == Me->TeamIndex) || IsAlly(O, Me)) continue; // b166: + story teams')
ed1(GM, '''		else if (Balls[i]->Score == Balls[WinnerIndex]->Score) bTie = true;
	}
''', '''		else if (Balls[i]->Score == Balls[WinnerIndex]->Score) bTie = true;
	}
	if (bClassicTeams && !bComboMode && Balls.Num() >= 4)
	{	// b166: story teams - the team with more points wins; its best scorer takes the banner
		int32 Ts[2] = { 0, 0 };
		for (int32 i = 0; i < Balls.Num(); ++i) if (Balls[i]) Ts[FMath::Min(TeamOfIndex(i), 1)] += Balls[i]->Score;
		bTie = Ts[0] == Ts[1];
		if (!bTie)
		{
			const int32 Wt = Ts[0] > Ts[1] ? 0 : 1;
			WinnerIndex = Wt * 2;
			for (int32 i = Wt * 2 + 1; i < FMath::Min(Balls.Num(), Wt * 2 + 2); ++i) if (Balls[i] && Balls[i]->Score > Balls[WinnerIndex]->Score) WinnerIndex = i;
		}
		UE_LOG(LogZoogi, Log, TEXT("TEAMS final %d - %d, winner P%d"), Ts[0], Ts[1], bTie ? 0 : WinnerIndex + 1);
	}
''')
ed1(ST, '''		const int32 Mode = M->Mode == TEXT("combo") ? 1 : (M->Mode == TEXT("royale") ? 2 : 0);
		if (G->Phase != EZoogiPhase::Menu) G->MenuAction(TEXT("Menu"), 0);
		G->MenuAction(TEXT("Mode"), Mode);
''', '''		static const bool bOldCombo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiStoryOldCombo"));
		const bool bTeams = M->Mode == TEXT("combo") && !bOldCombo; // b166: 2v2 story fights on the classic turn flow
		const int32 Mode = (M->Mode == TEXT("combo") && bOldCombo) ? 1 : (M->Mode == TEXT("royale") ? 2 : 0);
		if (G->Phase != EZoogiPhase::Menu) G->MenuAction(TEXT("Menu"), 0);
		G->MenuAction(TEXT("Mode"), Mode);
		G->bClassicTeams = bTeams;
''')
ed1(ST, 'GS.bWon = G->bComboMode ? G->ComboWinner == 0 : G->WinnerIndex == 0;', 'GS.bWon = G->bComboMode ? G->ComboWinner == 0 : G->bClassicTeams ? (G->WinnerIndex >= 0 && AZoogiGameMode::TeamOfIndex(G->WinnerIndex) == 0) : G->WinnerIndex == 0; // b166: story teams')
ed1(FR, '\t\t\t{ TEXT("B166"), TEXT("CHESTS NOW OPEN"),', '\t\t\t{ TEXT("B166"), TEXT("STORY 2V2 FIGHTS GET THE NEW CONTROLS"), TEXT("The six team battles in Story Mode (the Frozen Final, the Collapse, the Dunes, the Caldera Heart, the Sky Gate and Void Calling) now use today\'s arenas and the ITEM / SKILL / GUARD / ATTACK menu with drag-back launch. Your partner is still on your side.") },\n\t\t\t{ TEXT("B166"), TEXT("CHESTS NOW OPEN"),')
run()
