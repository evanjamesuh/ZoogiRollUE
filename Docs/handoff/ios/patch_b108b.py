# b108a: SKILL LAB off the skill list -> a black P5 button on home; lab = no player, orbs re-racked, < NAME > + PLAY + RESET,
# swipe orbits the camera; ZoogiSkills::ClearField; bigger skill tap radius (Grok: targets missed on the tablet)
import os, shutil
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
P = 'Source/ZoogiRoll/Private/'
# ---- no match HUD over the lab
ed(P + 'ZoogiHUD.cpp', [
 ('#include "ZoogiHUD.h"\n', '#include "ZoogiHUD.h"\n#include "ZoogiSkillLab.h"\n'),
 ('	ZoogiAimArrow::Draw(Canvas, G, PlayerOwner); // aim-arrow:', '	if (!ZoogiSkillLab::Open()) ZoogiAimArrow::Draw(Canvas, G, PlayerOwner); // b108: not in the lab; aim-arrow:'),
 ('	ZoogiAimArrow::Draw(Canvas, G, PlayerOwner); // b56:', '	if (!ZoogiSkillLab::Open()) ZoogiAimArrow::Draw(Canvas, G, PlayerOwner); // b56:'),
 ('	if (bNoHUD && G->Phase != EZoogiPhase::Menu) { UpdateAwakenWidget(G, true); return; }', '	if ((bNoHUD || ZoogiSkillLab::Open()) && G->Phase != EZoogiPhase::Menu) { UpdateAwakenWidget(G, true); return; } // b108: the lab draws its own'),
])
ed(P + 'ZoogiPowers.cpp', [
 ('#include "ZoogiGameMode.h"\n', '#include "ZoogiGameMode.h"\n#include "ZoogiSkillLab.h"\n'),
 ('	if (bRoyaleMode) return false; // Oct 28: 2v2 gets the SUPERPOWER (Wolf Pack) / REACTION buttons too', '	if (bRoyaleMode || ZoogiSkillLab::Open()) return false; // Oct 28: 2v2 gets the SUPERPOWER (Wolf Pack) / REACTION buttons too; b108: not in the lab'),
])
ed(P + 'ZoogiP5Command.cpp', [
 ('#include "ZoogiP5Command.h"\n', '#include "ZoogiP5Command.h"\n#include "ZoogiSkillLab.h"\n'),
 ('		if (UGameplayStatics::IsGamePaused(World)) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b79: nothing over the pause menu',
  '		if (UGameplayStatics::IsGamePaused(World) || ZoogiSkillLab::Open()) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b79: nothing over the pause menu; b108: or the lab'),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('#include "ZoogiArenaHUDWidget.h"\n', '#include "ZoogiArenaHUDWidget.h"\n#include "ZoogiSkillLab.h"\n'),
 ('G->Balls.Num() > 0 && !G->IsRoyaleMatch(); // Battle Royale draws its own HUD', 'G->Balls.Num() > 0 && !G->IsRoyaleMatch() && !ZoogiSkillLab::Open(); // Battle Royale draws its own HUD; b108: so does the lab'),
])
# ---- the home button
ed('Source/ZoogiRoll/Public/ZoogiMenuRoot.h', [('	void StartMapCard(int32 Index);', '	void StartMapCard(int32 Index);\n	/** b108: the home screen SKILL LAB button */\n	void StartSkillLab();')])
ed(P + 'ZoogiModeSelect.cpp', [
 ('#include "ZoogiMenuRoot.h"\n', '#include "ZoogiMenuRoot.h"\n#include "ZoogiSkillLab.h"\n'),
 ('void UZoogiMenuRoot::StartMapCard(int32 Index)\n{', '''void UZoogiMenuRoot::StartSkillLab()
{
	AZoogiGameMode* G = GM();
	if (!G) return;
	bMapSelect = false; bTrainingMaps = false; SetFrontBattle(false);
	UE_LOG(LogZoogi, Log, TEXT("SKILL LAB -> solo lab match"));
	Fire("Mode", 0);
	ZoogiSkillLab::RequestLab();
	G->ClassicStartTraining();
}

void UZoogiMenuRoot::StartMapCard(int32 Index)
{'''),
])
ed(P + 'ZoogiFront.cpp', [
 ('''	void ShowRewards(const FString& Title, FName Chest, const TArray<FZoogiFrontReward>& R)''', '''	void SkillLab()
	{	// b108 (Evan): the black P5 SKILL LAB button
		Sfx(TEXT("UiConfirm"), 0.9f);
		UE_LOG(LogZoogi, Log, TEXT("FRONT SKILL LAB"));
		TWeakObjectPtr<UZoogiMenuRoot> M = Menu;
		UWorld* W = Ctx.IsValid() ? Ctx->GetWorld() : nullptr;
		ZoogiP5::PlayWipe(W, [M]() { if (M.IsValid()) M->StartSkillLab(); }, Ink, TEXT("SKILL LAB"));
	}
	void ShowRewards(const FString& Title, FName Chest, const TArray<FZoogiFrontReward>& R)'''),
 ('''		Slab(Out, G, L + 3, TEXT("INVENTORY"), FVector2D(10.f, RY + 20.f), FVector2D(470.f, 210.f), Paper, Mag(), 72.f, [Self]() { Self->Go(1); });''',
  '''		Slab(Out, G, L + 3, TEXT("INVENTORY"), FVector2D(10.f, RY + 20.f), FVector2D(470.f, 210.f), Paper, Mag(), 72.f, [Self]() { Self->Go(1); });
		Slab(Out, G, L + 3, TEXT("SKILL LAB"), FVector2D(10.f, RY - 190.f), FVector2D(470.f, 150.f), Ink, Mag(), 64.f, [Self]() { Self->SkillLab(); }); // b108'''),
 ('''		Slab(Out, G, L, TEXT("NEWS"), FVector2D(40.f, 262.f), FVector2D(180.f, 86.f), Ink, Mag(), 40.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::News); });''',
  '''		Slab(Out, G, L, TEXT("NEWS"), FVector2D(40.f, 262.f), FVector2D(180.f, 86.f), Ink, Mag(), 40.f, [Self]() { Self->OpenPop(SZoogiFront::EPop::News); });
		Slab(Out, G, L, TEXT("SKILL LAB"), FVector2D(40.f, 384.f), FVector2D(180.f, 86.f), Ink, Mag(), 30.f, [Self]() { Self->SkillLab(); }); // b108'''),
])
# NEWS: B108 on top, B104 out
fp = R + 'Private/ZoogiFront.cpp'; raw = open(fp, 'rb').read(); bom = raw[:3] == b'\xef\xbb\xbf'; s = raw.decode('utf-8-sig'); nl = '\r\n' if '\r\n' in s else '\n'
i = s.index('\t\t\t{ TEXT("B107")'); j = s.index('\t\t\t{ TEXT("B104")'); k = s.index(nl, j) + len(nl)
row = '\t\t\t{ TEXT("B108"), TEXT("SKILL LAB BUTTON"), TEXT("SKILL LAB is a black button on the home screen now (not a skill). Just the effect and the orbs: pick with the arrows, PLAY, RESET cleans the field, swipe to turn the camera.") },' + nl
s = s[:i] + row + s[i:j] + s[k:]
open(fp, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + s.encode('utf-8'))
print('b108b ok')
