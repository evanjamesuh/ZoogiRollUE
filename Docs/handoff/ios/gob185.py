# Nov 8 b185 (Claude): theme music from game launch + themes load whole (ForceInline) for the phone
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '185 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
with open(r'C:\dev\ios\themes_inline.log', 'w') as lf:
    rc = subprocess.run(r'"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject" -run=pythonscript -script="C:\dev\ZoogiRollUnreal-p5style\Scripts\themes_inline.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput', shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('themes inline rc %d' % rc)
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "gattling_test"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b185ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for the dev check (b185_cards_ok)')
while not os.path.exists(r'C:\dev\ios\b185_cards_ok'): time.sleep(15)
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b185ios.py"])
