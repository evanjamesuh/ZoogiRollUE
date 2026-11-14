import os, re
p = os.path.join(os.environ['APPDATA'], r'Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\Log.txt')
t = open(p, encoding='utf-8', errors='replace').read().split('\n')
out = [l for l in t if ' error' in l.lower() or 'Compiling' in l or '[Remote]' in l or 'Building ' in l or l.startswith('[') ]
open(r'C:\dev\ios\errs.txt', 'w', encoding='utf-8').write('\n'.join(out[-150:]))
print('ERRS', len(out))
