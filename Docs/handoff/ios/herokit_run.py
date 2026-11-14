# Oct 31 (Claude): hero-build prep: Blender check + mannequin FBX export. Run detached.
import subprocess, time, os, glob
ST = 'C:/dev/ios/herokit_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
status('UP')
cands = []  # checked: Blender 5.0.1
glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe')
status('BLENDER ' + repr(cands))
if cands:
    r = subprocess.run([cands[-1], '--version'], capture_output=True, text=True, timeout=120)
    status('BLENDER_VERSION ' + r.stdout.strip().replace('\n', ' | ')[:200])
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(3)
open('C:/dev/unreal.lock', 'w').write('Mannequin FBX export (Claude)')
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\herokit.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s\\ZoogiRoll.uproject" -ExecutePythonScript="%s\\Scripts\\royale\\export_mannequin.py" -nop4 -nosplash -stdout -FullStdOutLogOutput' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('UE_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
