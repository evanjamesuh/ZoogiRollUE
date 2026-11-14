import subprocess
try:
    r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', 'user294974@TX544.macincloud.com', 'tail -c 1500 ~/zr/build32.log; ps aux | grep -c "[x]codebuild"'], capture_output=True, timeout=150)
    out = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
except Exception as e:
    out = 'EXC %r' % e
open(r'C:\dev\ios\chk32.txt', 'w').write(out)
