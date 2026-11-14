import subprocess
data = open(r'C:\dev\ios\ZoogiRoll_AppStore.mobileprovision', 'rb').read()
subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'cat > ~/zr/appstore.mobileprovision'], input=data, timeout=60)
fin = open(r'C:\dev\ios\fin.sh', 'rb').read().replace(b'\r\n', b'\n')
subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'cat > ~/zr/fin.sh; chmod +x ~/zr/fin.sh; nohup ~/zr/fin.sh 2 > ~/zr/fin.log 2>&1 < /dev/null &'], input=fin, timeout=60)
print('FIN_STARTED')
