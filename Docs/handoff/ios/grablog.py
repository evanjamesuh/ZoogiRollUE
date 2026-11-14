import os, shutil
p = os.path.join(os.environ['APPDATA'], r'Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\Log.txt')
shutil.copy(p, r'C:\dev\ios\uat_last.txt')
print('GRABBED', os.path.getsize(p))
