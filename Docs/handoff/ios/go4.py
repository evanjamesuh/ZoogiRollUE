# Oct 27: send build 4 to the Mac (ios4.py), then start the local verify runner (b4run.py, waits for the editor to close)
import subprocess
exec(open(r'C:\dev\ios\ios4.py').read())
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.Popen([py, r'C:\dev\ios\b4run.py'], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GO4_DONE')
