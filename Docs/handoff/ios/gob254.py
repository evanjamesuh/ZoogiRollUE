# Nov 11 b254 (Claude): Voyager back without the MetaHuman plugins
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '254 ' + s + '\n')
hb('start')
# Evan's phone first: stop the tablet (Android) packaging that holds the Unreal lock
for im in ('AutomationTool.exe', 'UnrealEditor-Cmd.exe', 'UnrealPak.exe', 'ShaderCompileWorker.exe'):
    subprocess.run('taskkill /F /T /IM ' + im, shell=True, capture_output=True)
time.sleep(10)
for k in range(30):
    if not os.path.exists('C:/dev/unreal.lock'): break
    if 'android' in open('C:/dev/unreal.lock').read(): os.remove('C:/dev/unreal.lock'); break
    time.sleep(10)
hb('android stopped (tablet test 2)')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(2):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "voy_wave"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(60)
if not ok:
    open(r'C:\dev\ios\b254ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b254_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b249ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b254ios.py"])
hb('done')
