# Nov 2 b67 (Claude): wait for the running capture pass to finish, then editor build + TestFlight build 67
import subprocess, time
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
t0 = time.time()
while time.time() - t0 < 1800:
    try:
        st = open(r'C:\dev\ios\p5_status.txt').read().strip().splitlines()
        if st and st[-1].endswith('DONE'): break
    except Exception: pass
    time.sleep(10)
time.sleep(20)
subprocess.Popen([py, r"C:\dev\ios\chainb67.py"], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
