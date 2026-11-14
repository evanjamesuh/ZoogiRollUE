# b128a (Evan 02:47, SHOWTIME screenshot: "remove the pulsing glow the orbs receive from showtime"): the yellow target pulse every 0.45 s
# during the SHOWTIME pick no longer goes on orbs (rival Zoogis still glow). -ZoogiShowtimeOrbGlow = the b122 pulse on every target.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiShowtime.cpp', '''				for (AActor* Tt : G->PublicLiveTargets(B)) AZoogiFX::Spawn(G->GetWorld(), Tt->GetActorLocation(), FLinearColor(1.f, 0.8f, 0.3f), 120.f, 0.4f, true, 6.f, Tt);''',
'''				static const bool bOrbGlow = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShowtimeOrbGlow"));
				for (AActor* Tt : G->PublicLiveTargets(B)) if (bOrbGlow || !Cast<AZoogiOrb>(Tt)) AZoogiFX::Spawn(G->GetWorld(), Tt->GetActorLocation(), FLinearColor(1.f, 0.8f, 0.3f), 120.f, 0.4f, true, 6.f, Tt); // b128: no pulse on orbs''')
print('b128a ok')
