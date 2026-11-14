# Nov 10 b221 (Claude): re-shoot the previews (wider turret top view, item-KO test long enough) - no iOS
import subprocess, time
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '221v ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
subprocess.run([py, r"C:\dev\ios\p5run.py", "tr_turretopts2,b221_itemko2"])
hb('done')
