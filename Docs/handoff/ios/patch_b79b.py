# b79b (Evan 01:22: "for horizontal the cam needs to sit lower behind the character with room to drag back"): landscape aim view lower + Zoogi higher on screen
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAimView.cpp', [
 ('TEXT("zoogi.Cam.AimPitch"), 38.f, TEXT("Aim view: camera pitch, degrees down (fixed;', 'TEXT("zoogi.Cam.AimPitch"), 24.f, TEXT("Aim view: camera pitch, degrees down (b79: 24 - lower behind the Zoogi, was 38; fixed;'),
 ('TEXT("zoogi.Cam.AimScreenY"), 0.56f, TEXT("Aim view: the Zoogi\'s screen height from the top (fraction of the full view;', 'TEXT("zoogi.Cam.AimScreenY"), 0.47f, TEXT("Aim view: the Zoogi\'s screen height from the top (b79: 0.47 - drag-back room below, was 0.56; fraction of the full view;'),
])
print('b79b ok')
