import subprocess
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=20', 'user294974@TX544.macincloud.com']
try:
    r = subprocess.run(SSH + ['tail -c 2500 ~/zr/build61.log; echo; echo ===ERR; grep -n "error:\\|UBT_RC\\|EXPORT_UPLOAD_RC" ~/zr/build61.log | tail -10; ps aux | grep -c xcodebuild'], capture_output=True, timeout=90)
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
except Exception as e:
    t = 'EXC %r' % e
open(r'C:\dev\ios\chk61.txt', 'w').write(t)
