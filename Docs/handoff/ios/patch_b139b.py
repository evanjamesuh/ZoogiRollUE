# b139b (Evan 10:41: "in Skill Lab the blast types should be able to cycle for ANY skill, and blast size"):
#  - new BLAST SIZE row (20% - 400%, saved in SkillLab.ini as a 7th field; used in matches too)
#  - every skill's impact points now take the lab's BLAST pick: skills with their own blast (meteors, shockwave, wolf blasts...) swap
#    to it; skills without one (water cannon, magic rays, precision, sword slash, apple mines, golden apple, bolt zaps, halo disc,
#    wolves) get the chosen blast added where they hit. BLAST = DEFAULT keeps each skill's own look.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
L = 'Private/ZoogiSkillLab.cpp'; LH = 'Public/ZoogiSkillLab.h'
ed1(L, '''		struct FTune { float Strength = 1.f, Radius = 1.f, Pull = 1.f, Size = 1.f; int32 Color = 0, Blast = 0; };''',
'''		struct FTune { float Strength = 1.f, Radius = 1.f, Pull = 1.f, Size = 1.f; int32 Color = 0, Blast = 0; float BlastSz = 1.f; }; // b139b: BLAST SIZE''')
ed1(L, '''T.Color = FCString::Atoi(*P[4]); T.Blast = FCString::Atoi(*P[5]);''', '''T.Color = FCString::Atoi(*P[4]); T.Blast = FCString::Atoi(*P[5]); if (P.Num() > 6) T.BlastSz = FCString::Atof(*P[6]);''')
ed1(L, '''S += FString::Printf(TEXT("%s=%.2f,%.2f,%.2f,%.2f,%d,%d\\n"), *P.Key.ToString(), P.Value.Strength, P.Value.Radius, P.Value.Pull, P.Value.Size, P.Value.Color, P.Value.Blast);''',
'''S += FString::Printf(TEXT("%s=%.2f,%.2f,%.2f,%.2f,%d,%d,%.2f\\n"), *P.Key.ToString(), P.Value.Strength, P.Value.Radius, P.Value.Pull, P.Value.Size, P.Value.Color, P.Value.Blast, P.Value.BlastSz);''')
ed1(L, '''		ZoogiFunCanvasRect(C, X0 - 8.f * U, Y - 10.f * U, W0 + 16.f * U, Rh * 8.4f, FLinearColor(0.03f, 0.02f, 0.04f, 0.72f));''',
'''		ZoogiFunCanvasRect(C, X0 - 8.f * U, Y - 10.f * U, W0 + 16.f * U, Rh * 9.4f, FLinearColor(0.03f, 0.02f, 0.04f, 0.72f));''')
ed1(L, '''		Row(TEXT("BLAST"), BlastNames[FMath::Clamp(T.Blast, 0, NumBlasts - 1)], 12, 13);''', '''		Row(TEXT("BLAST"), BlastNames[FMath::Clamp(T.Blast, 0, NumBlasts - 1)], 12, 13);
		Row(TEXT("BLAST SIZE"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(T.BlastSz * 100.f)), 30, 31); // b139b''')
ed1(L, '''		case 12: T.Blast = (T.Blast + NumBlasts - 1) % NumBlasts; break; case 13: T.Blast = (T.Blast + 1) % NumBlasts; break;''',
'''		case 12: T.Blast = (T.Blast + NumBlasts - 1) % NumBlasts; break; case 13: T.Blast = (T.Blast + 1) % NumBlasts; break;
		case 30: Step(T.BlastSz, -0.1f, 0.2f, 4.f); break; case 31: Step(T.BlastSz, 0.1f, 0.2f, 4.f); break; // b139b''')
ed1(L, '''	const TCHAR* BlastFx(const TCHAR* DefaultFx) {''', '''	float BlastSize() { const FTune* T = Cur(); return T ? FMath::Clamp(T->BlastSz, 0.2f, 4.f) : 1.f; }
	const TCHAR* ChosenBlast() { const FTune* T = Cur(); if (!T || T->Blast <= 0 || T->Blast >= NumBlasts) return nullptr; return BlastFxs[T->Blast]; }
	const TCHAR* BlastFx(const TCHAR* DefaultFx) {''')
ed1(LH, '''	ZOOGIROLL_API const TCHAR* BlastFx(const TCHAR* DefaultFx);''', '''	ZOOGIROLL_API const TCHAR* BlastFx(const TCHAR* DefaultFx);
	/** b139b: the BLAST SIZE row (1 = default) */
	ZOOGIROLL_API float BlastSize();
	/** b139b: the BLAST picked for the current skill, nullptr = DEFAULT ("" = NONE) */
	ZOOGIROLL_API const TCHAR* ChosenBlast();''')
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		float SkillBlastSc() { return (GSkillPow >= 0.99f ? 1.f : 0.8f) * ZoogiSkillLab::FxSize(); }''',
'''		float SkillBlastSc() { return (GSkillPow >= 0.99f ? 1.f : 0.8f) * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize(); } // b139b: x BLAST SIZE
		/** b139b: where a skill hits, the lab's chosen BLAST goes off (only when one is picked - DEFAULT adds nothing) */
		void LabImpact(UWorld* W, const FVector& At, float Base = 0.35f)
		{
			const TCHAR* Bx = ZoogiSkillLab::ChosenBlast();
			if (!W || !Bx || !*Bx) return;
			AZoogiFX::Stylized(W, Bx, At, Base * ZoogiSkillLab::FxSize() * ZoogiSkillLab::BlastSize());
		}
		/** b139b: a skill's own fixed blast honours the lab pick + BLAST SIZE */
		void SkillStyl(UWorld* W, const TCHAR* Def, const FVector& At, float Sc0) { const TCHAR* Bx = ZoogiSkillLab::BlastFx(Def); if (W && Bx && *Bx) AZoogiFX::Stylized(W, Bx, At, Sc0 * ZoogiSkillLab::BlastSize()); }''')
# fixed Mid_Explosion uses -> SkillStyl
b = open(R + S, 'rb').read().decode('utf-8')
reps = [
 ('AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), B0, 0.3f);', 'SkillStyl(W, TEXT("Mid_Explosion"), B0, 0.3f); // b139b'),
 ('AZoogiFX::Stylized(Wg, TEXT("Mid_Explosion"), Ground, 0.55f * FxA);', '{ FTuneScope Tg(1.f, FName(TEXT("golden_apple"))); SkillStyl(Wg, TEXT("Mid_Explosion"), Ground, 0.55f * FxA); } // b139b'),
 ('AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), Ground, 0.42f);', 'SkillStyl(W, TEXT("Mid_Explosion"), Ground, 0.42f); // b139b'),
 ('AZoogiFX::Stylized(G->GetWorld(), TEXT("Mid_Explosion"), Gp, 0.4f);', '{ FTuneScope Tg(1.f, FName(TEXT("golden_apple"))); SkillStyl(G->GetWorld(), TEXT("Mid_Explosion"), Gp, 0.4f); } // b139b'),
]
for o, n in reps:
    assert b.count(o) == 1, o; b = b.replace(o, n)
open(R + S, 'wb').write(b.encode('utf-8'))
# Blast() helper (sword slash, apple mines, fire meteors, old meteor rain): add the lab blast at the centre
ed1(S, '''		int32 Blast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up, const AZoogiBall* Spare, int32 Credit)
		{
			int32 N = 0;''', '''		int32 Blast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up, const AZoogiBall* Spare, int32 Credit)
		{
			int32 N = 0;
			LabImpact(G ? G->GetWorld() : nullptr, P, 0.3f * FMath::Max(0.6f, R / 300.f)); // b139b''')
print('b139b part1 ok')
