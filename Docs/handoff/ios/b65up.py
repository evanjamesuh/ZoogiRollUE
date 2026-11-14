# Nov 2 14:35 (Claude): build 65 retry from the upload step (the first ssh upload got "Connection closed" and hung).
import subprocess, time, os, sys, tarfile, filecmp
ST = 'C:/dev/ios/b65ios_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
status('RETRY_UP')
subprocess.run('taskkill /F /IM ssh.exe', shell=True, capture_output=True); time.sleep(5)
P5 = r'C:\dev\ZoogiRollUnreal-p5style'
CB = r'C:\dev\ZoogiRollUnreal-combined'
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=30', 'user294974@TX544.macincloud.com']
for att in range(4):
    r = subprocess.run(SSH + ['echo up'], capture_output=True, timeout=90)
    status('SSH_TEST %d rc=%d %s' % (att, r.returncode, (r.stdout + r.stderr).decode(errors='replace').strip()[:150]))
    if r.returncode == 0: break
    time.sleep(60)
for att in range(3):
    status('UPSTAGE_START try %d' % att)
    t0 = time.time()
    log = open(r'C:\dev\ios\upstage65.log', 'w')
    p = subprocess.Popen(SSH + ['rm -rf ~/zr/stage_new && mkdir -p ~/zr/stage_new && tar -xf - -C ~/zr/stage_new && rm -rf ~/zr/stage && mv ~/zr/stage_new ~/zr/stage && du -sh ~/zr/stage'], stdin=subprocess.PIPE, stdout=log, stderr=log)
    try:
        with tarfile.open(fileobj=p.stdin, mode='w|') as tf:
            tf.add(P5 + r'\Saved\StagedBuilds\IOS', arcname='.')
        p.stdin.close()
    except Exception as e:
        status('UPSTAGE_EXC %r' % e)
        try: p.kill()
        except Exception: pass
    urc = p.wait()
    log.write('\nrc=%d %.0fs\n' % (urc, time.time() - t0)); log.close()
    status('UPSTAGE_RC=%d %.0fs' % (urc, time.time() - t0))
    if urc == 0: break
    time.sleep(60)
if urc != 0:
    status('DONE'); raise SystemExit
MAC = '$HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
r = subprocess.run(SSH + ['rm -rf "$HOME/~"; cd "$TMPDIR" && rm -rf DTAppAnalyzerExtractorOutput-*.zip XcodeDistPipeline.* *.xcdistributionlogs; df -h /System/Volumes/Data | tail -1'], capture_output=True, timeout=120)
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
open(r'C:\dev\ios\ios65_files.txt', 'w').write('\n'.join(out))
status('SOURCES_UP %d' % len(files))
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = subprocess.run(SSH + ['cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 65 > ~/zr/build65.log 2>&1 < /dev/null & echo started'], input=b3, capture_output=True, timeout=120)
status('IOS65 ' + r.stdout.decode(errors='replace').strip() + r.stderr.decode(errors='replace').strip())
for k in range(40):
    time.sleep(64)
    try: r = subprocess.run(SSH + ['tail -c 3000 ~/zr/build65.log; echo; echo ===ERR; grep -n "error:\|UBT_RC" ~/zr/build65.log | head -40; ls -la $HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Binaries/IOS/ZoogiRoll-IOS-Shipping'], capture_output=True, timeout=120)
    except Exception as e: status('POLL_EXC %r' % e); continue
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
    open(r'C:\dev\ios\tail65.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS65_FINISHED'); break
try:
    UE = r'C:\Program Files\Epic Games\UE_5.6'
    0 and subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', CB + r'\ZoogiRoll.uproject'], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e: status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')
