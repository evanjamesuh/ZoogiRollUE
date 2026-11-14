import os
p = os.path.join(os.environ['APPDATA'], r'Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\Log.txt')
t = open(p, encoding='utf-8', errors='replace').read()
open(r'C:\dev\ios\tail.txt', 'w', encoding='utf-8').write(t[-15000:])
print('TAIL')
