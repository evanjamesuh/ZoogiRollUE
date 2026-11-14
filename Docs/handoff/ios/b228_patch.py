# b228 (Claude): ORB RELAY tap-to-dash + combo refund + short slow-mo + raid damage; crowd option B (toon Zoogi fans holding signs);
# ITEMS option A in full (big count pill, empty = GET FROM CHESTS, +N NEW tag, chests drop items)
import os, sys
SRC = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:110]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)

# ------------------------------------------------------------------ ORB RELAY
patch('Private/ZoogiSkills.cpp', [
('struct FDashRun { TWeakObjectPtr<AZoogiBall> B; int32 Left = 10; bool bCpu = false, bRolled = false, bHasTap = false, bPrevDown = false; FVector Tap = FVector::ZeroVector; double T0 = 0.0, NextCpu = 0.0; };',
 'struct FDashRun { TWeakObjectPtr<AZoogiBall> B; int32 Left = 10, MaxLeft = 10, Combo = 0; bool bCpu = false, bRolled = false, bHasTap = false, bPrevDown = false, bDashHit = true; FVector Tap = FVector::ZeroVector; double T0 = 0.0, NextCpu = 0.0, LastDashAt = -10.0; }; // b228: combo refund'),
('''			--R.Left;
			UWorld* W = G->GetWorld();
			AZoogiFX::Spawn(W, Bl, GatGold, 160.f, 0.25f, true, 10.f);''',
 '''			--R.Left;
			if (!R.bDashHit && FPlatformTime::Seconds() - R.LastDashAt < 1.2) R.Combo = 0; // b228: the last dash missed
			R.LastDashAt = FPlatformTime::Seconds(); R.bDashHit = false;
			UWorld* W = G->GetWorld();
			AZoogiFX::Spawn(W, Bl, GatGold, 160.f, 0.25f, true, 10.f);'''),
('''				if (!Db || Db->bOut || R.Left <= 0 || (R.bRolled && !bMine) || Tn - R.T0 > 40.0)''',
 '''				if (!Db || Db->bOut || (R.Left <= 0 && Tn - R.LastDashAt > 1.0) || (R.bRolled && !bMine) || Tn - R.T0 > 40.0) // b228: the last dash still has 1 s to hit (refund)'''),
('''				if (!bMine) continue;
				if (R.bCpu)''',
 '''				if (!bMine) continue;
				if (!R.bDashHit && Tn - R.LastDashAt < 1.0)
				{	// b228 (Evan picked "combo refund" + "raid damage"): a dash that hits an orb, a rival or (raid) the boss's weak point within 1 s
					// gives the dash back and builds the RELAY combo; the weak point also takes a hit
					const FVector Bp = Db->BallLocation(); bool bHit = false, bBossHit = false;
					for (const AZoogiOrb* O : G->Orbs) if (O && !O->bOut && FVector::Dist(O->OrbLocation(), Bp) < ZoogiConst::ZoogiRadius + ZoogiConst::OrbRadius + 18.f) { bHit = true; break; }
					if (!bHit) for (const AZoogiBall* Ob : G->Balls) if (Ob && Ob != Db && !Ob->bOut && !G->IsAlly(Db, Ob) && FVector::Dist(Ob->BallLocation(), Bp) < 2.f * ZoogiConst::ZoogiRadius + 18.f) { bHit = true; break; }
					if (ZoogiRaid::Active()) if (const AActor* Hand = ZoogiRaid::HandActor()) if (FVector::Dist2D(Hand->GetActorLocation(), Bp) < 150.f + ZoogiConst::ZoogiRadius + 40.f) { bHit = true; bBossHit = true; }
					if (bHit)
					{
						R.bDashHit = true; ++R.Combo; R.Left = FMath::Min(R.MaxLeft, R.Left + 1);
						if (bBossHit) { ZoogiCosmosBoss::Flinch(); ZoogiBossAttack::HitBoss(G, Db->PlayerIndex); if (R.Combo >= 3) ZoogiBossAttack::HitBoss(G, Db->PlayerIndex); }
						G->PublicPopup(bBossHit ? FString::Printf(TEXT("RELAY HIT x%d!"), R.Combo) : FString::Printf(TEXT("RELAY x%d  +1 DASH"), R.Combo), Bp + FVector(0, 0, 170.f), bBossHit ? FLinearColor(1.f, 0.35f, 0.3f) : GatGold);
						ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.6f, 1.2f + 0.08f * FMath::Min(R.Combo, 6));
						UE_LOG(LogZoogi, Log, TEXT("ORB RELAY P%d: %s hit - combo %d, %d dashes"), Db->PlayerIndex + 1, bBossHit ? TEXT("BOSS") : TEXT("dash"), R.Combo, R.Left);
					}
				}
				if (R.bCpu)'''),
('''					if (RapidTapPoint(G, Db->BallLocation().Z - ZoogiConst::ZoogiRadius, Tp))
					{
						R.Tap = Tp; R.bHasTap = true;
						AZoogiFX::Spawn(W, Tp + FVector(0.f, 0.f, 5.f), GatGold, 90.f, 0.35f, true, 8.f);
					}''',
 '''					if (RapidTapPoint(G, Db->BallLocation().Z - ZoogiConst::ZoogiRadius, Tp))
					{
						R.Tap = Tp; R.bHasTap = true;
						AZoogiFX::Spawn(W, Tp + FVector(0.f, 0.f, 5.f), GatGold, 90.f, 0.35f, true, 8.f);
						static const bool bSuperOnly = FParse::Param(FCommandLine::Get(), TEXT("ZoogiRelaySuperpower"));
						if (!bSuperOnly && R.Left > 0) DashOnce(G, R, Tp); // b228 (Evan picked "tap to dash"): the tap IS the dash (-ZoogiRelaySuperpower = b223)
					}'''),
('''			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("ORB RELAY!"), FString::Printf(TEXT("TAP WHERE TO GO - SUPERPOWER DASHES THERE  x%d"), R.Left), GatGold, 1.2f, 0.9f);''',
 '''			R.MaxLeft = R.Left;
			if (UZoogiFunSys* F = UZoogiFunSys::Get(G)) if (!Caster->bCPU) F->PublicCallout(TEXT("ORB RELAY!"), FString::Printf(TEXT("TAP THE COURT TO DASH THERE  x%d  -  HIT SOMETHING = THE DASH COMES BACK"), R.Left), GatGold, 1.2f, 0.9f);'''),
('''	bool DashActive(int32* OutLeft)
	{''',
 '''	bool DashSlowMo()
	{	// b228 (Evan picked "short slow-motion"): ~0.35 s of slow-mo after each human dash, to aim the next one
		static float Sl = -1.f; if (Sl < 0.f) { Sl = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiRelaySlowMo="), Sl); }
		for (const FDashRun& R : DashRuns) if (!R.bCpu && R.B.IsValid() && FPlatformTime::Seconds() - R.LastDashAt < Sl) return true;
		return false;
	}
	bool DashActive(int32* OutLeft)
	{'''),
])
patch('Public/ZoogiSkills.h', [
('''	ZOOGIROLL_API bool DashActive(int32* OutLeft = nullptr);''', '''	ZOOGIROLL_API bool DashActive(int32* OutLeft = nullptr);
	/** b228: true for a moment after each ORB RELAY dash (the world runs slowed) */
	ZOOGIROLL_API bool DashSlowMo();'''),
])
patch('Private/ZoogiActionFun.cpp', [
('''			else if (ZoogiSkills::RapidActive()) Want = 0.0001f; // b197''',
 '''			else if (ZoogiSkills::DashSlowMo()) Want = 0.3f; // b228: ORB RELAY - a short slow-mo after each dash
			else if (ZoogiSkills::RapidActive()) Want = 0.0001f; // b197'''),
])

# ------------------------------------------------------------------ ITEMS option A in full + chests drop items
patch('Private/ZoogiFront.cpp', [
('int32 Kind = 0; int32 HandSlot = -1; int32 Count = -1; /* b227: items - how many you carry */ };',
 'int32 Kind = 0; int32 HandSlot = -1; int32 Count = -1; /* b227: items - how many you carry */ int32 NewN = 0; /* b228: +N NEW from chests */ };'),
('''				I.Sub = FString::Printf(TEXT("%s ITEM  x%d"), ZRarityName(W[i].R), I.Count); // b137''',
 '''				I.NewN = P ? P->Data().ItemCounts.FindRef(FName(*(FString(TEXT("new_")) + W[i].K))) : 0; // b228
				I.Sub = FString::Printf(TEXT("%s ITEM  x%d"), ZRarityName(W[i].R), I.Count); // b137'''),
('''		const FLinearColor Dim = I.bOwned ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f);''',
 '''		const FLinearColor Dim = (I.bOwned && !(I.Kind == 2 && I.Count == 0)) ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1.f); // b228: an empty item is grey'''),
('''			const float Fs = FMath::Max(24.f, W * 0.15f), Pw = ZMeasure(Cn, Font(0, Fs)).X + Fs * 0.7f, Ph = Fs * 1.35f;''',
 '''			const float Fs = FMath::Max(28.f, W * 0.2f), Pw = ZMeasure(Cn, Font(0, Fs)).X + Fs * 0.7f, Ph = Fs * 1.3f; // b228: as big as the mockup'''),
('''			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? Paper : FLinearColor(0.6f, 0.6f, 0.65f));
		}''',
 '''			ZTextC(Out, G, L + 8, Cn, Font(0, Fs), Pt + FVector2D(Pw * 0.5f, Ph * 0.5f), I.Count > 0 ? Paper : FLinearColor(0.6f, 0.6f, 0.65f));
			if (I.Count == 0)
			{	// b228: none left - where to get more
				const FString Gt = TEXT("GET FROM CHESTS"); const float Gf = FMath::Max(16.f, W * 0.085f), Gw = FMath::Min(W - 16.f, ZMeasure(Gt, Font(0, Gf)).X + 20.f);
				ZRect(Out, G, L + 6, TL + FVector2D(W * 0.5f - Gw * 0.5f, W * 0.36f), FVector2D(Gw, Gf * 1.6f), FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));
				ZTextC(Out, G, L + 7, Gt, ZFitFont(Gt, 0, Gf, Gw - 8.f), TL + FVector2D(W * 0.5f, W * 0.36f + Gf * 0.8f), FLinearColor(0.85f, 0.85f, 0.9f));
			}
			if (I.NewN > 0)
			{	// b228: +N NEW (from a chest), top-left, until you open the item
				const FString Nw = FString::Printf(TEXT("+%d NEW"), I.NewN); const float Nf = FMath::Max(20.f, W * 0.12f), Nwid = ZMeasure(Nw, Font(0, Nf)).X + 18.f;
				ZRect(Out, G, L + 6, TL + FVector2D(6.f, 14.f), FVector2D(Nwid, Nf * 1.35f), FLinearColor(0.27f, 0.88f, 0.43f, 1.f));
				ZTextC(Out, G, L + 7, Nw, Font(0, Nf), TL + FVector2D(6.f + Nwid * 0.5f, 14.f + Nf * 0.68f), Paper);
			}
		}'''),
('''			else AddHit(TL, FVector2D(W, W), [Self, i]() { Self->ItemSel = i; Self->Pop = SZoogiFront::EPop::Item; Self->PopT = Self->T; Self->Sfx(TEXT("UiClick"), 0.8f, 1.15f); });''',
 '''			else { const FString Wk = It[i].WeaponKey; AddHit(TL, FVector2D(W, W), [Self, i, Wk]() { Self->ItemSel = i; Self->Pop = SZoogiFront::EPop::Item; Self->PopT = Self->T; Self->Sfx(TEXT("UiClick"), 0.8f, 1.15f);
				if (!Wk.IsEmpty()) if (UZoogiFrontProfileSubsystem* Pn = Self->FP()) if (Pn->Mutable().ItemCounts.Remove(FName(*(FString(TEXT("new_")) + Wk))) > 0) Pn->Save(); }); } // b228: opening it clears +N NEW'''),
# chest reward tiles: items show their art + xN
('''			if (R.Kind == FName(TEXT("card")))
			{
				if (UTexture2D* A = ZLoadPng(TEXT("Data/CardArt/") + R.Id.ToString())) ZTexFit(Out, TG, Lv + 3, A, O + FVector2D(5.f, 5.f), Sz - FVector2D(10.f, 10.f));''',
 '''			if (R.Kind == FName(TEXT("item")))
			{	// b228: an item from the chest
				const FString Ip = R.Id == FName(TEXT("apple")) ? FString(TEXT("Data/CardArt/golden_apple")) : FString(TEXT("Data/MenuArt/Weapons/w_")) + R.Id.ToString();
				if (UTexture2D* A = ZLoadPng(Ip)) ZTexFit(Out, TG, Lv + 3, A, O + FVector2D(5.f, 5.f), Sz - FVector2D(10.f, 10.f));
				Badge = FString::Printf(TEXT("x%d"), R.Amount);
			}
			else if (R.Kind == FName(TEXT("card")))
			{
				if (UTexture2D* A = ZLoadPng(TEXT("Data/CardArt/") + R.Id.ToString())) ZTexFit(Out, TG, Lv + 3, A, O + FVector2D(5.f, 5.f), Sz - FVector2D(10.f, 10.f));'''),
('''			{ TEXT("B227"),''',
 '''			{ TEXT("B228"), TEXT("ORB RELAY 2 + SIGN-WAVING CROWD"), TEXT("ORB RELAY: tap the court and you dash there at once (no SUPERPOWER), a short slow-mo follows each dash, and a dash that hits an orb or a rival gives the dash back and builds a RELAY combo. In a RAID, dashing into the weak point hurts the boss. The crowd is now round Zoogi fans in team colours, and some hold up signs (GO BLUE!, KO!, BOOM!...) that pop up on big moments. Chests now drop items too: the ITEM tab shows +N NEW, the count in the corner, and GET FROM CHESTS when you run out.") },
			{ TEXT("B227"),'''),
])
patch('Private/ZoogiFrontProfile.cpp', [
('''	Got.ValueSort([](int32 A, int32 B) { return A > B; });''',
 '''	{	// b228 (Evan: "items have counters" / "chests drop items"): wooden 1, royal 2, epic 3 items (+1 each, carry up to 5; a full one pays 25 gold)
		static const TCHAR* Its[] = { TEXT("gatling"), TEXT("grenade"), TEXT("sword"), TEXT("staff"), TEXT("disc"), TEXT("apple") };
		const int32 Ni = Type == FName(TEXT("epic")) ? 3 : Type == FName(TEXT("royal")) ? 2 : 1;
		TMap<FName, int32> GotI; int32 Full = 0;
		for (int32 k = 0; k < Ni; ++k)
		{
			const FName It(Its[R.RandRange(0, 5)]);
			int32& Have = D.ItemCounts.FindOrAdd(It);
			if (Have >= 5) { ++Full; continue; }
			++Have; GotI.FindOrAdd(It) += 1; D.ItemCounts.FindOrAdd(FName(*(FString(TEXT("new_")) + It.ToString()))) += 1;
		}
		for (const TPair<FName, int32>& P : GotI) Out.Add({ FName(TEXT("item")), P.Key, P.Value });
		if (Full > 0) { D.Gold += 25 * Full; Out.Add({ FName(TEXT("gold")), NAME_None, 25 * Full }); }
	}
	Got.ValueSort([](int32 A, int32 B) { return A > B; });'''),
])

# ------------------------------------------------------------------ crowd option B
patch('Public/ZoogiFrozenFun.h', [
('''	struct FMember { int32 Mesh = 0, Inst = 0, Islet = 0; FVector Base; float Phase = 0.f, Energy = 1.f, Scale = 1.f, Ang = 0.f; FVector Off; bool bSpinner = false; TWeakObjectPtr<AZoogiArenaPiece> Stand; float Fall = -1.f; };''',
 '''	struct FMember { int32 Mesh = 0, Inst = 0, Islet = 0; FVector Base; float Phase = 0.f, Energy = 1.f, Scale = 1.f, Ang = 0.f; FVector Off; bool bSpinner = false; TWeakObjectPtr<AZoogiArenaPiece> Stand; float Fall = -1.f;
		int32 Col = 0, Sign = -1, BodyI = 0, EyeI = 0, PupI = 0, StickI = -1, BoardI = -1; }; // b228: the toon fan parts
	/** b228 (Evan picked crowd option B): toon Zoogi fans (team-colour spheres with ears, eyes, hands), ~30% hold up a sign */
	bool BuildToon(const TArray<FVector>& Seats, const TArray<int32>& Islet);
	void TickToon(float Dt);
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> ToonISMs;
	TArray<TArray<FTransform>> ToonXf;
	bool bToon = false; int32 NCol = 0, NSign = 0;'''),
])
patch('Private/ZoogiFrozenFun.cpp', [
('#include "Materials/MaterialInstanceDynamic.h"\n', '#include "Materials/MaterialInstanceDynamic.h"\n#include "ImageUtils.h" // b228: crowd signs\n#include "Engine/Texture2D.h"\n'),
('''void AZoogiFrozenCrowd::BuildCrowd(const TArray<FVector>& Seats, const TArray<int32>& Islet)
{''',
 '''bool AZoogiFrozenCrowd::BuildToon(const TArray<FVector>& Seats, const TArray<int32>& Islet)
{
	auto NewISM = [&](const TCHAR* Shape, UMaterialInterface* M) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* I = NewObject<UInstancedStaticMeshComponent>(this);
		I->SetStaticMesh(ZoogiUtil::BasicShape(Shape)); I->SetMobility(EComponentMobility::Movable); I->SetupAttachment(Root);
		I->SetCollisionEnabled(ECollisionEnabled::NoCollision); I->SetCastShadow(false); I->SetCanEverAffectNavigation(false); I->SetGenerateOverlapEvents(false);
		I->RegisterComponent(); if (M) I->SetMaterial(0, M); ToonISMs.Add(I); return I;
	};
	static const TCHAR* Cols[] = { TEXT("#4696FF"), TEXT("#FF5046"), TEXT("#FFC832"), TEXT("#AA5AFF"), TEXT("#5AD278"), TEXT("#FF78C8") };
	NCol = UE_ARRAY_COUNT(Cols);
	for (const TCHAR* C : Cols) NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(C), 0.5f));
	NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, FLinearColor(0.95f, 0.95f, 0.97f), 0.3f)); // eye whites
	NewISM(TEXT("Sphere"), ZoogiUtil::SurfaceMID(this, FLinearColor(0.02f, 0.02f, 0.03f), 0.2f)); // pupils
	NewISM(TEXT("Cylinder"), ZoogiUtil::SurfaceMID(this, ZoogiUtil::Hex(TEXT("#6E4F32")), 0.8f)); // sign sticks
	UMaterialInterface* SignBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Env/Cosmic/M_CosmicPlanet.M_CosmicPlanet"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	NSign = 0;
	for (int32 s = 0; s < 8 && SignBase; ++s)
	{
		const FString Pth = FPaths::ProjectContentDir() / FString::Printf(TEXT("Data/Crowd/sign_%d.png"), s);
		UTexture2D* Tx = FPaths::FileExists(Pth) ? FImageUtils::ImportFileAsTexture2D(Pth) : nullptr;
		if (!Tx) continue;
		UMaterialInstanceDynamic* Mi = UMaterialInstanceDynamic::Create(SignBase, this);
		Mi->SetTextureParameterValue(TEXT("Tex"), Tx); Mi->SetScalarParameterValue(TEXT("Bright"), 1.15f);
		NewISM(TEXT("Plane"), Mi); ++NSign;
	}
	TArray<AZoogiArenaPiece*> Stands;
	for (TActorIterator<AZoogiArenaPiece> It(GetWorld()); It; ++It) if (It->GetKind() == TEXT("stand")) Stands.Add(*It);
	FRandomStream Rng(31337);
	static float SignChance = -1.f; if (SignChance < 0.f) { SignChance = 0.3f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiCrowdSigns="), SignChance); }
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		FMember M;
		M.Islet = Islet.IsValidIndex(i) ? Islet[i] : 0;
		M.Base = Seats[i]; M.Phase = Rng.FRandRange(0.f, 2.f * PI); M.Energy = Rng.FRandRange(0.6f, 1.25f); M.Scale = Rng.FRandRange(0.88f, 1.1f);
		M.bSpinner = Rng.FRand() < 0.12f; M.Ang = FMath::Atan2(Seats[i].Y, Seats[i].X);
		M.Col = Rng.RandRange(0, NCol - 1);
		float Best = 1.e9f;
		for (AZoogiArenaPiece* S : Stands) { const float D = FVector::Dist2D(S->GetComponentsBoundingBox().GetCenter(), Seats[i]); if (D < Best) { Best = D; M.Stand = S; } }
		const FTransform Z(FRotator::ZeroRotator, M.Base, FVector(0.01f));
		M.BodyI = ToonISMs[M.Col]->GetInstanceCount(); for (int32 k = 0; k < 5; ++k) ToonISMs[M.Col]->AddInstance(Z, true); // body, 2 ears, 2 hands
		M.EyeI = ToonISMs[NCol]->GetInstanceCount(); for (int32 k = 0; k < 2; ++k) ToonISMs[NCol]->AddInstance(Z, true);
		M.PupI = ToonISMs[NCol + 1]->GetInstanceCount(); for (int32 k = 0; k < 2; ++k) ToonISMs[NCol + 1]->AddInstance(Z, true);
		if (NSign > 0 && Rng.FRand() < SignChance)
		{
			M.Sign = Rng.RandRange(0, NSign - 1);
			M.StickI = ToonISMs[NCol + 2]->AddInstance(Z, true);
			M.BoardI = ToonISMs[NCol + 3 + M.Sign]->AddInstance(Z, true);
		}
		Members.Add(M);
	}
	ToonXf.SetNum(ToonISMs.Num());
	for (int32 m = 0; m < ToonISMs.Num(); ++m) ToonXf[m].SetNum(ToonISMs[m]->GetInstanceCount());
	bToon = true;
	FinishBuild();
	int32 Ns = 0; for (const FMember& M : Members) if (M.Sign >= 0) ++Ns;
	UE_LOG(LogZoogi, Log, TEXT("CROWD: %d toon Zoogi fans (%d holding signs, %d sign kinds) on %d stands"), Members.Num(), Ns, NSign, Stands.Num());
	return true;
}

void AZoogiFrozenCrowd::TickToon(float Dt)
{
	Clock3 += Dt;
	const float Tm = Clock3;
	static float SignSpin = -1000.f; if (SignSpin < -999.f) { SignSpin = 0.f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiSignSpin="), SignSpin); }
	static const bool bSignFlip = FParse::Param(FCommandLine::Get(), TEXT("ZoogiSignFlip"));
	for (FMember& M : Members)
	{
		FVector P = M.Base;
		const FVector ToLook = Look - M.Base;
		float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToLook.Y, ToLook.X));
		float Pitch = 0.f, Roll = 0.f; float Sq = 0.f;
		const float Ph = M.Phase, E = M.Energy;
		P.Z += 2.5f * FMath::Sin(Tm * 1.7f + Ph);
		Yaw += 7.f * FMath::Sin(Tm * 0.8f + Ph * 1.3f);
		if (Hype > 0.01f) P.Z += 7.f * Hype * E * FMath::Abs(FMath::Sin(Tm * 6.5f + Ph));
		if (Cheer > 0.01f)
		{
			P.Z += 36.f * Cheer * E * FMath::Abs(FMath::Sin(Tm * 9.5f * (0.85f + 0.3f * E) + Ph));
			Yaw += 18.f * Cheer * FMath::Sin(Tm * 13.f + Ph);
			if (M.bSpinner && M.Sign < 0) Yaw += Cheer * FMath::Fmod(Tm * 520.f + Ph * 57.f, 360.f);
			Sq = 0.14f * Cheer * FMath::Sin(Tm * 19.f + Ph);
		}
		if (Gasp > 0.01f) { P += FVector(-ToLook.X, -ToLook.Y, 0.f).GetSafeNormal() * 16.f * Gasp; Pitch -= 16.f * Gasp; Roll += 5.f * Gasp * FMath::Sin(Tm * 31.f + Ph); }
		if (WaveA > -100.f)
		{
			float D = FMath::Fmod(M.Ang - WaveA, 2.f * PI); if (D > PI) D -= 2.f * PI; if (D < -PI) D += 2.f * PI;
			const float Wv = FMath::Exp(-D * D / (2.f * 0.12f * 0.12f)); P.Z += 48.f * Wv; Pitch += 10.f * Wv;
		}
		float Sc = M.Scale;
		if (M.Stand.IsValid() && M.Stand->IsBroken()) { if (M.Fall < 0.f) M.Fall = 0.f; M.Fall += Dt; P.Z -= 0.5f * 980.f * M.Fall * M.Fall; Roll += M.Fall * 400.f; if (M.Fall > 1.2f) Sc = 0.001f; }
		else M.Fall = -1.f;
		const FRotator R(Pitch, Yaw, Roll); const FQuat Q(R);
		const FVector C = P + FVector(0.f, 0.f, 34.f * Sc);
		auto At = [&](const FVector& L) { return C + Q.RotateVector(L * Sc); };
		auto PutX = [&](int32 Isl, int32 Idx, const FVector& L, const FVector& S, const FQuat& Rq) { if (ToonXf[Isl].IsValidIndex(Idx)) ToonXf[Isl][Idx] = FTransform(Rq, At(L), S * Sc); };
		const float Up = FMath::Clamp(Cheer * 1.4f, 0.f, 1.f);
		const bool bSign = M.Sign >= 0;
		PutX(M.Col, M.BodyI + 0, FVector::ZeroVector, FVector(0.6f, 0.6f, 0.6f * (1.f + Sq)), Q);
		PutX(M.Col, M.BodyI + 1, FVector(-3.f, -17.f, 26.f), FVector(0.2f), Q);
		PutX(M.Col, M.BodyI + 2, FVector(-3.f, 17.f, 26.f), FVector(0.2f), Q);
		const FVector HandDn(6.f, 31.f, -6.f), HandUp(6.f, 30.f, 34.f + 6.f * FMath::Sin(Tm * 14.f + Ph));
		PutX(M.Col, M.BodyI + 3, FMath::Lerp(HandDn, HandUp, Up) * FVector(1.f, -1.f, 1.f), FVector(0.16f), Q);
		PutX(M.Col, M.BodyI + 4, bSign ? FVector(8.f, 30.f, 22.f) : FMath::Lerp(HandDn, HandUp, Up), FVector(0.16f), Q);
		PutX(NCol, M.EyeI + 0, FVector(24.f, -10.f, 8.f), FVector(0.1f, 0.13f, 0.15f), Q);
		PutX(NCol, M.EyeI + 1, FVector(24.f, 10.f, 8.f), FVector(0.1f, 0.13f, 0.15f), Q);
		PutX(NCol + 1, M.PupI + 0, FVector(29.f, -10.f, 7.f), FVector(0.07f), Q);
		PutX(NCol + 1, M.PupI + 1, FVector(29.f, 10.f, 7.f), FVector(0.07f), Q);
		if (bSign)
		{	// the sign goes up (and shakes) on big moments
			const float Lift = 18.f + 34.f * Up, Shake = 6.f * Up * FMath::Sin(Tm * 16.f + Ph);
			PutX(NCol + 2, M.StickI, FVector(10.f, 30.f, 22.f + Lift * 0.5f + 25.f), FVector(0.035f, 0.035f, 0.55f), Q);
			const FVector Fwd = Q.GetForwardVector(), Right = Q.GetRightVector();
			FQuat Bq = FRotationMatrix::MakeFromZX(Fwd, bSignFlip ? -Right : Right).ToQuat();
			if (FMath::Abs(SignSpin) > 0.1f) Bq = FQuat(Fwd, FMath::DegreesToRadians(SignSpin)) * Bq;
			Bq = FQuat(Fwd, FMath::DegreesToRadians(Shake)) * Bq;
			PutX(NCol + 3 + M.Sign, M.BoardI, FVector(12.f, 30.f, 62.f + Lift), FVector(0.92f, 0.46f, 1.f), Bq);
		}
	}
	for (int32 m = 0; m < ToonISMs.Num(); ++m)
		if (ToonISMs[m] && ToonXf[m].Num() > 0) ToonISMs[m]->BatchUpdateInstancesTransforms(0, ToonXf[m], true, true, true);
}

void AZoogiFrozenCrowd::BuildCrowd(const TArray<FVector>& Seats, const TArray<int32>& Islet)
{
	static const bool bOldCrowd = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldCrowd")); // b228: the old-style meshes (rendered as black balls)
	if (!bOldCrowd && BuildToon(Seats, Islet)) return;'''),
('''	Super::Tick(Dt);
	if (Members.Num() == 0) return;
	Clock3 += Dt;''',
 '''	Super::Tick(Dt);
	if (Members.Num() == 0) return;
	if (bToon) { TickToon(Dt); return; } // b228
	Clock3 += Dt;'''),
])
print('patched b228')
