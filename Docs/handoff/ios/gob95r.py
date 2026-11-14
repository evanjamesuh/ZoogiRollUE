# Nov 3 (Claude): retry b95 iOS (the upload to the Mac failed once: UPSTAGE OSError 22)
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
open(r'C:\dev\ios\gob95r_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start\n')
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\b95ios.py"])
