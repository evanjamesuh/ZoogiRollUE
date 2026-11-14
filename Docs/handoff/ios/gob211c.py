# Nov 9 b202 (Claude): clean home background capture - no iOS
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '211c ' + s + '\n')
try: os.close(os.open(r'C:\dev\ios\b211c.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "b211_buddy,b211_first"])
hb('done')
