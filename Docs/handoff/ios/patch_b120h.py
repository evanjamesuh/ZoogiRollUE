# b120h (Evan 19:23: "Fix reaction because game sticks and I can't select a target sometimes and if no target game stalls; if no target
# within arc radius no slowdown"):
#  - REDIRECT pick closes the moment no target is left inside the arc range (it used to hold the near-frozen world for 30 s)
#  - bigger tap window round each glowing target (0.20 x short side + 80 px, was 0.12 + 50)
#  - a pad press blocks field taps for 0.15 s after the finger lifts (was 0.35 s - a quick tap right after REACTION was eaten)
#  - a tap may wobble 26 px before it counts as a camera drag (was 18)
#  - the guided-turn REACTION slow-down only while a target is in range
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
RD = 'Private/ZoogiRedirect.cpp'
ed1(RD, '''			else if (Now() - PickT0 > 30.0) { Close(); UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick timed out (nothing spent)")); } // b67: 30 s, the game stopped''',
'''			else if (Candidates(G, PickBall.Get()).Num() == 0)
			{	// b120h (Evan: "if no target the game stalls"): nothing left in the arc range - close at once, play goes on
				if (AZoogiBall* Pb = PickBall.Get()) G->PublicPopup(TEXT("NO TARGET IN RANGE"), Pb->BallLocation() + FVector(0, 0, 170.f), FLinearColor(1.f, 0.6f, 0.4f));
				Close(); UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick closed: no target in range (nothing spent)"));
			}
			else if (Now() - PickT0 > 30.0) { Close(); UE_LOG(LogZoogi, Log, TEXT("REDIRECT pick timed out (nothing spent)")); } // b67: 30 s, the game stopped''')
ed1(RD, '''		if (Best && BestD < 0.12f * FMath::Max(1, FMath::Min(Vx, Vy)) + 50.f) { Close(); Fire(G, B, Best); }''',
'''		if (Best && BestD < 0.20f * FMath::Max(1, FMath::Min(Vx, Vy)) + 80.f) { Close(); Fire(G, B, Best); } // b120h: a bigger tap window (was 0.12 + 50)
		else UE_LOG(LogZoogi, Log, TEXT("REDIRECT tap missed (nearest target %.0f px away)"), BestD);''')
ed1('Private/ZoogiClassicPad.cpp', '''	return (GPadHeld && Dt < 6.0) || Dt < 0.35; // (a lost touch-end never blocks the field for more than 6 s)''', '''	return (GPadHeld && Dt < 6.0) || Dt < 0.15; // (a lost touch-end never blocks the field for more than 6 s); b120h: 0.15 s after the lift (was 0.35)''')
ed1('Private/ZoogiPauseOrbit.cpp', '''FVector2D::Distance(P, GOrbPress) > 18.f''', '''FVector2D::Distance(P, GOrbPress) > 26.f''')
ed1('Private/ZoogiActionFun.cpp', '''			else if (ZoogiGuide::WantSlow()) Want = 0.3f;''', '''			else if (ZoogiGuide::WantSlow() && ZoogiRedirect::Available(G)) Want = 0.3f; // b120h: only with a target in the arc range''')
print('b120h ok')
