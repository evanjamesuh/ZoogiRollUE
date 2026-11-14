# Nov 2 (Claude): is Evan's Android tablet visible to adb?
import subprocess, os
adb = os.path.expandvars(r'%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe')
out = []
try:
    r = subprocess.run([adb, 'devices', '-l'], capture_output=True, timeout=60); out.append(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
    r = subprocess.run([adb, 'shell', 'getprop ro.product.model; getprop ro.build.version.release; wm size; getprop ro.hardware.egl; getprop ro.board.platform'], capture_output=True, timeout=60); out.append(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
    r = subprocess.run([adb, 'shell', 'pm list packages | grep -i zoogi'], capture_output=True, timeout=60); out.append(r.stdout.decode(errors='replace'))
except Exception as e: out.append('EXC %r' % e)
open(r'C:\dev\ios\adbprobe.txt', 'w').write('\n'.join(out))
