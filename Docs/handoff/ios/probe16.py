# read-only: is the hologram material cooked into the iOS stage? -> C:/dev/ios/probe16.txt
import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com',
    'U=~/zr/stage/cookeddata/zoogiroll/content/paks/zoogiroll-ios.utoc; echo ZAFX_ADD=$(grep -a -c M_ZAFX_Add $U) ZAFX_ANY=$(grep -a -c ZAFX $U) WOLF=$(grep -a -c -i wolfgang $U) GLOW=$(grep -a -c M_ZoogiGlow $U); strings -n 6 $U | grep -i -m 8 "zafx\\|awakenfx"'], capture_output=True, timeout=60)
open(r'C:\dev\ios\probe16.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
