# Stream Saved\StagedBuilds\IOS to the Mac (~/zr/stage) as a tar over ssh, in a background thread. Log -> C:\dev\ios\upstage.log
import subprocess, tarfile, threading, time, os
subprocess.run('taskkill /f /im tar.exe', shell=True, capture_output=True)
def worker():
    log = open(r'C:\dev\ios\upstage.log', 'w')
    t0 = time.time()
    try:
        p = subprocess.Popen(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com',
                              'rm -rf ~/zr/stage && mkdir -p ~/zr/stage && tar -xf - -C ~/zr/stage && du -sh ~/zr/stage'],
                             stdin=subprocess.PIPE, stdout=log, stderr=log)
        with tarfile.open(fileobj=p.stdin, mode='w|') as tf:
            tf.add(r'C:\dev\ZoogiRollUnreal-combined\Saved\StagedBuilds\IOS', arcname='.')
        p.stdin.close()
        rc = p.wait()
        log.write('\n%s %.0fs\n' % ('UP_OK' if rc == 0 else 'UP_FAIL rc=%d' % rc, time.time() - t0))
    except Exception as e:
        log.write('UP_FAIL %r\n' % e)
    log.close()
threading.Thread(target=worker, daemon=True).start()
print('UPSTAGE2_STARTED')
