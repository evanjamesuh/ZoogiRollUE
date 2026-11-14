# b147b: NEWS B147
import os
p = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiFront.cpp'); b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B146"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B147"), TEXT("BOSS ATTACKS: BRACE, HANG ON, SCORE"), TEXT("TAP TO BRACE when the boss hand is on you: you get knocked just over the rim and hang on - tap to pull yourself back up (free). Knock a rival or orb into the red zone and the boss ring-out scores for YOU. CPU Zoogis now dodge the zones too (or GUARD against the slam). A boss camera shows each attack land, and the zones glow brighter.") },\r\n'
open(p, 'wb').write(b.replace(o, n + o)); print('b147b ok')
