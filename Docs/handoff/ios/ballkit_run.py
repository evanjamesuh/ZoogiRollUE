# Oct 31 (Claude): headless import of the ball skins (T_Ball_*) + M_ZoogiBallSkin. Run DETACHED (it kills UnrealEditor.exe).
import subprocess, time, os
ST = 'C:/dev/ios/ballkit_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
status('KIT_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('Ball kit import (Claude, ZoogiRollUnreal-p5style)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\ballkit.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\Scripts\\royale\\make_ball_kit.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('KIT_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
