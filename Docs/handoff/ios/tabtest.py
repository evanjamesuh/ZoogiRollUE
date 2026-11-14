# Nov 11 (Claude): Evan 12:44 "tablet connected so you can test". Package the forward (B249) game for Android (Development = logs),
# install on the tablet, launch, screenshots + logcat -> does it open? where does it die?
import subprocess, os, time, sys
if not os.environ.get('TT_CHILD'):
    try: os.close(os.open(r'C:\dev\ios\tabtest.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    except FileExistsError: raise SystemExit
    e = dict(os.environ); e['TT_CHILD'] = '1'
    subprocess.Popen([sys.executable, __file__], env=e, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True); raise SystemExit
ST = r'C:\dev\ios\tabtest_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
def a(*args, t=120):
    try: r = subprocess.run([adb] + list(args), capture_output=True, timeout=t); return (r.stdout + r.stderr).decode(errors='replace')
    except Exception as ex: return 'EXC %r' % ex
d = a('devices', '-l'); status('DEVICES ' + d.replace('\r', '').replace('\n', ' | '))
if 'device product' not in d: status('NO_TABLET'); status('DONE'); raise SystemExit
status('TABLET ' + a('shell', 'getprop ro.product.model; dumpsys package com.zoogiroll.arena | grep -E "versionName|versionCode|lastUpdateTime"; df -h /data | tail -1').replace('\r', '').replace('\n', ' | '))
status('PKG_START')
with open(r'C:\dev\ios\tabtest_pkg.out', 'w') as f:
    r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', r'C:\dev\ZoogiRollUnreal-p5style\Scripts\package_android.ps1', '-Config', 'Development', '-Out', r'C:\Games\ZoogiRollAndroidDev', '-LockWaitMinutes', '240'], stdout=f, stderr=subprocess.STDOUT)
status('PKG_RC=%d' % r.returncode)
t = open(r'C:\dev\ios\tabtest_pkg.out', errors='replace').read()
if 'PACKAGE_EXIT=0' not in t or 'APK=' not in t: status('PKG_FAILED'); status('DONE'); raise SystemExit
i = a('install', '-r', r'C:\Games\ZoogiRoll.apk', t=2400); status('INSTALL ' + i[-300:].replace('\r', '').replace('\n', ' | '))
if 'Success' not in i: status('DONE'); raise SystemExit
D = r'C:\dev\proofs\android\b251'; os.makedirs(D, exist_ok=True)
a('logcat', '-c'); a('shell', 'input keyevent KEYCODE_WAKEUP')
status('LAUNCH ' + a('shell', 'monkey -p com.zoogiroll.arena -c android.intent.category.LAUNCHER 1')[-120:].replace('\n', ' '))
for k in range(12):
    time.sleep(10)
    try:
        with open(os.path.join(D, 'tab_%02d.png' % k), 'wb') as f: f.write(subprocess.run([adb, 'exec-out', 'screencap', '-p'], capture_output=True, timeout=60).stdout)
    except Exception as ex: status('cap %r' % ex)
    status('t+%ds pid=%s' % ((k + 1) * 10, a('shell', 'pidof com.zoogiroll.arena').strip()))
open(r'C:\dev\ios\tabtest_logcat.txt', 'w', encoding='utf-8', errors='replace').write(a('logcat', '-d', '-v', 'time', t=300))
status('DONE')
