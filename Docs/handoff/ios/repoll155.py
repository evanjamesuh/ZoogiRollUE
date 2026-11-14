# Nov 6 08:05 (Claude): b155ios.py's poller stopped updating at 07:30 (tail155 empty; likely an ssh pipe hang). Re-poll the Mac build
# with file output (no pipes), then mark b155 DONE so gob156 proceeds.
import subprocess, time, os
ST = 'C:/dev/ios/b155ios_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=2', 'user294974@TX544.macincloud.com']
status('REPOLL_START')
for k in range(60):
    try:
        with open(r'C:\dev\ios\tail155.txt', 'w') as f:
            subprocess.run(SSH + ['tail -c 3000 ~/zr/build155.log; echo; echo ===ERR; grep -n "error:\\|UBT_RC\\|EXPORT_UPLOAD_RC" ~/zr/build155.log | head -40'], stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, timeout=90)
    except Exception as e: status('REPOLL_EXC %r' % e)
    t = open(r'C:\dev\ios\tail155.txt', errors='replace').read()
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS155_FINISHED (repoll)'); break
    time.sleep(45)
if 'DONE' not in open(ST).read(): status('DONE')
