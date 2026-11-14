# Nov 7 b172a (Claude): chest slots by state, marble finder, arena weather, under-court rescue - Win64 build + captures
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '172a ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "front_iphone,weather_meadow,weather_frozen,weather_volc"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
hb('done - check the shots')
