import os
p = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Programs\UnrealBuildTool\ToolChain\RemoteMac.cs'
L = open(p, encoding='utf-8', errors='replace').read().split('\n')
out = []
for i, l in enumerate(L):
    if 'ExportCertificate' in l or 'MobileProvision' in l or 'bAutomaticSigning' in l or 'IPhonePackager' in l:
        out.append('### %d' % i); out += L[max(0, i-25):i+25]
open(r'C:\dev\ios\rm.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('RM_DONE', len(out))
