# b88 (Evan 14:24-14:28): Wolf Pack summon clip plays inside the caster's avatar circle (not the top card over the targets);
# Energy Shockwave = gravity suck then a blast at 2x the Wolf Pack explosion; Wolf Pack costs 3 SP and banks 3 wolves;
# the Micro Werewolf is back as a purple hologram that hunts its target and explodes on impact with it.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ROOTC = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style')

# ---------------- avatar clip
ed(H + 'ZoogiAnimeSubsystem.h', [
 ('	bool PowerIntroOn() const { return CrashState > 0 && CrashId.StartsWith(TEXT("Anime_Awaken_")); }\n',
  '''	bool PowerIntroOn() const { return CrashState > 0 && CrashId.StartsWith(TEXT("Anime_Awaken_")); }
	/** b88 (Evan: "move the wolf skill animation to play in that player's avi space - I can't see the target when it plays"): the
	 *  Wolf Pack summon clip runs inside the caster's avatar circle (ZoogiArenaHUDWidget swaps the avatar brush to MovieTexture).
	 *  True while its frames show; -ZoogiWolfClipTop = the top card. */
	bool AviClip(int32& OutPlayer) const;
	int32 IntroPlayer = -1;
'''),
])
ed(P + 'ZoogiAnimeSubsystem.cpp', [
 ('	IntroSeen.Add(Seen);\n', '	IntroSeen.Add(Seen);\n	IntroPlayer = Player; // b88\n'),
 ('''void UZoogiAnimeSubsystem::AfterCrash(TFunction<void()> Fn)''',
  '''bool UZoogiAnimeSubsystem::AviClip(int32& OutPlayer) const
{
	static const bool bTop = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfClipTop"));
	if (bTop || CrashState < 2 || !bCrashFrames || IntroPlayer < 0 || !CrashId.StartsWith(TEXT("Anime_Awaken_WolfPack"))) return false;
	FVector2D P, S;
	if (!ZoogiArenaHUD::AvatarRect(IntroPlayer, P, S)) return false;
	OutPlayer = IntroPlayer;
	return true;
}

void UZoogiAnimeSubsystem::AfterCrash(TFunction<void()> Fn)'''),
 ('''		if (!MovieTexture || !MovieTexture->GetResource()) return;
		float K = 0.f;''',
  '''		if (!MovieTexture || !MovieTexture->GetResource()) return;
		{ int32 Ap = -1; if (AviClip(Ap)) return; } // b88: the wolf summon plays in the caster's avatar instead
		float K = 0.f;'''),
])
ed(H + 'ZoogiArenaHUDWidget.h', [
 ('TWeakObjectPtr<UTexture2D> AviBase; bool bAviUlt = false; };', 'TWeakObjectPtr<UTexture2D> AviBase; bool bAviUlt = false; bool bAviClip = false; FSlateBrush AviSaved; };'),
 ('	void SetSPText(class AZoogiGameMode* G, FUnitRefs& U, int32 i);', '	void SyncAviClip(class AZoogiGameMode* G, FUnitRefs& U, int32 i); // b88: Wolf Pack summon clip in the avatar circle\n	void SetSPText(class AZoogiGameMode* G, FUnitRefs& U, int32 i);'),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('		SetSPText(G, U, i); // b69\n		if (const UZoogiFunSys* Fun', '		SetSPText(G, U, i); // b69\n		SyncAviClip(G, U, i); // b88\n		if (const UZoogiFunSys* Fun'),
 ('		SetSPText(G, U, i); // b69\n		const float E = SM ?', '		SetSPText(G, U, i); // b69\n		SyncAviClip(G, U, i); // b88\n		const float E = SM ?'),
 ('				const bool bUlt = Fun->IsTransformed(i);\n				if (bUlt != U.bAviUlt)', '				const bool bUlt = Fun->IsTransformed(i);\n				if (bUlt != U.bAviUlt && !U.bAviClip)'),
 ('void UZoogiArenaHUDWidget::SetSPText(AZoogiGameMode* G, FUnitRefs& U, int32 i)',
  '''void UZoogiArenaHUDWidget::SyncAviClip(AZoogiGameMode* G, FUnitRefs& U, int32 i)
{
	UImage* Av = U.Avi.Get();
	if (!Av || !G) return;
	UZoogiAnimeSubsystem* An = G->GetWorld() ? G->GetWorld()->GetSubsystem<UZoogiAnimeSubsystem>() : nullptr;
	int32 Who = -1;
	const bool bOn = An && An->MovieTexture && An->AviClip(Who) && Who == i;
	if (bOn == U.bAviClip) return;
	if (bOn)
	{	// the 16:9 clip centre-cropped into the round portrait
		U.AviSaved = Av->GetBrush();
		FSlateBrush Br = U.AviSaved;
		Br.SetResourceObject(An->MovieTexture);
		Br.SetUVRegion(FBox2f(FVector2f(0.21875f, 0.f), FVector2f(0.78125f, 1.f)));
		Av->SetBrush(Br);
		UE_LOG(LogZoogi, Log, TEXT("ANIME wolf summon clip -> P%d avatar"), i + 1);
	}
	else Av->SetBrush(U.AviSaved);
	U.bAviClip = bOn;
}

void UZoogiArenaHUDWidget::SetSPText(AZoogiGameMode* G, FUnitRefs& U, int32 i)'''),
])

# ---------------- Wolf Pack: 3 SP, banks 3
cp = os.path.join(ROOTC, 'Content/Data/Cards.csv')
s = open(cp, 'rb').read().decode('utf-8')
old = 'wolf_pack,WOLF PACK,2,Utility,ActingZoogi,LeadActionEnd,3,0,0,true,Void,Cast on yourself: the wolf pack joins your roll - 3 free REACTION redirects this turn for a boosted combo.'
new = 'wolf_pack,WOLF PACK,3,Utility,ActingZoogi,LeadActionEnd,3,0,0,true,Void,Cast on yourself: 3 hologram wolves join your roll - tap SUPERPOWER while rolling and pick up to 3 targets.'
assert s.count(old) == 1; s = s.replace(old, new); open(cp, 'wb').write(s.encode('utf-8'))
ed(P + 'ZoogiSkills.cpp', [
 ('''			ArmWolf(G, Pi);
			AZoogiFX::Spawn(W, MeFloor, FLinearColor(0.75f, 0.45f, 1.f), 300.f, 0.6f, true, 8.f);''',
  '''			// b88 (Evan: "increase Wolf Pack SP to 3 but you get three"): one cast banks 3 wolves (-ZoogiWolfOne = 1)
			static const bool bOneWolf = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOne"));
			for (int32 k = 0; k < (bOneWolf ? 1 : 3); ++k) ArmWolf(G, Pi);
			AZoogiFX::Spawn(W, MeFloor, FLinearColor(0.75f, 0.45f, 1.f), 300.f, 0.6f, true, 8.f);'''),
 ('F->PublicCallout(TEXT("WOLF PACK READY!"), Caster->bCPU ? TEXT("THE PACK IS COMING") : TEXT("TAP THE WOLF WHILE YOU ROLL")',
  'F->PublicCallout(TEXT("3 WOLVES READY!"), Caster->bCPU ? TEXT("THE PACK IS COMING") : TEXT("LAUNCH, THEN TAP SUPERPOWER")'),
 # energy shockwave
 ('''			const FVector Pc = At(P.Ctr);
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector D = Pc - O->OrbLocation(); const float Dd = D.Size2D();
				if (Dd > P.R || Dd < 30.f) continue;
				Kick(O->Sphere, D.GetSafeNormal2D() * (120.f + 380.f * Dd / P.R));
				O->LastHitBy = Pi;
			}''',
  '''			const FVector Pc = At(P.Ctr);
			static const bool bOldSw = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldShockwave"));
			if (bOldSw)
			for (AZoogiOrb* O : G->Orbs)
			{
				if (!O || O->bOut || !O->Sphere) continue;
				const FVector D = Pc - O->OrbLocation(); const float Dd = D.Size2D();
				if (Dd > P.R || Dd < 30.f) continue;
				Kick(O->Sphere, D.GetSafeNormal2D() * (120.f + 380.f * Dd / P.R));
				O->LastHitBy = Pi;
			}
			else
			{	// b88 (Evan: "increase energy shockwave gravity suck, then explode 2x the force of Wolf Pack explosions"): two strong pulls
				// (orbs and rival Zoogis), then a blast at twice a wolf's (420 + 1380 K, K = 1 at the centre) with twice its hop
				const float Rw = P.R * 1.25f;
				const FLinearColor Pu(0.65f, 0.35f, 1.f);
				auto Suck = [WG, Pc, Rw, Pi, Pu]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					for (AZoogiOrb* O : Gm->Orbs)
					{
						if (!O || O->bOut || !O->Sphere) continue;
						const FVector D = Pc - O->OrbLocation(); const float Dd = D.Size2D();
						if (Dd > Rw || Dd < 25.f) continue;
						Kick(O->Sphere, D.GetSafeNormal2D() * (380.f + 900.f * Dd / Rw));
						O->LastHitBy = Pi;
					}
					for (AZoogiBall* Bz : Gm->Balls)
					{
						if (!Bz || Bz->bOut || Bz->PlayerIndex == Pi || !Bz->Sphere) continue;
						const FVector D = Pc - Bz->BallLocation(); const float Dd = D.Size2D();
						if (Dd > Rw || Dd < 25.f) continue;
						Kick(Bz->Sphere, D.GetSafeNormal2D() * (260.f + 640.f * Dd / Rw));
					}
					AZoogiFX::Spawn(Gm->GetWorld(), Pc + FVector(0, 0, 6.f), Pu, Rw, 0.35f, true, 8.f);
				};
				Suck();
				Later(G, 0.3f, Suck);
				Later(G, 0.85f, [WG, Pc, Rw, Pi, Pu]()
				{
					AZoogiGameMode* Gm = WG.Get(); if (!Gm) return;
					UWorld* Wb = Gm->GetWorld();
					int32 Nb = 0;
					auto Push = [&](UPrimitiveComponent* Body, const FVector& L)
					{
						if (!Body || !Body->IsSimulatingPhysics()) return false;
						FVector D = L - Pc; D.Z = 0.f;
						const float Dist = D.Size();
						if (Dist > Rw) return false;
						const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
						const float K = 1.f - Dist / Rw;
						const FVector V = Body->GetPhysicsLinearVelocity();
						Body->WakeAllRigidBodies();
						Body->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * 2.f * (420.f + 1380.f * K) + FVector(0.f, 0.f, 320.f * K));
						++Nb; return true;
					};
					for (AZoogiOrb* O : Gm->Orbs) if (O && !O->bOut && Push(O->Sphere, O->OrbLocation())) O->LastHitBy = Pi;
					for (AZoogiBall* Bz : Gm->Balls) if (Bz && !Bz->bOut && Bz->PlayerIndex != Pi && Push(Bz->Sphere, Bz->BallLocation())) Bz->LastHitPlayer = Pi;
					AZoogiFabFX::Play(Wb, FX(TEXT("NS_Energy_shockwave")), Pc, Sc(FName(TEXT("energy_shockwave")), 0.45f) * 1.4f, 2.f);
					AZoogiFX::Stylized(Wb, TEXT("Big_Explosion_v2"), Pc, 0.7f);
					AZoogiFX::Spawn(Wb, Pc + FVector(0, 0, 6.f), Pu, Rw * 1.2f, 0.6f, true, 10.f);
					UZoogiScreenFx::Shockwave(Wb, Pc, Rw + 60.f, Pu);
					ZoogiUtil::PlaySfx(Gm, TEXT("CritBoom"), 1.f, 0.85f);
					ZoogiHaptics::Kick(0.8f, Wb, TEXT("shockwave_blast"));
					UE_LOG(LogZoogi, Log, TEXT("SKILL energy_shockwave P%d: suck x2 then blast (2x wolf) - %d pushed"), Pi + 1, Nb);
				});
			}'''),
])

# ---------------- hologram Micro Werewolves
ed(H + 'ZoogiClone.h', [
 ('	bool bDetonated = false;\n', '	bool bDetonated = false;\n	bool bImpactBlow = false; // b88: hit its target -> blows next tick\n'),
])
ed(P + 'ZoogiClone.cpp', [
 ('''	float MWYawOffset() { static float Y = [] { float V = 180.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWolfYaw="), V); return V; }(); return Y; }''',
  '''	float MWYawOffset() { static float Y = [] { float V = 180.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWolfYaw="), V); return V; }(); return Y; }
	/** b88 (Evan: "bring back the wolf from the micro unit pack and make them purple holograms - they seek your target and explode on
	 *  impact with that target"): Micro Werewolf in every mode, purple glowing hologram, explodes on its target. -ZoogiOldWolfClones = b87. */
	bool HoloWolves() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWolfClones")); return b; }
	const FLinearColor HoloPurple(0.62f, 0.3f, 1.f);'''),
 ('		if (Gm && !Gm->bRoyaleMode && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoughtWolvesClassic"))) return false;',
  '		if (Gm && !Gm->bRoyaleMode && !HoloWolves() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiBoughtWolvesClassic"))) return false;'),
 ('''	if (UMaterialInterface* M = MWLoad<UMaterialInterface>(TEXT("M_MicroWerewolf")))
	{''',
  '''	if (HoloWolves()) { Tint = HoloPurple; Body->SetCastShadow(false); }
	if (UMaterialInterface* M = MWLoad<UMaterialInterface>(TEXT("M_MicroWerewolf")))
	{'''),
 ('''		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, BodyMID);
	}
	AnimWalk''',
  '''		if (HoloWolves()) { BodyMID->SetScalarParameterValue(TEXT("TintAmt"), 0.92f); BodyMID->SetScalarParameterValue(TEXT("Glow"), 1.5f); BodyMID->SetScalarParameterValue(TEXT("BaseLit"), 0.9f); }
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, BodyMID);
	}
	AnimWalk'''),
 ('''	if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.12f + 0.9f * Charge''',
  '''	if (BodyMID && HoloWolves())
	{	// hologram: a humming purple glow with the odd flicker frame
		const bool bGlitch = FMath::FRand() < 0.035f && Charge < 0.5f;
		Body->SetVisibility(!bGlitch);
		BodyMID->SetScalarParameterValue(TEXT("Glow"), 1.3f + 0.45f * FMath::Sin(Age * 11.f) + 1.4f * Charge + (bGlitch ? -1.f : 0.f));
	}
	else if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("Glow"), 0.12f + 0.9f * Charge'''),
 ('''	if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("TintAmt"), bDire ? 0.55f : 0.35f);''',
  '''	if (BodyMID) BodyMID->SetScalarParameterValue(TEXT("TintAmt"), HoloWolves() ? 0.92f : (bDire ? 0.55f : 0.35f));'''),
 ('''	Age += DeltaSeconds;
	if (bDetonated) return;
''', '''	Age += DeltaSeconds;
	if (bDetonated) return;
	if (bImpactBlow) { Detonate(TEXT("impact")); return; } // b88
'''),
 ('''	AActor* T = Target.Get();
	bool bTargetGone = !T || T->IsHidden();''',
  '''	AActor* T = Target.Get();
	bool bTargetGone = !T || T->IsHidden();
	if (HoloWolves() && bTargetGone && !bHit) // b88: the hunt goes on - next target
		if (UZoogiWolfPackSys* Wp = UZoogiWolfPackSys::Get(this)) if (AActor* Nt = Wp->PickReboundTarget(this, nullptr)) { Target = Nt; T = Nt; bTargetGone = false; }'''),
 ('''	PlayBody(1, true); // the werewolf bites whatever it hits
	bHit = true;''',
  '''	PlayBody(1, true); // the werewolf bites whatever it hits
	if (HoloWolves() && !Recall.IsValid())
	{	// b88: it explodes on impact with its own target; anything else it brushes past keeps it hunting
		AActor* Tg = Target.Get();
		if (OtherActor == Tg || !Tg || Tg->IsHidden()) { bImpactBlow = true; bHit = true; UE_LOG(LogZoogi, Log, TEXT("WOLF hologram P%d hits its target %s -> boom"), OwnerPlayer + 1, *OtherActor->GetName()); }
		return;
	}
	bHit = true;'''),
])
print('b88 ok')
