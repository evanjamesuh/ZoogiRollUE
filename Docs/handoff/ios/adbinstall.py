# Nov 2 (Claude): install the Android build on Evan's tablet, launch it, grab screenshots (C:\dev\proofs\android\)
import subprocess, os, time
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
out = open(r'C:\dev\ios\adbinstall.txt', 'w')
def run(*a, t=900):
    r = subprocess.run([adb] + list(a), capture_output=True, timeout=t)
    out.write('$ adb %s\n%s%s\n' % (' '.join(a), r.stdout.decode(errors='replace')[-3000:], r.stderr.decode(errors='replace')[-1500:])); out.flush()
    return r
run('devices', '-l', t=60)
r = run('install', '-r', r'C:\Games\ZoogiRoll.apk', t=1800)
if b'Success' in r.stdout:
    run('shell', 'monkey -p com.zoogiroll.arena -c android.intent.category.LAUNCHER 1', t=60)
    os.makedirs(r'C:\dev\proofs\android', exist_ok=True)
    for k in range(6):
        time.sleep(20)
        with open(r'C:\dev\proofs\android\tab_%02d.png' % k, 'wb') as f:
            f.write(subprocess.run([adb, 'exec-out', 'screencap', '-p'], capture_output=True, timeout=60).stdout)
out.write('DONE %s\n' % time.strftime('%H:%M:%S')); out.close()
