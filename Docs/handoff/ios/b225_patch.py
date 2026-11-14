# b225 (Claude): GATLING TURRET in the Skill Lab (Evan: "add turret to skill lab so I can adjust the settings like the other skills")
import os, sys
SRC = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else '~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll')
def patch(path, pairs):
    p = os.path.join(SRC, path); s = open(p).read()
    for a, b in pairs:
        assert a in s, (path, a[:100]); s = s.replace(a, b, 1)
    open(p, 'w').write(s)
patch('Private/ZoogiSkillLab.cpp', [
('float BlastSz = 1.f; float Height = 1.f; };', 'float BlastSz = 1.f; float Height = 1.f; int32 Shots = 10, Ammo = 0; float Rate = 1.f; }; // b225: the turret rows (SHOTS / AMMO / FIRE RATE)'),
('TEXT("ko_orb"), TEXT("ko_player") };', 'TEXT("gattling_turret"), /* b225 */ TEXT("ko_orb"), TEXT("ko_player") };'),
('if (P.Num() > 6) T.BlastSz = FCString::Atof(*P[6]); if (P.Num() > 7) T.Height = FCString::Atof(*P[7]);',
 'if (P.Num() > 6) T.BlastSz = FCString::Atof(*P[6]); if (P.Num() > 7) T.Height = FCString::Atof(*P[7]); if (P.Num() > 10) { T.Shots = FCString::Atoi(*P[8]); T.Ammo = FCString::Atoi(*P[9]); T.Rate = FCString::Atof(*P[10]); } // b225'),
('%.2f,%.2f\\n"), *P.Key.ToString()', '%.2f,%.2f,%d,%d,%.2f\\n"), *P.Key.ToString()'),
('P.Value.BlastSz, P.Value.Height);', 'P.Value.BlastSz, P.Value.Height, P.Value.Shots, P.Value.Ammo, P.Value.Rate);'),
('''	const TCHAR* BlastFx(const TCHAR* DefaultFx) {''',
'''	// b225: the GATLING TURRET's own lab rows
	namespace { const FTune* TurTune() { Load(); return GTunes.Find(FName(TEXT("gattling_turret"))); } const int32 AmmoStyles[] = { 0, 5, 2, 3, 4, 6 }; const TCHAR* AmmoNames[] = { TEXT("INVENTORY"), TEXT("ROCKET"), TEXT("TRACERS"), TEXT("PLASMA"), TEXT("LASER"), TEXT("RING") }; }
	int32 TurretShots() { const FTune* T = TurTune(); return T ? FMath::Clamp(T->Shots, 1, 40) : 10; }
	int32 TurretAmmo() { const FTune* T = TurTune(); return T ? AmmoStyles[FMath::Clamp(T->Ammo, 0, 5)] : 0; }
	float TurretRate() { const FTune* T = TurTune(); return T ? FMath::Clamp(T->Rate, 0.2f, 4.f) : 1.f; }
	float SizeOf(FName Skill) { Load(); const FTune* T = GTunes.Find(Skill); return T ? FMath::Clamp(T->Size, 0.2f, 4.f) : 1.f; }
	const TCHAR* BlastFx(const TCHAR* DefaultFx) {'''),
('''		ZoogiFunCanvasRect(C, X0 - 8.f * U, Y - 10.f * U, W0 + 16.f * U, Rh * 10.4f, FLinearColor(0.03f, 0.02f, 0.04f, 0.72f));''',
 '''		const bool bTur = FCString::Strcmp(Skills[FMath::Clamp(GSel, 0, NumSkills - 1)], TEXT("gattling_turret")) == 0; // b225
		ZoogiFunCanvasRect(C, X0 - 8.f * U, Y - 10.f * U, W0 + 16.f * U, Rh * (bTur ? 12.4f : 10.4f), FLinearColor(0.03f, 0.02f, 0.04f, 0.72f));'''),
('''		Row(TEXT("PULL"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Pull * 100.f)), 6, 7);''',
 '''		if (bTur)
		{	// b225: the turret - FIRE RATE (instead of PULL), SHOTS, AMMO (INVENTORY = the player's pick)
			Row(TEXT("FIRE RATE"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Rate * 100.f)), 34, 35);
			Row(TEXT("SHOTS"), FString::Printf(TEXT("%d"), FMath::Clamp(T.Shots, 1, 40)), 36, 37);
			Row(TEXT("AMMO"), AmmoNames[FMath::Clamp(T.Ammo, 0, 5)], 38, 39);
		}
		else Row(TEXT("PULL"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.Pull * 100.f)), 6, 7);'''),
('''		case 30: Step(T.BlastSz, -0.1f, 0.2f, 4.f); break;''',
 '''		case 34: Step(T.Rate, -0.1f, 0.2f, 4.f); break; case 35: Step(T.Rate, 0.1f, 0.2f, 4.f); break; // b225 turret
		case 36: T.Shots = FMath::Clamp(T.Shots - 1, 1, 40); break; case 37: T.Shots = FMath::Clamp(T.Shots + 1, 1, 40); break;
		case 38: T.Ammo = (T.Ammo + 5) % 6; break; case 39: T.Ammo = (T.Ammo + 1) % 6; break;
		case 30: Step(T.BlastSz, -0.1f, 0.2f, 4.f); break;'''),
('''		if ((Id >= 2 && Id <= 13) || Id == 18) Save();''', '''		if ((Id >= 2 && Id <= 13) || Id == 18 || (Id >= 30 && Id <= 39)) Save(); // b225: + the height / blast size / turret rows'''),
('''		if (TestOn() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)''',
 '''		{	// b225 preview: -ZoogiLabOnly=<skill> PLAYs that one skill and takes 6 frames over 6 s
			static FString Only; static bool bOnlyRead = false; if (!bOnlyRead) { bOnlyRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiLabOnly="), Only); }
			if (TestOn() && !Only.IsEmpty() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)
			{
				for (int32 s = 0; s < NumSkills; ++s) if (Only == Skills[s]) GSel = s;
				GCam = 0; ApplyPreset(); CastNow(G);
				for (int32 k = 0; k < 6; ++k) G->TakeShotDelayed(FString::Printf(TEXT("lab_%s_%d"), *Only, k), 0.6f + 1.0f * k);
				GTestIdx = NumSkills; return;
			}
		}
		if (TestOn() && GTestIdx < NumSkills && FPlatformTime::Seconds() >= GTestNext)'''),
])
patch('Public/ZoogiSkillLab.h', [
('''	ZOOGIROLL_API bool TintFor(FName Skill, FLinearColor& Out);''',
 '''	ZOOGIROLL_API bool TintFor(FName Skill, FLinearColor& Out);
	/** b225: the GATLING TURRET lab rows - shots per turret, forced ammo style (0 = the player's inventory pick), fire-rate scale; FX SIZE of any skill */
	ZOOGIROLL_API int32 TurretShots();
	ZOOGIROLL_API int32 TurretAmmo();
	ZOOGIROLL_API float TurretRate();
	ZOOGIROLL_API float SizeOf(FName Skill);'''),
])
patch('Private/ZoogiSkills.cpp', [
# lab ammo override
('''			if (Forced >= 1) return FMath::Clamp(Forced, 1, 6);
			const AZoogiBall* Ob''', '''			if (Forced >= 1) return FMath::Clamp(Forced, 1, 6);
			if (const int32 La = ZoogiSkillLab::TurretAmmo()) return La; // b225: the Skill Lab's AMMO row (not INVENTORY)
			const AZoogiBall* Ob'''),
# lab colour + FX size on the shots
('''			const FLinearColor Col = Cols[Style];''', '''			FLinearColor Col = Cols[Style];
			{ FLinearColor Lt; if (ZoogiSkillLab::TintFor(FName(TEXT("gattling_turret")), Lt)) Col = Lt; } // b225: the lab COLOUR
			GShotFx = ZoogiSkillLab::SizeOf(FName(TEXT("gattling_turret"))); // b225: the lab FX SIZE'''),
('''		AStaticMeshActor* ShotCarrier(UWorld* W, const TCHAR* Shape, const FVector& At, const FRotator& Rot, const FVector& Scl, const FLinearColor& Col, float Glow)''',
 '''		float GShotFx = 1.f; // b225
		AStaticMeshActor* ShotCarrier(UWorld* W, const TCHAR* Shape, const FVector& At, const FRotator& Rot, const FVector& Scl, const FLinearColor& Col, float Glow)'''),
('''			Mc->SetMaterial(0, ZoogiUtil::FxMID(C, Col, Glow)); C->SetActorScale3D(Scl);''',
 '''			Mc->SetMaterial(0, ZoogiUtil::FxMID(C, Col, Glow));
			const bool bCyl = FCString::Strcmp(Shape, TEXT("Cylinder")) == 0; // the beam keeps its length
			C->SetActorScale3D(bCyl ? FVector(Scl.X * GShotFx, Scl.Y * GShotFx, Scl.Z) : Scl * GShotFx);'''),
# shots + fire rate + firing in the lab
('''		T.EndTurn = G->TurnCounter + 5 * FMath::Max(1, G->Balls.Num());''', '''		T.ShotsLeft = ZoogiSkillLab::TurretShots(); // b225: the lab SHOTS row (10)
		T.EndTurn = G->TurnCounter + 5 * FMath::Max(1, G->Balls.Num());'''),
('''		G->PublicPopup(TEXT("TURRET UP - 10 SHOTS"), P + FVector(0, 0, 200.f), GatGold); // b223''',
 '''		G->PublicPopup(FString::Printf(TEXT("TURRET UP - %d SHOTS"), T.ShotsLeft), P + FVector(0, 0, 200.f), GatGold); // b223'''),
('''					if (bInFlight || G->Phase != EZoogiPhase::Rolling || Tn < T.NextShot || T.ShotsLeft <= 0''',
 '''					if (bInFlight || (G->Phase != EZoogiPhase::Rolling && !ZoogiSkillLab::Open()) || Tn < T.NextShot || T.ShotsLeft <= 0'''),
('''					T.NextShot = Tn + 0.35; ++T.Shots; --T.ShotsLeft; // b223: 10 shots in all''',
 '''					T.NextShot = Tn + 0.35 / ZoogiSkillLab::TurretRate(); ++T.Shots; --T.ShotsLeft; // b223: 10 shots in all; b225 lab FIRE RATE'''),
# in the lab the turret stands between the hidden caster and the orb rack
('''		else if (Id == FName(TEXT("gattling_turret"))) TurretStart(G, Caster, P.Spots.Num() ? P.Spots[0] : Caster->BallLocation() + FRotator(0.f, Caster->AimYaw, 0.f).Vector() * 400.f); // b190''',
 '''		else if (Id == FName(TEXT("gattling_turret"))) TurretStart(G, Caster, ZoogiSkillLab::Open() && P.Spots.Num() ? FMath::Lerp(Caster->BallLocation(), P.Spots[0], 0.42f) : P.Spots.Num() ? P.Spots[0] : Caster->BallLocation() + FRotator(0.f, Caster->AimYaw, 0.f).Vector() * 400.f); // b190; b225: lab spot'''),
])
patch('Private/ZoogiFront.cpp', [
('''			{ TEXT("B224"),''',
 '''			{ TEXT("B225"), TEXT("TURRET IN THE SKILL LAB"), TEXT("The Gatling Turret is in the Skill Lab: tune its STRENGTH, RADIUS, HEIGHT, FIRE RATE, SHOTS, AMMO, FX SIZE, COLOUR, BLAST and BLAST SIZE, then PLAY to watch it fire at the orb rack.") },
			{ TEXT("B224"),'''),
])
print('patched b225')
