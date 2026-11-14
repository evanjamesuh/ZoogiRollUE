import subprocess
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
import os
try: os.close(os.open(r'C:\dev\ios\b213e.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit  # double-click = 2 launches
subprocess.Popen([py, r"C:\dev\ios\gob213d.py"], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
