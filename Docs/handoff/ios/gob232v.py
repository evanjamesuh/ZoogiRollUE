# Nov 10 b232v (Claude): re-film the wall cine options (preview only) after B232 upstage
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '232v ' + s + '\n')
hb('start')
ST = r'C:\dev\ios\b232ios_status.txt'
while not (os.path.exists(ST) and 'UPSTAGE' in open(ST).read()): time.sleep(20)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
subprocess.run([py, r"C:\dev\ios\p5run.py", "b232_cine2"])
hb('done')
