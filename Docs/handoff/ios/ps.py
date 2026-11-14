import subprocess
o = subprocess.run('tasklist /fo csv | findstr /i "tar.exe ssh.exe cmd.exe"', capture_output=True, shell=True).stdout.decode(errors='replace')
open(r'C:\dev\ios\ps.txt', 'w').write(o)
print('PS')
