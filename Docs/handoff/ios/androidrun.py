# Nov 2 (Claude): runs package_android.ps1 and waits (log: android_pkg.out)
import subprocess, time
with open(r'C:\dev\ios\android_pkg.out', 'w') as f:
    f.write('START %s\n' % time.strftime('%H:%M:%S')); f.flush()
    r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', r'C:\dev\ZoogiRollUnreal-p5style\Scripts\package_android.ps1', '-Install', '-Config', 'Shipping', '-LockWaitMinutes', '240'], stdout=f, stderr=subprocess.STDOUT)
    f.write('\nPS_RC=%d %s\n' % (r.returncode, time.strftime('%H:%M:%S')))
