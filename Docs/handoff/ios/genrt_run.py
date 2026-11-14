# Oct 31 (Claude): headless GASP -> crystal hero retarget (Scripts\royale\retarget_actions.py). Run detached.
import subprocess, time, os
ST = 'C:/dev/ios/genrt_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
status('KIT_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('generic hero action retarget (Claude, ZoogiRollUnreal-p5style)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\genrt.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s\\ZoogiRoll.uproject" -ExecutePythonScript="%s\\Scripts\\royale\\retarget_actions.py" -nop4 -nosplash -stdout -FullStdOutLogOutput -log' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('KIT_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
