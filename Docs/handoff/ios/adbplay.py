# Nov 2 (Claude): drive Evan's tablet into a match (claim login reward, BATTLE) and screenshot the effects every 4 s
import subprocess, os, time
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
D = r'C:\dev\proofs\android\play'
os.makedirs(D, exist_ok=True)
log = open(r'C:\dev\ios\adbplay.txt', 'w')
def sh(c): r = subprocess.run([adb, 'shell', c], capture_output=True, timeout=60); log.write('$ %s\n%s\n' % (c, r.stdout.decode(errors='replace')[-500:])); log.flush()
def cap(n):
    with open(os.path.join(D, '%02d.png' % n), 'wb') as f: f.write(subprocess.run([adb, 'exec-out', 'screencap', '-p'], capture_output=True, timeout=60).stdout)
sh('wm size'); cap(0)
sh('input tap 658 485'); time.sleep(2); cap(1)   # CLAIM
sh('input tap 1090 672'); time.sleep(4); cap(2)  # BATTLE
for k in range(3, 30):
    time.sleep(4); cap(k)
    if k in (6, 9, 12, 15, 18, 21, 24): sh('input swipe 670 520 670 700 600')  # drag back + release = launch
log.write('DONE %s\n' % time.strftime('%H:%M:%S')); log.close()
