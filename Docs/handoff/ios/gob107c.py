# Nov 3 b107c (Claude): after TestFlight build 107 is DONE, package + install the Android build for the tablet
import subprocess, time, os
HB = r'C:\dev\ios\gob107c_hb.txt'
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'start\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
T0 = time.time()
S = r'C:\dev\ios\b107ios_status.txt'
def ios_done():
    try: return os.path.getmtime(S) > T0 and 'DONE' in open(S).read()
    except Exception: return False
while not ios_done(): time.sleep(30)
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'ios done, android\n')
subprocess.run([py, r"C:\dev\ios\androidrun.py"])
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'android finished\n')
