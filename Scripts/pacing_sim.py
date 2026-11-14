#!/usr/bin/env python3
# Zoogi Roll pacing sim v2 (Oct 14, Evan Q5). Reads the SAME data the game loads (Content/Data/Progression/*.csv), so the
# proof and the shipped tuning cannot drift. Expected-value per match; NO purchases (real money never used).
# Target: skilled players own the 4 starter powers in 3-4 days, typical 5-7, casual progresses without grind.
# Usage: python3 pacing_sim.py [path/to/Content/Data/Progression]   (exit code 1 if a target is missed)
import csv, os, sys
D = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'Content', 'Data', 'Progression')
def rows(n):
    with open(os.path.join(D, n), newline='') as f: return list(csv.DictReader(l for l in f if not l.lstrip().startswith('#')))
ECO = {r['key']: r['value'] for r in rows('Economy.csv')}
CAP = int(ECO['daily_match_gem_cap']); DRN = int(ECO['dr_after_matches']); DRM = float(ECO['dr_mult'])
CURVE = [(int(r['level']), int(r['cumulative_xp']), int(r['level_gems'])) for r in rows('MasteryCurve.csv')]
REW = {(r['source'], r['tag']): r for r in rows('MatchRewards.csv')}
TIER = {r['tier']: r for r in rows('CPUTiers.csv')}
POW = [r for r in rows('Powers.csv') if r['tier'] == 'starter']
ACH = rows('Achievements.csv')
def lvl(xp): return max(l for l, c, _ in CURVE if xp >= c)
def rv(src, tag=''): r = REW[(src, tag)]; return int(r['gems']), int(r['xp']), int(r['per_match_cap'])
# Personas: expected counts per match. ev_focus = share of element-tagged XP (KO/ring-out/power/elemental tricks) that lands on
# the focus element (players lean on the element they are training).
P = {
 'casual':  dict(tier='Normal', mpd=3, win=0.40, ko=0.8, ro=1.0, tricks={'Trick.BankStrike':0.3,'Trick.Other':0.4}, power=1.0, orbs=1.5, episode=0.1, ev_focus=0.5),
 'typical': dict(tier='Normal', mpd=5, win=0.50, ko=1.5, ro=1.5, tricks={'Trick.BankStrike':0.6,'Trick.MomentumSteal':0.3,'Trick.Other':0.6}, power=1.5, orbs=2.0, episode=0.3, ev_focus=0.6),
 'skilled': dict(tier='Hard',   mpd=8, win=0.80, ko=3.0, ro=2.0, tricks={'Trick.BankStrike':1.2,'Trick.DoubleImpact':0.6,'Trick.MomentumSteal':0.6,'Trick.CrystalCollapse':0.5,'Trick.Other':0.8}, power=2.5, orbs=3.0, episode=0.8, ev_focus=0.65),
}
def match(p):
    """Expected (base gems, focus xp, event xp) for one match before the tier multiplier."""
    g = x = ex = 0.0
    for src, n in (('completion', 1), ('win', p['win']), ('ko', p['ko']), ('ring_out', p['ro']), ('power_used', p['power']), ('orb_collected', p['orbs']), ('episode', p['episode'])):
        gg, xx, cap = rv(src); n = min(n, cap); g += gg * n
        if REW[(src, '')]['xp_to'] == 'Focus': x += xx * n
        else: ex += xx * n
    for tag, n in p['tricks'].items():
        gg, xx, cap = rv('trick', tag); n = min(n, cap); g += gg * n
        if REW[('trick', tag)]['xp_to'] == 'Focus': x += xx * n
        else: ex += xx * n
    return g, x, ex
def run(name, buy=True, days=30):
    p = P[name]; t = TIER[p['tier']]; gm, xm = float(t['gem_mult']), float(t['xp_mult'])
    bg, bx, bex = match(p)
    gems = float(ECO['start_gems']); xp = {'Void': 0.0, 'Fire': 0.0, 'Shock': 0.0, 'Frost': 0.0}
    focus = 'Void'; owned = {}; stats = dict(matches_completed=0, wins=0, kos=0, tricks=0, episodes=0); got = set(); m = 0; snaps = []
    for day in range(1, days + 1):
        today = 0.0
        for k in range(p['mpd']):
            m += 1; dr = 1.0 if k < DRN else DRM
            before = {e: lvl(v) for e, v in xp.items()}
            pay = min(bg * gm * dr, max(0.0, CAP - today)); today += pay; gems += pay   # 500/day cap on repeatable match gems
            other = 'Fire' if focus != 'Fire' else 'Void'
            xp[focus] += (bx + bex * p['ev_focus']) * xm * dr; xp[other] += bex * (1 - p['ev_focus']) * xm * dr
            for e, v in xp.items():
                for L, c, lg in CURVE:
                    if before[e] < L <= lvl(v): gems += lg                      # mastery level-up gems (not capped)
            stats['matches_completed'] += 1; stats['wins'] += p['win']; stats['kos'] += p['ko']; stats['tricks'] += sum(p['tricks'].values()); stats['episodes'] += p['episode']
            st = dict(stats, max_mastery_level=max(lvl(v) for v in xp.values()), min_core_mastery_level=min(lvl(v) for v in xp.values()))
            for a in ACH:
                if a['id'] not in got and a['stat'] in st and st[a['stat']] >= int(a['target']): got.add(a['id']); gems += int(a['gems'])
            for pw in POW:
                n, el, req, cost = pw['name'], pw['element'], int(pw['mastery_level']), int(pw['gem_cost'])
                if n in owned: continue
                if lvl(xp[el]) >= req: owned[n] = (day, m, 'mastery'); continue
                if buy and gems >= cost: gems -= cost; owned[n] = (day, m, 'gems')
            # focus plan: train Void until Void reaches its last starter requirement, then Fire until Ember is owned
            vmax = max(int(x['mastery_level']) for x in POW if x['element'] == 'Void')
            focus = 'Fire' if (lvl(xp['Void']) >= vmax and 'Ember' not in owned) else 'Void'
        snaps.append((day, m, round(gems), lvl(xp['Void']), lvl(xp['Fire']), len(owned)))
    all_day = max(v[0] for v in owned.values()) if len(owned) == len(POW) else None
    return owned, all_day, snaps
TARGET = {'skilled': (3, 4), 'typical': (5, 7)}
ok = True
print('| Persona | Path | ' + ' | '.join(x['name'] for x in POW) + ' | All 4 starter powers | Target | Result |')
print('|---|---|' + '---|' * len(POW) + '---|---|---|')
for name in P:
    for buy in (True, False):
        owned, d, _ = run(name, buy)
        cells = ['day %d (m%d, %s)' % owned[x['name']] if x['name'] in owned else '>30 d' for x in POW]
        tgt = TARGET.get(name) if buy else None
        res = '-' if not tgt else ('PASS' if d and tgt[0] <= d <= tgt[1] else 'FAIL')
        if res == 'FAIL': ok = False
        print('| %s | %s | %s | %s | %s | %s |' % (name, 'mixed (earned gems + mastery)' if buy else 'mastery only', ' | '.join(cells), 'day %d' % d if d else '>30 d', '%d-%d days' % tgt if tgt else ('no grind' if name == 'casual' and buy else '-'), res))
print()
for name in P:
    _, _, s = run(name)
    print(name, 'end-of-day (day, matches, gems, Void L, Fire L, powers):', [x for x in s[:8]])
sys.exit(0 if ok else 1)
