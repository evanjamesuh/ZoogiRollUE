# Nov 2 16:40 (Claude): Evan - stop the TX242 setup, go back to TX544. Kill setup242 (+UBT/rsync children), undo its config edits, retry b65 on TX544.
import subprocess, time, os, shutil
ST = 'C:/dev/ios/tx242_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
ps = "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*setup242.py*' -and $_.Name -like 'python*' } | ForEach-Object { taskkill /F /T /PID $_.ProcessId }"
r = subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True)
status('STOP242 ' + (r.stdout + r.stderr).decode(errors='replace').replace('\r\n', ' ')[:300])
time.sleep(3)
for proj in [r'C:\dev\ZoogiRollUnreal-combined', r'C:\dev\ZoogiRollUnreal-p5style']:
    ini = proj + r'\Config\DefaultEngine.ini'
    if os.path.exists(ini + '.tx544'): shutil.copyfile(ini + '.tx544', ini)
AJ = r'C:\Program Files\Epic Games\UE_5.6\Engine\Config\Apple\Apple_SDK.json'
if os.path.exists(AJ + '.bak-tx242'): shutil.copyfile(AJ + '.bak-tx242', AJ)
status('REVERTED ini + Apple_SDK (26.9.0)')
BK = r'C:\dev\backups\combined_Binaries_tx242'
rc = subprocess.run(r'robocopy "%s" "C:\dev\ZoogiRollUnreal-combined\Binaries" /E /XC /XN /XO /NFL /NDL /NJH /NJS /NP' % BK, shell=True).returncode
status('BIN_RESTORE_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('BACK_TO_TX544 -> b65up.py')
subprocess.run([r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe', r'C:\dev\ios\b65up.py'])
status('DONE')
