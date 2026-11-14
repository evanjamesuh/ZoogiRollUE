# b136a (Evan 08:22 #1): the cosmos boss watcher - ZoogiCosmosBoss.h/.cpp + hooks (match/round start, anime events, REDIRECT combos)
import os, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
S = os.path.expanduser('~/mnt/dev/ios/b136/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
shutil.copy(S + 'ZoogiCosmosBoss.h', R + 'Public/ZoogiCosmosBoss.h')
shutil.copy(S + 'ZoogiCosmosBoss.cpp', R + 'Private/ZoogiCosmosBoss.cpp')
SS = 'Private/ZoogiStageShift.cpp'
ed1(SS, '#include "ZoogiCosmicMarkers.h"\n', '#include "ZoogiCosmicMarkers.h"\n#include "ZoogiCosmosBoss.h"\n')
ed1(SS, '''		ZoogiCosmicMarkers::OnMatchStart(G); // b87''', '''		ZoogiCosmicMarkers::OnMatchStart(G); // b87
		ZoogiCosmosBoss::OnMatchStart(G); // b136 (Evan): the boss watching over the cosmos arena''')
ed1(SS, '''		ZoogiCosmicMarkers::OnRoundStart(G); // b87 (Evan): Cosmic grows one rune marker per round''', '''		ZoogiCosmicMarkers::OnRoundStart(G); // b87 (Evan): Cosmic grows one rune marker per round
		ZoogiCosmosBoss::OnRoundStart(G); // b136''')
AN = 'Private/ZoogiAnimeSubsystem.cpp'
ed1(AN, '#include "ZoogiAnimeSubsystem.h"\n', '#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiCosmosBoss.h"\n')
ed1(AN, '''	if (G) G->NotifyAnimeEvent(Ev); // announcer + music surge (KO / Critical / Combo)''', '''	if (G) G->NotifyAnimeEvent(Ev); // announcer + music surge (KO / Critical / Combo)
	ZoogiCosmosBoss::OnEvent(GetWorld(), Ev); // b136: the cosmos boss reacts''')
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '#include "Misc/CommandLine.h"\n', '#include "Misc/CommandLine.h"\n#include "ZoogiCosmosBoss.h"\n')
ed1(RD, '''			if (Pi >= 0 && Pi < 4 && Bolt[Pi]) Zap(G, B);
			if (UZoogiFunSys* F = Fun(G))
				F->PublicCallout(FString::Printf(TEXT("COMBO x%d"), ComboN)''', '''			if (Pi >= 0 && Pi < 4 && Bolt[Pi]) Zap(G, B);
			if (ComboN >= 3) ZoogiCosmosBoss::OnEvent(W, TEXT("Combo")); // b136: the cosmos boss leans in
			if (UZoogiFunSys* F = Fun(G))
				F->PublicCallout(FString::Printf(TEXT("COMBO x%d"), ComboN)''')
print('b136a ok')
