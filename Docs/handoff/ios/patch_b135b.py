# b135b (Evan 07:52: "Getting knocked off resets awaken meter"): any fall off the arena (knocked off, or rolling off on your own shot)
# empties that Zoogi's AWAKEN meter (was +15 catch-up). -ZoogiOldMeters = old catch-up gain.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiActionFun.cpp', '''	for (const FZoogiLedgerEntry& LE : G->Ledger) if (LE.bFall && LE.Victim != Player && Players.IsValidIndex(LE.Victim)) AddMeter(G, LE.Victim, 15.f, TEXT("knocked off"));''',
'''	if (ZoogiAwakenMeter())
	{	// b135b (Evan): getting knocked off resets the AWAKEN meter
		for (const FZoogiLedgerEntry& LE : G->Ledger)
			if (LE.bFall && Players.IsValidIndex(LE.Victim) && Players[LE.Victim].Meter > 0.f)
			{
				UE_LOG(LogZoogi, Log, TEXT("FUN AWAKEN meter P%d %.0f -> 0 (knocked off)"), LE.Victim + 1, Players[LE.Victim].Meter);
				Players[LE.Victim].Meter = 0.f;
			}
	}
	else for (const FZoogiLedgerEntry& LE : G->Ledger) if (LE.bFall && LE.Victim != Player && Players.IsValidIndex(LE.Victim)) AddMeter(G, LE.Victim, 15.f, TEXT("knocked off"));''')
print('b135b ok')
