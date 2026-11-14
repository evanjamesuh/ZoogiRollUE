# Nov 3 (Claude, Evan 11:08 "tablet later"): stop the b83 Android packaging + tablet test, free the Unreal lock for b84
import subprocess, time, os
log = open(r'C:\dev\ios\stopandroid.txt', 'w')
def ps(cmd):
    r = subprocess.run(['powershell', '-NoProfile', '-Command', cmd], capture_output=True, timeout=120)
    log.write('> ' + cmd[:160] + '\n' + r.stdout.decode(errors='replace')[-1500:] + r.stderr.decode(errors='replace')[-800:] + '\n'); log.flush()
ps("Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*tabtest83*' } | ForEach-Object { 'kill ' + $_.ProcessId + ' ' + $_.Name; Stop-Process -Id $_.ProcessId -Force }")
lock = r'C:\dev\unreal.lock'
try:
    t = open(lock).read(); log.write('lock: %s\n' % t)
    if 'android' in t.lower() and 'pid=' in t:
        pid = t.split('pid=')[1].split()[0]
        r = subprocess.run(['taskkill', '/PID', pid, '/T', '/F'], capture_output=True); log.write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
except Exception as e: log.write('lock read %r\n' % e)
ps("Get-CimInstance Win32_Process | Where-Object { ($_.CommandLine -like '*package_android*') -or ($_.CommandLine -like '*-platform=Android*') -or ($_.CommandLine -like '*Android*' -and ($_.Name -like 'UnrealBuildTool*' -or $_.Name -like 'UnrealEditor-Cmd*' -or $_.Name -like 'dotnet*' -or $_.Name -like 'cmd*')) } | ForEach-Object { 'kill ' + $_.ProcessId + ' ' + $_.Name; Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }")
time.sleep(5)
ps("Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*Android*' } | Select-Object ProcessId, Name | Format-Table -AutoSize | Out-String")
try:
    if os.path.exists(lock) and 'android' in open(lock).read().lower(): os.remove(lock); log.write('lock removed\n')
except Exception as e: log.write('lock remove %r\n' % e)
log.write('DONE %s\n' % time.strftime('%H:%M:%S')); log.close()
