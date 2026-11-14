# retry sign + export + upload of build 31 after "exportArchive Zip failed" (no rebuild)
import subprocess, time
ST = 'C:/dev/ios/fin31_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com']
r = subprocess.run(SSH + ['cd "$TMPDIR" && rm -rf DTAppAnalyzerExtractorOutput-*.zip XcodeDistPipeline.* *.xcdistributionlogs; df -h /System/Volumes/Data | tail -1; du -sh $HOME/zr 2>/dev/null; ls -t /var/folders/4k/1pr6nm0n4gz8_qb3l1mgnvkw0000j7/T/ | head -3; nohup ~/zr/fin.sh 31 > ~/zr/fin31.log 2>&1 < /dev/null & echo started'], capture_output=True, timeout=120)
status('START ' + r.stdout.decode(errors='replace').replace('\n', ' | ') + r.stderr.decode(errors='replace'))
for k in range(20):
    time.sleep(45)
    r = subprocess.run(SSH + ['tail -c 2500 ~/zr/fin31.log'], capture_output=True, timeout=120)
    t = r.stdout.decode(errors='replace')
    open(r'C:\dev\ios\fin31_tail.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t:
        status('FINISHED ' + t[t.find('EXPORT_UPLOAD_RC'):].strip()); break
status('DONE')
