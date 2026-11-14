import subprocess
subprocess.Popen(['powershell', '-NoProfile', '-Command', "Start-Process powershell -Verb RunAs -WindowStyle Hidden -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File C:\\dev\\ios\\fix_sdk.ps1'"])
print('FIX_SDK_UAC_REQUESTED')
