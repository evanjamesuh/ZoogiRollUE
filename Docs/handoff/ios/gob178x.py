# b178 (Claude): rebuild + intro tweaks, Stone Pillar x3, Rock Rampart along the swipe, arrowhead
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '178x ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
time.sleep(5)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "pk_stone_pillar,pk_rock_rampart,dragon_peek,item_test"])
hb('done')
