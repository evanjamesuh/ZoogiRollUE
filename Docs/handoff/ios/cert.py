# Evan approved: put the exported Apple Distribution identity where Unreal's remote build expects it (password "A").
import subprocess, base64, os
K = 'C:/dev/k_t.key'
r = subprocess.run(['ssh', '-i', K, '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=no', 'user294974@TX544.macincloud.com', 'base64 -i ~/zr/all.p12'], capture_output=True, timeout=60)
p12 = base64.b64decode(b''.join(r.stdout.split()))
prov = os.path.join(os.environ['LOCALAPPDATA'], 'Apple Computer', 'MobileDevice', 'Provisioning Profiles', 'ZoogiRoll.mobileprovision')
ticks = os.stat(prov).st_mtime_ns // 100 + 621355968000000000
res = []
for cfg in ['Development', 'Shipping']:
    d = r'C:\dev\ZoogiRollUnreal-combined\Intermediate\Remote\ZoogiRoll\IOS' + '\\' + cfg
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, 'Certificate.p12'), 'wb').write(p12)
    open(os.path.join(d, 'Certificate.txt'), 'w', newline='').write('%s\n%d' % (prov, ticks))
    res.append(d)
c = subprocess.run(['certutil', '-user', '-f', '-p', 'A', '-importPFX', 'My', os.path.join(res[0], 'Certificate.p12')], capture_output=True)
open(r'C:\dev\ios\cert_result.txt', 'w').write('p12 bytes %d\n%s\ncertutil rc=%d\n%s' % (len(p12), res, c.returncode, c.stdout.decode(errors='replace')[-800:]))
print('CERT_DONE', len(p12), c.returncode)
