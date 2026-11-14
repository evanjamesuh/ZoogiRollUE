# Oct 27: local verify of the TRAINING sandbox (b5run.py waits for the editor to close, builds, captures)
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.Popen([py, r'C:\dev\ios\b5run.py'], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GO5_DONE')
