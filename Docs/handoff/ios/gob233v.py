# Nov 10 b233v (Claude): Voyager royale hero preview (pure royale)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '233v ' + s + '\n')
hb('start')
ST = r'C:\dev\ios\b233ios_status.txt'
while not (os.path.exists(ST) and 'UPSTAGE' in open(ST).read()): time.sleep(20)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
subprocess.run([py, r"C:\dev\ios\p5run.py", "b233_royale2"])
hb('done')
