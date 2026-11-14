# b145b: NEWS B145
import os
p = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiFront.cpp'); b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B144"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B145"), TEXT("AWAKEN ANIME IN THE TOP CARD"), TEXT("Hotstreak\'s METEOR STRIKE Awaken now plays one anime clip (the second one is gone), and it plays in the top anime card instead of full screen - the meteor rain keeps going underneath so you can watch it land.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o)); print('b145b ok')
