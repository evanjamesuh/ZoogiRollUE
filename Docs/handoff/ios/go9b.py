import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
for s in (r'C:\dev\ios\ios7.py', r'C:\dev\ios\b9run.py'):
    subprocess.Popen([py, s], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GO9B_DONE')
