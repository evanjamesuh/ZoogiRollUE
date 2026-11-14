# Run C:/dev/ios/cmd.sh on the MacinCloud Mac over SSH (Unreal remote-build key); output -> C:/dev/ios/out.txt
import subprocess, os, shutil, time
K = 'C:/dev/k_t.key'
cmd = open('C:/dev/ios/cmd.sh', encoding='utf-8').read().replace('\r', '')
t = time.time()
TIMEOUT = globals().get('MAC_TIMEOUT', 170)
try:
    r = subprocess.run(['ssh', '-i', K, '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=20',
                        'user294974@TX544.macincloud.com', 'bash -s'], input=cmd.encode('utf-8'), capture_output=True, timeout=TIMEOUT)
    out = 'RC=%d\n%s\n--stderr--\n%s' % (r.returncode, r.stdout.decode('utf-8','replace'), r.stderr.decode('utf-8','replace'))
except Exception as e:
    out = 'EXC %r' % e
open('C:/dev/ios/out.txt', 'w', encoding='utf-8').write(out + '\n--took %.1fs--\n' % (time.time() - t))
print('MAC_DONE', len(out))
