import subprocess, glob, os
out = []
for pat in [r'C:\Program Files\Epic Games\UE_5.6\Engine\Extras\ThirdPartyNotUE\libimobiledevice\**\idevice_id.exe',
            r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\DotNET\IOS\**\*.exe']:
    out += glob.glob(pat, recursive=True)
res = '\n'.join(out) + '\n'
for exe in out:
    if exe.lower().endswith('idevice_id.exe'):
        try:
            r = subprocess.run([exe, '-l'], capture_output=True, timeout=20)
            res += 'ID: rc=%d %s %s\n' % (r.returncode, r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace'))
            info = os.path.join(os.path.dirname(exe), 'ideviceinfo.exe')
            if os.path.exists(info):
                r = subprocess.run([info, '-k', 'ProductType'], capture_output=True, timeout=20)
                res += 'INFO: %s %s\n' % (r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace'))
        except Exception as e:
            res += 'EXC %r\n' % e
svc = subprocess.run(['sc', 'query', 'Apple Mobile Device Service'], capture_output=True)
res += 'SVC: ' + svc.stdout.decode(errors='replace')
open(r'C:\dev\ios\udid.txt', 'w').write(res)
print('UDID_DONE')
