import os, glob
d = os.path.join(os.environ['APPDATA'], r'Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6')
fs = sorted(glob.glob(os.path.join(d, '*')), key=os.path.getmtime)
out = '\n'.join('%s %d %d' % (os.path.basename(f), os.path.getsize(f), os.path.getmtime(f)) for f in fs[-25:])
cands = [f for f in fs if 'Cook' in os.path.basename(f) or 'IPhonePackager' in os.path.basename(f) or 'backup' in os.path.basename(f)]
open(r'C:\dev\ios\tail2.txt', 'w', encoding='utf-8').write(out)
print('TAIL2')
