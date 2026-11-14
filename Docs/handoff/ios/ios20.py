# Oct 29 build 20 (Frozen fun pack + action cams + impact frames + fireworks; staged content re-uploaded by b20ios.py): upload changed sources, then ~/zr/build3.sh 20.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
files = ['Source/ZoogiRoll/Public/ZoogiFrozenFun.h', 'Source/ZoogiRoll/Private/ZoogiFrozenFun.cpp', 'Source/ZoogiRoll/Public/ZoogiArenaPiece.h', 'Source/ZoogiRoll/Public/ZoogiArena.h', 'Source/ZoogiRoll/Private/ZoogiArenaFrozen.cpp', 'Source/ZoogiRoll/Private/ZoogiArenaProps.cpp', 'Source/ZoogiRoll/Public/ZoogiOrb.h', 'Source/ZoogiRoll/Private/ZoogiOrb.cpp', 'Source/ZoogiRoll/Private/ZoogiGameMode.cpp', 'Source/ZoogiRoll/Private/ZoogiPowers.cpp', 'Source/ZoogiRoll/Private/ZoogiAnimeSubsystem.cpp', 'Source/ZoogiRoll/Private/ZoogiPlayerController.cpp', 'Source/ZoogiRoll/Public/ZoogiPlayerController.h', 'Config/DefaultGame.ini']
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
# Oct 29 build 20: app icon catalog for fin.sh (actool on the Mac)
XC = ROOT + r'\Build\IOS\Resources\Assets.xcassets'
ssh('mkdir -p ~/zr/AppIcon.xcassets/AppIcon.appiconset')
for rel in ['Contents.json', 'AppIcon.appiconset\\Contents.json', 'AppIcon.appiconset\\Icon1024.png']:
    r = ssh('cat > ~/zr/AppIcon.xcassets/%s' % rel.replace('\\', '/'), open(XC + '\\' + rel, 'rb').read())
    out.append('icon %s rc=%d' % (rel, r.returncode))
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 20 > ~/zr/build20.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios20_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
