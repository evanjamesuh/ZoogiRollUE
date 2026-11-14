# Nov 11 (Claude): win demo + pad UI capture after B246 hands the PC back
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
def go():
    while 'DONE' not in open(r'C:\dev\ios\b246ios_status.txt').read(): time.sleep(20)
    subprocess.run([py, r"C:\dev\ios\p5run.py", "win_demo,b246_padui"])
if os.environ.get('ZD_CHILD'): go()
else:
    try: os.close(os.open(r'C:\dev\ios\b246d.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    except FileExistsError: raise SystemExit
    e = dict(os.environ); e['ZD_CHILD'] = '1'
    subprocess.Popen([py, __file__], env=e, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
