# b94 (Evan 17:34): the end of the duel plays the old loser sequence (the loser's form is sent flying - backstroke - then its crash
# anime clip); if the rival DEFENDED, a new anime-style guard forms instead: shield shards rush in and lock into a glowing dome
# around the defender, a flash and a shock ring, plus the defender's GuardBlock cut-in. -ZoogiDuelNoLoser = no flight.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiActionFun.h', [
 ('	void HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);\n', '	void HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);\n	void DuelV2GuardForm(AZoogiGameMode* G, AZoogiBall* Def);\n'),
 ('int32 VPick = -1, VRivalWant = 3; float VGradeAt = 0.f; // b90 duel v2', 'int32 VPick = -1, VRivalWant = 3; float VGradeAt = 0.f; float VDmgA = 0.f, VDmgD = 0.f; // b90 duel v2 (b94: damage taken each side)'),
])
ed(P + 'ZoogiDuelV2.cpp', [
 ('#include "Misc/Parse.h"\n', '#include "Misc/Parse.h"\n#include "Containers/Ticker.h"\n#include "Engine/StaticMeshActor.h"\n#include "Components/StaticMeshComponent.h"\n#include "ZoogiAnimeSubsystem.h"\n#include "ZoogiScreenFx.h"\n'),
 ('	constexpr float VIntro = 1.2f, VRing = 1.6f, VMenuMax = 10.f, VRivalMax = 6.f, VResolve = 2.4f;',
  '	constexpr float VIntro = 1.2f, VRing = 1.6f, VMenuMax = 10.f, VRivalMax = 6.f, VResolve = 2.4f, VResolveGuard = 3.1f;'),
 ('	float& Hp = Players[Victim->PlayerIndex].HP;\n	Hp = FMath::Max(0.f, Hp - Dmg);\n	const int32 Side = Victim == Clash.Def.Get() ? 1 : 0;',
  '	float& Hp = Players[Victim->PlayerIndex].HP;\n	Hp = FMath::Max(0.f, Hp - Dmg);\n	const int32 Side = Victim == Clash.Def.Get() ? 1 : 0;\n	(Side ? Clash.VDmgD : Clash.VDmgA) += Dmg; // b94: who lost the exchange'),
 ('				DuelV2Damage(G, D, A, Dmg, Clash.bVDefend ? TEXT("defended hit") : TEXT("hit"));',
  '				DuelV2Damage(G, D, A, Dmg, Clash.bVDefend ? TEXT("defended hit") : TEXT("hit"));\n				if (Clash.bVDefend && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoGuardForm"))) DuelV2GuardForm(G, D); // b94: the guard takes shape'),
 ('		if (Clash.DAge >= VResolve) { if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_e_result")); DuelV2End(G, true); return; }',
  '		if (Clash.DAge >= (Clash.bVDefend ? VResolveGuard : VResolve)) { if (G->ShotDir.Len()) G->TakeShot(TEXT("duel2_e_result")); DuelV2End(G, true); return; }'),
 ('''	EndFighters();
	const FVector At = Clash.Center;
	Clash = FClash();
	bResolveAfterClash = bFought && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashKeepRolling"));''',
  '''	// b94 (Evan): the loser of the exchange gets the old ending - sent flying in its form, then its crash anime clip (not after a
	// successful DEFEND - that played the guard forming - and not on a knockout, which blasts it off the arena)
	if (bFought && KOs.Num() == 0 && !Clash.bVDefend && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiDuelNoLoser")) && (Clash.VDmgA > 0.f || Clash.VDmgD > 0.f))
	{
		const bool bAttWinsX = Clash.VDmgD >= Clash.VDmgA;
		AZoogiBall* Lb = bAttWinsX ? D : A; AZoogiBall* Wb = bAttWinsX ? A : D;
		if (Lb && Lb->Sphere && !Lb->bOut)
		{
			const FVector Away = bAttWinsX ? Clash.Axis : -Clash.Axis;
			Lb->Sphere->WakeRigidBody();
			Lb->Sphere->SetPhysicsLinearVelocity(Away * 1150.f + FVector(0.f, 0.f, 480.f));
			if (Wb) Lb->LastHitPlayer = Wb->PlayerIndex;
			StartFlung(bAttWinsX ? 1 : 0); // takes the loser's fighter over: backstroke flight, then the crash clip on landing
			UE_LOG(LogZoogi, Log, TEXT("DUEL2 loser P%d sent flying (the classic ending + crash clip)"), Lb->PlayerIndex + 1);
		}
	}
	EndFighters();
	const FVector At = Clash.Center;
	Clash = FClash();
	bResolveAfterClash = bFought && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiClashKeepRolling"));'''),
 ('''void UZoogiFunSys::HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why)''',
  '''void UZoogiFunSys::DuelV2GuardForm(AZoogiGameMode* G, AZoogiBall* Def)
{
	// b94 (Evan: "if the opposing player defends successfully a new animation plays off the guard effect forming and taking shape in epic
	// anime action style"): 14 glowing shield shards rush in from all sides, lock into a dome around the defender's form, a white-hot
	// flash, a shock ring, then the dome pulses and fades; the defender's GuardBlock cut-in plays (in its profile corner)
	if (!G || !Def) return;
	UWorld* W = GetWorld();
	AActor* Fd = Clash.F[1].Actor.Get();
	const FVector C = (Fd ? Fd->GetActorLocation() : Def->BallLocation()) + FVector(0.f, 0.f, 70.f);
	const FLinearColor Cy(0.45f, 0.85f, 1.f), Pc = ZoogiUtil::PlayerColor(Def->PlayerIndex);
	struct FShard { TWeakObjectPtr<AActor> A; FVector From, To; FRotator Spin; };
	struct FForm { TArray<FShard> Shards; TWeakObjectPtr<AActor> Dome; float T = 0.f; bool bLocked = false; };
	TSharedRef<FForm> Fm = MakeShared<FForm>();
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float R = 150.f;
	for (int32 k = 0; k < 14; ++k)
	{
		const float Az = 2.f * PI * k / 7.f + (k >= 7 ? PI / 7.f : 0.f), El = k >= 7 ? 0.95f : 0.35f;
		const FVector Dir(FMath::Cos(Az) * FMath::Cos(El), FMath::Sin(Az) * FMath::Cos(El), FMath::Sin(El));
		FShard S; S.To = C + Dir * R; S.From = C + Dir * FMath::FRandRange(520.f, 720.f) + FVector(0, 0, FMath::FRandRange(-60.f, 200.f));
		S.Spin = FRotator(FMath::FRandRange(-720.f, 720.f), FMath::FRandRange(-720.f, 720.f), 0.f);
		if (AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Dir.Rotation(), S.From), SP))
		{
			A->SetMobility(EComponentMobility::Movable);
			A->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cube")));
			A->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			A->GetStaticMeshComponent()->SetCastShadow(false);
			A->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(A, k % 2 ? Cy : FMath::Lerp(Cy, Pc, 0.5f), 12.f));
			A->SetActorScale3D(FVector(0.75f, 0.75f, 0.06f)); // a flat hex-ish plate
			S.A = A;
		}
		Fm->Shards.Add(S);
	}
	if (UZoogiAnimeSubsystem* An = W->GetSubsystem<UZoogiAnimeSubsystem>())
	{
		FZoogiCombatEvent E; E.Event = FName(TEXT("GuardBlock")); E.Player = Def->PlayerIndex; E.ZoogiName = Def->Def.Name; E.Location = Def->BallLocation(); E.Magnitude = 1.f;
		An->Emit(E);
	}
	Callout(TEXT("GUARD FORMED!"), FString::Printf(TEXT("%s BLOCKS THE BLOW"), *Def->Def.Name.ToUpper()), Cy, 1.6f, 1.1f);
	ZoogiUtil::PlaySfx(G, TEXT("AwakenCharge"), 0.8f, 1.4f);
	TWeakObjectPtr<UWorld> WW = W;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Fm, WW, C, R, Cy, G](float Dt) -> bool
	{
		UWorld* Wl = WW.Get();
		auto Kill = [&Fm]() { for (FShard& S : Fm->Shards) if (AActor* A = S.A.Get()) A->Destroy(); if (AActor* D = Fm->Dome.Get()) D->Destroy(); };
		if (!Wl) { Kill(); return false; }
		Fm->T += FMath::Min(Dt, 0.05f);
		const float T = Fm->T;
		const float K = FMath::Clamp(T / 0.45f, 0.f, 1.f), Ke = 1.f - FMath::Pow(1.f - K, 4.f); // they slam into place
		for (FShard& S : Fm->Shards)
			if (AActor* A = S.A.Get())
			{
				A->SetActorLocation(FMath::Lerp(S.From, S.To, Ke));
				if (K < 1.f) A->AddActorWorldRotation(S.Spin * Dt * (1.f - K));
				else A->SetActorRotation((S.To - C).Rotation() + FRotator(90.f, 0.f, 0.f)); // locked flat onto the dome
			}
		if (!Fm->bLocked && K >= 1.f)
		{
			Fm->bLocked = true;
			FActorSpawnParameters Sp2; Sp2.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AStaticMeshActor* Dm = Wl->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(C), Sp2))
			{
				Dm->SetMobility(EComponentMobility::Movable);
				Dm->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
				Dm->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Dm->GetStaticMeshComponent()->SetCastShadow(false);
				Dm->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(Dm, Cy, 1.2f));
				Dm->SetActorScale3D(FVector(2.f * R / 100.f));
				Fm->Dome = Dm;
			}
			AZoogiFX::Spawn(Wl, C, FLinearColor(1.f, 1.f, 1.f), 2.4f * R, 0.25f, false, 20.f); // white-hot lock flash
			AZoogiFX::Spawn(Wl, C - FVector(0, 0, 70.f), Cy, 3.f * R, 0.6f, true, 10.f);
			UZoogiScreenFx::Shockwave(Wl, C, 3.f * R, Cy);
			AZoogiFX::Stylized(Wl, TEXT("ElectricShock"), C, 1.2f);
			ZoogiUtil::PlaySfx(G, TEXT("GuardClang"), 1.f, 1.f);
			ZoogiHaptics::Kick(0.9f, Wl, TEXT("guard formed"));
		}
		if (AActor* D = Fm->Dome.Get())
		{
			const float P = 1.f + 0.06f * FMath::Sin(T * 18.f);
			D->SetActorScale3D(FVector(2.f * R / 100.f * P * FMath::Max(0.f, 1.f - FMath::Max(0.f, T - 1.6f) / 0.4f)));
		}
		if (T < 2.f) return true;
		Kill();
		return false;
	}));
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 guard forms around P%d"), Def->PlayerIndex + 1);
}

void UZoogiFunSys::HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why)'''),
])
print('b94 ok')
