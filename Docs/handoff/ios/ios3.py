import subprocess
def ssh(cmd, data=None):
    return subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=90)
src = open(r'C:\dev\ZoogiRollUnreal-combined\Source\ZoogiRoll\Private\ZoogiRoyaleInput.cpp', 'rb').read()
ssh('cat > ~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Source/ZoogiRoll/Private/ZoogiRoyaleInput.cpp', src)
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
ssh('cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh', fin)
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = ssh('cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 3 > ~/zr/build3.log 2>&1 < /dev/null & echo started', b3)
print('IOS3', r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace'))
