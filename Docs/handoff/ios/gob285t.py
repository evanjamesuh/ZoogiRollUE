# Nov 13 (Claude): B285 fall-test preview only (PC), after iOS 285 is done
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b285t.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '285t ' + s + '\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
hb('start - waiting for iOS 285')
while not os.path.exists(r'C:\dev\ios\b285ios_status.txt') or 'DONE' not in open(r'C:\dev\ios\b285ios_status.txt').read(): time.sleep(20)
subprocess.run([py, r"C:\dev\ios\p5run.py", "b285_fall"])
hb('preview done')
