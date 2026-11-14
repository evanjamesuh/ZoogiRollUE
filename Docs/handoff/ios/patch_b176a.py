# b176a (Evan: "Add all effects as new skills that can be unlocked or purchased"): 9 skills from the bought Ice Attacks / Earthbender /
# Stylized Slashes packs. Cards.csv rows + card art (Data/CardArt) make them chest unlocks and shop card deals.
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiSkills.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('''			{ TEXT("precision"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 TARGETS, THEN FIRE") } };''','''			{ TEXT("precision"), EAim::MultiTap, 0.f, 3, TEXT("TAP UP TO 3 TARGETS, THEN FIRE") },
			// b176a: the bought Fab packs (Ice Attacks, Earthbender, Stylized Slashes)
			{ TEXT("frost_spikes"), EAim::Area, 360.f, 1, TEXT("DRAG THE CIRCLE - ICE SPIKES BURST THERE") },
			{ TEXT("glacier_lance"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("ice_shards"), EAim::Spots, 0.f, 3, TEXT("TAP 3 SPOTS FOR THE ICE SHARDS") },
			{ TEXT("stone_pillar"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("earthquake"), EAim::Area, 560.f, 1, TEXT("DRAG THE CIRCLE - THE GROUND CRACKS THERE") },
			{ TEXT("rock_rampart"), EAim::Line, 0.f, 1, TEXT("SWIPE WHERE THE ROCK WALL BURSTS UP") },
			{ TEXT("crescent_slash"), EAim::Line, 0.f, 1, TEXT("SWIPE THE SLASH'S PATH") },
			{ TEXT("cross_slash"), EAim::Tap, 0.f, 1, TEXT("TAP A RIVAL OR AN ORB") },
			{ TEXT("ground_slash"), EAim::Line, 0.f, 1, TEXT("SWIPE THE WAVE'S PATH") } };''')
ed('''	void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id)
	{''','''	// b176a: the Fab-pack skills - one Niagara effect per hit spot (or along the swipe), then a knock: 0 area, 1 tap, 2 spots,
	// 3 line (side push), 4 line (forward sweep)
	static bool ExecPack(AZoogiGameMode* G, AZoogiBall* Caster, FName Id, const FPick& P, float FZ)
	{
		struct FPk { const TCHAR* Id; const TCHAR* Fx; float Scale; int32 Kind; float R; float Speed; float Up; float Delay; FLinearColor Col; };
		static const FPk Pks[] = {
			{ TEXT("frost_spikes"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV06.NS_IceAttackV06"), 1.6f, 0, 360.f, 1100.f, 650.f, 0.25f, FLinearColor(0.5f, 0.85f, 1.f) },
			{ TEXT("glacier_lance"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV08.NS_IceAttackV08"), 1.8f, 1, 190.f, 1500.f, 900.f, 0.3f, FLinearColor(0.45f, 0.8f, 1.f) },
			{ TEXT("ice_shards"), TEXT("/Game/IceAttack/IceAttack/Particles/NiagaraSystems/NS_IceAttackV02.NS_IceAttackV02"), 1.2f, 2, 230.f, 700.f, 420.f, 0.2f, FLinearColor(0.6f, 0.9f, 1.f) },
			{ TEXT("stone_pillar"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V01.NS_Earthbender_V01"), 1.4f, 1, 190.f, 1400.f, 850.f, 0.25f, FLinearColor(0.55f, 0.6f, 0.8f) },
			{ TEXT("earthquake"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V41.NS_Earthbender_V41"), 1.6f, 0, 560.f, 900.f, 380.f, 0.35f, FLinearColor(0.8f, 0.7f, 0.55f) },
			{ TEXT("rock_rampart"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V30.NS_Earthbender_V30"), 1.4f, 3, 170.f, 900.f, 360.f, 0.25f, FLinearColor(0.55f, 0.6f, 0.8f) },
			{ TEXT("crescent_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_EzrealSlash02.NS_EzrealSlash02"), 2.0f, 4, 170.f, 1000.f, 260.f, 0.1f, FLinearColor(0.3f, 0.6f, 1.f) },
			{ TEXT("cross_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_CrossSlash08.NS_CrossSlash08"), 1.8f, 1, 210.f, 1200.f, 300.f, 0.1f, FLinearColor(1.f, 0.55f, 0.15f) },
			{ TEXT("ground_slash"), TEXT("/Game/StylizedSlashesVol1/Particles/NiagaraSystems/NS_GroundSlash03.NS_GroundSlash03"), 2.0f, 4, 170.f, 1100.f, 600.f, 0.15f, FLinearColor(0.65f, 0.35f, 1.f) } };
		const FPk* D = nullptr;
		for (const FPk& K : Pks) if (Id == FName(K.Id)) D = &K;
		if (!D || !G || !Caster) return false;
		UWorld* W = G->GetWorld();
		const int32 Pi = Caster->PlayerIndex;
		auto At = [FZ](const FVector& V) { return FVector(V.X, V.Y, FZ); };
		const float Scl = Sc(Id, D->Scale);
		TWeakObjectPtr<AZoogiGameMode> WG = G; TWeakObjectPtr<AZoogiBall> WC = Caster;
		const FPk Dd = *D;
		auto Credit = [Pi](AZoogiGameMode* Gm, const FVector& Ctr, float Rr)
		{
			for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz->PlayerIndex != Pi && FVector::Dist2D(Bz->BallLocation(), Ctr) <= Rr * ZoogiSkillLab::Radius()) Bz->LastHitPlayer = Pi;
		};
		if (Dd.Kind <= 2)
		{
			TArray<FVector> Hits;
			if (Dd.Kind == 0) Hits.Add(At(P.Ctr));
			else if (Dd.Kind == 1) { for (const TWeakObjectPtr<AActor>& T : P.Targets) if (AActor* A = T.Get()) Hits.Add(At(A->GetActorLocation())); if (Hits.Num() == 0) Hits.Add(At(P.Ctr)); }
			else { for (const FVector& V : P.Spots) Hits.Add(At(V)); if (Hits.Num() == 0) Hits.Add(At(P.Ctr)); }
			for (const FVector& H : Hits)
			{
				AZoogiFabFX::Play(W, Dd.Fx, H, Scl, 3.f);
				AZoogiFX::Spawn(W, H + FVector(0, 0, 6.f), Dd.Col, Dd.R, 0.45f, true, 9.f);
				Later(G, Dd.Delay, [WG, WC, H, Dd, Credit, Pi]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					Credit(Gm, H, Dd.R);
					const int32 N = Blast(Gm, H, Dd.R, Dd.Speed, Dd.Up, WC.Get(), Pi);
					UZoogiScreenFx::Shockwave(Gm->GetWorld(), H, Dd.R + 60.f, Dd.Col);
					ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 0.8f, 1.f);
					ZoogiHaptics::Kick(0.6f, Gm->GetWorld(), TEXT("pack_skill"));
					UE_LOG(LogZoogi, Log, TEXT("SKILL %s P%d at (%.0f, %.0f): %d pushed"), Dd.Id, Pi + 1, H.X, H.Y, N);
				});
			}
		}
		else
		{	// a swipe: the effect plays along the line, then everything within R of it is thrown (to the side, or forward)
			const FVector A = At(P.A), B = At(P.B);
			FVector Dir = (B - A).GetSafeNormal2D(); if (Dir.IsNearlyZero()) Dir = FVector(1.f, 0.f, 0.f);
			const float Len = FMath::Max(100.f, FVector::Dist2D(A, B));
			const FRotator Rot = Dir.Rotation();
			const int32 NFx = FMath::Clamp(FMath::RoundToInt(Len / 260.f), 1, 4);
			for (int32 k = 0; k < NFx; ++k)
			{
				const FVector Pk = A + Dir * (Len * (k + 0.5f) / NFx);
				AZoogiFabFX::Play(W, Dd.Fx, Pk, Scl, 3.f, nullptr, FVector::ZeroVector, Rot);
			}
			Later(G, Dd.Delay, [WG, WC, A, Dir, Len, Dd, Pi]()
			{
				AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
				int32 N = 0;
				const FVector Side(-Dir.Y, Dir.X, 0.f);
				auto Hit = [&](UPrimitiveComponent* Body, const FVector& L) -> bool
				{
					if (!Body) return false;
					const FVector Rel = L - A; const float Along = FVector::DotProduct(Rel, Dir), Across = FVector::DotProduct(Rel, Side);
					if (Along < -60.f || Along > Len + 60.f || FMath::Abs(Across) > Dd.R) return false;
					const FVector Push = Dd.Kind == 3 ? Side * (Across >= 0.f ? 1.f : -1.f) : Dir;
					Kick(Body, Push * Dd.Speed * (1.f - 0.4f * FMath::Abs(Across) / Dd.R) + FVector(0, 0, Dd.Up)); ++N; return true;
				};
				for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz != WC.Get() && Hit(Bz->Sphere, Bz->BallLocation())) Bz->LastHitPlayer = Pi;
				for (AZoogiOrb* O : Gm->Orbs) if (O && !O->bOut && O->Sphere && Hit(O->Sphere, O->OrbLocation())) O->LastHitBy = Pi;
				UZoogiScreenFx::Shockwave(Gm->GetWorld(), A + Dir * Len * 0.5f, Len * 0.5f + 80.f, Dd.Col);
				ZoogiUtil::PlaySfx(Gm, TEXT("Whoosh"), 1.f, 0.9f);
				ZoogiHaptics::Kick(0.6f, Gm->GetWorld(), TEXT("pack_skill"));
				UE_LOG(LogZoogi, Log, TEXT("SKILL %s P%d line %.0f cm: %d pushed"), Dd.Id, Pi + 1, Len, N);
			});
		}
		if (!G->ShotDir.IsEmpty()) { const FString Nn = FString::Printf(TEXT("skill_%s"), *Id.ToString()); G->TakeShotDelayed(Nn + TEXT("_a"), 0.45f); G->TakeShotDelayed(Nn + TEXT("_b"), 1.1f); }
		return true;
	}

	void Execute(AZoogiGameMode* G, AZoogiBall* Caster, FName Id)
	{''')
ed('''		UE_LOG(LogZoogi, Log, TEXT("SKILL cast %s by P%d"), *Id.ToString(), Pi + 1);''','''		UE_LOG(LogZoogi, Log, TEXT("SKILL cast %s by P%d"), *Id.ToString(), Pi + 1);
		if (ExecPack(G, Caster, Id, P, FZ)) { GLastCastAt = FPlatformTime::Seconds(); return; } // b176a''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
