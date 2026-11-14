# Nov 11 (Claude): copy the engine's base configs (gc limits for iOS) to C:\dev\ios\engcfg
import shutil, os, glob
D = r'C:\dev\ios\engcfg'; os.makedirs(D, exist_ok=True)
E = r'C:\Program Files\Epic Games\UE_5.6\Engine\Config'
for f in [E + r'\BaseEngine.ini', E + r'\IOS\IOSEngine.ini', E + r'\IOS\BaseIOSEngine.ini', E + r'\Android\AndroidEngine.ini', E + r'\Android\BaseAndroidEngine.ini']:
    if os.path.exists(f): shutil.copy(f, os.path.join(D, f.replace(E + '\\', '').replace('\\', '_')))
out = []
for f in glob.glob(r'C:\dev\ZoogiRollUnreal-p5style\Saved\StagedBuilds\IOS\cookeddata\**\*.ini', recursive=True)[:50]: out.append(f)
open(os.path.join(D, 'staged_inis.txt'), 'w').write('\n'.join(out))
