# Oct 30 build 28 (stylized clash panel + ultimate avatars; stage re-uploaded by b28ios.py): upload changed sources, then ~/zr/build3.sh 23 (stage fixed in build 22).
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
# build 29: every source file changed since the build 26 upload (ios26_out.txt) - build 28's list missed the launch-pad / forming files
import os
_since = os.path.getmtime(r'C:\dev\ios\ios26_out.txt')
files = []
for _d in ('Public', 'Private'):
    for _f in os.listdir(ROOT + '\\Source\\ZoogiRoll\\' + _d):
        _p = ROOT + '\\Source\\ZoogiRoll\\' + _d + '\\' + _f
        if os.path.isfile(_p) and os.path.getmtime(_p) > _since:
            files.append('Source/ZoogiRoll/%s/%s' % (_d, _f))
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
r = ssh('grep -c "" %s/Binaries/IOS/../../Intermediate/Remote/ZoogiRoll/IOS/Shipping/Manifest.xml 2>/dev/null; cat ~/zr/stage/ZoogiRoll/uecommandline.txt 2>/dev/null | head -c 200' % MAC)
out.append('CHECK ' + r.stdout.decode(errors='replace'))
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 29 > ~/zr/build29.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios29_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
