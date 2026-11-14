# Nov 13 (Claude): re-upload 286 after Evan accepted the Apple agreement (iOS only, no preview)
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b286u.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '286u ' + s + '\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
hb('re-upload start')
try: os.replace(r'C:\dev\ios\b286ios_status.txt', r'C:\dev\ios\b286ios_status_try1.txt')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\b286ios.py"])
hb('ios done')
