import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.Popen([py, r'C:\dev\ios\p5run.py', 'crit_land,crit_port'], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GOP5M_DONE')
