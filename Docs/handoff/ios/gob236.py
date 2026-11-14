# Nov 10 b236 (Claude): no spider / crew / cine soldiers; Tripo heads (CPU access step first); crystals + ball dash in the shooter modes
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '236 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(10)
open(r'C:\dev\unreal.lock', 'w').write('b236 head_cpu.py (Claude)')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
with open(r'C:\dev\ios\head_cpu.log', 'w') as lf:
    subprocess.run(r'"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject" -run=pythonscript -script="C:\dev\ios\voy\head_cpu.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput', shell=True, stdout=lf, stderr=subprocess.STDOUT)
try: os.remove(r'C:\dev\unreal.lock')
except Exception: pass
hb('head_cpu done')
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b236_htl,b236_tdm"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b236ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b236_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b235ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b236ios.py"])
hb('done')
