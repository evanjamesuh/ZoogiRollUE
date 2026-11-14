import subprocess, base64, os
exec(open('C:/dev/ios/mac.py').read())
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'base64 -i ~/zr/dist.p12'], capture_output=True, timeout=60)
p12 = base64.b64decode(b''.join(r.stdout.split()))
for cfg in ['Development', 'Shipping']:
    d = r'C:\dev\ZoogiRollUnreal-combined\Intermediate\Remote\ZoogiRoll\IOS' + '\\' + cfg
    open(os.path.join(d, 'Certificate.p12'), 'wb').write(p12)
print('DIST_P12', len(p12))
