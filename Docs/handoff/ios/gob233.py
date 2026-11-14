# Nov 10 b233 (Claude): Voyager TPS in Zoogi Roll - A cine fighters, B royale hero, C turret FX + HMG, D tomb spider + raid crew
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '233 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b233_cine,b233_royale,b233_raid"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b233ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b233_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b232ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b233ios.py"])
hb('done')
