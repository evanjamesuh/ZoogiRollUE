# Nov 9 b208 (Claude): Tripo Ballers import (Blue, Frosti, Lumi, Vex, Hotstreak) for the 3D view
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '208 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r'C:\dev\ios\p5run.py', 'none'])  # closes the editor + builds first
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(5)
open(r'C:\dev\unreal.lock','w').write('ballers import (Claude)')
subprocess.run([py, r'C:\dev\ios\import_ballers_run.py'])
try: os.remove(r'C:\dev\unreal.lock')
except Exception: pass
hb('ballers import ' + open(r'C:\dev\ios\import_ballers_rc.txt').read().strip())
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b208_ballers"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b208ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for the dev check (b208_cards_ok)')
while not os.path.exists(r'C:\dev\ios\b208_cards_ok'): time.sleep(15)
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b208ios.py"])
