# Oct 27 build 9 (free cam, no-delay transform/jump, no phase tracker/crystals, classic SUPERPOWER/REACTION, strategic zoom, launch on release): upload the changed sources to the Mac mirror, then ~/zr/build3.sh 4 (Mac UBT + fin.sh sign/upload). Detached on the Mac.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
files = ['Source/ZoogiRoll/Private/ZoogiArenaHUDWidget.cpp', 'Source/ZoogiRoll/Private/ZoogiDevMode.cpp', 'Source/ZoogiRoll/Private/ZoogiGameMode.cpp', 'Source/ZoogiRoll/Private/ZoogiHUD.cpp', 'Source/ZoogiRoll/Private/ZoogiMorph.cpp', 'Source/ZoogiRoll/Private/ZoogiPlayerController.cpp', 'Source/ZoogiRoll/Private/ZoogiPowers.cpp', 'Source/ZoogiRoll/Private/ZoogiRoyale.cpp', 'Source/ZoogiRoll/Private/ZoogiRoyaleHUD.cpp', 'Source/ZoogiRoll/Private/ZoogiTargeting.cpp', 'Source/ZoogiRoll/Public/ZoogiDevMode.h', 'Source/ZoogiRoll/Public/ZoogiGameMode.h', 'Source/ZoogiRoll/Public/ZoogiHUD.h', 'Source/ZoogiRoll/Public/ZoogiRoyale.h']
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 9 > ~/zr/build9.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS4 ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios9_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
