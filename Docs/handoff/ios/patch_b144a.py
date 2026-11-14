# b144a (Evan 13:19 "Grenades model never generated or launched when item selected and fired"): the thrown grenade was wiped on the
# next tick - the grenade ticker re-syncs to the match (Sync() clears every drop / lob) the first time it sees a new match, and an ITEM
# grenade thrown before any orb-generator drop that match (round 1, or a saved grenade) was that first time. Lob() now syncs first.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
G = 'Private/ZoogiGrenadeItem.cpp'
ed1(G, '''		if (!G || !B) return;
		Ensure();
		const FVector From = B->BallLocation() + FVector(0.f, 0.f, 60.f);''', '''		if (!G || !B) return;
		Ensure();
		Sync(G); // b144a: sync to this match NOW - the first tick's Sync() used to wipe this very throw
		const FVector From = B->BallLocation() + FVector(0.f, 0.f, 60.f);''')
print('b144a ok')
