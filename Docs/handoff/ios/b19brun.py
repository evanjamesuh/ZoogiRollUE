# Oct 29 (Claude): build 19 - big COMBO badge back (classic + portrait) + cinematic aim drag follow / push-in. Win64 editor build + capture runs, then (arg 3 = ios) ios19 + tail polling.
# status -> C:/dev/ios/b19b_status.txt, frames -> C:/dev/proofs/build19_oct29/<run>/
import subprocess, time, os, sys
ST = 'C:/dev/ios/b19b_status.txt'
def status(s):
    open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
def editor_running():
    out = subprocess.run('tasklist /FI "IMAGENAME eq UnrealEditor.exe"', shell=True, capture_output=True, text=True).stdout
    return 'UnrealEditor.exe' in out
ONLY = sys.argv[1] if len(sys.argv) > 1 else ''
SKIPBUILD = len(sys.argv) > 2 and sys.argv[2] == 'nobuild'
t0 = time.time()
status('RUNNER_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); subprocess.run('taskkill /F /IM CrashReportClientEditor.exe', shell=True, capture_output=True); time.sleep(3)
while editor_running() and time.time() - t0 < 900:
    time.sleep(5)
if editor_running():
    status('EDITOR_STILL_OPEN'); raise SystemExit
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + build 19 capture runs (Claude, ZoogiRollUnreal-combined)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'
rc = 0
if not SKIPBUILD:
    status('BUILD_START')
    with open('C:/dev/ios/b19b_build.log', 'w') as lf:
        rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
    status('BUILD_RC=%d' % rc)
if rc == 0:
    shots = r'C:\dev\proofs\build19b_oct29'
    os.makedirs(shots, exist_ok=True)
    common = '-ZoogiAuto -ZoogiHumanTest -ZoogiClassicButtonsTest -ZoogiSpireShots -ZoogiNoScreenShatter'
    runs = (('cine_land', 1600, 900, 50, common + ' -ZoogiCineDragTest -ZoogiArena=4 -ZoogiPlayers=2'),
            ('cine_port', 470, 1000, 50, common + ' -ZoogiCineDragTest -ZoogiArena=1 -ZoogiPlayers=2'))
    for runname, rx, ry, secs, extra in runs:
        if ONLY and runname not in ONLY.split(','): continue
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=%d -ResY=%d -WinX=200 -WinY=40 -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\b19b_%s.log -nosplash' % (UE, P, rx, ry, d, extra, runname)
        status('RUN_' + runname)
        pr = subprocess.Popen(args, shell=True)
        time.sleep(secs)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
    subprocess.run([sys.executable, r'C:\dev\ios\probe16.py'], capture_output=True, timeout=120)
    status('PROBE_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
if rc == 0 and len(sys.argv) > 3 and sys.argv[3] == 'ios':
    status('IOS19_START')
    r = subprocess.run([sys.executable, r'C:\dev\ios\ios19.py'], capture_output=True, text=True, timeout=900)
    status('IOS19_RC=%d' % r.returncode)
    for k in range(40):
        time.sleep(90)
        subprocess.run([sys.executable, r'C:\dev\ios\tail19.py'], capture_output=True, timeout=120)
        t = open(r'C:\dev\ios\tail19.txt', errors='replace').read()
        if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or 'ERROR: ' in t:
            status('IOS19_FINISHED'); break
    status('IOS19_POLL_END')
status('DONE')
