# b79c (Evan 01:28 "move combo text to the left so it doesn't overlay the REACTION button"; 00:44 "P5 notes only while aiming")
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiActionFun.cpp', [
 ('''	const FVector2D Ctr(Wd - Len * 0.5f - 24.f * U + Slide, Ht * (bPort ? 0.66f : 0.57f)); // clear of the SUPERPOWER button''',
  '''	static const bool bRightBanners = FParse::Param(FCommandLine::Get(), TEXT("ZoogiBannersRight"));
	const FVector2D Ctr = bRightBanners ? FVector2D(Wd - Len * 0.5f - 24.f * U + Slide, Ht * (bPort ? 0.66f : 0.57f)) // clear of the SUPERPOWER button
		: FVector2D(Len * 0.5f + 24.f * U - Slide, Ht * (bPort ? 0.62f : 0.55f)); // b79 (Evan: "move the combo text to the left - it overlays the REACTION button"): left side, sliding in from the left'''),
 ('''		const bool bPlay = G->Phase == EZoogiPhase::Aiming || G->Phase == EZoogiPhase::Rolling;''',
  '''		static const bool bNotesRolling = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNotesRolling"));
		const bool bPlay = G->Phase == EZoogiPhase::Aiming || (bNotesRolling && G->Phase == EZoogiPhase::Rolling); // b79 (Evan: "P5 notes only while aiming"): gone after the launch'''),
 ('''			if (A->bCPU && bFast) Notes.Add({ TEXT(">> 2X  CPU TURN"), FLinearColor(0.55f, 0.85f, 1.f) });''',
  '''			if (A->bCPU && bFast && bPlay) Notes.Add({ TEXT(">> 2X  CPU TURN"), FLinearColor(0.55f, 0.85f, 1.f) });'''),
])
print('b79c ok')
