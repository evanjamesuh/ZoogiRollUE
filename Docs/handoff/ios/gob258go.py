# Nov 11 (Claude): B258 = B254 + MetaHuman samples never cooked (B254 cook failed on Face_AnimBP). Editor already built -> straight to iOS.
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b258go.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
open(r'C:\dev\ios\gob120_hb.txt', 'a').write(time.strftime('%H:%M:%S ') + '258 start\n')
subprocess.Popen([py, r"C:\dev\ios\b258ios.py"], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
