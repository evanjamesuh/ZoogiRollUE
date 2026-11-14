# b144d: NEWS B144
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
p = R + 'Private/ZoogiFront.cpp'; b = open(p, 'rb').read()
o = b'\t\t\t{ TEXT("B143"),'
assert b.count(o) == 1
n = b'\t\t\t{ TEXT("B144"), TEXT("SKY DIVE CAM + GRENADE FIX"), TEXT("Thrown high into the air (a banked Bolt Balls, a bounce off the stands)? The camera now swoops up above your Zoogi with a burst of slow-mo and a SKY HIGH! callout, then rides it back down to the landing with a thump - and it never gets stuck up high afterwards. A saved PRISM GRENADE thrown early in a match now always flies and explodes.") },\r\n'
b = b.replace(o, n + o); open(p, 'wb').write(b); print('b144d ok')
