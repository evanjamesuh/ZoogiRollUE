# Nov 10 (Claude): re-shoot the Awaken trailer clips (the new SHOWTIME Awaken, filmed from its start, with audio)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '215t ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock') or not os.path.exists(r'C:\dev\ios\trailer_footage_done'): time.sleep(15)
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "tr_rockblast,tr_rockframes,tr_aw_blue,tr_aw_vex,tr_aw_frosti,tr_aw_hotstreak,tr_aw_lumi"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
open(r'C:\dev\ios\aw_footage_done', 'w').write('ok')
hb('done')
