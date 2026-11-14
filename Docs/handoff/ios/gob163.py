# Nov 6 b163 (Claude): SOUNDSCAPE (synth_ambience -> /Game/Audio/Sounds/Amb, ZoogiSoundscape) + LEAGUE TRAINING COURT (build_court ->
# /Game/Env/LeagueCourt, BuildLeagueCourt) -> tests -> TestFlight 163
import subprocess, time, os, glob
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '163 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
def st(n): p = r'C:\dev\ios\b%dios_status.txt' % n; return open(p).read() if os.path.exists(p) else ''
while 'SOURCES_UP' not in st(162) and 'DONE' not in st(162) and 'SKIPPED' not in st(162): time.sleep(15)
for p in ('patch_b163a.py', 'patch_b163b.py'):
    r = subprocess.run([py, r'C:\dev\ios\\' + p], capture_output=True, text=True); hb(p + ' ' + (r.stdout + r.stderr).strip()[-160:])
    if ' ok' not in r.stdout: hb('PATCH FAILED'); raise SystemExit
# the court art (Blender) while the Mac builds b162
B = sorted(glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe')); A = r'C:\dev\art\ancient_meadows\grok_design\blender\arena'
r = subprocess.run([B[-1], '-b', '--factory-startup', '--python', A + r'\build_court.py', '--', A], capture_output=True, text=True, timeout=3600)
open(r'C:\dev\ios\court_blender_stdout.txt', 'w').write(r.stdout[-20000:] + '\n---\n' + r.stderr[-8000:]); hb('court blender rc %d' % r.returncode)
while 'DONE' not in st(162) and 'SKIPPED' not in st(162): time.sleep(15)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('b163 soundscape + league court import (Claude)')
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style'
for nm, sc in (('amb', r'Scripts\import_amb_sfx.py'), ('court', r'Scripts\arenas\ancient\import_court.py')):
    with open(r'C:\dev\ios\%s163.log' % nm, 'w') as lf:
        rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\%s" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P, sc), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
    hb('%s UE_RC=%d' % (nm, rc))
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\p5run.py", "court_match,story_spar,boss_frozen,boss_volcano,front_land"])
s = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in s:
    open(r'C:\dev\ios\b163ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('tests done -> ios')
subprocess.run([py, r"C:\dev\ios\b163ios.py"])
