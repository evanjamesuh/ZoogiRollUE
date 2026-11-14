# Nov 6 (Claude): fresh Ancient Collision Meadows shots for the map art (Evan 12:43 "Update map image"); Win64 only, no iOS
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'DONE' not in (open(r'C:\dev\ios\b160ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b160ios_status.txt') else 'DONE'): time.sleep(15)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "anc_shots,pmap_ancient"])
