# Nov 10 b218 (Claude): rigged Meadow golem gets a proper stone material (B217 rendered it near-black)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '218 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
LOCK = r'C:\dev\unreal.lock'
while os.path.exists(LOCK): time.sleep(15)
open(LOCK, 'w').write('b218 golem rig material (Claude)')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(5)
hb('rig mat')
with open(r'C:\dev\ios\golem_rig_mat_run.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s" -run=pythonscript -script="C:\\dev\\ios\\golem_rig_mat.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('rig mat rc=%d' % rc)
try: os.remove(LOCK)
except Exception: pass
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b218_meadow"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b218ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b218_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b217ios_status.txt').read(): time.sleep(30)  # B217's Mac build still uses ~/zr/stage
hb('217 done -> 218 ios')
subprocess.run([py, r"C:\dev\ios\b218ios.py"])
hb('done')
