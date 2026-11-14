# b176d (Evan 16:05-16:08): the trainer waits for a tap between story steps; "unlock all skills" is a Dev setting (earning stays);
# the marble finder's trail is gone (ring + pin stay).
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for old,new in pairs:
        done=False
        for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
            if s.count(o)==1: s=s.replace(o,n); done=True; break
        if not done: print('MISS',f,repr(old[:70])); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
# ---- 1. guide: tap to continue
patch('Private/ZoogiGuide.cpp',[
('''		const float T = (float)(Now() - S2At);
		switch (S2)
		{
		case G2Intro: if (T > Hold2(S2)) Set2(G2BoltTalk); break;
		case G2BoltTalk: if (T > Hold2(S2)) {''','''		const float T = (float)(Now() - S2At);
		// b176d (Evan: "should not auto switch to the next ... game waits until screen is tapped"): a story step stays up until a tap;
		// capture runs (-ZoogiAuto / -ZoogiGuideAuto) keep the timed steps
		static const bool bAutoAdv = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideAuto"));
		bool bTap = false;
		{
			static bool bWasDown = false;
			bool bDown = false;
			if (APlayerController* PC = G->GetWorld() ? G->GetWorld()->GetFirstPlayerController() : nullptr)
			{
				float Tx = 0.f, Ty = 0.f; bool bT = false;
				PC->GetInputTouchState(ETouchIndex::Touch1, Tx, Ty, bT);
				bDown = bT || PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Enter);
			}
			bTap = bDown && !bWasDown; bWasDown = bDown;
		}
		auto Ready = [&]() { return bAutoAdv ? T > Hold2(S2) : (T > 0.5f && bTap); };
		switch (S2)
		{
		case G2Intro: if (Ready()) Set2(G2BoltTalk); break;
		case G2BoltTalk: if (Ready()) {'''),
('''		case G2BoltSee: if (T > Hold2(S2)) Set2(G2WallTalk); break;
		case G2WallTalk: if (T > Hold2(S2)) {''','''		case G2BoltSee: if (Ready()) Set2(G2WallTalk); break;
		case G2WallTalk: if (Ready()) {'''),
('''		case G2WallSee: if (T > Hold2(S2)) Set2(G2BlockTalk); break;
		case G2BlockTalk: if (T > Hold2(S2)) Set2(G2ShotTalk); break;''','''		case G2WallSee: if (Ready()) Set2(G2BlockTalk); break;
		case G2BlockTalk: if (Ready()) Set2(G2ShotTalk); break;'''),
('''			if (T > Hold2(S2) && A && !A->bCPU)''','''			if (Ready() && A && !A->bCPU)'''),
('''		const float Hold = Hold2(S2);
		if (Hold > 0.f)
		{''','''		const float Hold = Hold2(S2);
		static const bool bAutoDraw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiAuto")) || FParse::Param(FCommandLine::Get(), TEXT("ZoogiGuideAuto"));
		if (Hold > 0.f && !bAutoDraw && T > 0.5f)
		{	// b176d: waits for a tap
			const float Bl = 0.55f + 0.45f * FMath::Sin((float)Now() * 5.f);
			ZoogiFunCanvasText(C, TEXT("TAP TO CONTINUE  >"), TX + TW, BoxY + BoxH - 40.f * U, 30.f * U, FLinearColor(0.47f, 0.78f, 1.f, Bl), 2, 0);
		}
		else if (Hold > 0.f && bAutoDraw)
		{'''),
])
# ---- 2. dev setting: unlock all skills
patch('Public/ZoogiFrontProfile.h',[('#pragma once\n','''#pragma once

// b176d (Evan: "Leave the unlocking system ... can be added as a dev setting"): Dev panel ALL SKILLS (testing) - every skill owned
namespace ZoogiDevSkills { ZOOGIROLL_API bool AllOpen(); ZOOGIROLL_API void SetAllOpen(bool bOn); }
''')])
patch('Private/ZoogiFrontProfile.cpp',[('''static bool ZAllSkillsOpen() { static const bool bLock = FParse::Param(FCommandLine::Get(), TEXT("ZoogiLockSkills")); return !bLock; }''','''namespace ZoogiDevSkills
{
	static int32 GAll = -1;
	static FString FlagPath() { return FPaths::ProjectSavedDir() / TEXT("ZoogiDevAllSkills.txt"); }
	bool AllOpen()
	{
		if (GAll < 0) { FString S; GAll = (FFileHelper::LoadFileToString(S, *FlagPath()) && S.TrimStartAndEnd() == TEXT("1")) ? 1 : 0; }
		return GAll == 1;
	}
	void SetAllOpen(bool bOn)
	{
		GAll = bOn ? 1 : 0;
		FFileHelper::SaveStringToFile(bOn ? TEXT("1") : TEXT("0"), *FlagPath());
		UE_LOG(LogZoogi, Log, TEXT("DEV all skills %s (next match)"), bOn ? TEXT("ON") : TEXT("OFF"));
	}
}
static bool ZAllSkillsOpen() { return ZoogiDevSkills::AllOpen(); } // b176d: a dev setting now (earning is the default again)''')])
patch('Private/ZoogiDevMode.cpp',[('#include "ZoogiAnimeCues.h"\n','#include "ZoogiAnimeCues.h"\n#include "ZoogiFrontProfile.h"\n'),
('''						+ SScrollBox::Slot()[Row(Btn(T(TEXT("ANIME DIRECTOR")),''','''						+ SScrollBox::Slot()[Row(Btn(TAttribute<FText>::CreateLambda([]() { return T(ZoogiDevSkills::AllOpen() ? TEXT("ALL SKILLS: ON") : TEXT("ALL SKILLS: OFF")); }), [](AZoogiDevMode* M) { ZoogiDevSkills::SetAllOpen(!ZoogiDevSkills::AllOpen()); }, Gold),
							Btn(T(TEXT("(TEST - NEXT MATCH)")), [](AZoogiDevMode* M) { ZoogiDevSkills::SetAllOpen(!ZoogiDevSkills::AllOpen()); }, Grey))] // b176d
						+ SScrollBox::Slot()[Row(Btn(T(TEXT("ANIME DIRECTOR")),''')])
# ---- 3. no finder trail
patch('Private/ZoogiActionFun.cpp',[('''		for (int32 k = 1; k < Tr.Num(); ++k)
		{
			FVector2D A0, A1;''','''		static const bool bTrailOn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiFinderTrail")); // b176d (Evan: "Remove trail")
		for (int32 k = 1; bTrailOn && k < Tr.Num(); ++k)
		{
			FVector2D A0, A1;''')])
