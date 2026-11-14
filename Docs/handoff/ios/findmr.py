import os, glob
roots = [os.environ.get('LOCALAPPDATA',''), os.environ.get('APPDATA',''), r'C:\ProgramData', r'C:\Program Files\Epic Games\UE_5.6\Engine\Intermediate', r'C:\dev\ZoogiRollUnreal-combined\Intermediate']
hits = []
for r in roots:
    for p in glob.glob(os.path.join(r, '**', 'MarketplaceRules.dll'), recursive=True):
        hits.append('%s %d %s' % (p, os.path.getsize(p), os.path.getmtime(p)))
mk = glob.glob(r'C:\Program Files\Epic Games\UE_5.6\Engine\Plugins\Marketplace\*')
open(r'C:\dev\ios\findmr.txt', 'w').write('\n'.join(hits) + '\n--marketplace plugins--\n' + '\n'.join(mk))
print('FINDMR', len(hits))
