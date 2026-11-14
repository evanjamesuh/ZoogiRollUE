# Nov 9 b207 (Claude): preview re-run after the compile fix - no iOS (gob207 waits at its b207_cards_ok gate)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '207c ' + s + '\n')
try: os.close(os.open(r'C:\dev\ios\b207c.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "b207_raid,b207_classic"])
hb('p5 done')
try: subprocess.run([py, r"C:\dev\ios\mac_backup.py"], timeout=1500)
except Exception as e: hb('mac backup %r' % e)
hb('done')
