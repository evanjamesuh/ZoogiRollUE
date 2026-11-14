# b157 (Claude): Blender (headless) -> SM_Arena_PlazaPropsTrick.fbx, then the UE import. Called by gob157 (sources up, no lock).
import subprocess, time, os, glob
ST = 'C:/dev/ios/trickprops_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
B = sorted(glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe'))
status('BLENDER %r' % B)
if not B: status('NO_BLENDER'); raise SystemExit
r = subprocess.run([B[-1], '-b', '--factory-startup', '--python', r'C:\dev\ZoogiRollUnreal-p5style\Scripts\arenas\ancient\make_trick_props.py'], capture_output=True, text=True, timeout=900)
open(r'C:\dev\ios\trickprops_blender_stdout.txt', 'w').write(r.stdout[-6000:] + '\n---\n' + r.stderr[-4000:])
status('BLENDER_RC=%d' % r.returncode)
if not os.path.exists(r'C:\dev\art\ancient_meadows\grok_design\blender\arena\pieces_v14_trick\SM_Arena_PlazaPropsTrick.fbx'): status('NO_FBX'); raise SystemExit
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('Trick shot plaza props import (Claude, ZoogiRollUnreal-p5style)')
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\trickprops_import.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\Scripts\\arenas\\ancient\\import_trick_props.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('UE_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
