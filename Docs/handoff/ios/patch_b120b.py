# b120b (Evan: "add the weapons we created in tripo starting with the grenade, sword, staff, disc"): Battle Royale robot form carries
# the equipped weapon (FZoogiFrontProfile::EquippedWeapon, -ZoogiWeapon=<key> overrides, -ZoogiWeaponCycle = capture runs cycle them):
#   GRENADE (SM_RelicLantern = the Prism Grenade Tripo model): lobbed along the aim, bursts on landing - area knockback
#   SWORD   (SM_RelicSword = Crystal Blade): dash + wide slash in front
#   STAFF   (SM_RelicStaff = Scepter): heavy magic bolts (x2.2 power, slower) from the orb on top
#   DISC    (SM_RelicEmblem = Halo Ring): thrown like a boomerang, cuts through rivals out and back
# The Gatling stays the default (and hides while another weapon is equipped). Relic pick-ups still work on top.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
P = 'Private/ZoogiRoyaleHero.cpp'
ed1(P, '''#include "ZoogiPlayerController.h"''', '''#include "ZoogiPlayerController.h"
#include "ZoogiFrontProfile.h"''')
ed1(P, '''		C->SetRelativeScale3D(FVector(MaxDim / FMath::Max(1.f, E.GetMax())));
	}
}''', '''		C->SetRelativeScale3D(FVector(MaxDim / FMath::Max(1.f, E.GetMax())));
	}
	// ---- b120 weapons (the robot form's weapon, picked in the inventory WEAPON tab)
	enum EWeapon : int32 { WGatling, WGrenade, WSword, WStaff, WDisc, NumWeapons };
	static const TCHAR* WeaponKey[NumWeapons] = { TEXT("gatling"), TEXT("grenade"), TEXT("sword"), TEXT("staff"), TEXT("disc") };
	static const TCHAR* WeaponName[NumWeapons] = { TEXT("GATLING"), TEXT("PRISM GRENADE"), TEXT("CRYSTAL SWORD"), TEXT("CRYSTAL STAFF"), TEXT("HALO DISC") };
	static const int32 WeaponRelic[NumWeapons] = { -1, Lantern, Blade, Staff, Emblem }; // the Tripo meshes (/Game/Relics/*)
	static const float WeaponSize[NumWeapons] = { 0.f, 46.f, 125.f, 150.f, 82.f };   // longest side in the hand, cm
	static const FLinearColor WeaponCol(0.78f, 0.62f, 1.f);
	int32 Weapon = WGatling;
	float WSwing = 0.f;
	TWeakObjectPtr<UStaticMeshComponent> WeaponMesh;
	struct FThrow { TWeakObjectPtr<AActor> A; FVector P = FVector::ZeroVector, V = FVector::ZeroVector, Dir = FVector::ForwardVector; float Age = 0.f, Spin = 0.f; int32 Kind = 0; bool bBack = false; TSet<int32> Hit; };
	TArray<FThrow> Throws;
	bool DiscOut() { for (const FThrow& Tw : Throws) if (Tw.Kind == WDisc) return true; return false; }
	int32 WeaponFromKey(const FString& K) { for (int32 i = 0; i < NumWeapons; ++i) if (K.Equals(WeaponKey[i], ESearchCase::IgnoreCase)) return i; return WGatling; }
	/** puts a fitted relic mesh so its bounds centre lands on Ctr */
	void PlaceMesh(UStaticMeshComponent* C, const FVector& Ctr, const FRotator& Rot)
	{
		if (!C || !C->GetStaticMesh()) return;
		const FVector O = C->GetStaticMesh()->GetBounds().Origin * C->GetComponentScale();
		C->SetWorldLocationAndRotation(Ctr - Rot.RotateVector(O), Rot);
	}
	void BuildWeaponMesh(AActor* A, USkeletalMeshComponent* SK)
	{
		if (UStaticMeshComponent* Old = WeaponMesh.Get()) Old->DestroyComponent();
		WeaponMesh = nullptr;
		if (!A || !SK || Weapon == WGatling) return;
		UStaticMesh* M = LoadRelic(WeaponRelic[Weapon]);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(true);
		C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
		if (M) { C->SetStaticMesh(M); FitMesh(C, M, WeaponSize[Weapon]); }
		else { C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Cone"))); C->SetRelativeScale3D(FVector(0.3f)); C->SetMaterial(0, ZoogiUtil::GlowMID(A, WeaponCol, 2.f)); }
		C->RegisterComponent(); C->AttachToComponent(SK, FAttachmentTransformRules::KeepWorldTransform);
		WeaponMesh = C;
		UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: %s in hand (%s)"), WeaponName[Weapon], M ? *M->GetName() : TEXT("mesh missing - glow cone"));
	}
	AActor* SpawnThrown(UWorld* W, int32 Kind, const FVector& At)
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), SP);
		if (!A) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A, TEXT("Thrown"));
		UStaticMesh* M = LoadRelic(WeaponRelic[Kind]);
		if (M) { C->SetStaticMesh(M); FitMesh(C, M, WeaponSize[Kind] * (Kind == WDisc ? 1.25f : 1.f)); }
		else { C->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); C->SetRelativeScale3D(FVector(0.35f)); C->SetMaterial(0, ZoogiUtil::GlowMID(A, WeaponCol, 2.f)); }
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(true);
		A->SetRootComponent(C); C->RegisterComponent();
		return A;
	}
}''')
ed1(P, '''	Relics.Reset(); Held = FHeld(); H = FState(); NextDrop = 18.f; NextType = 0;''', '''	Relics.Reset(); Held = FHeld(); H = FState(); NextDrop = 18.f; NextType = 0;
	for (FThrow& Tw : Throws) if (AActor* Ta = Tw.A.Get()) Ta->Destroy();
	Throws.Reset(); WeaponMesh = nullptr; WSwing = 0.f;
	{	// b120: the equipped weapon (inventory WEAPON tab); -ZoogiWeapon=<key> overrides
		FString Wk;
		if (const UZoogiFrontProfileSubsystem* FPs = UZoogiFrontProfileSubsystem::Get(this)) if (!FPs->Data().EquippedWeapon.IsNone()) Wk = FPs->Data().EquippedWeapon.ToString();
		FParse::Value(FCommandLine::Get(), TEXT("ZoogiWeapon="), Wk);
		Weapon = WeaponFromKey(Wk);
		if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeaponCycle"))) Weapon = WGrenade;
		UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon equipped: %s"), WeaponName[Weapon]);
	}''')
ed1(P, '''	H.Actor = A; H.Mesh = SK; H.Yaw = Royale.P.IsValidIndex(H.Owner) ? Royale.P[H.Owner].FacingYaw : 0.f;''', '''	H.Actor = A; H.Mesh = SK; H.Yaw = Royale.P.IsValidIndex(H.Owner) ? Royale.P[H.Owner].FacingYaw : 0.f;
	BuildWeaponMesh(A, SK); // b120''')
ed1(P, '''			if (Royale.P[H.Owner].bRobot) { Royale.P[H.Owner].MorphCD = 0.f; Royale.P[H.Owner].FireCD = 0.f; H.bStartMech = false; H.bWasRobot = true; }''',
'''			if (Royale.P[H.Owner].bRobot) { Royale.P[H.Owner].MorphCD = 0.f; Royale.P[H.Owner].FireCD = 0.f; H.bStartMech = false; H.bWasRobot = true; if (Weapon != WGatling && B) AddPopup(FString::Printf(TEXT("%s!"), WeaponName[Weapon]), B->BallLocation() + FVector(0, 0, 170.f), WeaponCol); }''')
ed1(P, '''				const bool bRifleOn = !SK->bHiddenInGame && R->bRobot && !Held.InHand.IsValid();''', '''				const bool bRifleOn = !SK->bHiddenInGame && R->bRobot && !Held.InHand.IsValid() && Weapon == WGatling; // b120: another weapon equipped -> no Gatling''')
ed1(P, '''			if (R->HurtAt > H.LastHurt + 0.01f && Now - R->HurtAt < 0.2f) { H.LastHurt = R->HurtAt;''', '''			// b120: the equipped weapon in the right hand (sword swings with each cut; the disc is gone while it flies)
			{
				static const bool bCycle = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeaponCycle"));
				static float CycleAt = 9.f;
				if (bCycle && Royale.Step == EZoogiRoyaleStep::Fight && Royale.FightTime > CycleAt)
				{
					CycleAt = Royale.FightTime + 9.f;
					Weapon = Weapon % (NumWeapons - 1) + 1; // grenade -> sword -> staff -> disc -> grenade
					BuildWeaponMesh(H.Actor.Get(), SK);
					AddPopup(FString::Printf(TEXT("%s!"), WeaponName[Weapon]), L + FVector(0, 0, 170.f), WeaponCol);
				}
			}
			WSwing = FMath::Max(0.f, WSwing - Dt * 3.2f);
			if (UStaticMeshComponent* Wm = WeaponMesh.Get())
			{
				const bool bWOn = !SK->bHiddenInGame && R->bRobot && !Held.InHand.IsValid() && !(Weapon == WDisc && DiscOut());
				Wm->SetHiddenInGame(!bWOn);
				if (bWOn)
				{
					const FVector Hand = !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : L + FVector(0, 0, 60.f);
					const FVector Fwd = FRotator(0.f, H.Yaw, 0.f).Vector(), Up = FVector::UpVector;
					const float Sz = WeaponSize[Weapon];
					if (Weapon == WSword)
					{	// raised over the shoulder, cutting down and forward on a swing
						const float K = FMath::Sin(FMath::Clamp(1.f - WSwing, 0.f, 1.f) * PI * 0.5f);
						const FVector Ax = WSwing > 0.f ? FMath::Lerp(Fwd * 0.9f - Up * 0.35f, Up * 0.85f + Fwd * 0.45f, K).GetSafeNormal() : (Up * 0.85f + Fwd * 0.45f).GetSafeNormal();
						PlaceMesh(Wm, Hand + Ax * Sz * 0.36f, FRotationMatrix::MakeFromZX(Ax, Fwd).Rotator());
					}
					else if (Weapon == WStaff) { const FVector Ax = (Up + Fwd * 0.18f).GetSafeNormal(); PlaceMesh(Wm, Hand + Ax * Sz * 0.18f, FRotationMatrix::MakeFromZX(Ax, Fwd).Rotator()); }
					else if (Weapon == WDisc) PlaceMesh(Wm, Hand + Fwd * 18.f + Up * 8.f, FRotator(0.f, H.Yaw - 90.f, 0.f)); // ring facing the camera behind
					else PlaceMesh(Wm, Hand + Fwd * 8.f + Up * 4.f, FRotator(0.f, H.Yaw, 0.f));
				}
			}
			if (R->HurtAt > H.LastHurt + 0.01f && Now - R->HurtAt < 0.2f) { H.LastHurt = R->HurtAt;''')
ed1(P, '''	// ---- relics on the field: spin / bob, expire, picked up by the human''', '''	// ---- b120 thrown weapons: the grenade (arc, bursts on landing) and the halo disc (boomerang)
	if (Throws.Num())
	{
		const FZoogiRoyaleTune& Tn = FZoogiRoyaleTune::Get();
		const int32 Me = H.Owner;
		AZoogiBall* MyBall = Balls.IsValidIndex(Me) ? Balls[Me].Get() : nullptr;
		auto HitJ = [&](int32 j, const FVector& Push, int32 Dmg)
		{
			FZoogiRoyaleKick K; K.Victim = j; K.DeltaV = Push; Royale.Kicks.Add(K);
			Royale.P[j].LastHitBy = Me; Royale.P[j].LastHitAt = Clock; Royale.P[j].StunLeft = FMath::Max(Royale.P[j].StunLeft, 0.5f);
			RoyaleDamage(j, Me, Dmg, true, Balls[j]->BallLocation());
		};
		auto ForRivals = [&](TFunctionRef<void(int32, const FVector&)> Fn)
		{
			for (int32 j = 0; j < Balls.Num() && j < Royale.P.Num(); ++j)
				if (j != Me && Balls[j] && !Balls[j]->bOut && Royale.P[j].IsIn() && Royale.P[j].RespawnIn < 0.f && Royale.P[j].ShieldLeft <= 0.f) Fn(j, Balls[j]->BallLocation());
		};
		FCollisionQueryParams Q(FName(TEXT("ZoogiThrow")), false);
		for (int32 k = Throws.Num() - 1; k >= 0; --k)
		{
			FThrow& Tw = Throws[k];
			AActor* A = Tw.A.Get();
			Tw.Age += Dt;
			bool bDone = !A || Royale.Step == EZoogiRoyaleStep::Over;
			if (!bDone && Tw.Kind == WGrenade)
			{
				const FVector P0 = Tw.P;
				Tw.V.Z -= 1800.f * Dt;
				Tw.P += Tw.V * Dt;
				FHitResult Hr;
				bool bBoom = GetWorld()->LineTraceSingleByObjectType(Hr, P0, Tw.P, FCollisionObjectQueryParams(ECC_WorldStatic), Q);
				if (bBoom) Tw.P = Hr.ImpactPoint + Hr.ImpactNormal * 10.f;
				ForRivals([&](int32 j, const FVector& Pj) { if (FVector::Dist(Pj, Tw.P) < ZoogiConst::ZoogiRadius + 30.f) bBoom = true; });
				if (Tw.Age > 3.f || Tw.P.Z < ZoogiConst::FallOutZ) bBoom = true;
				A->SetActorLocationAndRotation(Tw.P, FRotator(-Tw.Age * 620.f, Tw.Dir.Rotation().Yaw, 0.f));
				if (FMath::Fmod(Tw.Age, 0.05f) < Dt) AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 34.f, 0.25f, false, 8.f); // sparkle trail
				if (bBoom)
				{
					int32 N = 0;
					ForRivals([&](int32 j, const FVector& Pj) { const FVector D = Pj - Tw.P; if (D.Size2D() < 430.f && FMath::Abs(D.Z) < 400.f) { HitJ(j, D.GetSafeNormal2D() * Tn.BoltKnock * 2.6f + FVector(0, 0, 260.f), 85); ++N; } });
					AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 430.f, 0.6f, true, 10.f);
					AZoogiFX::Spawn(GetWorld(), Tw.P + FVector(0, 0, 50.f), FLinearColor::White, 200.f, 0.25f, false, 14.f);
					AZoogiFX::Stylized(GetWorld(), TEXT("ElectricShock"), Tw.P, 1.1f);
					ZoogiUtil::PlaySfx(this, TEXT("CritBoom"), 0.6f, 1.1f);
					UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: grenade burst at (%.0f,%.0f,%.0f) hit %d"), Tw.P.X, Tw.P.Y, Tw.P.Z, N);
					bDone = true;
				}
			}
			else if (!bDone && Tw.Kind == WDisc)
			{
				const FVector P0 = Tw.P;
				if (!Tw.bBack)
				{
					Tw.P += Tw.Dir * 1900.f * Dt;
					FHitResult Hr;
					if (Tw.Age > 0.6f || GetWorld()->LineTraceSingleByObjectType(Hr, P0, Tw.P, FCollisionObjectQueryParams(ECC_WorldStatic), Q)) { Tw.bBack = true; Tw.Hit.Reset(); }
				}
				else
				{
					const FVector Home = MyBall ? MyBall->BallLocation() + FVector(0, 0, 70.f) : Tw.P;
					const FVector D = Home - Tw.P;
					if (!MyBall || D.Size() < 120.f || Tw.Age > 3.5f) { bDone = true; if (Royale.P.IsValidIndex(Me)) Royale.P[Me].FireCD = FMath::Min(Royale.P[Me].FireCD, 0.12f); }
					else Tw.P += D.GetSafeNormal() * 2300.f * Dt;
				}
				const FVector Mv = (Tw.P - P0).GetSafeNormal2D();
				ForRivals([&](int32 j, const FVector& Pj) { if (!Tw.Hit.Contains(j) && FVector::Dist(Pj, Tw.P) < ZoogiConst::ZoogiRadius + 95.f) { Tw.Hit.Add(j); HitJ(j, Mv * Tn.BoltKnock * 1.8f + FVector(0, 0, 140.f), 45); AZoogiFX::Spawn(GetWorld(), Pj, WeaponCol, 150.f, 0.3f, false, 12.f); } });
				Tw.Spin += 1500.f * Dt;
				A->SetActorLocationAndRotation(Tw.P, FRotator(0.f, Tw.Spin, 90.f)); // flat, spinning
				if (FMath::Fmod(Tw.Age, 0.04f) < Dt) AZoogiFX::Spawn(GetWorld(), Tw.P, WeaponCol, 60.f, 0.2f, false, 10.f);
			}
			if (bDone) { if (A) A->Destroy(); Throws.RemoveAtSwap(k); }
		}
	}
	// ---- relics on the field: spin / bob, expire, picked up by the human''')
ed1(P, '''	if (Held.Type == Staff) { Power = 2.5f; Spend(); }''', '''	// b120: the equipped weapon (relics above still take over while held)
	if (Weapon != WGatling && Me && R.bRobot)
	{
		const FVector Hand = SK && !H.HandBone.IsNone() ? SK->GetBoneLocation(H.HandBone) : L + FVector(0, 0, 60.f);
		if (Weapon == WGrenade)
		{	// lob it at the crosshair (3.5 - 17 m), ~1 s in the air
			const float Dist = FMath::Clamp(FVector::Dist2D(R.AimPoint, L), 350.f, 1700.f);
			const float Vz = 760.f, Tf = (Vz + FMath::Sqrt(Vz * Vz + 2.f * 1800.f * 120.f)) / 1800.f;
			FThrow Tw; Tw.Kind = WGrenade; Tw.P = Hand + Flat * 30.f; Tw.Dir = Flat; Tw.V = Flat * (Dist / Tf) + FVector(0, 0, Vz);
			Tw.A = SpawnThrown(GetWorld(), WGrenade, Tw.P);
			Throws.Add(Tw);
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.4f, 1.9f);
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.55f, 1.25f);
			R.FireCD = 0.85f;
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: grenade thrown (%.0f cm)"), Dist);
			return true;
		}
		if (Weapon == WSword)
		{	// dash + a wide cut in front (reach 4.6 m)
			int32 N = 0;
			{ FZoogiRoyaleKick Sk; Sk.Victim = Idx; Sk.DeltaV = Flat * 1000.f; Royale.Kicks.Add(Sk); }
			Rivals([&](int32 j, const FVector& Pj) { const FVector D = Pj - L; if (D.Size2D() < 460.f && FVector::DotProduct(D.GetSafeNormal2D(), Flat) > 0.25f) { Hit(j, Flat * T.BoltKnock * 3.f + FVector(0, 0, 200.f), 110); ++N; } });
			for (int32 a = -3; a <= 3; ++a) AZoogiFX::Spawn(GetWorld(), L + Flat.RotateAngleAxis(a * 18.f, FVector::UpVector) * 230.f + FVector(0, 0, 50.f), WeaponCol, 75.f, 0.22f, false, 12.f);
			if (SK) HeroAction(TEXT("slash"), 0.45f, 1.6f);
			WSwing = 1.f;
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.7f, 1.05f);
			R.FireCD = 0.5f;
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: sword slash hit %d"), N);
			return true;
		}
		if (Weapon == WDisc)
		{	// boomerang: out ~11 m along the aim, back to the hand; one disc at a time
			if (DiscOut()) { R.FireCD = 0.1f; return true; }
			FThrow Tw; Tw.Kind = WDisc; Tw.P = Hand + Flat * 40.f; Tw.Dir = Flat;
			Tw.A = SpawnThrown(GetWorld(), WDisc, Tw.P);
			Throws.Add(Tw);
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.35f, 2.f);
			ZoogiUtil::PlaySfx(this, TEXT("Whoosh"), 0.6f, 1.4f);
			R.FireCD = 5.f; // the return resets it
			UE_LOG(LogZoogi, Log, TEXT("ROYALE weapon: disc thrown"));
			return true;
		}
		if (Weapon == WStaff)
		{	// heavy magic bolts out of the orb on top (HEAVY ROUNDS relic stacks on it)
			Power = 2.2f;
			if (Held.Type == Staff) { Power *= 2.5f; Spend(); }
			if (const UStaticMeshComponent* Wm = WeaponMesh.Get()) Muzzle = Wm->Bounds.Origin + Wm->GetUpVector() * WeaponSize[WStaff] * 0.42f;
			if (SK && H.ActionLeft <= 0.1f) HeroAction(TEXT("cast_a_spell"), 0.25f, 2.f);
			AZoogiFX::Spawn(GetWorld(), Muzzle, WeaponCol, 60.f, 0.15f, false, 14.f);
			ZoogiUtil::PlaySfx(this, TEXT("Zap"), 0.35f, FMath::FRandRange(1.3f, 1.5f));
			R.FireCD = 0.32f;
			return false;
		}
	}
	if (Held.Type == Staff) { Power = 2.5f; Spend(); }''')
print('b120b ok')
