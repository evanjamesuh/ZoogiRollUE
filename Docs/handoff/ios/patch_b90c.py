# b90c (Evan 15:38: "critical hits damage HP by 10"; wood shield stays until the next turn or HP is zero): a critical hit takes
# 10 HP from the Zoogi it hits (0 HP = KNOCKOUT, same as the duel); HP shows next to SP under each avatar.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiActionFun.h', [
 ('	bool ClashOn() const { return Clash.bOn; }\n',
  '''	bool ClashOn() const { return Clash.bOn; }
	/** b90: duel HP (100 per Zoogi, kept for the match); b90c: critical hits take 10. 0 HP = KNOCKOUT (blasted off, back at 100). */
	float PlayerHP(int32 P) const { return Players.IsValidIndex(P) ? Players[P].HP : 100.f; }
	void HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why);
'''),
 ('	void DuelV2End(AZoogiGameMode* G, bool bFought);\n', '	void DuelV2End(AZoogiGameMode* G, bool bFought);\n	void HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn);\n'),
])
ed(P + 'ZoogiDuelV2.cpp', [
 ('''	for (AZoogiBall* L : KOs)
	{	// 0 HP = KNOCKOUT: blasted off the arena (the ring-out credits the other fighter), back at full HP
		AZoogiBall* Wn = L == A ? D : A;
		Players[L->PlayerIndex].HP = 100.f;''',
  '''	for (AZoogiBall* L : KOs) HpKnockout(G, L, L == A ? D : A);
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 0.9f, 0.6f), 160.f, 0.35f, true, 10.f);
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 over (%s)"), bFought ? TEXT("fought") : TEXT("missed the circle - normal hit"));
}

void UZoogiFunSys::HpDamage(AZoogiGameMode* G, AZoogiBall* Victim, AZoogiBall* By, float Dmg, const TCHAR* Why)
{
	if (!G || !Victim || Victim->bOut || !Players.IsValidIndex(Victim->PlayerIndex) || Dmg <= 0.f) return;
	float& Hp = Players[Victim->PlayerIndex].HP;
	Hp = FMath::Max(0.f, Hp - Dmg);
	G->AddPopup(FString::Printf(TEXT("-%d HP"), FMath::RoundToInt(Dmg)), Victim->BallLocation() + FVector(0, 0, 250.f), FLinearColor(1.f, 0.35f, 0.3f));
	UE_LOG(LogZoogi, Log, TEXT("HP P%d -%.0f (%s) -> %.0f"), Victim->PlayerIndex + 1, Dmg, Why, Hp);
	if (Hp <= 0.f) HpKnockout(G, Victim, By);
}

void UZoogiFunSys::HpKnockout(AZoogiGameMode* G, AZoogiBall* L, AZoogiBall* Wn)
{
	if (!G || !L || !Players.IsValidIndex(L->PlayerIndex)) return;
	{	// 0 HP = KNOCKOUT: blasted off the arena (the ring-out credits the other fighter), back at full HP
		Players[L->PlayerIndex].HP = 100.f;'''),
 ('''		UE_LOG(LogZoogi, Log, TEXT("DUEL2 KNOCKOUT P%d (by P%d)"), L->PlayerIndex + 1, Wn ? Wn->PlayerIndex + 1 : 0);
	}
	AZoogiFX::Spawn(GetWorld(), At, FLinearColor(1.f, 0.9f, 0.6f), 160.f, 0.35f, true, 10.f);
	UE_LOG(LogZoogi, Log, TEXT("DUEL2 over (%s)"), bFought ? TEXT("fought") : TEXT("missed the circle - normal hit"));
}''', '''		UE_LOG(LogZoogi, Log, TEXT("HP KNOCKOUT P%d (by P%d)"), L->PlayerIndex + 1, Wn ? Wn->PlayerIndex + 1 : 0);
	}
}'''),
])
ed(P + 'ZoogiAwaken.cpp', [
 ('	if (bLastHitCrit && OB) ZoogiSkills::GuardHit(this, OB, Shooter->BallLocation(), 1.5f, TEXT("CRITICAL")); // b83: the GUARD barrier cracks',
  '''	if (bLastHitCrit && OB) ZoogiSkills::GuardHit(this, OB, Shooter->BallLocation(), 1.5f, TEXT("CRITICAL")); // b83: the GUARD barrier cracks
	if (bLastHitCrit && OB && !ZoogiSkills::GuardBusy(this, OB->PlayerIndex) && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCritHp")))
		if (UZoogiFunSys* Fh = UZoogiFunSys::Get(this)) Fh->HpDamage(this, OB, Shooter, 10.f, TEXT("critical")); // b90c (Evan): critical hits take 10 HP'''),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('	const FString Str = FString::Printf(TEXT("SP %d / %d"), FMath::Max(0, PS->Energy), Max);',
  '	const UZoogiFunSys* FunHp = UZoogiFunSys::Get(G);\n	const FString Str = FunHp ? FString::Printf(TEXT("SP %d/%d  HP %d"), FMath::Max(0, PS->Energy), Max, FMath::RoundToInt(FunHp->PlayerHP(i))) : FString::Printf(TEXT("SP %d / %d"), FMath::Max(0, PS->Energy), Max); // b90c: HP beside SP'),
])
print('b90c ok')
