import os
p=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiSpectacle.cpp')
s=open(p,encoding='utf-8').read()
def ed(old,new):
    global s; assert s.count(old)==1,old[:60]; s=s.replace(old,new)
ed('#include "ZoogiMusicSubsystem.h"\n','#include "ZoogiMusicSubsystem.h"\n#include "ZoogiRedirect.h"\n#include "ZoogiSkills.h"\n')
ed("		double GCamT0 = 0.0; float GCamLen = 0.f;",
   "		double GCamT0 = 0.0; float GCamLen = 0.f;\n		bool GRet = false; double GRetT0 = 0.0; FVector GRetPos = FVector::ZeroVector; FRotator GRetRot = FRotator::ZeroRotator; float GRetFov = 82.f; // b282: real-time glide back to the match view")
ed("""		void TickCam()
		{
			ACameraActor* C = GCamActor.Get();
			if (!C || GCam == ECam::None) { GCam = ECam::None; return; }
			const float Ts = (float)(Now() - GCamT0);
			AZoogiBall* B = GCamBall.Get();
			if (!B || Ts >= GCamLen) { EndCam(0.45f); return; }""",
"""		bool WorldHeld()
		{	// b282: the game is paused / slowed for a REACTION (redirect pick, skill aim, clash / timing tap slow-mo) - a view-target blend would freeze
			UWorld* W = GG.IsValid() ? GG->GetWorld() : nullptr;
			const float Td = (W && W->GetWorldSettings()) ? W->GetWorldSettings()->TimeDilation : 1.f;
			return ZoogiRedirect::Picking() || ZoogiSkills::Picking() || Td < 0.15f;
		}
		void BeginReturn()
		{	// b282 (Evan: "automatically begin transition to normal view after launch the moment the gameplay is paused for reaction"):
			// glide the ride camera back to the match camera in REAL time (0.45 s), then hand the view back with no blend
			ACameraActor* C = GCamActor.Get();
			if (!C || GRet) return;
			GRet = true; GRetT0 = Now(); GRetPos = C->GetActorLocation(); GRetRot = C->GetActorRotation(); GRetFov = C->GetCameraComponent()->FieldOfView;
			UE_LOG(LogZoogi, Log, TEXT("SPECTACLE shot cam: game held for a reaction - gliding back to the match view"));
		}
		void TickCam()
		{
			ACameraActor* C = GCamActor.Get();
			if (!C || GCam == ECam::None) { GCam = ECam::None; GRet = false; return; }
			if (GRet)
			{
				AActor* Prev = GCamPrev.Get();
				APlayerController* P = GG.IsValid() ? PC(GG.Get()) : nullptr;
				FMinimalViewInfo Vi; bool bHave = false;
				if (Prev && Prev != C) { Prev->CalcCamera(0.f, Vi); bHave = true; }
				const float A = FMath::Clamp((float)(Now() - GRetT0) / 0.45f, 0.f, 1.f), E = A * A * (3.f - 2.f * A);
				if (bHave)
				{
					C->SetActorLocationAndRotation(FMath::Lerp(GRetPos, Vi.Location, E), FQuat::Slerp(GRetRot.Quaternion(), Vi.Rotation.Quaternion(), E).Rotator());
					C->GetCameraComponent()->SetFieldOfView(FMath::Lerp(GRetFov, Vi.FOV, E));
				}
				if (A >= 1.f || !bHave)
				{
					if (P && P->GetViewTarget() == C) P->SetViewTarget(Prev ? Prev : P->GetPawn());
					C->SetLifeSpan(0.5f); GCamActor.Reset(); GCam = ECam::None; GRet = false;
				}
				return;
			}
			static const bool bNoReturn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoReactCamReturn"));
			if (GCam == ECam::Shot && !bNoReturn && WorldHeld()) { BeginReturn(); return; }
			const float Ts = (float)(Now() - GCamT0);
			AZoogiBall* B = GCamBall.Get();
			if (!B || Ts >= GCamLen) { if (GCam == ECam::Shot && !bNoReturn) BeginReturn(); else EndCam(0.45f); return; }""")
ed("			GCamActor.Reset(); GCam = ECam::None;\n		}\n\n		bool StartCam","			GCamActor.Reset(); GCam = ECam::None; GRet = false;\n		}\n\n		bool StartCam")
q=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiFront.cpp'); t=open(q,encoding='utf-8').read(); a='{ TEXT("B281"),'; assert t.count(a)==1
t=t.replace(a,'{ TEXT("B282"), TEXT("CAMERA BACK FOR REACTION"), TEXT("After a strong launch the camera rides behind your Zoogi - now, the moment the game pauses for a REACTION, it glides straight back to the normal match view so you can see your targets.") },\n'+a)
open(q,'w',encoding='utf-8').write(t)
open(p,'w',encoding='utf-8').write(s); print('ok')
