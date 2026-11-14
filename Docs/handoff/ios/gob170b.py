# Nov 6 b170b (Claude): verify portrait-scaled chest shows + story 2v2 fights on classic before TestFlight 166
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '170b ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "modes_land,modes_port"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
hb('done - check the shots, then b166_cards_ok')
