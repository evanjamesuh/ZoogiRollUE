# Nov 1 (Claude): wait for build 48 (Mac build + upload) to finish, then cook + build + upload 49 (Aurex cooked)
import subprocess, time
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
t0 = time.time()
while time.time() - t0 < 4 * 3600:
    try:
        if 'DONE' in open('C:/dev/ios/b48ios_status.txt').read(): break
    except Exception: pass
    time.sleep(30)
time.sleep(20)
subprocess.run([py, r'C:\dev\ios\b49ios.py'])
