# b146b: wire ZoogiBossAttack (action ticker + brace slow-mo + the turn hold) + NEWS B146 + p5run boss_test
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''	ZoogiSkyDiveCam::Tick(G, RealDt);''', '''	ZoogiBossAttack::Tick(G, RealDt); // b146 (Evan): the Cosmos boss attacks in the closing rounds
	ZoogiSkyDiveCam::Tick(G, RealDt);''')
ed1(AF, '''			else if (ZoogiSkyDiveCam::SlowMo() > 0.f)''', '''			else if (ZoogiBossAttack::SlowMo() > 0.f) Want = ZoogiBossAttack::SlowMo(); // b146: TAP TO BRACE
			else if (ZoogiSkyDiveCam::SlowMo() > 0.f)''')
ed1(AF, '#include "ZoogiSkyDiveCam.h"', '#include "ZoogiSkyDiveCam.h"\n#include "ZoogiBossAttack.h"')
GM = 'Private/ZoogiGameMode.cpp'
ed1(GM, '''|| ClimaxLeft > 0.f || ZoogiEdgeHang::Active();''', '''|| ClimaxLeft > 0.f || ZoogiEdgeHang::Active() || ZoogiBossAttack::Hold(this); // b146: the boss attack lands first''')
ed1(GM, '''			const bool bBusy = (Anime && Anime->IsBusy()) || ZoogiEdgeHang::Active(); // b58: never resolve mid edge hang''', '''			const bool bBusy = (Anime && Anime->IsBusy()) || ZoogiEdgeHang::Active() || ZoogiBossAttack::Hold(this); // b58: never resolve mid edge hang; b146: nor before / during the boss attack''')
ed1(GM, '#include "ZoogiShowtime.h"', '#include "ZoogiShowtime.h"\n#include "ZoogiBossAttack.h"')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B145"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B146"), TEXT("THE COSMOS BOSS ATTACKS"), TEXT("In the closing rounds on Cosmos the boss wakes up. Each round it marks a red zone on the floor with a countdown - HAND SWEEP (a slice of the outer court) or GROUND SLAM (rings around a spot) - and it lands after everyone has had a turn. Roll out of the zone, hide behind a pillar or wall (cover stops the hand), GUARD to anchor against the slam, and when the hand is on you TAP to brace. It usually goes after the leader. Boss ring-outs score for nobody.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
p = os.path.expanduser('~/mnt/dev/ios/p5run.py'); s = open(p).read()
o = "            ('skydive_test',"
assert s.count(o) == 1
s = s.replace(o, "            ('boss_test', 1600, 900, 150, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=3'),\n" + o)
open(p, 'w').write(s)
print('b146b ok')
