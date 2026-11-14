# Nov 11 (Claude): tablet says "Storage permission" and Android won't let Evan allow it -> grant over adb, relaunch, screenshots.
import subprocess, os, time
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
L = open(r'C:\dev\ios\tabperm.txt', 'w')
def a(*args, t=60):
    try: r = subprocess.run([adb] + list(args), capture_output=True, timeout=t); s = (r.stdout + r.stderr).decode(errors='replace')
    except Exception as e: s = 'EXC %r' % e
    L.write('$ adb %s\n%s\n' % (' '.join(args), s[-800:])); L.flush(); return s
P = 'com.zoogiroll.arena'
a('shell', 'getprop ro.build.version.release; getprop ro.build.version.sdk')
a('shell', 'am force-stop ' + P)
for perm in ('android.permission.READ_EXTERNAL_STORAGE', 'android.permission.WRITE_EXTERNAL_STORAGE', 'android.permission.READ_MEDIA_IMAGES', 'android.permission.READ_MEDIA_VIDEO', 'android.permission.READ_MEDIA_AUDIO'):
    a('shell', 'pm grant %s %s' % (P, perm))
a('shell', 'appops set %s MANAGE_EXTERNAL_STORAGE allow' % P)
a('shell', 'appops set %s READ_EXTERNAL_STORAGE allow' % P)
a('shell', 'appops set %s WRITE_EXTERNAL_STORAGE allow' % P)
a('shell', 'dumpsys package %s | grep -i "storage\\|MEDIA" | head -20' % P)
a('logcat', '-c')
a('shell', 'monkey -p %s -c android.intent.category.LAUNCHER 1' % P)
D = r'C:\dev\proofs\android\b253p'; os.makedirs(D, exist_ok=True)
for k in range(9):
    time.sleep(10)
    with open(os.path.join(D, 'tab_%02d.png' % k), 'wb') as f: f.write(subprocess.run([adb, 'exec-out', 'screencap', '-p'], capture_output=True, timeout=60).stdout)
    a('shell', 'pidof ' + P)
open(r'C:\dev\ios\tabperm_logcat.txt', 'w', encoding='utf-8', errors='replace').write(a('logcat', '-d', '-v', 'time', t=300))
L.write('DONE\n'); L.close()
