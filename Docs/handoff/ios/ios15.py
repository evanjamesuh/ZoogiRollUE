# Oct 29 build 15 (screen shatter, clone shockwave, thrust portals on every arch, Dev Mode obstacles + bumpers): upload changed
# sources (the build 14 map-card patch pak stays in the stage), probe whether the Vefects shockwave content is cooked, then ~/zr/build3.sh 15.
import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=120)
ROOT = r'C:\dev\ZoogiRollUnreal-combined'
MAC = '~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
pub = ['ZoogiScreenFx.h', 'ZoogiBumpers.h', 'ZoogiClone.h', 'ZoogiPortals.h', 'ZoogiArena.h', 'ZoogiDevMode.h']
prv = ['ZoogiScreenFx.cpp', 'ZoogiBumpers.cpp', 'ZoogiClone.cpp', 'ZoogiPortals.cpp', 'ZoogiGameMode.cpp', 'ZoogiArena.cpp', 'ZoogiArenaProps.cpp', 'ZoogiDevMode.cpp', 'ZoogiEpisodePresent.cpp']
files = ['Source/ZoogiRoll/Public/' + f for f in pub] + ['Source/ZoogiRoll/Private/' + f for f in prv]
out = []
for f in files:
    r = ssh('cat > %s/%s' % (MAC, f), open(ROOT + '\\' + f.replace('/', '\\'), 'rb').read())
    out.append('%s rc=%d %s' % (f, r.returncode, r.stderr.decode(errors='replace')[:200]))
r = ssh('ls -la ~/zr/stage/cookeddata/zoogiroll/content/paks/; echo VEFECTS_IN_UTOC=$(grep -a -c Vefects ~/zr/stage/cookeddata/zoogiroll/content/paks/zoogiroll-ios.utoc); echo SHOCK_IN_UTOC=$(grep -a -c -i shockwave ~/zr/stage/cookeddata/zoogiroll/content/paks/zoogiroll-ios.utoc)')
out.append('STAGE ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')[:200])
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 15 > ~/zr/build15.log 2>&1 < /dev/null & echo started', b3)
out.append('IOS ' + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
open(r'C:\dev\ios\ios15_out.txt', 'w').write('\n'.join(out))
print('\n'.join(out))
