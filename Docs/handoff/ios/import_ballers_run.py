import os, sys
try: os.close(os.open(r'C:\dev\ios\b208_import.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: sys.exit(0)  # a second launcher instance
import subprocess
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\import_ballers_run.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="C:\\dev\\ios\\import_ballers.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
open(r'C:\dev\ios\import_ballers_rc.txt', 'w').write('RC=%d\n' % rc)
open(r'C:\dev\ios\b208_import_done','w').write('ok')
