# b142o: -ZoogiShowtimeTest drives SHOWTIME itself (the P5 test menu got stuck on its skill step): on the human's 2nd aim it fills
# the AWAKEN meter, uses it (AwakenUse) and starts SHOWTIME; the arm watcher presses SUPERPOWER 0.8 s into the roll.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
G = 'Private/ZoogiGameMode.cpp'
ed1(G, '''#include "Containers/Ticker.h"''', '''#include "Containers/Ticker.h"
#include "ZoogiShowtime.h"''')
ed1(G, '''	ZoogiUtil::SetComboColors(false);
	ClearMatch();''', '''	{	// b142o: -ZoogiShowtimeTest - SHOWTIME on the human's 2nd aim (the arm watcher fires it mid-roll)
		static bool bStTest = false;
		if (!bStTest && FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeTest")))
		{
			bStTest = true;
			TWeakObjectPtr<AZoogiGameMode> WSt = this; int32 Seen = 0, LastTurn = -1;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WSt, Seen, LastTurn](float) mutable -> bool
			{
				AZoogiGameMode* Gs = WSt.Get(); if (!Gs) return false;
				AZoogiBall* Bs = Gs->GetActiveBall();
				if (!Bs || Bs->bCPU || Gs->Phase != EZoogiPhase::Aiming || Gs->TurnCounter == LastTurn || Gs->PhaseTime < 1.0f) return true;
				LastTurn = Gs->TurnCounter;
				if (++Seen < 2) return true;
				if (UZoogiFunSys* Ft = UZoogiFunSys::Get(Gs)) Ft->FillMeterForTest(Gs, Bs->PlayerIndex);
				UE_LOG(LogZoogi, Log, TEXT("SHOWTIME TEST: P%d presses SHOWTIME (turn %d)"), Bs->PlayerIndex + 1, Gs->TurnCounter);
				Gs->MenuAction(TEXT("AwakenUse"), 0); ZoogiShowtime::Begin(Gs, Bs);
				Gs->TakeShotDelayed(TEXT("showtime_armed"), 1.5f);
				return false;
			}));
		}
	}
	ZoogiUtil::SetComboColors(false);
	ClearMatch();''')
print('b142o ok')
