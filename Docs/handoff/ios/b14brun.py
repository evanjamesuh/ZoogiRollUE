# Oct 29 (Claude): build 14 (classic MAP SELECTION) + arena showcase captures for the map cards - Win64 editor build + capture runs (portrait menu, classic landscape / portrait, 2v2). Detached.
# status -> C:/dev/ios/b14b_status.txt, build log -> C:/dev/ios/b14b_build.log, frames -> C:/dev/proofs/build12_oct28/<run>/
import subprocess, time, os
ST = 'C:/dev/ios/b14b_status.txt'
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
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + build 12 capture runs (Claude, ZoogiRollUnreal-combined)')
status('BUILD_START')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'
with open('C:/dev/ios/b14b_build.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('BUILD_RC=%d' % rc)
ONLY = 'menu,menu_p'
if rc == 0:
    shots = r'C:\dev\proofs\build14b_oct29'
    os.makedirs(shots, exist_ok=True)
    sc = '-ZoogiAuto -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=3'
    runs = (('menu', 1600, 900, 50, ''), ('menu_p', 470, 1000, 50, ''))
    for runname, rx, ry, secs, extra in runs:
        if ONLY and runname not in ONLY.split(','): continue
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=%d -ResY=%d -WinX=200 -WinY=40 -ZoogiRoyaleFrames -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\b14b_%s.log -nosplash' % (UE, P, rx, ry, d, extra, runname)
        status('RUN_' + runname)
        pr = subprocess.Popen(args, shell=True)
        time.sleep(secs)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
