# Oct 29 build 17 (holographic spire with Lilita text atlas; build 16 was not uploaded): upload changed sources, then ~/zr/build3.sh 17.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
files = ['Source/ZoogiRoll/Public/ZoogiHoloSpire.h', 'Source/ZoogiRoll/Private/ZoogiHoloSpire.cpp', 'Source/ZoogiRoll/Private/ZoogiHoloFontData.inl', 'Source/ZoogiRoll/Private/ZoogiCardSubsystem.cpp']
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 17 > ~/zr/build17.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios17_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
