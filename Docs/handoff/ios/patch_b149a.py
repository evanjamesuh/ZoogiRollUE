# b149a: wire ZoogiRoyaleMix (BATTLE ROYALE = alternating classic / action rounds over rotating arenas, points per round)
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); n = n.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if b.count(o) != 1: o2 = o.replace(b'\r\n', b'\n'); n2 = n.replace(b'\r\n', b'\n'); assert b.count(o2) == 1, (p, o[:90]); o, n = o2, n2
    open(p, 'wb').write(b.replace(o, n))
GM = 'Private/ZoogiGameMode.cpp'
ed1(GM, '''	if (bRoyaleMode) { RoyaleStartMatch(); return; } // Battle Royale (ZoogiRoyale.cpp)''', '''	if (bRoyaleMode && !bRoyaleTraining && ZoogiRoyaleMix::Enabled() && !ZoogiRoyaleMix::Active()) { ZoogiRoyaleMix::Begin(this); return; } // b149 (Evan): BATTLE ROYALE = classic / action rounds
	if (bRoyaleMode) { RoyaleStartMatch(); return; } // Battle Royale (ZoogiRoyale.cpp)''')
ed1(GM, '''void AZoogiGameMode::FinishGame()
{''', '''void AZoogiGameMode::FinishGame()
{
	if (ZoogiRoyaleMix::OnClassicFinished(this)) return; // b149: a royale-mix classic round - the next round follows''')
ed1(GM, '''void AZoogiGameMode::ReturnToMenu()
{''', '''void AZoogiGameMode::ReturnToMenu()
{
	ZoogiRoyaleMix::OnMenu(this); // b149''')
ed1(GM, '#include "ZoogiBossAttack.h"', '#include "ZoogiBossAttack.h"\n#include "ZoogiRoyaleMix.h"')
RY = 'Private/ZoogiRoyale.cpp'
ed1(RY, '''void AZoogiGameMode::RoyaleFinish()
{''', '''void AZoogiGameMode::RoyaleFinish()
{
	if (ZoogiRoyaleMix::OnRoyaleFinished(this)) return; // b149: a royale-mix action round ends on its clock''')
ed1(RY, '''	if (!bRoyaleTraining) R.Lives = FMath::Max(0, R.Lives - 1); // training: endless''', '''	if (!bRoyaleTraining && !ZoogiRoyaleMix::ActionRound()) R.Lives = FMath::Max(0, R.Lives - 1); // training: endless; b149: a royale-mix action round too (falls cost points)''')
b = open(R + RY, 'rb').read()
inc = b'#include "ZoogiRoyale.h"'
assert b.count(inc) >= 1
i = b.index(inc); j = b.index(b'\n', i)
b = b[:j+1] + b'#include "ZoogiRoyaleMix.h"' + (b'\r\n' if b[j-1:j] == b'\r' else b'\n') + b[j+1:]
open(R + RY, 'wb').write(b)
AF = 'Private/ZoogiActionFun.cpp'
ed1(AF, '''	ZoogiSkyDiveCam::Tick(G, RealDt);''', '''	ZoogiSkyDiveCam::Tick(G, RealDt);
	ZoogiRoyaleMix::Tick(G, RealDt); // b149: the royale-mix action round clock''')
ed1(AF, '#include "ZoogiBossAttack.h"', '#include "ZoogiBossAttack.h"\n#include "ZoogiRoyaleMix.h"')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B148"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B149"), TEXT("BATTLE ROYALE: CLASSIC + ACTION ROUNDS"), TEXT("Battle Royale now alternates CLASSIC rounds (take turns, one shot each - the map boss attacks after the last shot) and ACTION rounds (drive, jump, transform and shoot for 45 seconds), and every round moves to the next arena. Points per round: classic rounds score like Classic, action rounds give + points per ring-out / destroy and - points per fall. The totals carry over and the highest total after the last round (always a classic round) wins.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o))
print('b149a ok')
