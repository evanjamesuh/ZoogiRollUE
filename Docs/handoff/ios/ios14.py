# Oct 29 build 14 (classic MAP SELECTION + map card art as a patch pak zoogiroll-ios_1_P.pak in the stage): upload changed sources + the pak, then ~/zr/build3.sh 14. Detached on the Mac.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
files = ['Source/ZoogiRoll/Private/ZoogiModeSelect.cpp', 'Source/ZoogiRoll/Public/ZoogiMenuRoot.h']
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
r = ssh('cat > ~/zr/stage/cookeddata/zoogiroll/content/paks/zoogiroll-ios_1_P.pak; ls -la ~/zr/stage/cookeddata/zoogiroll/content/paks/', open(r'C:\dev\ios\zoogiroll-ios_1_P.pak', 'rb').read())
out.append('PAK rc=%d %s %s' % (r.returncode, r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace')[:200]))
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 14 > ~/zr/build14.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS4 ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios14_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
