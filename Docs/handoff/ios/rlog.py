import os
p = os.path.join(os.environ['APPDATA'], r'Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\UBA-ZoogiRollEditor-Win64-Development_Remote.txt')
open(r'C:\dev\ios\remote_log.txt', 'w', encoding='utf-8').write(open(p, encoding='utf-8', errors='replace').read())
print('RLOG')
