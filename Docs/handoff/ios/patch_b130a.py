# b130a (Evan 03:25): grenade weapon art (Content only) + the orb generator's PRISM GRENADE pickup (round 2+), ITEM -> grenade pick with
# 2x the arc range, lobbed grenade -> Energy Shockwave where it lands (ZoogiGrenadeItem.h/.cpp). CPUs grab and throw them too.
import os, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
IOS = os.path.expanduser('~/mnt/dev/ios/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
shutil.copy(IOS + 'ZoogiGrenadeItem.h', R + 'Public/ZoogiGrenadeItem.h')
shutil.copy(IOS + 'ZoogiGrenadeItem.cpp', R + 'Private/ZoogiGrenadeItem.cpp')
SH = 'Public/ZoogiSkills.h'; S = 'Private/ZoogiSkills.cpp'
ed1(SH, '''	ZOOGIROLL_API void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id);''', '''	ZOOGIROLL_API void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id);
	/** b130: ITEM -> PRISM GRENADE - a target circle at twice the arc range; FIRE lobs it (ZoogiGrenade::Lob) */
	ZOOGIROLL_API bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B);
	/** b130: a thrown grenade landed - the Energy Shockwave (pull, then blast) bursts at At */
	ZOOGIROLL_API void GrenadeBoom(AZoogiGameMode* G, AZoogiBall* B, const FVector& At);''')
ed1(S, '#include "ZoogiCPUSmart.h"\n', '#include "ZoogiCPUSmart.h"\n#include "ZoogiGrenadeItem.h"\n')
ed1(S, '''			bool bComboCast = false; // b122c''', '''			bool bGrenade = false; // b130: the PRISM GRENADE throw pick (no card, no SP)
			bool bComboCast = false; // b122c''')
ed1(S, '''const bool bWasCombo = S.bComboCast; ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); Cancel(TEXT("CANCEL"));''',
       '''const bool bWasCombo = S.bComboCast || S.bGrenade; ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.7f); Cancel(TEXT("CANCEL"));''')
ed1(S, '''		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bComboCast)''', '''		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bGrenade)
		{	// b130: throw the grenade at the circle
			AZoogiBall* Cb = S.Caster.Get(); const FVector At = S.Ctr;
			S.bConfirmed = true; Reset();
			if (Cb && ZoogiGrenade::Take(G, Cb->PlayerIndex)) { ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.1f); ZoogiGrenade::Lob(G, Cb, At); }
			return;
		}
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bComboCast)''')
ed1(S, '''		FString Title = S.Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper();''', '''		FString Title = S.bGrenade ? FString(TEXT("PRISM GRENADE")) : S.Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(); // b130''')
ed1(S, '''		FString Hint = FString(D ? D->Hint : TEXT(""));''', '''		FString Hint = S.bGrenade ? FString(TEXT("DRAG WHERE IT LANDS - 2X THE ARC RANGE")) : FString(D ? D->Hint : TEXT(""));''')
ed1(S, '''		case EAim::Area:
			Ring(C, PC, Floor(S.Ctr), S.R, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f), 48, false);''', '''		case EAim::Area:
			if (S.bGrenade)
			{	// b130: the throw arc from the Zoogi to the circle
				const FVector A0 = S.Origin + FVector(0, 0, 60.f), A1 = Floor(S.Ctr);
				const float Hh = FMath::Max(300.f, FVector::Dist2D(A0, A1) * 0.35f);
				FVector2D Prev; bool bPrev = false;
				for (int32 k = 0; k <= 24; ++k)
				{
					const float Kk = k / 24.f; FVector P = FMath::Lerp(A0, A1, Kk); P.Z += 4.f * Hh * Kk * (1.f - Kk);
					FVector2D Sp;
					if (PC->ProjectWorldLocationToScreen(P, Sp, true)) { if (bPrev && (k % 2 == 1)) { FCanvasLineItem Li(Prev, Sp); Li.LineThickness = 6.f * U; Li.SetColor(FLinearColor(0.85f, 0.6f, 1.f, 0.95f)); C->DrawItem(Li); } Prev = Sp; bPrev = true; }
					else bPrev = false;
				}
			}
			Ring(C, PC, Floor(S.Ctr), S.R, 10.f * U, FLinearColor(0.f, 0.f, 0.f, 0.6f), 48, false);''')
ed1(S, '''	void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id)
	{''', '''	bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B)
	{
		const FName Sw(TEXT("energy_shockwave"));
		const FDef* Dg = Find(Sw);
		if (!G || !B || !Dg || Picking() || ZoogiGrenade::Count(G, B->PlayerIndex) <= 0) return false;
		Reset();
		S.Slot = -1; S.Id = Sw; S.Aim = EAim::Area; S.R = Dg->R; S.Count = 1; S.Caster = B; S.bGrenade = true;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
		S.Range = 2.f * FMath::Max(400.f, G->ArcRange()); // b130 (Evan): "the arc line is 2x farther than normal"
		S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
		FVector AimAt = S.Origin + FRotator(0.f, B->AimYaw, 0.f).Vector() * S.Range * 0.5f;
		float BestD = 1e9f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < BestD) { BestD = Dd; AimAt = O->BallLocation(); } }
		S.Ctr = ClampRange(AimAt, 0.f); S.A = S.B = S.Ctr;
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("GRENADE pick open P%d (range %.0f)"), B->PlayerIndex + 1, S.Range);
		return true;
	}
	void GrenadeBoom(AZoogiGameMode* G, AZoogiBall* B, const FVector& At)
	{
		const FName Sw(TEXT("energy_shockwave"));
		const FDef* Dg = Find(Sw);
		if (!G || !B || !Dg) return;
		const FPick Saved = S; // a pick the player opened meanwhile survives the blast
		Reset();
		S.Slot = -1; S.Id = Sw; S.Aim = EAim::Area; S.R = Dg->R; S.Count = 1; S.Caster = B;
		S.Origin = B->BallLocation(); S.FloorZ = At.Z; S.Ctr = At; S.A = S.B = At;
		S.bOn = true; S.bConfirmed = true;
		Execute(G, B, Sw);
		S = Saved;
	}
	void ComboPutBack(AZoogiGameMode* G, int32 Player, FName Id)
	{''')
# ITEM menu
P = 'Private/ZoogiP5Command.cpp'
ed1(P, '#include "ZoogiCPUSmart.h"\n', '#include "ZoogiCPUSmart.h"\n#include "ZoogiGrenadeItem.h"\n')
ed1(P, '''		if (W.Mode == 2) { FSkillRow R; R.Name = TEXT("NO ITEMS YET"); R.bOn = false; W.Skills.Add(R); }''',
'''		if (W.Mode == 2)
		{	// b130: PRISM GRENADE (from the orb generator)
			const int32 Ng = ZoogiGrenade::Count(G, B->PlayerIndex);
			if (Ng > 0) { FSkillRow R; R.Name = FString::Printf(TEXT("PRISM GRENADE  x%d"), Ng); R.Rule = TEXT("Throw it up to 2x the arc range - a shockwave pulls everything in, then blasts it away."); R.Slot = -100; R.bOn = G->Phase == EZoogiPhase::Aiming; W.Skills.Add(R); }
			else { FSkillRow R; R.Name = TEXT("NO ITEMS YET"); R.Rule = TEXT("From round 2 the orb generator drops a PRISM GRENADE - roll into it first to take it."); R.bOn = false; W.Skills.Add(R); }
		}''')
ed1(P, '''	static void DoSkill(int32 R)
	{
		AZoogiGameMode* G = GS.G.Get();
		if (!G || !GS.W.IsValid()) return;
		SP5Cmd& W = *GS.W;''', '''	static void DoSkill(int32 R)
	{
		AZoogiGameMode* G = GS.G.Get();
		if (!G || !GS.W.IsValid()) return;
		SP5Cmd& W = *GS.W;
		if (W.Mode == 2 && R >= 0)
		{	// b130: ITEM list - the grenade opens its throw pick
			if (W.Skills.IsValidIndex(R) && W.Skills[R].Slot == -100 && W.Skills[R].bOn)
				if (AZoogiBall* Hb = Human(G)) if (ZoogiSkills::BeginGrenade(G, Hb)) { Dismiss(G); return; }
			ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f);
			return;
		}''')
# the generator drops one each round from round 2
ed1('Private/ZoogiGameMode.cpp', '''	if (bOff || bRoyaleMode || bComboMode || !Arena || RoundNumber <= 1) return;''', '''	if (bOff || bRoyaleMode || bComboMode || !Arena || RoundNumber <= 1) return;
	ZoogiGrenade::OnRound(this); // b130 (Evan): the orb generator spits a PRISM GRENADE after round one''')
ed1('Private/ZoogiGameMode.cpp', '#include "ZoogiCPUSmart.h"\n', '#include "ZoogiCPUSmart.h"\n#include "ZoogiGrenadeItem.h"\n')
print('b130a ok')
