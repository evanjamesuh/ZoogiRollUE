# b132a (b131 proof cosmic_markers/30_grenade_drop: the top-band corner banner ran under the centre turn / ROUND header):
# in landscape the top band stops short of the centre header.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
ed1('Private/ZoogiActionFun.cpp', '''	const float MaxLen = Band == 1 ? (bPort ? Wd * 0.70f : Wd * 0.42f) : (bPort ? Wd * 0.94f : Wd * 0.48f); // b116: the bottom band stays clear of SUPERPOWER''',
'''	const float MaxLen = Band == 1 ? (bPort ? Wd * 0.70f : Wd * 0.42f) : (bPort ? Wd * 0.94f : FMath::Min(Wd * 0.48f, Wd * 0.5f - 150.f * U)); // b116: the bottom band stays clear of SUPERPOWER; b132: the top band stops short of the centre ROUND header''')
print('b132a ok')
