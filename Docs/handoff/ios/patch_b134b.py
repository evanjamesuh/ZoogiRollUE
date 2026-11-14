# b134b (Evan 07:08 + 07:22, Golden Apple): the apple forms on the caster (Apple of Eden), THEN a golden apple is lobbed in an arc to where the
# tapped orb was (it lands there even if the orb moved), and only when it lands does a bigger blast go off (650 cm, ~1.3x a wolf blast),
# which throws every orb around it - the tapped one too (no gold orb any more). -ZoogiOldGoldenApple / -ZoogiAppleInstant = old versions.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''				else
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector Oc = O->OrbLocation();''', '''				else if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiAppleInstant")))
				{	// b134 (Evan: "the blast happens where the apple lands and only after the apple does its animation"): form -> lob -> land -> blast
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector From = Caster->BallLocation() + FVector(0.f, 0.f, 110.f);
					const FVector To(O->OrbLocation().X, O->OrbLocation().Y, FZ + 30.f); // where the orb is now - it lands there even if the orb moves
					const float FormT = 0.7f, FlyT = FMath::Clamp(0.55f + FVector::Dist2D(From, To) / 2600.f, 0.65f, 1.3f);
					FActorSpawnParameters SPa; SPa.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					TWeakObjectPtr<AActor> WApple;
					if (AStaticMeshActor* Ap = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(From), SPa))
					{
						Ap->SetMobility(EComponentMobility::Movable);
						UStaticMeshComponent* Am = Ap->GetStaticMeshComponent();
						if (UStaticMesh* AppleM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/StylizedToonVFX3/Models/SM_Apple.SM_Apple"), nullptr, LOAD_NoWarn | LOAD_Quiet))
						{
							Am->SetStaticMesh(AppleM);
							Am->SetRelativeScale3D(FVector(70.f / FMath::Max(1.f, AppleM->GetBoundingBox().GetSize().GetMax())));
						}
						else { Am->SetStaticMesh(ZoogiUtil::BasicShape(TEXT("Sphere"))); Ap->SetActorScale3D(FVector(0.6f)); }
						Am->SetMaterial(0, ZoogiUtil::GlowMID(Ap, FLinearColor(1.f, 0.78f, 0.2f), 4.f));
						Am->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Am->SetCastShadow(false);
						Ap->SetActorHiddenInGame(true);
						WApple = Ap;
					}
					const double T0 = FPlatformTime::Seconds();
					const int32 PiA = Pi; const float FZA = FZ;
					FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WG, WApple, From, To, FormT, FlyT, T0, PiA, FZA](float) -> bool
					{
						AZoogiGameMode* Gm = WG.Get();
						AActor* Ap = WApple.Get();
						if (!Gm) { if (Ap) Ap->Destroy(); return false; }
						const float T = (float)(FPlatformTime::Seconds() - T0);
						if (T < FormT) return true; // the Apple of Eden forms on the caster
						const float K = FMath::Clamp((T - FormT) / FlyT, 0.f, 1.f);
						FVector P = FMath::Lerp(From, To, K); P.Z += 4.f * FMath::Max(260.f, FVector::Dist2D(From, To) * 0.32f) * K * (1.f - K); // the lob
						if (Ap) { Ap->SetActorHiddenInGame(false); Ap->SetActorLocation(P); Ap->AddActorWorldRotation(FRotator(0.f, 9.f, 4.f)); }
						static double LastSpark = 0.0;
						if (FPlatformTime::Seconds() - LastSpark > 0.05) { LastSpark = FPlatformTime::Seconds(); AZoogiFX::Spawn(Gm->GetWorld(), P, FLinearColor(1.f, 0.8f, 0.25f), 55.f, 0.3f, false, 10.f); }
						if (K < 1.f) return true;
						if (Ap) Ap->Destroy();
						// lands: the blast (bigger than a wolf's, the tapped orb goes flying too)
						UWorld* Wg = Gm->GetWorld();
						const FVector Oc(To.X, To.Y, FZA + 40.f);
						const float R = 650.f;
						int32 Pushed = 0;
						for (AZoogiOrb* Ob : Gm->Orbs)
						{
							if (!Ob || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
							FVector D = Ob->OrbLocation() - Oc; D.Z = 0.f;
							const float Dist = D.Size();
							if (Dist > R) continue;
							const FVector Dir = Dist > 1.f ? D / Dist : FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
							const float Kk = 1.f - Dist / R;
							const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
							Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (520.f + 1700.f * Kk) + FVector(0.f, 0.f, 200.f * Kk));
							Ob->LastHitBy = PiA;
							++Pushed;
						}
						const FVector Ground(Oc.X, Oc.Y, FZA + 4.f);
						const FLinearColor Gold(1.f, 0.8f, 0.25f);
						AZoogiFabFX::Play(Wg, FX(TEXT("NS_Apple_of_Eden")), Ground, Sc(FName(TEXT("golden_apple")), 0.3f) * 1.3f, 2.f);
						AZoogiFX::Spawn(Wg, Oc, FLinearColor(1.f, 0.95f, 0.7f), 160.f, 0.25f, false, 16.f);
						AZoogiFX::Spawn(Wg, Ground, Gold, R, 0.6f, true, 10.f);
						AZoogiFX::Shockwave(Wg, Ground, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Gold, false);
						UZoogiScreenFx::Shockwave(Wg, Ground, R + 80.f, Gold);
						AZoogiFX::Stylized(Wg, TEXT("Mid_Explosion"), Ground, 0.55f);
						ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 0.9f, 1.05f);
						UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: the apple lands - blast r %.0f pushed %d orbs"), R, Pushed);
						return false;
					}));
					UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: Eden forms on P%d, lob %.0f cm in %.2f s"), Pi + 1, FVector::Dist2D(From, To), FlyT);
				}
				else
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector Oc = O->OrbLocation();''')
ed1(S, '''				O->bStar = true; O->PowerColor = FLinearColor(1.f, 0.8f, 0.25f);
				AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(1.f, 0.8f, 0.25f), 120.f, 30.f, true, 6.f, O);''',
'''				if (bOldApple || FParse::Param(FCommandLine::Get(), TEXT("ZoogiAppleInstant")))
				{	// b134 (Evan: "it doesn't turn gold, it just gets blasted away"): only the old versions make a gold orb
					O->bStar = true; O->PowerColor = FLinearColor(1.f, 0.8f, 0.25f);
					AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(1.f, 0.8f, 0.25f), 120.f, 30.f, true, 6.f, O);
				}''')
print('b134b ok')
