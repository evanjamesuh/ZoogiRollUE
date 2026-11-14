import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'grep -n "UBT_RC\\|OcclusionFade\\|ZoogiActionFun\\|\\] Compile\\|Link" ~/zr/build24.log | head; echo ---; grep -n "ZoogiActionFun\\|UBT_RC" ~/zr/build23.log | head -5'], capture_output=True, timeout=60)
open(r'C:\dev\ios\chk24.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
print('CHK24_OK')
