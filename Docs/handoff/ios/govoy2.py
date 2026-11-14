# Nov 11 (Claude): voyager port step 2 PC proof - permanent plugins/config, SHOOTER cards -> Voyager levels; NO iOS build
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + 'voy2 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "voy_wave,voy_ai,voy_city,b239_front,b238_live"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\voy2_status.txt', 'w').write('BUILD FAILED\n'); hb('BUILD FAILED'); raise SystemExit
open(r'C:\dev\ios\voy2_status.txt', 'w').write('DONE\n'); hb('done')
