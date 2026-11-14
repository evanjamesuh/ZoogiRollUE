# Oct 27 (Claude): Win64 editor build + royale capture run for the Marble Clash camera / HUD pass. Detached.
# status -> C:/dev/ios/b4_status.txt, build log -> C:/dev/ios/b10_build.log, frames -> C:/dev/proofs/royale_oct27/
import subprocess, time, os
ST = 'C:/dev/ios/b10_status.txt'
def status(s):
    open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
def editor_running():
    out = subprocess.run('tasklist /FI "IMAGENAME eq UnrealEditor.exe"', shell=True, capture_output=True, text=True).stdout
    return 'UnrealEditor.exe' in out
t0 = time.time()
status('RUNNER_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); subprocess.run('taskkill /F /IM CrashReportClientEditor.exe', shell=True, capture_output=True); time.sleep(3)
while editor_running() and time.time() - t0 < 900:
    time.sleep(5)
    if int(time.time() - t0) % 30 < 5: status('WAIT_EDITOR')
if editor_running():
    status('EDITOR_STILL_OPEN'); raise SystemExit
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + royale capture (Claude, ZoogiRollUnreal-combined)')
status('BUILD_START')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'
with open('C:/dev/ios/b10_build.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('BUILD_RC=%d' % rc)
if rc == 0:
    shots = r'C:\dev\proofs\build10_oct27'
    os.makedirs(shots, exist_ok=True)
    for runname, extra in (('frozen', '-ZoogiArena=1 -ZoogiFrozenRoamTest -ZoogiClassicButtonsTest -ZoogiFrozenKnockTest'), ('devfrozen', '-ZoogiArena=1 -ZoogiDev -ZoogiDevAuto -ZoogiDevAutoAt=8')):
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=1600 -ResY=738 -ZoogiAuto -ZoogiHumanTest -ZoogiRoyaleFrames -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\b10_%s.log -nosplash' % (UE, P, d, extra, runname)
        status('RUN_' + runname)
        pr = subprocess.Popen(args, shell=True)
        time.sleep(95)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
