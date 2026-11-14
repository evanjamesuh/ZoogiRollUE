# b107a: SKILL LAB (see lab/ZoogiSkillLab.h) - copy the module in + the hooks
import os, shutil
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
shutil.copy(os.path.expanduser('~/mnt/dev/ios/lab/ZoogiSkillLab.h'), R + 'Public/ZoogiSkillLab.h')
shutil.copy(os.path.expanduser('~/mnt/dev/ios/lab/ZoogiSkillLab.cpp'), R + 'Private/ZoogiSkillLab.cpp')
P = 'Source/ZoogiRoll/Private/'
ed('Source/ZoogiRoll/Public/ZoogiSkills.h', [('	ZOOGIROLL_API void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id);',
 '	ZOOGIROLL_API void AwakenCast(AZoogiGameMode* G, AZoogiBall* B, FName Id);\n	/** b107 SKILL LAB: cast Id from Caster (auto-aimed, card strength x the lab tuning), whoever\'s turn it is. */\n	ZOOGIROLL_API void LabCast(AZoogiGameMode* G, AZoogiBall* Caster, FName Id);')])
ed(P + 'ZoogiSkills.cpp', [
 ('#include "ZoogiFilm.h"\n', '#include "ZoogiFilm.h"\n#include "ZoogiSkillLab.h"\n'),
 ('			{ TEXT("bolt_balls"), EAim::Self, 0.f, 1, TEXT("") } };', '			{ TEXT("bolt_balls"), EAim::Self, 0.f, 1, TEXT("") },\n			{ TEXT("skill_lab"), EAim::Self, 0.f, 1, TEXT("") } }; // b107: opens the SKILL LAB'),
 ('		const TCHAR* SkillBlastFx() { return GSkillPow >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion"); } // b100 (Evan): smaller billow blasts for skills\n		float SkillBlastSc() { return GSkillPow >= 0.99f ? 1.f : 0.8f; }',
  '		const TCHAR* SkillBlastFx() { return ZoogiSkillLab::BlastFx(GSkillPow >= 0.99f ? TEXT("Big_Explosion_v2") : TEXT("Mid_Explosion")); } // b100 (Evan): smaller billow blasts for skills; b107: the lab\'s pick\n		float SkillBlastSc() { return (GSkillPow >= 0.99f ? 1.f : 0.8f) * ZoogiSkillLab::FxSize(); }'),
 ('		int32 Blast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up, const AZoogiBall* Spare, int32 Credit)\n		{\n			int32 N = 0;',
  '		int32 Blast(AZoogiGameMode* G, const FVector& P, float R, float Speed, float Up, const AZoogiBall* Spare, int32 Credit)\n		{\n			int32 N = 0;\n			R *= ZoogiSkillLab::Radius(); // b107'),
 ('''		GSkillPow = GAwakenNext ? 1.f : CardPow(); GAwakenNext = false; // b100''', '''		if (Id == FName(TEXT("skill_lab"))) { if (!Caster->bCPU) ZoogiSkillLab::OpenLab(G); Reset(); return; } // b107: the SKILL LAB card
		ZoogiSkillLab::SetCurrent(Id);
		GSkillPow = (GAwakenNext ? 1.f : CardPow()) * ZoogiSkillLab::Strength(); GAwakenNext = false; // b100; b107 x the lab tuning'''),
 ('		Mul *= GSkillPow; // b100: a card skill\'s blasts are 0.6\n		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)); // a wolf\'s ShockR',
  '		Mul *= GSkillPow; // b100: a card skill\'s blasts are 0.6\n		const float R = 430.f * FMath::Sqrt(FMath::Max(Mul, 0.1f)) * ZoogiSkillLab::Radius(); // a wolf\'s ShockR; b107 x the lab radius'),
 ('				const float Rw = P.R * 1.25f;\n				const FLinearColor Pu(0.65f, 0.35f, 1.f);', '				const float Rw = P.R * 1.25f * ZoogiSkillLab::Radius(); // b107\n				const FLinearColor Pu(0.65f, 0.35f, 1.f);'),
 ('						Kick(O->Sphere, D.GetSafeNormal2D() * (380.f + 900.f * Dd / Rw));', '						Kick(O->Sphere, D.GetSafeNormal2D() * (380.f + 900.f * Dd / Rw) * ZoogiSkillLab::Pull()); // b107: the lab pull'),
 ('						Kick(Bz->Sphere, D.GetSafeNormal2D() * (260.f + 640.f * Dd / Rw));', '						Kick(Bz->Sphere, D.GetSafeNormal2D() * (260.f + 640.f * Dd / Rw) * ZoogiSkillLab::Pull());'),
 ('	void Tick(AZoogiGameMode* G, float RealDt)\n	{\n		WolfTick(G);', '	void Tick(AZoogiGameMode* G, float RealDt)\n	{\n		WolfTick(G);\n		ZoogiSkillLab::Tick(G, RealDt); // b107'),
 ('''	bool AutoCast(AZoogiGameMode* G, FName Id)
	{''', '''	void LabCast(AZoogiGameMode* G, AZoogiBall* B, FName Id)
	{
		if (!G || !B || !IsSkill(Id)) return;
		const FDef* D = Find(Id);
		Reset();
		S.bOn = true; S.bConfirmed = true; S.Id = Id; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius; S.Range = FMath::Max(1400.f, G->ArcRange());
		TArray<AActor*> Cand; Candidates(G, Cand);
		Cand.RemoveAll([B](AActor* X) { return X == B; });
		Cand.Sort([&](const AActor& X, const AActor& Y) { return FVector::Dist2D(ActorPos(const_cast<AActor*>(&X)), S.Origin) < FVector::Dist2D(ActorPos(const_cast<AActor*>(&Y)), S.Origin); });
		const FVector Fwd = Cand.Num() ? ActorPos(Cand[0]) : S.Origin + FVector(600.f, 0, 0);
		S.Ctr = Fwd; S.A = S.Origin + (Fwd - S.Origin) * 0.6f + FVector(0, 300.f, 0); S.B = S.Origin + (Fwd - S.Origin) * 0.6f - FVector(0, 300.f, 0);
		for (int32 k = 0; k < Cand.Num() && k < D->Count; ++k) S.Targets.Add(Cand[k]);
		for (int32 k = 0; k < D->Count; ++k) S.Spots.Add(Fwd + FVector(FMath::Cos(k * 2.1f), FMath::Sin(k * 2.1f), 0.f) * 220.f);
		Execute(G, B, Id);
	}
	bool AutoCast(AZoogiGameMode* G, FName Id)
	{'''),
])
s=open(R+'Private/ZoogiSkills.cpp',encoding='utf-8-sig').read()
i=s.find('	void CpuCast(AZoogiGameMode* G, FName Id)\n	{\n')
assert i>0
ed(P + 'ZoogiSkills.cpp', [('	void CpuCast(AZoogiGameMode* G, FName Id)\n	{\n', '	void CpuCast(AZoogiGameMode* G, FName Id)\n	{\n		if (Id == FName(TEXT("skill_lab"))) return; // b107: a CPU never opens the lab\n')])
ed(P + 'ZoogiFabFX.cpp', [
 ('	if (Tune) { Scale *= Tune->Size; HeightMul = Tune->Height; RealLife *= Tune->Life; }', '	if (Tune) { Scale *= Tune->Size; HeightMul = Tune->Height; RealLife *= Tune->Life; }\n	const bool bLab = ZoogiSkills::EffectBusy(); // b107 SKILL LAB: the playing skill\'s FX size / colour\n	if (bLab) Scale *= ZoogiSkillLab::FxSize();'),
 ('	if (Tune && (Tune->Tint.R + Tune->Tint.G + Tune->Tint.B) > 0.f) {', '	FLinearColor LabTint; if (bLab && ZoogiSkillLab::Tint(LabTint)) { A->NC->SetVariableLinearColor(FName(TEXT("User.Color")), LabTint); A->NC->SetVariableLinearColor(FName(TEXT("Color")), LabTint); A->NC->SetVariableLinearColor(FName(TEXT("User.Tint")), LabTint); }\n	else if (Tune && (Tune->Tint.R + Tune->Tint.G + Tune->Tint.B) > 0.f) {'),
])
s=open(R+'Private/ZoogiFabFX.cpp',encoding='utf-8-sig').read()
fi=[l for l in s.splitlines() if l.startswith('#include')][0]
ed(P + 'ZoogiFabFX.cpp', [(fi+'\n', fi+'\n#include "ZoogiSkills.h"\n#include "ZoogiSkillLab.h"\n')])
ed(P + 'ZoogiPlayerController.cpp', [
 ('	if (ZoogiSkills::Picking()) { ZoogiSkills::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }',
  '	if (ZoogiSkillLab::Open()) { ZoogiSkillLab::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; } // b107\n	if (ZoogiSkills::Picking()) { ZoogiSkills::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }'),
 ('		const bool bSpecialV = (FunV && FunV->SpecialCamOn())', '		const bool bSpecialV = ZoogiSkillLab::Open() || (FunV && FunV->SpecialCamOn())'),
 ('#include "ZoogiWolfPack.h"\n', '#include "ZoogiWolfPack.h"\n#include "ZoogiSkillLab.h"\n'),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('	ZoogiSkills::Draw(G, C, PC); // b63', '	ZoogiSkillLab::Draw(G, C, PC); // b107\n	ZoogiSkills::Draw(G, C, PC); // b63'),
 ('#include "ZoogiEpic.h"\n', '#include "ZoogiEpic.h"\n#include "ZoogiSkillLab.h"\n'),
])
p = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Content/Data/Cards.csv')
b = open(p,'rb').read()
if b'skill_lab,' not in b:
    b = b.rstrip(b'\r\n') + b'\r\nskill_lab,SKILL LAB,0,Utility,ActingZoogi,LeadActionEnd,0,0,0,true,Neutral,Cast it to open the SKILL LAB - tune every skill (strength / radius / pull / size / colour / blast) and watch it replay from any camera.\r\n'
    open(p,'wb').write(b)
print('b107a ok')
