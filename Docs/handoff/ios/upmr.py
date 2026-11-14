import subprocess, base64, os, glob
src_dir = os.path.join(os.environ['LOCALAPPDATA'], r'UnrealEngine\Intermediate\Build\BuildRules')
res = []
for f in glob.glob(os.path.join(src_dir, 'MarketplaceRules.*')):
    data = base64.b64encode(open(f, 'rb').read())
    name = os.path.basename(f)
    cmd = 'D="$HOME/Library/Application Support/Epic/UnrealEngine/Intermediate/Build/BuildRules"; mkdir -p "$D"; base64 -D > "$D/%s"; ls -la "$D"' % name
    r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=60)
    res.append('%s rc=%d %s %s' % (name, r.returncode, r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace')))
open(r'C:\dev\ios\upmr.txt', 'w').write('\n'.join(res))
print('UPMR', len(res))
exec(open('C:/dev/ios/pkg_ios.py').read())
