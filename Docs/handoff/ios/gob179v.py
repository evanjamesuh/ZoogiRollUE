# b179 (Claude): import the synthesized dragon roar, then build + test (skills C, bounty, birds, golem slash, frozen raid, dragon)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '179v ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
time.sleep(5)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "front_port,front_land,meadow_v3,item_test"])
hb('done')
