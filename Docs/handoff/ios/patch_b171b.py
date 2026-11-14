import sys,re
R='C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/'
import os
if not os.path.exists(R): R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
dry='dry' in sys.argv
def ed(f,old,new,cnt=1):
    p=R+f; s=open(p,'rb').read().decode('utf-8')
    for o,n in ((old,new),(old.replace('\n','\r\n'),new.replace('\n','\r\n'))):
        if s.count(o)==cnt:
            s=s.replace(o,n)
            if not dry: open(p,'wb').write(s.encode('utf-8'))
            print('OK',f); return
    print('MISS',f,repr(old[:60])); sys.exit(1)
ed('Private/ZoogiGameMode.cpp','''static const bool bOldKOSpot = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldKOSpot"));
			const FVector KOAt = bOldKOSpot ? EdgePoint(L, Arena) :''','''// b171 (Evan: "revert back to how we had the knockout explosion before"): the blast is back on the court edge point; -ZoogiKOAtZoogi = the b140 spot
			static const bool bKOAtZoogi = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKOAtZoogi"));
			const FVector KOAt = !bKOAtZoogi ? EdgePoint(L, Arena) :''')
ed('Public/ZoogiPlayerController.h','''	void OnImpactFeel(float Strength);
''','''	void OnImpactFeel(float Strength);
	/** b171: a big blast went off - full camera shake in every camera mode (Settings CAMERA SHAKE off = none). Strength 0..2. */
	void OnBigBlast(float Strength);
''')
ed('Private/ZoogiPlayerController.cpp','''void AZoogiPlayerController::ExtendSlowMoUntil(double RealTimeUntil)
''','''void AZoogiPlayerController::OnBigBlast(float Strength)
{	// b171 (Evan: "make the screen shake everytime a big blast occurs"): no tactical 0.4 cut; -ZoogiNoBlastShake; Settings CAMERA SHAKE off = none
	if (bMenuCam) return;
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoBlastShake"));
	if (bOff) return;
	int32 On = 1; if (GConfig) GConfig->GetInt(TEXT("ZoogiRoll.MenuSettings"), TEXT("CameraShake"), On, GGameUserSettingsIni);
	if (!On) return;
	const float K = FMath::Clamp(Strength, 0.f, 2.f);
	if (ImpactAge < 0.2f && ImpactStrength >= K) return; // already shaking at least this hard
	ImpactAge = 0.f; ImpactStrength = K;
	UE_LOG(LogZoogi, Log, TEXT("BLAST SHAKE k=%.2f"), K);
}

void AZoogiPlayerController::ExtendSlowMoUntil(double RealTimeUntil)
''')
ed('Public/ZoogiFX.h','''	static void Stylized(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale = 1.f, const FRotator& Rotation = FRotator::ZeroRotator);
''','''	static void Stylized(UWorld* World, const TCHAR* Name, const FVector& Location, float Scale = 1.f, const FRotator& Rotation = FRotator::ZeroRotator);
	/** b171: screen shake for a big blast (every Big_Explosion and every player knockout). */
	static void BigBlastShake(UWorld* World, float Scale = 1.f);
''')
ed('Private/ZoogiFX.cpp','''#include "ZoogiKFXExtras.h"
''','''#include "ZoogiKFXExtras.h"
#include "ZoogiPlayerController.h"
''')
ed('Private/ZoogiFX.cpp','''	const FString N(Name);
	const float Life = N.Contains(TEXT("Smoke"))''','''	const FString N(Name);
	if (N.Contains(TEXT("Big_Explosion"))) BigBlastShake(World, Scale); // b171 (Evan): the screen shakes on every big blast
	const float Life = N.Contains(TEXT("Smoke"))''')
ed('Private/ZoogiFX.cpp','''UNiagaraComponent* AZoogiFX::StylizedLoop(''','''void AZoogiFX::BigBlastShake(UWorld* World, float Scale)
{
	if (!World) return;
	if (AZoogiPlayerController* PC = Cast<AZoogiPlayerController>(World->GetFirstPlayerController()))
		PC->OnBigBlast(FMath::Clamp(1.1f + 0.6f * Scale, 1.2f, 2.f));
}

UNiagaraComponent* AZoogiFX::StylizedLoop(''')
ed('Private/ZoogiArenaReact.cpp','''		const float Sz = ZoogiSkillLab::BlastSizeFor(KoPlayerId);
		AZoogiFX::Spawn(W, At, Col, 220.f * Sz, 0.7f, false, 7.f);''','''		const float Sz = ZoogiSkillLab::BlastSizeFor(KoPlayerId);
		AZoogiFX::BigBlastShake(W, 1.5f * Sz); // b171 (Evan): every knockout blast shakes the screen
		AZoogiFX::Spawn(W, At, Col, 220.f * Sz, 0.7f, false, 7.f);''')
