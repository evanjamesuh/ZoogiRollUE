# Nov 6 (Claude): EP1 practice field capture (story m1_spar on the Training arena) while gob161 waits for the map cards
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'waiting for map cards' not in open(r'C:\dev\ios\gob120_hb.txt').read().split('161 start')[-1]: time.sleep(15)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "story_spar"])
open(r'C:\dev\ios\gobspar_done.txt', 'w').write(time.strftime('%H:%M:%S') + ' done\n')
