# Oct 31 (Claude): one-off look at the Mac build log (build 46)
import subprocess
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=20', 'user294974@TX544.macincloud.com']
try:
    r = subprocess.run(SSH + ['tail -c 2500 ~/zr/build47.log; echo; echo ===; grep -n "error:\\|UBT_RC\\|EXPORT_UPLOAD_RC" ~/zr/build47.log | tail -20; ps aux | grep -c build3'], capture_output=True, timeout=90)
    out = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
except Exception as e: out = 'EXC %r' % e
open(r'C:\dev\ios\macpoll.txt', 'w').write(out)
