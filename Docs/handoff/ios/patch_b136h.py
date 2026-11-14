# b136h (Evan 09:04: "the old original skills that don't describe how to use them need revisions to say how to use them, and if
# they can not be used grey out and lock them"): plain how-to rules for the old cards (Cards.csv); BANK STRIKE, ORB RELAY and
# RECOVERY ROLL have no way to be played in classic yet (no bank / partner / safe-zone pick) - they show LOCKED and grey in the
# SKILLS tab and in the SKILL list. Golden Apple's text follows its b134 behaviour.
import os, csv, io
ROOT = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
R = ROOT + 'Source/ZoogiRoll/'
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
NEW = {
 'storm': 'Cast before you launch: this roll steers sharper - your swipes and REDIRECTs bend its path more.',
 'guard': 'Cast on your turn: the next rival hit on you is softened (12 less then x0.75) - you get pushed much less, once.',
 'heavy_roll': 'Cast before you launch: this roll hits 25% harder, but your aim wobbles a little more.',
 'ember': 'Cast before you launch: rivals you hit this roll fly 20% further - it also melts any Frost lane you cross.',
 'titan': 'Cast before you launch: your Zoogi is 35% heavier this turn - you push harder and get pushed 20% less.',
 'frost': 'Cast before you launch: a slippery ice lane forms along your path - everything slides further on it (one lane at a time).',
 'momentum_steal': 'Cast before you launch: clip a rival at an angle and you steal up to a quarter of its speed (that hit does 60% damage).',
 'crystal_collapse': 'Cast before you launch: the next breakable arena piece you hit cracks one stage further (pillars never break).',
 'orb_magnet': 'Lights up when an orb is close to you - cast it and that orb is pulled toward your Zoogi for a moment.',
 'double_impact': 'Cast before you launch: your next hit bursts twice - a second, half-strength blast right after the first.',
 'anchor_guard': 'Cast on your turn: once your Zoogi stops it anchors - the next rival hit pushes you only 30% as far.',
 'bank_strike': 'LOCKED - not playable in classic yet. (Mark a wall or bumper to bank off for a bonus.)',
 'orb_relay': 'LOCKED - not playable in classic yet. (2v2: pass an orb to your partner for a combo.)',
 'recovery_roll': 'LOCKED - not playable in classic yet. (Brake hard near a safe zone you choose.)',
 'golden_apple': 'Tap an orb inside your arc range: the Apple of Eden forms on you, a golden apple is lobbed there and a big blast where it lands throws everything (that orb too) away.',
}
p = ROOT + 'Content/Data/Cards.csv'; raw = open(p, 'rb').read(); nl = b'\r\n' if b'\r\n' in raw else b'\n'
lines = raw.decode('utf-8').split(nl.decode()); out = []; done = set()
for ln in lines:
    if ln and not ln.startswith('#'):
        parts = ln.split(',', 11)
        if len(parts) == 12 and parts[0] in NEW:
            parts[11] = NEW[parts[0]]; done.add(parts[0]); ln = ','.join(parts)
    out.append(ln)
assert len(done) == len(NEW), set(NEW) - done
open(p, 'wb').write(nl.join(o.encode('utf-8') for o in out))
# lock list
F = 'Private/ZoogiFront.cpp'
ed1(F, '''				FItem I; I.Kind = 3; I.Name = C.Name.ToUpper(); I.CardId = C.Id.ToString(); I.Desc = C.Rule; I.bOwned = P->IsCardOwned(C.Id); I.Level = P->CardLevelOf(C.Id);''',
'''				FItem I; I.Kind = 3; I.Name = C.Name.ToUpper(); I.CardId = C.Id.ToString(); I.Desc = C.Rule; I.bOwned = P->IsCardOwned(C.Id); I.Level = P->CardLevelOf(C.Id);
				if (ZoogiCardLockedInClassic(C.Id)) { I.bOwned = false; I.Price = 0; } // b136 (Evan): no way to play it yet - grey + LOCKED''')
ed1(F, '''	static UTexture2D* ZBallIcon(const FString& Ball)''', '''	/** b136 (Evan): old cards that have no way to be played in classic yet */
	static bool ZoogiCardLockedInClassic(FName Id) { return Id == FName(TEXT("bank_strike")) || Id == FName(TEXT("orb_relay")) || Id == FName(TEXT("recovery_roll")); }
	static UTexture2D* ZBallIcon(const FString& Ball)''')
P5 = 'Private/ZoogiP5Command.cpp'
ed1(P5, '''				R.bOn = G->CardsSlotPlayableNow(k) && (!D || D->Cost <= PS->Energy);''', '''				R.bOn = G->CardsSlotPlayableNow(k) && (!D || D->Cost <= PS->Energy);
				{	// b136 (Evan): cards with no way to be played in classic are greyed and locked
					const FName Ck = PS->Hand[k].CardId;
					if (Ck == FName(TEXT("bank_strike")) || Ck == FName(TEXT("orb_relay")) || Ck == FName(TEXT("recovery_roll"))) { R.bOn = false; R.Name += TEXT("  - LOCKED"); }
				}''')
print('b136h ok')
