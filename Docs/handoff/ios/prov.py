import os, shutil
d = os.path.join(os.environ['LOCALAPPDATA'], 'Apple Computer', 'MobileDevice', 'Provisioning Profiles')
os.makedirs(d, exist_ok=True)
for n in ['ZoogiRoll.mobileprovision', '4df5c2f7-f6c9-481f-a714-7f7fb5805085.mobileprovision']:
    shutil.copy(r'C:\dev\ios\ZoogiRoll_AppStore.mobileprovision', os.path.join(d, n))
print('PROV_COPIED', os.listdir(d))
exec(open('C:/dev/ios/build_ios.py').read())
