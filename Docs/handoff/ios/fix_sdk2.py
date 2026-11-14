import shutil
d = r'C:\Program Files\Epic Games\UE_5.6\Engine\Config\Apple'
try:
    shutil.copy(d + r'\Apple_SDK.json', d + r'\Apple_SDK.json.bak-claude')
    shutil.copy(r'C:\dev\ios\Apple_SDK.new.json', d + r'\Apple_SDK.json')
    r = 'OK ' + [l for l in open(d + r'\Apple_SDK.json') if '"MaxVersion"' in l][0]
except Exception as e:
    r = 'FAIL %r' % e
open(r'C:\dev\ios\fix_sdk_result.txt', 'w').write(r)
print('FIX_SDK2', r)
