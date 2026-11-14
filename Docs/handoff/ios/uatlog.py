import os, glob, subprocess, shutil
d = os.path.join(os.environ['APPDATA'], 'Unreal Engine', 'AutomationTool', 'Logs')
fs = sorted(glob.glob(os.path.join(d, '**', '*.txt'), recursive=True), key=os.path.getmtime)
out = '\n'.join('%s %d' % (f, os.path.getsize(f)) for f in fs[-5:])
if fs:
    t = open(fs[-1], encoding='utf-8', errors='replace').read()
    out += '\n----\n' + t[-20000:]
tl = subprocess.run('tasklist /fi "imagename eq dotnet.exe" /fo csv', capture_output=True, shell=True).stdout.decode(errors='replace')
out += '\n----\n' + tl
open(r'C:\dev\ios\uatlog.txt', 'w', encoding='utf-8').write(out)
print('UATLOG')
