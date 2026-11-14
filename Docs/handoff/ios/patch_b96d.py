# b96d (Evan 18:41): "I should not see more wolves than my counter ... Pause gameplay on bird eye view, players have 10 secs to choose,
# wolves can reach the entire arena, players must choose the full amount of targets. Max level is 3 starting from lvl 1 with one
# wolf, improved by using it."
#  - the pack = exactly the banked wolves (no padding to 3 / Blood Moon / level-5 extras on a banked summon)
#  - human summon: play freezes, top-down camera over the whole arena, 10 s to tap EXACTLY N targets (every target is tappable);
#    time out -> the rest are auto-picked. Wolves live long enough to cross the arena.
#  - WOLF PACK skill level per player per match: Lv1 = 1 wolf, Lv2 = 2, Lv3 = 3 (max); every cast levels it up.
#  Old: -ZoogiWolfOldPaint (slow-mo paint, up to N), -ZoogiWolfNoLevels (3 per cast)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiWolfPack.h', [
 ('	void StartKillCam(const FVector& At);', '	void StartKillCam(const FVector& At);\n	void StartBird();   // b96d: top-down pick view\n	void EndBird();\n	int32 PaintNeed() const;'),
 ('	TWeakObjectPtr<ACameraActor> KillCam; TWeakObjectPtr<AActor> PrevView; float KillCamEnd = -1.f;',
  '	TWeakObjectPtr<ACameraActor> KillCam; TWeakObjectPtr<AActor> PrevView; float KillCamEnd = -1.f;\n	TWeakObjectPtr<ACameraActor> BirdCam; TWeakObjectPtr<AActor> BirdPrev; bool bBirdPick = false;'),
])
W = P + 'ZoogiWolfPack.cpp'
ed(W, [
 ('''	PaintMax = BankedWolves > 0 ? BankedWolves : NumWolves; // b87: one painted target per banked wolf
	NumWolves = FMath::Max(NumWolves, BankedWolves);''',
  '''	PaintMax = BankedWolves > 0 ? BankedWolves : NumWolves; // b87: one painted target per banked wolf
	static const bool bOldPaint = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOldPaint"));
	if (bOldPaint) NumWolves = FMath::Max(NumWolves, BankedWolves);
	else if (BankedWolves > 0) NumWolves = BankedWolves; // b96d (Evan): never more wolves than the counter
	bBirdPick = !bOldPaint && bHumanOwner;'''),
 ('''	SetDil(0.2f);
	Phase = EPhase::Paint; PhaseT0 = RealNow();
	PaintLimit = bHumanOwner ? (PaintSeen.Contains(Owner) ? 4.f : 5.f) + 0.8f * FMath::Max(0, PaintMax - 3) : 0.7f; // b55 (Evan: "more time to select targets"): was 1.6 / 2.8 s; b87: more per extra target
	if (bHumanOwner) Toast(''',
  '''	SetDil(bBirdPick ? 0.0005f : 0.2f); // b96d: the game pauses while you pick
	Phase = EPhase::Paint; PhaseT0 = RealNow();
	PaintLimit = bHumanOwner ? (PaintSeen.Contains(Owner) ? 4.f : 5.f) + 0.8f * FMath::Max(0, PaintMax - 3) : 0.7f; // b55 (Evan: "more time to select targets"): was 1.6 / 2.8 s; b87: more per extra target
	if (bBirdPick) { PaintLimit = 10.f; StartBird(); Toast(FString::Printf(TEXT("%d WOLF%s!"), NumWolves, NumWolves == 1 ? TEXT("") : TEXT("VES")), FString::Printf(TEXT("PICK %d TARGET%s ANYWHERE - 10 SECONDS"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S")), PackTint(B), 2.f, 1.f); }
	else if (bHumanOwner) Toast('''),
 ('''void UZoogiWolfPackSys::Release()
{
	RestoreDil();''', '''int32 UZoogiWolfPackSys::PaintNeed() const { return FMath::Max(1, FMath::Min(PaintMax, Targets(Caster.Get()).Num())); }

void UZoogiWolfPackSys::StartBird()
{
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr; AZoogiGameMode* G = GM();
	if (!PC || !G) return;
	if (!BirdCam.IsValid()) { FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; BirdCam = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SP); }
	ACameraActor* Cam = BirdCam.Get(); if (!Cam) return;
	// frame the whole knockoff circle (+ margin) on whichever screen axis is tighter
	int32 Vx = 16, Vy = 9; PC->GetViewportSize(Vx, Vy); Vx = FMath::Max(Vx, 1); Vy = FMath::Max(Vy, 1);
	const float Fov = 70.f, Th = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5f)), Tv = Th * (float)Vy / (float)Vx;
	const float R = (G->Arena ? G->Arena->GetKnockoffRadius() : ZoogiConst::KnockoffRadius) * 1.08f;
	const FVector Ctr = G->Arena ? FVector(G->Arena->GetActorLocation().X, G->Arena->GetActorLocation().Y, 0.f) : FVector::ZeroVector;
	const float Yaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : 0.f;
	Cam->SetActorLocation(Ctr + FVector(0, 0, R / FMath::Max(0.05f, FMath::Min(Th, Tv))));
	Cam->SetActorRotation(FRotator(-89.9f, Yaw, 0.f));
	Cam->GetCameraComponent()->SetFieldOfView(Fov);
	Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	if (!BirdPrev.IsValid()) BirdPrev = PC->GetViewTarget();
	PC->SetViewTargetWithBlend(Cam, 0.f);
	UE_LOG(LogZoogi, Log, TEXT("WOLF bird's-eye pick: P%d picks %d target(s), 10 s, game paused"), Owner + 1, PaintNeed());
}

void UZoogiWolfPackSys::EndBird()
{
	UWorld* W = GetWorld(); APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	if (PC && BirdPrev.IsValid() && BirdCam.IsValid() && PC->GetViewTarget() == BirdCam.Get()) PC->SetViewTargetWithBlend(BirdPrev.Get(), 0.f);
	BirdPrev.Reset();
}

void UZoogiWolfPackSys::Release()
{
	EndBird();
	RestoreDil();'''),
 ('''	RestoreDil(); EndKillCam();
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack of P%d cleared''', '''	RestoreDil(); EndKillCam(); EndBird();
	UE_LOG(LogZoogi, Log, TEXT("WOLF pack of P%d cleared'''),
 ('''		Cl->ExtraLife = (Level >= 2 ? 0.6f : 0.f) + (bIce ? 0.6f : 0.f);''',
  '''		Cl->ExtraLife = (Level >= 2 ? 0.6f : 0.f) + (bIce ? 0.6f : 0.f);
		if (bBirdPick && T) Cl->ExtraLife += FMath::Clamp(FVector::Dist2D(WolfPos(T), L) / 1750.f, 0.f, 2.5f); // b96d: reaches anywhere on the arena'''),
 ('''		else if (Now - PhaseT0 > PaintLimit || Painted.Num() >= PaintMax) Release();
		if (!Caster.IsValid() || Caster->bOut) { RestoreDil(); Phase = EPhase::Idle; }''',
  '''		else if (Now - PhaseT0 > PaintLimit || Painted.Num() >= (bBirdPick ? PaintNeed() : PaintMax)) Release(); // b96d: the full amount (or time out -> auto-fill)
		if (!Caster.IsValid() || Caster->bOut) { EndBird(); RestoreDil(); Phase = EPhase::Idle; }'''),
 ('''		Text(FString::Printf(TEXT("TAP UP TO %d TARGETS  -  %d / %d PICKED"), PaintMax, Painted.Num(), PaintMax), Wd * 0.5f, Ht * 0.66f, 1.25f * U, FLinearColor::White);
		const float Bw = 420.f * U, Bx = Wd * 0.5f - Bw * 0.5f, By = Ht * 0.70f;''',
  '''		if (bBirdPick) Text(FString::Printf(TEXT("PICK %d TARGET%s  -  %d / %d   %d s"), PaintNeed(), PaintNeed() == 1 ? TEXT("") : TEXT("S"), Painted.Num(), PaintNeed(), FMath::CeilToInt(Left)), Wd * 0.5f, Ht * 0.9f, 1.35f * U, FLinearColor::White);
		else Text(FString::Printf(TEXT("TAP UP TO %d TARGETS  -  %d / %d PICKED"), PaintMax, Painted.Num(), PaintMax), Wd * 0.5f, Ht * 0.66f, 1.25f * U, FLinearColor::White);
		const float Bw = 420.f * U, Bx = Wd * 0.5f - Bw * 0.5f, By = bBirdPick ? Ht * 0.93f : Ht * 0.70f;'''),
 ('''			for (int32 i = 0; i < FMath::Min(8, Orbs.Num()); ++i) Keep.Add(Orbs[i]);''',
  '''			for (int32 i = 0; i < FMath::Min(bBirdPick ? 40 : 8, Orbs.Num()); ++i) Keep.Add(Orbs[i]); // b96d: every target on the arena'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''			static const bool bOneWolf = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOne"));
			for (int32 k = 0; k < (bOneWolf ? 1 : 3); ++k) ArmWolf(G, Pi);''',
  '''			static const bool bOneWolf = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfOne"));
			// b96d (Evan: "Max level is 3 starting from lvl 1 with one wolf improved by using it"): Lv1 = 1 wolf, Lv2 = 2, Lv3 = 3;
			// each cast levels it up (per player, per match). -ZoogiWolfNoLevels = 3 every cast.
			static const bool bNoLv = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoLevels"));
			static TWeakObjectPtr<const AZoogiGameMode> LvG; static TMap<int32, int32> WolfLv; static int32 LvTurn = 0;
			if (LvG.Get() != G || G->TurnCounter < LvTurn) { WolfLv.Reset(); LvG = G; }
			LvTurn = G->TurnCounter;
			int32& Lv = WolfLv.FindOrAdd(Pi); Lv = FMath::Clamp(Lv, 1, 3);
			const int32 Give = bOneWolf ? 1 : (bNoLv ? 3 : Lv);
			for (int32 k = 0; k < Give; ++k) ArmWolf(G, Pi);
			const FString LvTitle = bNoLv ? FString(TEXT("3 WOLVES READY!")) : FString::Printf(TEXT("WOLF PACK LV%d: %d WOLF%s READY!"), Lv, Give, Give == 1 ? TEXT("") : TEXT("VES"));
			const FString LvSub = Caster->bCPU ? FString(TEXT("THE PACK IS COMING")) : (Lv < 3 && !bNoLv ? FString::Printf(TEXT("LAUNCH, THEN TAP SUPERPOWER - NEXT CAST LV%d"), Lv + 1) : FString(TEXT("LAUNCH, THEN TAP SUPERPOWER - MAX LEVEL")));
			UE_LOG(LogZoogi, Log, TEXT("WOLF PACK level %d for P%d: %d wolf(s) banked"), Lv, Pi + 1, Give);
			if (!bNoLv) Lv = FMath::Min(3, Lv + 1);'''),
 ('''			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(TEXT("3 WOLVES READY!"), Caster->bCPU ? TEXT("THE PACK IS COMING") : TEXT("LAUNCH, THEN TAP SUPERPOWER"), FLinearColor(0.75f, 0.45f, 1.f), 1.6f, 1.f);''',
  '''			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) F->PublicCallout(LvTitle, LvSub, FLinearColor(0.75f, 0.45f, 1.f), 1.8f, 1.f);'''),
])
print('b96d ok')
