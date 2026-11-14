# Oct 29 (Claude): build 21 failed on launch ("Failed to open descriptor file") - the -iterate cook staged uecommandline.txt with
# "-iterative" (loose-file deploy). Rewrite it on the Mac stage and re-sign / upload as build 22 (fin.sh only, same binary).
import subprocess, time
def ssh(cmd, data=None, t=900):
    r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=t)
    return r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
out = []
out.append(ssh('cat ~/zr/stage/uecommandline.txt; echo; printf %s \'-project="../../../ZoogiRoll/ZoogiRoll.uproject"\' > ~/zr/stage/uecommandline.txt; echo NOW:; cat ~/zr/stage/uecommandline.txt'))
out.append(ssh('nohup ~/zr/fin.sh 22 > ~/zr/fin22.log 2>&1 < /dev/null & echo started'))
open(r'C:\dev\ios\fix22_out.txt', 'w').write('\n'.join(out))
for k in range(30):
    time.sleep(30)
    t = ssh('tail -c 2500 ~/zr/fin22.log', t=60)
    open(r'C:\dev\ios\fix22_tail.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'rror' in t[-600:]:
        break
open(r'C:\dev\ios\fix22_out.txt', 'a').write('\nFIX22_DONE\n')
