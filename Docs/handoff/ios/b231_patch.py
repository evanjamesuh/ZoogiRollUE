import io, os, sys, shutil
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
shutil.copy(os.path.expanduser('~/mnt/dev/ios/cine/ZoogiWallCine.cpp'), os.path.join(S, 'Private', 'ZoogiWallCine.cpp'))
p = os.path.join(S, 'Private', 'ZoogiFront.cpp'); t = io.open(p, encoding='utf-8-sig').read()
a = '			{ TEXT("B230"),'
assert t.count(a) == 1
t = t.replace(a, '			{ TEXT("B231"), TEXT("CINEMATIC POLISH"), TEXT("Behind the scenes: the wall-smash cinematic now plays on a clean screen (no menus or orbs in the way) with a clear view of the rock.") },\n' + a)
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok b231')
