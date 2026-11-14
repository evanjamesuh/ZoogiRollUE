# Nov 3 (Claude): b83 code on Evan's Android tablet - package (Shipping) + install, wake, launch, play a few turns, screenshots
import subprocess, os, time
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
ST = r'C:\dev\ios\tabtest83.txt'
D = r'C:\dev\proofs\android\b83'
os.makedirs(D, exist_ok=True)
log = open(ST, 'w')
def st(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
def a(*args, t=120):
    r = subprocess.run([adb] + list(args), capture_output=True, timeout=t)
    st('$ adb %s -> %s %s' % (' '.join(args)[:80], r.stdout.decode(errors='replace')[-300:].strip(), r.stderr.decode(errors='replace')[-200:].strip())); return r
def cap(n):
    with open(os.path.join(D, '%02d.png' % n), 'wb') as f: f.write(subprocess.run([adb, 'exec-out', 'screencap', '-p'], capture_output=True, timeout=60).stdout)
st('PACKAGE_START')
with open(r'C:\dev\ios\android_pkg.out', 'w') as f:
    r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', r'C:\dev\ZoogiRollUnreal-p5style\Scripts\package_android.ps1', '-Config', 'Shipping', '-LockWaitMinutes', '240'], stdout=f, stderr=subprocess.STDOUT)
st('PACKAGE_RC=%d' % r.returncode)
a('devices', '-l', t=60)
r = a('install', '-r', r'C:\Games\ZoogiRoll.apk', t=1800)
st('INSTALL ' + ('OK' if b'Success' in r.stdout else 'FAILED'))
a('shell', 'input keyevent 224'); time.sleep(1)
a('shell', 'wm dismiss-keyguard'); a('shell', 'input swipe 640 700 640 200 300'); time.sleep(2)
a('shell', 'am force-stop com.zoogiroll.arena'); time.sleep(1)
a('shell', 'monkey -p com.zoogiroll.arena -c android.intent.category.LAUNCHER 1')
a('shell', 'wm size')
for k in range(0, 8): time.sleep(5); cap(k)
a('shell', 'input tap 658 485'); time.sleep(2); cap(8)    # CLAIM (login reward)
a('shell', 'input tap 1090 672'); time.sleep(5); cap(9)   # BATTLE
for k in range(10, 60):
    time.sleep(3); cap(k)
    if k in (16, 24, 32, 40, 48): a('shell', 'input tap 900 520'); time.sleep(1); a('shell', 'input swipe 670 520 670 720 700')  # ATTACK, then drag back + release
st('DONE')
log.close()
