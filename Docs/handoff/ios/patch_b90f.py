# b90f (Evan 16:12): METEOR STRIKE (the Explosion ultimate) brings the meteor rain - meteors land across the arena, each a Wolf Pack
# blast x1.2; the METEOR RAIN skill becomes one big meteor that zooms down like a missile, sonic wave on impact, Wolf Pack blast.
# -ZoogiOldMeteorRain = the 7 small meteors; -ZoogiNoUltMeteors = the plain Meteor Strike blast.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);\n',
  '''	ZOOGIROLL_API void WolfTick(AZoogiGameMode* G);
	/** b90f: a Wolf Pack clone's detonation at At (orbs + rival Zoogis), Mul scales radius / force (1 = a wolf) */
	ZOOGIROLL_API void WolfBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, float Mul);
	/** b90f: meteor rain over Ctr (radius R): Count meteors land one after another, each a WolfBlast(Mul) */
	ZOOGIROLL_API void MeteorRain(AZoogiGameMode* G, const FVector& Ctr, float R, int32 Owner, int32 Count, float Mul);
'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('#include "ZoogiScreenFx.h"\n', '#include "ZoogiScreenFx.h"\n#include "Containers/Ticker.h"\n'),
 ('''		else if (Id == FName(TEXT("meteor_rain")))
		{''', '''		else if (Id == FName(TEXT("meteor_rain")) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMeteorRain")))
		{	// b90f (Evan: "Meteor Rain becomes one big strike zooming fast down like a missile, a sonic wave, a blast equal to a Wolf Pack blast")
			const FVector Hit = At(P.Ctr);
			FVector Dir = Hit - Me; Dir.Z = 0.f; if (!Dir.Normalize()) Dir = FVector::ForwardVector;
			const FVector From = Hit - Dir * 900.f + FVector(0.f, 0.f, 3000.f);
			AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Hit, 1.f);
			AZoogiFX::Spawn(W, Hit + FVector(0, 0, 4.f), FLinearColor(1.f, 0.3f, 0.1f), 300.f, 0.9f, true, 8.f); // target ring
			TWeakObjectPtr<AActor> WM;
			{
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* Mt = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(From), SP))
				{
					Mt->SetMobility(EComponentMobility::Movable);
					Mt->GetStaticMeshComponent()->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere")));
					Mt->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Mt->GetStaticMeshComponent()->SetMaterial(0, ZoogiUtil::FxMID(Mt, FLinearColor(1.f, 0.45f, 0.1f), 14.f));
					Mt->SetActorScale3D(FVector(1.5f));
					WM = Mt;
				}
			}
			TSharedRef<float> Tm = MakeShared<float>(0.f); TSharedRef<float> Spk = MakeShared<float>(0.f); TSharedRef<bool> bBoom = MakeShared<bool>(false);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, WM, From, Hit, Pi, Tm, Spk, bBoom](float Dt) -> bool
			{
				AZoogiGameMode* Gm = WG.Get();
				if (!Gm) { if (AActor* A = WM.Get()) A->Destroy(); return false; }
				UWorld* Wm = Gm->GetWorld();
				*Tm += FMath::Min(Dt, 0.05f);
				const float K = FMath::Clamp(*Tm / 0.5f, 0.f, 1.f), Ke = K * K; // accelerating dive
				const FVector Pm = FMath::Lerp(From, Hit + FVector(0, 0, 40.f), Ke);
				if (AActor* A = WM.Get()) A->SetActorLocation(Pm);
				*Spk += Dt;
				if (*Spk > 0.03f) { *Spk = 0.f; AZoogiFX::Spawn(Wm, Pm, FLinearColor(1.f, 0.55f, 0.15f), 120.f, 0.35f, false, 10.f); }
				if (!*bBoom && K > 0.55f) { *bBoom = true; AZoogiFX::Spawn(Wm, Pm, FLinearColor(1.f, 1.f, 1.f), 360.f, 0.3f, false, 6.f); ZoogiUtil::PlaySfx(Gm, TEXT("Zap"), 0.8f, 0.5f); } // the sonic boom
				if (K < 1.f) return true;
				if (AActor* A = WM.Get()) A->Destroy();
				// impact: the sonic wave + a Wolf Pack blast
				AZoogiFabFX::Play(Wm, FX(TEXT("NS_Meteor_hit")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f) * 2.f, 2.5f);
				UZoogiScreenFx::Shockwave(Wm, Hit, 900.f, FLinearColor(1.f, 0.95f, 0.85f));
				AZoogiFX::ShockwaveByMag(Wm, Hit, 40000.f, nullptr, false, false);
				AZoogiFX::Spawn(Wm, Hit + FVector(0, 0, 6.f), FLinearColor(1.f, 1.f, 1.f), 900.f, 0.45f, true, 6.f);
				WolfBlast(Gm, Hit, Pi, 1.f);
				UE_LOG(LogZoogi, Log, TEXT("SKILL meteor_rain: the meteor strikes (%.0f, %.0f)"), Hit.X, Hit.Y);
				return false;
			}));
			if (UZoogiFunSys* Fm = UZoogiFunSys::Get(G)) Fm->PublicCallout(TEXT("METEOR STRIKE!"), TEXT("INCOMING"), FLinearColor(1.f, 0.5f, 0.2f), 1.2f, 1.f);
		}
		else if (Id == FName(TEXT("meteor_rain")))
		{'''),
 ('''	bool AutoCast(AZoogiGameMode* G, FName Id)''',
  '''	void WolfBlast(AZoogiGameMode* G, const FVector& At, int32 Owner, float Mul)
	{
		if (!G) return;
		UWorld* W = G->GetWorld();
		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)); // a wolf's ShockR
		int32 N = 0;
		auto Push = [&](UPrimitiveComponent* Body, const FVector& L, float Scale) -> bool
		{
			if (!Body || !Body->IsSimulatingPhysics()) return false;
			FVector D = L - At; D.Z = 0.f;
			const float Dist = D.Size();
			if (Dist > R) return false;
			const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
			const float K = 1.f - Dist / R;
			const FVector V = Body->GetPhysicsLinearVelocity();
			Body->WakeAllRigidBodies();
			Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) * Mul * Scale + FVector(0.f, 0.f, 160.f * K * Mul));
			++N; return true;
		};
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Push(O->Sphere, O->OrbLocation(), 1.f)) O->LastHitBy = Owner;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->PlayerIndex != Owner && Push(B->Sphere, B->BallLocation(), 0.6f)) B->LastHitPlayer = Owner;
		const FLinearColor Col(1.f, 0.5f, 0.15f);
		AZoogiFX::Spawn(W, At + FVector(0, 0, 40.f), FLinearColor(1.f, 0.9f, 0.7f), 120.f, 0.22f, false, 14.f);
		AZoogiFX::Spawn(W, At + FVector(0, 0, 4.f), Col, R, 0.55f, true, 10.f);
		AZoogiFX::Shockwave(W, At, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Col, false);
		UZoogiScreenFx::Shockwave(W, At, R + 60.f, Col);
		AZoogiFX::Stylized(W, TEXT("Big_Explosion_v2"), At, 0.45f * Mul);
		ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.9f, 1.f);
		ZoogiHaptics::Kick(0.6f, W, TEXT("wolf blast"));
		UE_LOG(LogZoogi, Log, TEXT("SKILL wolf-size blast x%.2f at (%.0f, %.0f): %d pushed"), Mul, At.X, At.Y, N);
	}

	void MeteorRain(AZoogiGameMode* G, const FVector& Ctr, float R, int32 Owner, int32 Count, float Mul)
	{
		if (!G) return;
		UWorld* W = G->GetWorld();
		TWeakObjectPtr<AZoogiGameMode> WG = G;
		const int32 V = 1 + (FMath::Max(Owner, 0) * 3 + FMath::RandRange(0, 2)) % 12;
		AZoogiFabFX::Play(W, *FString::Printf(TEXT("/Game/MeteorRain/Particles/NiagaraSystems/NS_MeteorRainV%02d.NS_MeteorRainV%02d"), V, V), Ctr, 0.25f * FMath::Max(1.f, R / 1030.f), 4.5f);
		AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Ctr, 0.9f);
		// aim at the rivals and the orbs first, then fill in at random
		TArray<FVector> Spots;
		for (AZoogiBall* B : G->Balls) if (B && !B->bOut && B->PlayerIndex != Owner && FVector::Dist2D(B->BallLocation(), Ctr) < R) Spots.Add(B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius));
		for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut && Spots.Num() < Count && FVector::Dist2D(O->OrbLocation(), Ctr) < R && FMath::FRand() < 0.5f) Spots.Add(O->OrbLocation());
		while (Spots.Num() < Count) { const float A = FMath::FRandRange(0.f, 2.f * PI), Rr = FMath::Sqrt(FMath::FRand()) * R * 0.85f; Spots.Add(Ctr + FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 0.f)); }
		if (Spots.Num() > Count) Spots.SetNum(Count);
		for (int32 k = 0; k < Spots.Num(); ++k)
		{
			const FVector Hit = Spots[k];
			Later(G, 0.55f + 0.22f * k, [WG, Hit, Owner, Mul, k]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				AZoogiFabFX::Play(Gm->GetWorld(), FX(k % 2 ? TEXT("NS_Meteor_hit") : TEXT("NS_Meteor_hit_2")), Hit, Sc(FName(TEXT("meteor_hit")), 0.22f) * 1.5f, 2.5f);
				WolfBlast(Gm, Hit, Owner, Mul);
			});
		}
		UE_LOG(LogZoogi, Log, TEXT("SKILL meteor rain: %d meteors over r %.0f (x%.2f wolf blasts)"), Spots.Num(), R, Mul);
	}

	bool AutoCast(AZoogiGameMode* G, FName Id)'''),
])
ed(P + 'ZoogiSuperFun.cpp', [
 ('''		Blast(G, L, 720.f, 650.f, 1500.f, 320.f, B, true, true);
		DamageArena(G, L, 800.f, B, 3);
		break;''', '''		Blast(G, L, 720.f, 650.f, 1500.f, 320.f, B, true, true);
		DamageArena(G, L, 800.f, B, 3);
		// b90f (Evan: "add the meteor rain to Meteor Strike - the blasts that land on the arena equal Wolf Pack blasts, slightly more devastating")
		if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoUltMeteors")))
			ZoogiSkills::MeteorRain(G, G->Arena ? G->Arena->GetActorLocation() : L, (G->Arena ? G->Arena->GetKnockoffRadius() : 1800.f) * 0.75f, B->PlayerIndex, 8, 1.2f);
		break;'''),
])
print('b90f ok')
