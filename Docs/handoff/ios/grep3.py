import os
base = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Programs\UnrealBuildTool'
out = []
def dump(rel, key, before, after):
    L = open(os.path.join(base, rel), encoding='utf-8', errors='replace').read().split('\n')
    for i, l in enumerate(L):
        if key in l:
            out.append('### %s:%d' % (rel, i)); out.extend(L[max(0, i-before):i+after])
dump(r'Platform\IOS\UEBuildIOS.cs', 'List<string> Certs = AppleCodeSign.FindCertificates();', 3, 30)
dump(r'ToolChain\AppleToolChain.cs', 'CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY=', 40, 4)
dump(r'Platform\IOS\IOSCodeSigning.cs', 'CodeSigningConfig.CodeSigningIdentity = CodeSigningConfig.bForDistribution', 30, 10)
open(r'C:\dev\ios\grep3.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('GREP3', len(out))
