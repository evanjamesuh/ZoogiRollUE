# b178 (Claude): import Meadow v3 + Shadow Realm v3, then Win64 build + captures
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '178v ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('Meadow v3 + Shadow Realm import (Claude, ZoogiRollUnreal-p5style)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\arena_v3_import.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\Scripts\\arenas\\meadow\\import_meadow_v3.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('import rc %d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\p5run.py", "meadow_shots,underrealm,meadow_v3,skill_lab"])
hb('done')
