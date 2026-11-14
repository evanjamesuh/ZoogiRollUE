import os
root = r'C:\Program Files\Epic Games\UE_5.6\Engine'
pats = ['XcodeProjectSettings', 'YourCompany', 'SigningIdentity', 'CodeSigningPrefix', 'bUseAutomaticCodeSigning', 'CODE_SIGN_STYLE', 'PROVISIONING_PROFILE']
out = []
for sub in [r'Source\Programs\UnrealBuildTool', r'Config']:
    for dp, dn, fn in os.walk(os.path.join(root, sub)):
        for f in fn:
            if not (f.endswith('.cs') or f.endswith('.ini')): continue
            p = os.path.join(dp, f)
            try: L = open(p, encoding='utf-8', errors='replace').read().split('\n')
            except Exception: continue
            for i, l in enumerate(L):
                if any(x in l for x in pats):
                    out.append('%s:%d: %s' % (p.replace(root, ''), i, l.strip()[:220]))
open(r'C:\dev\ios\grep2.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('GREP2', len(out))
