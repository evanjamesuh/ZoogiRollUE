# Nov 11 (Claude): after B248 hands the PC back: build the Voyager -> crystal hero retargeters + copy the engine headers I need.
import subprocess, time, os, shutil, glob
ST = 'C:/dev/ios/voyhero_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
if not os.environ.get('VH_CHILD'):
    try: os.close(os.open(r'C:\dev\ios\voyhero.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    except FileExistsError: raise SystemExit
    e = dict(os.environ); e['VH_CHILD'] = '1'
    subprocess.Popen([r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe", __file__], env=e, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
    raise SystemExit
open(ST, 'w').write('')
status('UP')
UE = r'C:\Program Files\Epic Games\UE_5.6'
os.makedirs(r'C:\dev\ios\hdr', exist_ok=True)
for pat in (r'\Engine\Plugins\Animation\IKRig\Source\IKRig\Public\**\AnimNode_RetargetPoseFromMesh.h', r'\Engine\Plugins\Animation\IKRig\Source\IKRig\Public\**\IKRetargeter.h',
            r'\Engine\Plugins\Animation\IKRig\Source\IKRig\Public\**\IKRetargetProcessor.h', r'\Engine\Source\Runtime\Engine\Public\Animation\AnimInstanceProxy.h',
            r'\Engine\Plugins\Animation\IKRig\Source\IKRig\IKRig.Build.cs', r'\Engine\Plugins\Animation\IKRig\IKRig.uplugin'):
    for f in glob.glob(UE + pat, recursive=True): shutil.copy(f, r'C:\dev\ios\hdr'); status('HDR ' + f)
H = r'C:\dev\ios\gob120_hb.txt'
while '248 done' not in open(H).read() and 'SKIPPED' not in (open(r'C:\dev\ios\b248ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b248ios_status.txt') else ''): time.sleep(20)
status('PC_FREE')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('Voyager hero retargeters (Claude)')
P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\voyhero.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s\\ZoogiRoll.uproject" -ExecutePythonScript="%s\\Scripts\\voyager\\voy_hero_rtg.py" -nop4 -nosplash -stdout -FullStdOutLogOutput -log' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT, timeout=1500).returncode
status('UE_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')
