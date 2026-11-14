# Nov 10 b216 (Claude): Meadow golem original colour + whole body on the rim
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '216 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
LOCK = r'C:\dev\unreal.lock'
while os.path.exists(LOCK): time.sleep(15)
open(LOCK, 'w').write('b214 Tripo boss import (Claude)')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(5)
hb('texture')
with open(r'C:\dev\ios\golem_tex_run.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s" -run=pythonscript -script="C:\\dev\\ios\\golem_tex.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('texture rc=%d' % rc)
try: os.remove(LOCK)
except Exception: pass
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b216_meadow"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b216ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b216_cards_ok', 'w').write('auto')
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b216ios.py"])
hb('done')
