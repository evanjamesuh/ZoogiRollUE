# Oct 29 night build 24: build 23 + see-through wall/stand fade removed (ZoogiOcclusionFade default off). Waits for build 23 on the Mac first.
import subprocess, time
ST = 'C:/dev/ios/b24_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
def ssh(cmd, data=None, t=120):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=t)
for k in range(40):
    t = ssh('tail -c 3000 ~/zr/build23.log').stdout.decode(errors='replace')
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or 'UBT_RC=' in t and 'UBT_RC=0' not in t: break
    time.sleep(60)
open('C:/dev/ios/tail23.txt', 'w').write(t)
status('B23_DONE ' + ('RC0' if 'EXPORT_UPLOAD_RC=0' in t else 'CHECK'))
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
for f in ['Source/ZoogiRoll/Private/ZoogiOcclusionFade.cpp']:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    status('%s rc=%d' % (f, r.returncode))
r = ssh('nohup ~/zr/build3.sh 24 > ~/zr/build24.log 2>&1 < /dev/null & echo started')
status('IOS24 ' + r.stdout.decode(errors='replace').strip())
for k in range(40):
    time.sleep(60)
    t = ssh('tail -c 4000 ~/zr/build24.log; echo; grep -n "error" ~/zr/build24.log | head -30').stdout.decode(errors='replace')
    open('C:/dev/ios/tail24.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or 'ERROR: ' in t: break
status('IOS24_END')
