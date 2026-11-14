p = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Programs\UnrealBuildTool\ToolChain\RemoteMac.cs'
L = open(p, encoding='utf-8', errors='replace').read().split('\n')
out = []
for i, l in enumerate(L):
    if 'TempDir =' in l or 'TempDir=' in l or 'DirectoryReference TempDir' in l:
        out.append('### %d' % i); out += L[max(0, i-8):i+8]
open(r'C:\dev\ios\rm2.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('RM2_DONE', len(out))
