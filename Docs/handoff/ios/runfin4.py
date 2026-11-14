# re-run only the Mac sign + upload for build 4 (after Evan signs back in to Xcode > Settings > Accounts on the Mac)
import subprocess
key = open(r'C:\dev\ios\keys\AuthKey_VY8Z87BCDX.p8', 'rb').read()
subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'cat > ~/zr/AuthKey_VY8Z87BCDX.p8; chmod 600 ~/zr/AuthKey_VY8Z87BCDX.p8'], input=key, timeout=60)
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh; nohup ~/zr/fin.sh 4 > ~/zr/fin4.log 2>&1 < /dev/null & echo started'], input=fin, capture_output=True, timeout=60)
print('FIN4', r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace'))
