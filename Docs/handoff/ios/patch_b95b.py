# b95b (Evan 17:46): the Meteor Rain portal sits much higher and further away; a camera swings over to look up at the portal and
# follows the boulder down onto the target, then hands back to the match camera.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiSkills.cpp', [
 ('#include "Containers/Ticker.h"\n', '#include "Containers/Ticker.h"\n#include "Camera/CameraActor.h"\n#include "Camera/CameraComponent.h"\n'),
 ('const FVector Portal = Hit - Dir * 380.f + FVector(0.f, 0.f, 560.f); // low enough to stay in the aim camera',
  'const FVector Portal = Hit - Dir * 1100.f + FVector(0.f, 0.f, 1500.f); // b95b (Evan): much higher and further away - the meteor cam follows it'),
 ('struct FVoid { TWeakObjectPtr<AActor> Disc, Ring, Rock, Arcs; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f, Spark = 0.f, Arc = 0.f; bool bHit = false; };',
  'struct FVoid { TWeakObjectPtr<AActor> Disc, Ring, Rock, Arcs, Cam, PrevView; TArray<TWeakObjectPtr<UStaticMeshComponent>> Segs; float T = 0.f, Spark = 0.f, Arc = 0.f; bool bHit = false, bCamBack = false; FVector Side = FVector::RightVector, Fwd = FVector::ForwardVector; };'),
 ('''			AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Portal, 0.8f);
			AZoogiFX::Spawn(W, Hit + FVector(0, 0, 4.f), Pu, 300.f, 0.9f, true, 8.f); // target ring''',
  '''			AZoogiFabFX::Sound(W, SFX(TEXT("S_Fire_meteors")), Portal, 0.8f);
			AZoogiFX::Spawn(W, Hit + FVector(0, 0, 4.f), Pu, 300.f, 0.9f, true, 8.f); // target ring
			// b95b: the meteor cam - beside the target looking up at the portal, then down the boulder's dive (-ZoogiNoMeteorCam = none)
			V->Fwd = Dir; V->Side = FVector::CrossProduct(FVector::UpVector, Dir);
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoMeteorCam")))
				if (APlayerController* PCm = W->GetFirstPlayerController())
					if (ACameraActor* Cm = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(Hit + Dir * 900.f + V->Side * 650.f + FVector(0, 0, 380.f)), SP))
					{
						Cm->GetCameraComponent()->SetFieldOfView(80.f);
						Cm->GetCameraComponent()->bConstrainAspectRatio = false;
						Cm->SetActorRotation((Portal - Cm->GetActorLocation()).Rotation());
						V->PrevView = PCm->GetViewTarget(); V->Cam = Cm;
						PCm->SetViewTargetWithBlend(Cm, 0.35f, VTBlend_Cubic);
					}'''),
 ('''				// the boulder: out of the portal at 0.4 s, slams down at 0.85 s
				const float Kf = FMath::Clamp((T - 0.4f) / 0.45f, 0.f, 1.f);''',
  '''				// the boulder: out of the portal at 0.6 s, slams down at 1.35 s (b95b: the longer drop)
				const float Kf = FMath::Clamp((T - 0.6f) / 0.75f, 0.f, 1.f);'''),
 ('					Rk->SetActorHiddenInGame(T < 0.4f);', '					Rk->SetActorHiddenInGame(T < 0.6f);'),
 ('				const float Open = T < 0.35f ? T / 0.35f : (T > 1.3f ? FMath::Max(0.f, 1.f - (T - 1.3f) / 0.3f) : 1.f);',
  '				const float Open = T < 0.4f ? T / 0.4f : (T > 1.9f ? FMath::Max(0.f, 1.f - (T - 1.9f) / 0.35f) : 1.f);'),
 ('				const bool bArcs = T > 0.4f && T < 2.f;', '				const bool bArcs = T > 0.6f && T < 2.5f;'),
 ('''				if (T < 2.f) return true;
				Kill();
				return false;
			}));''',
  '''				// the meteor cam: eyes on the portal, then on the boulder, then the crater; back to the match camera at 2.4 s
				if (AActor* Cm = V->Cam.Get())
				{
					const FVector Want = V->bHit ? Hit + V->Fwd * 620.f + V->Side * 420.f + FVector(0, 0, 260.f) : FMath::Lerp(Hit + V->Fwd * 900.f + V->Side * 650.f + FVector(0, 0, 380.f), Hit + V->Fwd * 700.f + V->Side * 480.f + FVector(0, 0, 300.f), Kf);
					Cm->SetActorLocation(FMath::VInterpTo(Cm->GetActorLocation(), Want, Dt, 6.f));
					const FVector Look = V->bHit ? Hit + FVector(0, 0, 60.f) : (T < 0.6f ? Portal : RockAt);
					Cm->SetActorRotation(FMath::RInterpTo(Cm->GetActorRotation(), (Look - Cm->GetActorLocation()).Rotation(), Dt, V->bHit ? 5.f : 9.f));
					if (!V->bCamBack && T > 2.4f)
					{
						V->bCamBack = true;
						if (APlayerController* PCm = Wm->GetFirstPlayerController()) if (PCm->GetViewTarget() == Cm) { if (AActor* Pv = V->PrevView.Get()) PCm->SetViewTargetWithBlend(Pv, 0.45f, VTBlend_Cubic); }
					}
				}
				if (T < 2.95f) return true;
				if (AActor* Cm = V->Cam.Get()) { if (APlayerController* PCm = Wm->GetFirstPlayerController()) if (PCm->GetViewTarget() == Cm) { if (AActor* Pv = V->PrevView.Get()) PCm->SetViewTarget(Pv); } Cm->Destroy(); }
				Kill();
				return false;
			}));'''),
])
print('b95b ok')
