# b138a: the card art studio (test only, -ZoogiCardArtShoot) - ZoogiCardArtStudio.h/.cpp, armed at match start
import os, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
S0 = os.path.expanduser('~/mnt/dev/ios/b138/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
shutil.copy(S0 + 'ZoogiCardArtStudio.h', R + 'Public/ZoogiCardArtStudio.h')
shutil.copy(S0 + 'ZoogiCardArtStudio.cpp', R + 'Private/ZoogiCardArtStudio.cpp')
SS = 'Private/ZoogiStageShift.cpp'
ed1(SS, '#include "ZoogiCosmosBoss.h"\n', '#include "ZoogiCosmosBoss.h"\n#include "ZoogiCardArtStudio.h"\n')
ed1(SS, '''		ZoogiCosmosBoss::OnMatchStart(G); // b136 (Evan): the boss watching over the cosmos arena''', '''		ZoogiCosmosBoss::OnMatchStart(G); // b136 (Evan): the boss watching over the cosmos arena
		ZoogiCardArtStudio::OnMatchStart(G); // b138: test-only card art captures (-ZoogiCardArtShoot)''')
print('b138a ok')
