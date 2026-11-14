# Oct 29 build 16 (holographic spire): upload changed sources, probe the hologram material in the cooked data, then ~/zr/build3.sh 16.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
files = ['Source/ZoogiRoll/Public/ZoogiHoloSpire.h', 'Source/ZoogiRoll/Private/ZoogiHoloSpire.cpp', 'Source/ZoogiRoll/Private/ZoogiCardSubsystem.cpp']
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
r = ssh('U=~/zr/stage/cookeddata/zoogiroll/content/paks/zoogiroll-ios.utoc; echo ZAFX_ADD=$(grep -a -c M_ZAFX_Add $U) ZAFX_ANY=$(grep -a -c ZAFX $U) WOLF=$(grep -a -c -i wolfgang $U) MENUART=$(grep -a -c -i menuart ~/zr/stage/Manifest_UFSFiles_IOS.txt)')
out.append('PROBE ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')[:200])
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 16 > ~/zr/build16.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios16_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
