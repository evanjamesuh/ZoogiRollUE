# Oct 30 (Claude): build 31 = the P5 branch for real. Build 30's source upload quoted the Mac path ("~/...") so the files landed in a
# literal ~/~ folder and the Mac rebuilt nothing (build 30 = build 29 code + the P5 stage). The P5 stage is already on the Mac.
import subprocess, os, filecmp, time
ST = 'C:/dev/ios/ios31_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com']
P5 = r'C:\dev\ZoogiRollUnreal-p5style'
CB = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '$HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
r = subprocess.run(SSH + ['rm -rf "$HOME/~"; echo cleaned'], capture_output=True, timeout=120)
status('CLEAN ' + r.stdout.decode(errors='replace').strip())
files = []
for root, dirs, fs in os.walk(P5 + r'\Source'):
    for f in fs:
        pp = os.path.join(root, f)
        rel = os.path.relpath(pp, P5)
        cp = os.path.join(CB, rel)
        if not os.path.exists(cp) or not filecmp.cmp(pp, cp, shallow=False): files.append(rel)
out = []
for rel in files:
    d = os.path.dirname(rel).replace('\\', '/')
    r = subprocess.run(SSH + ['mkdir -p "%s/%s" && cat > "%s/%s" && ls -la "%s/%s"' % (MAC, d, MAC, rel.replace('\\', '/'), MAC, rel.replace('\\', '/'))], input=open(os.path.join(P5, rel), 'rb').read(), capture_output=True, timeout=120)
    out.append('%s rc=%d %s %s' % (rel, r.returncode, r.stdout.decode(errors='replace').strip(), r.stderr.decode(errors='replace')[:200]))
open(r'C:\dev\ios\ios31_files.txt', 'w').write('\n'.join(out))
status('SOURCES_UP %d' % len(files))
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = subprocess.run(SSH + ['cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 31 > ~/zr/build31.log 2>&1 < /dev/null & echo started'], input=b3, capture_output=True, timeout=120)
status('IOS31 ' + r.stdout.decode(errors='replace').strip() + r.stderr.decode(errors='replace').strip())
for k in range(40):
    time.sleep(60)
    r = subprocess.run(SSH + ['tail -c 3000 ~/zr/build31.log; echo; echo ===ERR; grep -n "error:\|UBT_RC" ~/zr/build31.log | head -40; ls -la $HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Binaries/IOS/ZoogiRoll-IOS-Shipping'], capture_output=True, timeout=120)
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
    open(r'C:\dev\ios\tail31.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS31_FINISHED'); break
status('DONE')
