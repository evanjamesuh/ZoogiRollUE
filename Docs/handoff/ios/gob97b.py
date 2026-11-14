# Nov 3 b97 (Claude): the compile + captures passed (19:06 BUILD_RC=0) but the chained iOS step never started - run it directly
import subprocess, time
open(r'C:\dev\ios\gob97_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'gob97b -> b97ios\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
subprocess.run([py, r"C:\dev\ios\b97ios.py"])
