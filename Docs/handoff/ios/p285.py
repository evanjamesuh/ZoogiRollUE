import os
B=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def edf(f,pairs):
    p=B+f; s=open(p,encoding='utf-8').read()
    for old,new in pairs:
        assert s.count(old)==1,(f,old[:70],s.count(old)); s=s.replace(old,new)
    open(p,'w',encoding='utf-8').write(s)
# ---- GameMode: fall guard
edf('Private/ZoogiGameMode.cpp',[
("void AZoogiGameMode::CheckKnockoffs()\n{",
"""// b285 (Evan: "fix the continuous free fall", options 1 + 3 + 4): a Zoogi that falls more than 3 m below the court or keeps falling with
// no floor under it for 1.5 s is a ring-out while rolling; in any other phase (a turn starting, aiming, waiting) it is put back on safe
// ground at once. The P5 menu waits while the turn player's Zoogi is in the air. Off: -ZoogiNoFallGuard
namespace ZoogiFallGuardData
{
	static TMap<const AZoogiBall*, float> GFallT;   // seconds falling with no floor under it
	static TMap<const AZoogiBall*, double> GLastSave;
	static bool Off() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoFallGuard")); return b; }
}
bool ZoogiFallGuardAirborne(const AZoogiBall* B)
{
	const float* T = B ? ZoogiFallGuardData::GFallT.Find(B) : nullptr;
	return T && *T > 0.15f;
}
static bool ZoogiFallGuardLongFall(const AZoogiBall* B, const FVector& L, float CourtZ)
{
	if (ZoogiFallGuardData::Off() || !B) return false;
	const float* T = ZoogiFallGuardData::GFallT.Find(B);
	return L.Z < CourtZ - 300.f || (T && *T > 1.5f);
}

void AZoogiGameMode::CheckKnockoffs()
{"""),
("		if (bZeroG || bCapOut || AZoogiArena::IsRingOut(L) || ZoogiArenaReact::FallOut(this, L)) // Frozen Ring 2: also out past r2300; b171 -ZoogiFallOut=",
 "		const bool bLongFall = !bZeroG && (!Arena || Arena->GetTheme() != EZoogiArena::Ancient) && ZoogiFallGuardLongFall(B, L, Arena ? Arena->GetActorLocation().Z : 0.f); // b285\n"
 "		if (bLongFall) UE_LOG(LogZoogi, Log, TEXT(\"FALLGUARD P%d fell off (%.0f, %.0f, %.0f) - ring-out\"), B->PlayerIndex + 1, L.X, L.Y, L.Z);\n"
 "		if (bZeroG || bCapOut || bLongFall || AZoogiArena::IsRingOut(L) || ZoogiArenaReact::FallOut(this, L)) // Frozen Ring 2: also out past r2300; b171 -ZoogiFallOut="),
("	// Oct 28 (Evan): menus and classic turn to portrait with the phone; Battle Royale / 2v2 stay landscape",
"""	if (!ZoogiFallGuardData::Off() && !bRoyaleMode && !bComboMode && Arena && Arena->GetTheme() != EZoogiArena::Ancient && Phase != EZoogiPhase::Menu && Phase != EZoogiPhase::GameOver)
	{	// b285: the fall guard (see ZoogiFallGuardData)
		const float Fz = Arena->GetActorLocation().Z;
		for (AZoogiBall* Bf : Balls)
		{
			if (!Bf || Bf->bOut || Bf->bParked || IsPadAirborne(Bf) || ZoogiEdgeHang::Forced(Bf)) { ZoogiFallGuardData::GFallT.Remove(Bf); continue; }
			const FVector Lf = Bf->BallLocation(), Vf = Bf->GetBallVelocity();
			FHitResult Hf; FCollisionQueryParams Qf(SCENE_QUERY_STAT(ZoogiFallGuard), false, Bf);
			const bool bFloor = GetWorld()->LineTraceSingleByObjectType(Hf, Lf, Lf - FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 260.f), FCollisionObjectQueryParams(ECC_WorldStatic), Qf);
			float& Tf = ZoogiFallGuardData::GFallT.FindOrAdd(Bf);
			Tf = (Vf.Z < -200.f && !bFloor) ? Tf + DeltaSeconds : 0.f;
			const bool bDeep = Lf.Z < Fz - 300.f;
			if (Phase == EZoogiPhase::Rolling) continue; // rolling: CheckKnockoffs makes it a ring-out
			if (!bDeep && Tf < 0.4f) continue;
			// not rolling (a turn starting, aiming, waiting): back on safe ground now (3: safe-ground check)
			double& Last = ZoogiFallGuardData::GLastSave.FindOrAdd(Bf);
			const double Nw = FPlatformTime::Seconds();
			const bool bAgain = Nw - Last < 6.0; Last = Nw;
			UE_LOG(LogZoogi, Warning, TEXT("FALLGUARD P%d falling in phase %d at (%.0f, %.0f, %.0f), vz %.0f - back on safe ground%s"), Bf->PlayerIndex + 1, (int32)Phase, Lf.X, Lf.Y, Lf.Z, Vf.Z, bAgain ? TEXT(" (court centre - its spot keeps dropping it)") : TEXT(""));
			if (Bf->Sphere) Bf->Sphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Tf = 0.f;
			if (!bAgain) RespawnBall(Bf);
			else { const FVector P = FindFreeSpot(Arena->GetActorLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 20.f)); Bf->PlaceAt(P, Bf->SpawnYaw); }
			if (UZoogiFunSys* Ff = UZoogiFunSys::Get(this)) Ff->PublicCallout(TEXT("RESCUED!"), TEXT("BACK ON SAFE GROUND"), FLinearColor(0.35f, 0.85f, 1.f), 1.f, 0.8f);
		}
	}
	// Oct 28 (Evan): menus and classic turn to portrait with the phone; Battle Royale / 2v2 stay landscape"""),
])
# ---- P5 menu waits while airborne
edf('Private/ZoogiP5Command.cpp',[
("		const bool bWant = bTurn && !bDismissed &&",
 "		const bool bWant = bTurn && !ZoogiFallGuardAirborne(B) && !bDismissed &&"),
("namespace ZoogiP5Cmd\n{", "bool ZoogiFallGuardAirborne(const AZoogiBall* B); // b285: ZoogiGameMode.cpp\nnamespace ZoogiP5Cmd\n{"),
])
# ---- raid loading screen: 4 cards
p=B+'Private/ZoogiLoadScreen.cpp'; s=open(p,encoding='utf-8').read()
old="""				Tips.Add({ TEXT("HIT THE WEAK POINT"), bDrag ? TEXT("Roll into her glowing heart crystal. Every orb you knock out fires a volley at her - star orbs hit harder.") : TEXT("Roll into its glowing weak point. Every orb you knock out fires a volley at it - star orbs hit harder."), Gold });"""
new="""				Tips.Add({ TEXT("KNOCK ORBS OFF = DAMAGE"), bDrag ? TEXT("Every orb you knock off the arena fires a volley at her. More orbs in one shot = more damage, gold star orbs hit harder.") : TEXT("Every orb you knock off the arena fires a volley at it. More orbs in one shot = more damage, gold star orbs hit harder."), Orange }); // b285 (Evan: "the loading screen doesn't mention knocking orbs off")
				Tips.Add({ TEXT("HIT THE WEAK POINT"), bDrag ? TEXT("Roll straight into her glowing heart crystal for big hits - bounce off walls to reach it.") : TEXT("Roll straight into its glowing weak point for big hits - bounce off walls to reach it."), Gold });"""
assert s.count(old)==1; s=s.replace(old,new)
# layout: fit 4 cards
old2="""			if (bPort) { Tx0 = 34.f * U; Tw = Wd - 68.f * U; Th = 150.f * U; Gap = 22.f * U; Ty0 = Ah + 40.f * U; }
			else if (S.bRaid) { Tx0 = Wd * 0.465f; Tw = Wd * 0.49f; Th = FMath::Min(150.f * U, Ht * 0.19f); Gap = Ht * 0.035f; Ty0 = Ht * 0.12f; }"""
new2="""			if (bPort) { Tx0 = 34.f * U; Tw = Wd - 68.f * U; Th = 150.f * U; Gap = 22.f * U; Ty0 = Ah + 40.f * U; }
			else if (S.bRaid) { Tx0 = Wd * 0.465f; Tw = Wd * 0.49f; Th = FMath::Min(150.f * U, Ht * (Tips.Num() > 3 ? 0.155f : 0.19f)); Gap = Ht * (Tips.Num() > 3 ? 0.025f : 0.035f); Ty0 = Ht * (Tips.Num() > 3 ? 0.07f : 0.12f); } // b285: 4 raid cards"""
assert s.count(old2)==1; s=s.replace(old2,new2)
open(p,'w',encoding='utf-8').write(s)
# NEWS
edf('Private/ZoogiFront.cpp',[('{ TEXT("B284"),','{ TEXT("B285"), TEXT("NO MORE ENDLESS FALLS + RAID TIPS"), TEXT("A Zoogi that drops off the arena can no longer fall forever: while rolling it counts as a ring-out, and at any other time it is rescued back onto safe ground. The menu waits until your Zoogi has landed. The RAID loading screen now leads with KNOCK ORBS OFF = DAMAGE.") },\n{ TEXT("B284"),')])
print('ok')
