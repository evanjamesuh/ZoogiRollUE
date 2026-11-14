# b143c (Evan 12:32 "Remove the auto and arc button at the bottom"): the classic aim bar has no AUTO / ARC (drag to launch); the
# Frozen outer ring keeps its JUMP IN button. -ZoogiAimButtons = back.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
W = 'Private/ZoogiAwakenWidget.cpp'
ed1(W, '''		TArray<int32> Steps; if (bOldModes) Steps = { 0, 1, 2, 3 }; else Steps = { 0 }; // b77 (Evan: "delete the LAUNCH button"): AUTO / [WOLF] / ARC''', '''		TArray<int32> Steps; if (bOldModes) Steps = { 0, 1, 2, 3 }; else Steps = { 0 }; // b77 (Evan: "delete the LAUNCH button"): AUTO / [WOLF] / ARC
		static const bool bAimBtns = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAimButtons")); // b143c (Evan): no AUTO / ARC
		const bool bOuterBar = G->ActiveInOuterRing();
		if (!bAimBtns) Steps.Reset();''')
ed1(W, '''		const bool bOuter = G->ActiveInOuterRing();
		MakePlateButton(P, bOuter ?''', '''		const bool bOuter = bOuterBar;
		if (bAimBtns || bOuter) MakePlateButton(P, bOuter ?''')
print('b143c ok')
