# Oct 29 (Claude): build 15 - screen shatter, clone shockwave, thrust portals on every arch, Dev Mode obstacles + bumpers.
# Win64 editor build + capture runs. Detached. status -> C:/dev/ios/b15_status.txt, frames -> C:/dev/proofs/build15_oct29/<run>/
import subprocess, time, os, sys
ST = 'C:/dev/ios/b15_status.txt'
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
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + build 15 capture runs (Claude, ZoogiRollUnreal-combined)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'
rc = 0
if not SKIPBUILD:
    status('BUILD_START')
    with open('C:/dev/ios/b15_build.log', 'w') as lf:
        rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
    status('BUILD_RC=%d' % rc)
if rc == 0:
    shots = r'C:\dev\proofs\build15_oct29'
    os.makedirs(shots, exist_ok=True)
    runs = (('fx_frozen', 1600, 900, 72, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiScreenFxTest -ZoogiPortalShots=%s'),
            ('devobs', 1600, 900, 70, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiDev -ZoogiDevObsTest'),
            ('fx_meadow', 1600, 900, 40, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiScreenFxTest'),
            ('fx_portrait', 470, 1000, 70, '-ZoogiAuto -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiScreenFxTest'),
            ('combo', 1600, 900, 35, '-ZoogiAuto -ZoogiCombo -ZoogiPlayers=4'))
    for runname, rx, ry, secs, extra in runs:
        if ONLY and runname not in ONLY.split(','): continue
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        if '%s' in extra: extra = extra % d
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=%d -ResY=%d -WinX=200 -WinY=40 -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\b15_%s.log -nosplash' % (UE, P, rx, ry, d, extra, runname)
        status('RUN_' + runname)
        pr = subprocess.Popen(args, shell=True)
        time.sleep(secs)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
if rc == 0 and len(sys.argv) > 3 and sys.argv[3] == 'ios':
    try: os.remove('C:/dev/unreal.lock')
    except Exception: pass
    status('IOS15_START')
    r = subprocess.run([sys.executable, r'C:\dev\ios\ios15.py'], capture_output=True, text=True, timeout=900)
    status('IOS15_RC=%d' % r.returncode)
    for k in range(40):
        time.sleep(90)
        subprocess.run([sys.executable, r'C:\dev\ios\tail15.py'], capture_output=True, timeout=120)
        t = open(r'C:\dev\ios\tail4.txt', errors='replace').read()
        if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or 'ERROR: ' in t:
            status('IOS15_FINISHED'); break
    status('IOS15_POLL_END')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
