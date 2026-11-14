# b133b (Evan 07:11: "add the option in training pause menu to add CPU opponents up to 4 to practice classic games and test animation scenes"):
# TRAINING pause menu row "CPU OPPONENTS: n" - each tap adds one (0 -> 1 -> 2 -> 3 -> 0; 4 Zoogis is the classic maximum) and restarts the
# training match with the new line-up (same as Settings > TRAINING BOTS). RESTART MATCH in training now stays in training.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''	else if (Action == "Rematch") StartMatch();''', '''	else if (Action == "Rematch") { if (bSoloTraining) bWantSoloTraining = true; StartMatch(); } // b133: a training restart stays in training
	else if (Action == "TrainBots")
	{	// b133 (Evan): TRAINING pause menu - CPU opponents (0..3) for classic practice / anime scene tests; restarts the training match
		ZoogiControls::Set(TEXT("TrainBots"), (float)FMath::Clamp(Index, 0, 3));
		bWantSoloTraining = true;
		UE_LOG(LogZoogi, Log, TEXT("TRAINING pause menu: %d CPU opponent(s)"), FMath::Clamp(Index, 0, 3));
		StartMatch();
	}''')
H = 'Private/ZoogiArenaHUDWidget.cpp'
ed1(H, '''	Rows.Add({ TEXT("RESUME"), 0, [this]() { SetPaused(false); } });
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome")))''', '''	Rows.Add({ TEXT("RESUME"), 0, [this]() { SetPaused(false); } });
	if (const AZoogiGameMode* Gt = GM()) if (Gt->bSoloTraining && !Gt->bRoyaleMode)
	{	// b133 (Evan: "option in training pause menu to add CPU opponents"): each tap adds one (max 3 + you), the match restarts
		static FString BotLabel;
		const int32 Nb = FMath::Clamp(Gt->MenuNumPlayers - 1, 0, 3);
		BotLabel = Nb >= 3 ? FString(TEXT("CPU OPPONENTS: 3  (TAP = NONE)")) : FString::Printf(TEXT("CPU OPPONENTS: %d  (TAP = +1)"), Nb);
		Rows.Add({ *BotLabel, 2, [this, Nb]() { SetPaused(false); if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("TrainBots"), (Nb + 1) % 4); } });
	}
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome")))''')
ed1(H, '''	const float RowStep = Rows.Num() > 5 ? 80.f : 92.f; // b79: 7 rows fit''', '''	const float RowStep = Rows.Num() > 7 ? 72.f : (Rows.Num() > 5 ? 80.f : 92.f); // b79: 7 rows fit; b133: 8 in training''')
print('b133b ok')
