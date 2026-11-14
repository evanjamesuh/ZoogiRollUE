# Oct 30 (Claude): read-only git survey for the P5-style branch (no changes)
import subprocess
def g(*a, cwd=r'C:\dev\ZoogiRollUnreal-combined'):
    r = subprocess.run(['git'] + list(a), cwd=cwd, capture_output=True, text=True, errors='replace')
    return '$ git ' + ' '.join(a) + '\n' + r.stdout[-6000:] + r.stderr[-2000:]
out = [g('status', '--porcelain=v1', '-uno'), g('status', '--porcelain=v1', '--untracked-files=normal', '--', 'Source', 'Content/Zoogis/Ultimates', 'Content/Data/MenuArt', 'Config'),
       g('log', '--oneline', '-8'), g('worktree', 'list'), g('branch', '--list', 'feat/*'), g('lfs', 'ls-files', '-n', '--', 'Content/Zoogis')]
open(r'C:\dev\ios\git_p5_status.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('GIT_P5_STATUS_DONE')
