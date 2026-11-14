# Nov 10 b220 (Claude): item knockouts (staff, sword, Gatling, grenade, disc ...) award +2 crystals
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '220 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b220_itemko"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b220ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b220_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b219ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b220ios.py"])
hb('done')
