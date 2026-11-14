# b104a (Evan 21:18: "move that duel scene to story mode. In classic keep the gameplay view - the SP options still work but we see it
# play on the field instead of that near / far view. This keeps the camera on the main camera"):
# classic duels = FIELD duels: no fighter forms, no duel camera, no split; the circle match / ATTACK / SKILL / DEFEND menus run over the
# frozen match view, the hits land on the balls themselves (streak, blast, HP popups, guard dome, the loser flies). Story missions keep
# the cinematic duel scene. -ZoogiDuelScene = the scene in classic too.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P='Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiActionFun.h', [
 ('		bool bFinish = false, bAttWins = true, bFinishHit = false, bCamFlip = false, bMeasured = false, bCamHigh = false;',
  '		bool bField = false; // b104: classic duel played on the field (no forms / duel camera)\n		bool bFinish = false, bAttWins = true, bFinishHit = false, bCamFlip = false, bMeasured = false, bCamHigh = false;'),
])
ed(P+'ZoogiSuperFun.cpp', [
 ('''	RestoreHiddenFx();
	HideFxAround(Clash.At, 3200.f, Att, Def);
	SpawnFighter(0, Att, FormIndex(Att));
	SpawnFighter(1, Def, FormIndex(Def));''', '''	RestoreHiddenFx();
	// b104 (Evan): classic duels play on the field - the cinematic duel scene (forms + duel camera) is for story missions
	Clash.bField = ZoogiDuelV2::ReplacesClash() && !ZoogiStory::InMission() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelScene"));
	if (Clash.bField)
	{
		for (AZoogiBall* Db : { Att, Def })
		{
			const FLinearColor Pc = ZoogiUtil::PlayerColor(Db->PlayerIndex);
			AZoogiFX::Spawn(GetWorld(), Db->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Pc, 230.f, 0.9f, true, 10.f);
			AZoogiFX::Spawn(GetWorld(), Db->BallLocation(), Pc, 150.f, 0.5f, false, 9.f, Db);
		}
		UE_LOG(LogZoogi, Log, TEXT("FUN duel on the field (no duel scene)"));
	}
	else
	{
		HideFxAround(Clash.At, 3200.f, Att, Def);
		SpawnFighter(0, Att, FormIndex(Att));
		SpawnFighter(1, Def, FormIndex(Def));
	}'''),
 ('#include "ZoogiEpic.h"\n', '#include "ZoogiEpic.h"\n#include "ZoogiStory.h"\n#include "ZoogiDuelV2.h"\n'),
])
ed(P+'ZoogiStrikeFun.cpp', [
 ('''void UZoogiFunSys::TickFighters(AZoogiGameMode* G, float RealDt)
{
	UWorld* W = GetWorld();
	if (!W) return;''', '''void UZoogiFunSys::TickFighters(AZoogiGameMode* G, float RealDt)
{
	UWorld* W = GetWorld();
	if (!W) return;
	if (Clash.bField)
	{	// b104: a field duel - no forms, no duel camera; the HUD burst sits between the two balls
		Clash.HitScreen = FVector2D(-1.f, -1.f);
		if (APlayerController* PCf = W->GetFirstPlayerController()) { FVector2D Sf; if (PCf->ProjectWorldLocationToScreen(Clash.Center, Sf, true)) Clash.HitScreen = Sf; }
		return;
	}'''),
])
ed(P+'ZoogiActionFun.cpp', [
 ('			AZoogiFX::bMuted = Clash.bOn; // b57 (Evan): no sparks during a clash', '			AZoogiFX::bMuted = Clash.bOn && !Clash.bField; // b57 (Evan): no sparks during a clash; b104: a field duel shows its hits'),
])
ed(P+'ZoogiDuelV2.cpp', [
 ('''				AActor* Fd = Clash.F[1].Actor.Get();
				AZoogiFabFX::Play(GetWorld(),''', '''				AActor* Fd = Clash.F[1].Actor.Get();
				if (Clash.bField)
				{	// b104: the strike crosses the field from the attacker to the defender
					const FVector Pa = A->BallLocation(), Pd = D->BallLocation();
					const FLinearColor Sc = Clash.VPick == 1 ? FLinearColor(1.f, 0.45f, 0.15f) : ZoogiUtil::PlayerColor(A->PlayerIndex);
					for (int32 k = 1; k <= 8; ++k) AZoogiFX::Spawn(GetWorld(), FMath::Lerp(Pa, Pd, k / 8.f) + FVector(0, 0, 30.f), Sc, 70.f + 10.f * k, 0.25f + 0.03f * k, false, 12.f);
					AZoogiFX::Spawn(GetWorld(), Pd - FVector(0, 0, ZoogiConst::ZoogiRadius - 4.f), Sc, 260.f, 0.6f, true, 10.f);
					ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.9f, 0.9f);
				}
				AZoogiFabFX::Play(GetWorld(),'''),
 ('''			PlayFighter(1, FormSignature(Clash.F[1].Form), 0.9f, false);
			const float Dmg = DmgCounter * (Clash.VPick == 2 ? 0.5f : 1.f);''', '''			PlayFighter(1, FormSignature(Clash.F[1].Form), 0.9f, false);
			if (Clash.bField) { const FVector Pa = A->BallLocation(), Pd = D->BallLocation(); for (int32 k = 1; k <= 8; ++k) AZoogiFX::Spawn(GetWorld(), FMath::Lerp(Pd, Pa, k / 8.f) + FVector(0, 0, 30.f), FLinearColor(0.4f, 0.85f, 1.f), 70.f + 10.f * k, 0.25f + 0.03f * k, false, 12.f); ZoogiUtil::PlaySfx(G, TEXT("Whoosh"), 0.9f, 1.1f); } // b104
			const float Dmg = DmgCounter * (Clash.VPick == 2 ? 0.5f : 1.f);'''),
])
print('b104a ok')
