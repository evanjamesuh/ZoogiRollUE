# b126a (Evan 22:05: "the redirect - the zoogi barely moves in that direction"). b125 proof (turn_iphone): fired 2210 cm/s, 0.25 s later
# 477 / 381 / 799 cm/s BEFORE reaching its target. The redirected Zoogi now keeps (at least 90% of) its launch speed all the way to the
# target it was sent at (up to 1.2 s), so it really travels there and smashes in. -ZoogiRedirectSoft = the b70 behaviour.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
D = 'Private/ZoogiRedirect.cpp'
ed1(D, '''		double SpdCheckAt = 0.0; TWeakObjectPtr<AZoogiBall> SpdCheckBall; float SpdFired = 0.f; // b125: speed 0.25 s after a redirect (log)''',
'''		double SpdCheckAt = 0.0; TWeakObjectPtr<AZoogiBall> SpdCheckBall; float SpdFired = 0.f; // b125: speed 0.25 s after a redirect (log)
		float HomeFloor = 0.f; // b126: the redirected Zoogi never drops below this until it reaches its target''')
ed1(D, '''			SpdCheckAt = Now() + 0.25; SpdCheckBall = B; SpdFired = Spd;''', '''			SpdCheckAt = Now() + 0.25; SpdCheckBall = B; SpdFired = Spd;
			HomeFloor = bSoft ? 0.f : Spd * 0.9f;''')
ed1(D, '''			Homing = T; HomingUntil = Now() + 0.9;''', '''			Homing = T; HomingUntil = Now() + (FParse::Param(FCommandLine::Get(), TEXT("ZoogiRedirectSoft")) ? 0.9 : 1.2);''')
ed1(D, '''				if (S > 200.f && FVector::Dist2D(Loc(T), A->BallLocation()) > ZoogiConst::ZoogiRadius * 2.5f) A->SetPlanarVelocity(Dir * S);''',
'''				// b126: and it keeps its redirect speed until it gets there (it used to bleed to ~25% in a quarter second)
				const float S2 = FMath::Max(S, HomeFloor);
				if ((S > 200.f || HomeFloor > 0.f) && FVector::Dist2D(Loc(T), A->BallLocation()) > ZoogiConst::ZoogiRadius * 2.5f) A->SetPlanarVelocity(Dir * S2);
				else if (FVector::Dist2D(Loc(T), A->BallLocation()) <= ZoogiConst::ZoogiRadius * 2.5f) HomeFloor = 0.f; // arrived: physics takes over''')
ed1(D, '''			else Homing.Reset();''', '''			else { Homing.Reset(); HomeFloor = 0.f; }''')
print('b126a ok')
