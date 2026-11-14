# b142f (Evan 11:47 "If a player is underneath the scoreboard like this hide the centre pieces"): while the Zoogi whose turn it is
# sits under the centre spire / scoreboard (within 8.5 m x arena scale of it), the spire + centre props fade away so the aim is
# clear. -ZoogiSpireAlways keeps it.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiHoloSpire.cpp', '''		const bool bShow = bAlways || (bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting() || bCritUp; // b86: up while it throws the new orbs''', '''		bool bUnder = false; // b142 (Evan): the turn's Zoogi under the scoreboard -> the centre pieces get out of the way
		if (const AZoogiBall* Ab = G->GetActiveBall())
		{
			const float Ur = 850.f * FMath::Max(1.f, G->Arena ? G->Arena->GetActorScale3D().X : 1.f);
			bUnder = !Ab->bOut && G->Phase != EZoogiPhase::Menu && FVector::Dist2D(Ab->BallLocation(), Sp->GetActorLocation()) < Ur;
			static bool bWasUnder = false;
			if (bUnder != bWasUnder) { bWasUnder = bUnder; UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE: P%d %s the scoreboard"), Ab->PlayerIndex + 1, bUnder ? TEXT("is under") : TEXT("left")); }
		}
		const bool bShow = bAlways || ((bShowRound && !bLaunched) || ZoogiSpireOrbs::Emitting() || bCritUp) && !(bUnder && !bCritUp); // b86: up while it throws the new orbs''')
print('b142f ok')
