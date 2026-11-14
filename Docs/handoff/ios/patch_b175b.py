# b175b (Evan: "make this model the volcano boss"): MAGMA TITAN = the Fab Stone Golem (/Game/StoneGolem) with its hammer + shield
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiCosmosBoss.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('''			if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: no Aurex mesh")); return; }''','''			// b175b (Evan: "make this model the volcano boss"): Volcanic's MAGMA TITAN is the Stone Golem (own materials, hammer + shield)
			static const bool bOldMagma = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldMagmaBoss"));
			bool bGolem = false;
			if (!bOldMagma && !Titan() && G->Arena->GetTheme() == EZoogiArena::Volcanic)
				if (USkeletalMesh* Gs = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/StoneGolem/Mesh/SKM_StoneGolem.SKM_StoneGolem"), nullptr, LOAD_NoWarn | LOAD_Quiet)) { SM = Gs; bGolem = true; }
			if (!SM) { UE_LOG(LogZoogi, Warning, TEXT("COSMOS BOSS: no Aurex mesh")); return; }''')
ed('''				P->Tint = GTheme.Tint;
				if (UMaterialInterface* AB = LoadObject<UMaterialInterface>''','''				P->Tint = GTheme.Tint;
				if (bGolem)
				{	// the golem keeps its own stone materials; the hammer and shield go in its hands
					struct FW { const TCHAR* Mesh; const TCHAR* Sock; const TCHAR* Bone; };
					static const FW Ws[] = { { TEXT("SM_StoneGolem_Hammer"), TEXT("weapon_rSocket"), TEXT("weapon_r") }, { TEXT("SM_StoneGolem_Shield"), TEXT("weapon_lSocket"), TEXT("weapon_l") } };
					for (const FW& Wp : Ws)
						if (UStaticMesh* Wm = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/StoneGolem/Mesh/%s.%s"), Wp.Mesh, Wp.Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet))
						{
							UStaticMeshComponent* Wc = NewObject<UStaticMeshComponent>(P);
							Wc->SetStaticMesh(Wm); Wc->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wc->SetCastShadow(false); Wc->SetBoundsScale(3.f);
							const FName Sk = P->Body->DoesSocketExist(FName(Wp.Sock)) ? FName(Wp.Sock) : FName(Wp.Bone);
							Wc->SetupAttachment(P->Body, Sk); Wc->RegisterComponent();
						}
					UE_LOG(LogZoogi, Log, TEXT("COSMOS BOSS: MAGMA TITAN = Stone Golem (hammer + shield)"));
				}
				else if (UMaterialInterface* AB = LoadObject<UMaterialInterface>''')
if '#include "Components/StaticMeshComponent.h"' not in s:
    nl='\r\n' if '\r\n' in s else '\n'
    k=s.index(nl, s.index('#include'))+len(nl); s=s[:k]+'#include "Components/StaticMeshComponent.h"'+nl+'#include "Engine/StaticMesh.h"'+nl+s[k:]
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
