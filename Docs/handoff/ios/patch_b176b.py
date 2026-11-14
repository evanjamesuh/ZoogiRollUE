# b176b (Evan: "Some use as world effects to make the game look better"): Cross Slash on the clash finisher, an Earthbender rock burst under KO blasts.
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/')
def patch(f,pairs):
    s=open(R+f,'rb').read().decode('utf-8')
    for old,new in pairs:
        done=False
        for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
            if s.count(o)==1: s=s.replace(o,n); done=True; break
        if not done: print('MISS',f,repr(old[:60])); sys.exit(1)
    open(R+f,'wb').write(s.encode('utf-8')); print('OK',f)
patch('ZoogiStrikeFun.cpp',[('#include "ZoogiArenaPiece.h"\n','#include "ZoogiArenaPiece.h"\n#include "ZoogiFabFX.h"\n'),
('''			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.55f);
			UE_LOG(LogZoogi, Log, TEXT("FUN clash finisher lands (%s)"), *Fw.Move);''','''			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 1.f, 0.55f);
			if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWorldFx"))) if (AActor* La = Fl.Actor.Get()) // b176b: the Cross Slash pack on every finisher
				AZoogiFabFX::Play(GetWorld(), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_CrossSlash08.NS_CrossSlash08"), La->GetActorLocation() + FVector(0, 0, 60.f), 1.6f, 2.f, nullptr, FVector::ZeroVector, FRotator(0.f, Clash.AxisYaw, 0.f));
			UE_LOG(LogZoogi, Log, TEXT("FUN clash finisher lands (%s)"), *Fw.Move);''')])
patch('ZoogiArenaReact.cpp',[('#include "ZoogiSkillLab.h"\n','#include "ZoogiSkillLab.h"\n#include "ZoogiFabFX.h"\n'),
('''		else if (*Bx) AZoogiFX::Stylized(W, Bx, At, 1.0f * Sz);
	}''','''		else if (*Bx) AZoogiFX::Stylized(W, Bx, At, 1.0f * Sz);
		static const bool bNoWorld = ReactFlag(TEXT("ZoogiNoWorldFx")); // b176b: an Earthbender rock burst under every knockout
		if (!bNoWorld) AZoogiFabFX::Play(W, TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V04.NS_Earthbender_V04"), At - FVector(0, 0, 40.f), 1.3f * Sz, 3.f);
	}''')])
