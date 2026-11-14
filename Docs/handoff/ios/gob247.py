# Nov 11 b247 (Claude): Voyager auto cover + new Voyager card art; then the win demo
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '247 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(2):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b241_voy,b247_cover"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(60)
if not ok:
    open(r'C:\dev\ios\b247ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b247_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b246ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b247ios.py"])
hb('ios done -> win demo')
subprocess.run([py, r"C:\dev\ios\p5run.py", "win_demo2"])
hb('done')
