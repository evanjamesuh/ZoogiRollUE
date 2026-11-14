import subprocess
r = subprocess.run(['powershell', '-NoProfile', '-Command', "Get-Process | Sort-Object WorkingSet64 -Descending | Select-Object -First 25 Id, ProcessName, @{n='MB';e={[int]($_.WorkingSet64/1MB)}} | Format-Table -AutoSize | Out-String -Width 200"], capture_output=True, timeout=120)
open(r'C:\dev\ios\memtop.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
