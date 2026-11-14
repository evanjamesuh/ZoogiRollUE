import os, re
root = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Programs\UnrealBuildTool'
out = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        if f.endswith('.cs'):
            p = os.path.join(dp, f)
            try: L = open(p, encoding='utf-8', errors='replace').read().split('\n')
            except Exception: continue
            for i, l in enumerate(L):
                if 'Provision not specified' in l or 'class IOSProvisioningData' in l:
                    out.append('### %s:%d' % (p, i))
                    out += L[max(0, i-10):i+90]
open(r'C:\dev\ios\ubt_prov.txt', 'w', encoding='utf-8').write('\n'.join(out) if out else 'NOTFOUND ' + str(os.path.exists(root)))
print('GREP_DONE', len(out))
