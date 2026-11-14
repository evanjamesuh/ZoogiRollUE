# Nov 10 (Claude): the rock-impact blast options (demo rig) after the Awaken re-shoot
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '215b ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while not os.path.exists(r'C:\dev\ios\aw_footage_done'): time.sleep(10)
time.sleep(5)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(10)
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "tr_blastopts"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
open(r'C:\dev\ios\blastopts_done', 'w').write('ok')
hb('done')
