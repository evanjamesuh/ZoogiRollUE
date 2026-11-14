# Oct 30 (Claude): build 29 (clash glare, cut-in forms, P5R REACTION button). Was build 28 (stylized clash meter / banners + ultimate avatars). Was build 27 - power clash fist fight, AI transforms, launch without aim, REACTION tap button, clash on the scoreboard. (was: build 26 - action / fun / timing pack (early turn end, snappy CPU, short cine hold, cut-in throttle,
# impact timing tap, parry, boosts, on fire, final rounds x2, round replay) + wide action cams. Win64 editor build + capture runs,
# then (arg 3 = ios) ios26 + tail polling. status -> C:/dev/ios/b29_status.txt, frames -> C:/dev/proofs/build29_oct30/<run>/
import subprocess, time, os, sys
ST = 'C:/dev/ios/b29_status.txt'
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
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + build 29 capture runs (Claude, ZoogiRollUnreal-combined)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'
rc = 0
if not SKIPBUILD:
    status('BUILD_START')
    with open('C:/dev/ios/b29_build.log', 'w') as lf:
        rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
    status('BUILD_RC=%d' % rc)
if rc == 0:
    shots = r'C:\dev\proofs\build29_oct30'
    os.makedirs(shots, exist_ok=True)
    common = '-ZoogiAuto -ZoogiNoScreenShatter'
    runs = (('clash_arabian', 1600, 900, 80, common + ' -ZoogiHumanTest -ZoogiFunTest -ZoogiClashTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('cutin_meadow', 1600, 900, 75, common + ' -ZoogiHumanTest -ZoogiFunTest -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2'))
    for runname, rx, ry, secs, extra in runs:
        if ONLY and runname not in ONLY.split(','): continue
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=%d -ResY=%d -WinX=200 -WinY=40 -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\b29_%s.log -nosplash' % (UE, P, rx, ry, d, extra, runname)
        status('RUN_' + runname)
        pr = subprocess.Popen(args, shell=True)
        time.sleep(secs)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
if rc == 0 and len(sys.argv) > 3 and sys.argv[3] == 'ios':
    status('IOS29_START')
    r = subprocess.run([sys.executable, r'C:\dev\ios\ios29.py'], capture_output=True, text=True, timeout=900)
    status('IOS29_RC=%d' % r.returncode)
try:
    subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', P], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e:
    status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')
