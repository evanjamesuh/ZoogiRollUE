# b96c (Evan 18:30): "In training add in setting a way to add bots to practice duels and everything".
# Settings > TRAINING BOTS (NONE / 1 / 2 / 3). Classic TRAINING: you + that many CPU Zoogis (turns, duels, skills, crits - everything),
# still 30 rounds that never end. Royale TRAINING sandbox starts with the same bot count (its own +/- still works).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiFront.h', [
 ('	ZOOGIROLL_API bool WatchCPU();     // b93', '	ZOOGIROLL_API int32 TrainBots();   // b96c (Evan): Settings > TRAINING BOTS - CPU Zoogis that join you in TRAINING (0..3)\n	ZOOGIROLL_API bool WatchCPU();     // b93')])
ed(P + 'ZoogiFront.cpp', [
 ('	bool WatchCPU() { return Get(TEXT("WatchCPU"), 0.f, 0.f, 1.f) > 0.5f; }',
  '	bool WatchCPU() { return Get(TEXT("WatchCPU"), 0.f, 0.f, 1.f) > 0.5f; }\n	int32 TrainBots() { return FMath::RoundToInt(Get(TEXT("TrainBots"), 0.f, 0.f, 3.f)); }'),
 ('''		Toggle(TEXT("WATCH CPU PLAY"), bWatch ? TEXT("ON - NEXT MATCH") : TEXT("OFF"), bWatch, [Click, bWatch]() { ZoogiControls::Set(TEXT("WatchCPU"), bWatch ? 0.f : 1.f); if (Click) Click(1.f); });''',
  '''		Toggle(TEXT("WATCH CPU PLAY"), bWatch ? TEXT("ON - NEXT MATCH") : TEXT("OFF"), bWatch, [Click, bWatch]() { ZoogiControls::Set(TEXT("WatchCPU"), bWatch ? 0.f : 1.f); if (Click) Click(1.f); });
		const int32 TBots = ZoogiControls::TrainBots(); // b96c (Evan): bots to practice duels and everything in TRAINING
		static const TCHAR* BotNames[] = { TEXT("NONE - SOLO"), TEXT("1 BOT"), TEXT("2 BOTS"), TEXT("3 BOTS") };
		Toggle(TEXT("TRAINING BOTS"), BotNames[TBots & 3], TBots != 0, [Click, TBots]() { ZoogiControls::Set(TEXT("TrainBots"), (float)((TBots + 1) % 4)); if (Click) Click(1.f); });'''),
 ('		const FVector2D Sz(860.f, 800.f); // b93: + WATCH CPU PLAY row', '		const FVector2D Sz(860.f, 864.f); // b93: + WATCH CPU PLAY row; b96c: + TRAINING BOTS row'),
])
ed(P + 'ZoogiGameMode.cpp', [
 ('	if (bSoloTraining) { MenuNumPlayers = 1; if (MenuPlayers.Num() > 0) MenuPlayers[0].bCPU = false; MenuRounds = 30; }',
  '''	if (bSoloTraining)
	{	// b96c (Evan): Settings > TRAINING BOTS - you + 0..3 CPU Zoogis to practice duels and everything (still never ends)
		const int32 Bots = FMath::Clamp(ZoogiControls::TrainBots(), 0, FMath::Max(0, MenuPlayers.Num() - 1));
		MenuNumPlayers = 1 + Bots; MenuRounds = 30;
		for (int32 i = 0; i < MenuPlayers.Num(); ++i) MenuPlayers[i].bCPU = i > 0;
		UE_LOG(LogZoogi, Log, TEXT("TRAINING: you + %d bot(s)"), Bots);
	}'''),
])
ed(P + 'ZoogiRoyale.cpp', [('	bRoyaleTraining = true;\n	RoyaleTrainBots = 0;', '	bRoyaleTraining = true;\n	RoyaleTrainBots = FMath::Clamp(ZoogiControls::TrainBots(), 0, 3); // b96c: Settings > TRAINING BOTS')])
print('b96c ok')
