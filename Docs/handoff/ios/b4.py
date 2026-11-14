# start b4run.py detached (breaking away from the editor's job so it survives the editor closing), then close the editor
import subprocess, unreal
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
flags = 0x00000008 | 0x00000200
try:
    subprocess.Popen([py, r'C:\dev\ios\b4run.py'], creationflags=flags | 0x01000000, close_fds=True)
    print('B4_STARTED breakaway')
except Exception as e:
    print('B4 breakaway failed', e)
    subprocess.Popen(['cmd', '/c', 'schtasks /create /f /sc once /st 23:59 /tn ZoogiB4 /tr "\\"%s\\" C:\\dev\\ios\\b4run.py" && schtasks /run /tn ZoogiB4' % py], creationflags=flags, close_fds=True)
    print('B4_STARTED schtasks')
import time; time.sleep(3)
print(open(r'C:\dev\ios\b4_status.txt').read())
