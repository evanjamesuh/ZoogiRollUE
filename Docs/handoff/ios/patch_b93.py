# b93 (Evan 17:15: "in settings add an auto mode for me to watch the CPUs play"): Settings > WATCH CPU PLAY - every Zoogi is a CPU
# from the next match on (P1 is yours again when it is switched off).
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiFront.h', [
 ('	ZOOGIROLL_API bool ReplayCam();', '	ZOOGIROLL_API bool WatchCPU();     // b93 (Evan): Settings > WATCH CPU PLAY - every Zoogi is a CPU (from the next match)\n	ZOOGIROLL_API bool ReplayCam();'),
])
ed(P + 'ZoogiFront.cpp', [
 ('	bool ReplayCam() { return', '	bool WatchCPU() { return Get(TEXT("WatchCPU"), 0.f, 0.f, 1.f) > 0.5f; }\n	bool ReplayCam() { return'),
 ('''		auto Audio = [&](const TCHAR* Label, const TCHAR* Key)''',
  '''		const bool bWatch = ZoogiControls::WatchCPU(); // b93 (Evan): sit back and watch the CPUs play
		Toggle(TEXT("WATCH CPU PLAY"), bWatch ? TEXT("ON - NEXT MATCH") : TEXT("OFF"), bWatch, [Click, bWatch]() { ZoogiControls::Set(TEXT("WatchCPU"), bWatch ? 0.f : 1.f); if (Click) Click(1.f); });
		auto Audio = [&](const TCHAR* Label, const TCHAR* Key)'''),
])
ed(P + 'ZoogiGameMode.cpp', [
 ('	if (bSoloTraining) { MenuNumPlayers = 1; if (MenuPlayers.Num() > 0) MenuPlayers[0].bCPU = false; MenuRounds = 30; }',
  '''	if (bSoloTraining) { MenuNumPlayers = 1; if (MenuPlayers.Num() > 0) MenuPlayers[0].bCPU = false; MenuRounds = 30; }
	{	// b93 (Evan): Settings > WATCH CPU PLAY - every Zoogi is a CPU; switched off, P1 is the player's again
		static bool bWatchApplied = false;
		if (ZoogiControls::WatchCPU() && !bSoloTraining) { for (FZoogiPlayerSetup& Pw : MenuPlayers) Pw.bCPU = true; bWatchApplied = true; UE_LOG(LogZoogi, Log, TEXT("WATCH CPU PLAY: every Zoogi is a CPU this match")); }
		else if (bWatchApplied) { if (MenuPlayers.Num() > 0) MenuPlayers[0].bCPU = false; bWatchApplied = false; }
	}'''),
])
print('b93 ok')
