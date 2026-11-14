# Nov 11 (Claude): Evan "Clear space" - measure what is big under C:\dev and C:\Games (no deleting here)
import os, shutil
def size(p):
    t = 0
    for r, ds, fs in os.walk(p):
        for f in fs:
            try: t += os.path.getsize(os.path.join(r, f))
            except Exception: pass
    return t
out = []
du = shutil.disk_usage('C:\\'); out.append('C: free %.1f GB of %.1f GB' % (du.free / 1e9, du.total / 1e9))
rows = []
for base in (r'C:\dev', r'C:\Games'):
    if not os.path.isdir(base): continue
    for n in os.listdir(base):
        p = os.path.join(base, n)
        if os.path.isdir(p): rows.append((size(p), p))
        else:
            try: rows.append((os.path.getsize(p), p))
            except Exception: pass
P5 = r'C:\dev\ZoogiRollUnreal-p5style'
for sub in ('Saved\\Cooked', 'Saved\\StagedBuilds', 'Saved\\Logs', 'Intermediate', 'DerivedDataCache', 'Binaries', 'Content'):
    p = os.path.join(P5, sub); rows.append((size(p), p + '  [inside p5style]'))
for d in os.listdir(os.path.join(P5, 'Saved', 'Cooked')) if os.path.isdir(os.path.join(P5, 'Saved', 'Cooked')) else []:
    p = os.path.join(P5, 'Saved', 'Cooked', d); rows.append((size(p), p + '  [cooked]'))
rows.sort(reverse=True)
for s, p in rows[:60]: out.append('%8.1f GB  %s' % (s / 1e9, p))
open(r'C:\dev\ios\sizes.txt', 'w').write('\n'.join(out))
