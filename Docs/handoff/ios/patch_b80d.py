# b80d (Evan 01:46: "the fail after the clash animation is the only mostly-full-screen video allowed"): crash clips after a clash keep the full screen; skill summons stay in the top card
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('''	if (!bCrashFull)
	{
		if (!MovieTexture || !MovieTexture->GetResource()) return;''',
  '''	if (!bCrashFull && CrashId.StartsWith(TEXT("Anime_Awaken_"))) // b80: only the skill summons go to the top card; the post-clash fail clip stays full screen
	{
		if (!MovieTexture || !MovieTexture->GetResource()) return;'''),
])
print('b80d ok')
