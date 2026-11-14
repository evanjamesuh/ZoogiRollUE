import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'tail -c 4000 ~/zr/build27.log; echo; grep -n "error" ~/zr/build27.log | head -30'], capture_output=True, timeout=60)
open(r'C:\dev\ios\tail27.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
print('TAIL27_OK')
