import os, shutil
d = os.path.join(os.environ['LOCALAPPDATA'], 'Apple Computer', 'MobileDevice', 'Provisioning Profiles')
for f in os.listdir(d):
    if f.endswith('.mobileprovision'): os.remove(os.path.join(d, f))
for n in ['ZoogiRoll.mobileprovision', '4eef96ba-b81a-4086-8fb2-52d50289b2cd.mobileprovision']:
    shutil.copy(r'C:\dev\ios\ZoogiRoll_AppStore.mobileprovision', os.path.join(d, n))
prov = os.path.join(d, 'ZoogiRoll.mobileprovision')
ticks = os.stat(prov).st_mtime_ns // 100 + 621355968000000000
for cfg in ['Development', 'Shipping']:
    dd = r'C:\dev\ZoogiRollUnreal-combined\Intermediate\Remote\ZoogiRoll\IOS' + '\\' + cfg
    open(os.path.join(dd, 'Certificate.txt'), 'w', newline='').write('%s\n%d' % (prov, ticks))
print('PROV2', os.listdir(d))
exec(open('C:/dev/ios/pkg_ios.py').read())
