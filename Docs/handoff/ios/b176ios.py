# Nov 5 (Claude): TestFlight build 176 = TRAINING pause menu CPU OPPONENTS (0-3), SHOWTIME full-screen cutscene hook (Anime_Showtime_<Power>_01).
# Code + Content -> cook + stage.
# Same flow as b30ios.py, with the $HOME Mac path (ios31 fix), the $TMPDIR clean-up (ios31) and a poll that survives ssh timeouts (ios33).
# 1) seed p5style Saved\Cooked\IOS from the combined copy (same assets) so the cook can iterate
# 2) RunUAT BuildCookRun IOS Shipping -skipbuild -cook -iterate -stage -pak on the p5style project (new Data/Fonts go in the pak)
# 3) stream the stage to the Mac (~/zr/stage)  4) upload every p5style source file that differs from the combined copy into the
#    Mac mirror (list -> ios30_files.txt; restore those from combined before the next combined build)  5) build3.sh 30 + poll.
import subprocess, time, os, sys, tarfile, filecmp
# guard (Claude): a second launcher (double click) must not run this twice
import os as _os
_m = r'C:\dev\ios\b176ios.started'
if _os.path.exists(_m): raise SystemExit
open(_m, 'w').write('started')
ST = 'C:/dev/ios/b176ios_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
status('RUNNER_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('iOS cook + stage for build 176 (Claude, ZoogiRollUnreal-p5style)')
P5 = r'C:\dev\ZoogiRollUnreal-p5style'
CB = r'C:\dev\ZoogiRollUnreal-combined'
if not os.path.isdir(P5 + r'\Saved\Cooked\IOS'):
    rc = subprocess.run(r'robocopy "%s\Saved\Cooked\IOS" "%s\Saved\Cooked\IOS" /E /NFL /NDL /NJH /NJS /NP' % (CB, P5), shell=True).returncode
    status('SEED_COOKED_RC=%d' % rc)
if not os.path.exists(P5 + r'\Binaries\IOS\ZoogiRoll-IOS-Shipping.target'):  # staging wants the iOS receipt (the Mac builds the real binary)
    rc = subprocess.run(r'robocopy "%s\Binaries\IOS" "%s\Binaries\IOS" ZoogiRoll-IOS-Shipping ZoogiRoll-IOS-Shipping.target ZoogiRoll-IOS-Shipping.stub /NFL /NDL /NJH /NJS /NP' % (CB, P5), shell=True).returncode
    status('SEED_BIN_RC=%d' % rc)
uat = r'"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat"'
args = r' BuildCookRun -project="%s\ZoogiRoll.uproject" -noP4 -platform=IOS -clientconfig=Shipping -distribution -skipbuild -cook -iterate -stage -pak -unattended -utf8output' % P5
status('COOK_START')
with open(r'C:\dev\ios\cook176.log', 'w') as lf:
    rc = subprocess.run(uat + args, shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('COOK_RC=%d' % rc)
open(P5 + r'\Saved\StagedBuilds\IOS\uecommandline.txt', 'w', newline='').write('-project="../../../ZoogiRoll/ZoogiRoll.uproject"')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
if rc != 0:
    status('DONE'); raise SystemExit
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=30', 'user294974@TX544.macincloud.com']
status('UPSTAGE_START')
t0 = time.time()
log = open(r'C:\dev\ios\upstage34b.log', 'w')
p = subprocess.Popen(SSH + ['rm -rf ~/zr/stage_new && mkdir -p ~/zr/stage_new && tar -xf - -C ~/zr/stage_new && rm -rf ~/zr/stage && mv ~/zr/stage_new ~/zr/stage && du -sh ~/zr/stage'],
                     stdin=subprocess.PIPE, stdout=log, stderr=log)
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
    data = open(os.path.join(P5, rel), 'rb').read()
    for attempt in range(4):
        try:
            r = subprocess.run(SSH + ['mkdir -p "%s/%s" && cat > "%s/%s" && wc -c < "%s/%s"' % (MAC, d, MAC, rel.replace('\\', '/'), MAC, rel.replace('\\', '/'))], input=data, capture_output=True, timeout=120)
            if r.returncode == 0 and r.stdout.decode(errors='replace').strip() == str(len(data)): break
        except Exception as e: pass
        time.sleep(3)
    out.append('%s rc=%d %s %s' % (rel, r.returncode, r.stdout.decode(errors='replace').strip(), r.stderr.decode(errors='replace')[:200]))
open(r'C:\dev\ios\ios176_files.txt', 'w').write('\n'.join(out))
status('SOURCES_UP %d' % len(files))
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = subprocess.run(SSH + ['cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 176 > ~/zr/build176.log 2>&1 < /dev/null & echo started'], input=b3, capture_output=True, timeout=120)
status('IOS176 ' + r.stdout.decode(errors='replace').strip() + r.stderr.decode(errors='replace').strip())
for k in range(40):
    time.sleep(64)
    try: r = subprocess.run(SSH + ['tail -c 3000 ~/zr/build176.log; echo; echo ===ERR; grep -n "error:\|UBT_RC" ~/zr/build176.log | head -40; ls -la $HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Binaries/IOS/ZoogiRoll-IOS-Shipping'], capture_output=True, timeout=120)
    except Exception as e: status('POLL_EXC %r' % e); continue
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
    open(r'C:\dev\ios\tail176.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS176_FINISHED'); break
try:
    UE = r'C:\Program Files\Epic Games\UE_5.6'
    0 and subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', CB + r'\ZoogiRoll.uproject'], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e: status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')
