# Nov 6 (Claude): b158 iOS stage hit "ssh: Connection timed out" to the cloud Mac - retry the cook/stage/upload (no Win64 rebuild)
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + '158 retry ios\n')
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
try: os.remove(r'C:\dev\ios\b158ios.started')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\b158ios.py"])
