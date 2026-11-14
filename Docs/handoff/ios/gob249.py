# Nov 11 b249 (Claude): crystal Zoogi Voyager heroes
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '249 ' + s + '\n')
hb('start')
while 'DONE' not in (open(r'C:\dev\ios\voyhero_status.txt').read() if os.path.exists(r'C:\dev\ios\voyhero_status.txt') else ''): time.sleep(20)  # the hero retargeters first
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(2):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b249_hero"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(60)
if not ok:
    open(r'C:\dev\ios\b249ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\b249_cards_ok', 'w').write('auto')
hb('-> ios')
while 'DONE' not in open(r'C:\dev\ios\b248ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b249ios.py"])
hb('done')
