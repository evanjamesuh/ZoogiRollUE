# b137a (Evan 08:24 + 09:15): weapons + items in the ITEM list, a saved item collection with counters, one item per turn
# (ZoogiItems.h/.cpp). Profile: ItemCounts / SelWeapons / bItemsSeeded. WEAPON tab: tap selects up to 3 weapons for the ITEM list.
import os, shutil
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
S0 = os.path.expanduser('~/mnt/dev/ios/b137/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
shutil.copy(S0 + 'ZoogiItems.h', R + 'Public/ZoogiItems.h')
shutil.copy(S0 + 'ZoogiItems.cpp', R + 'Private/ZoogiItems.cpp')
# ---- profile
PH = 'Public/ZoogiFrontProfile.h'; PC = 'Private/ZoogiFrontProfile.cpp'
ed1(PH, '''	FName EquippedWeapon;         // b120: gatling / grenade / sword / staff / disc (the robot form's weapon); None = gatling''',
'''	FName EquippedWeapon;         // b120: gatling / grenade / sword / staff / disc (the robot form's weapon); None = gatling
	TMap<FName, int32> ItemCounts; // b137: the item collection (weapons / grenades) - classic ITEM uses spend it, unused pickups add to it
	TArray<FName> SelWeapons;     // b137: the weapons that ride along in the ITEM list (WEAPON tab, up to 3)
	bool bItemsSeeded = false;    // b137: 3 of each weapon given once''')
ed1(PC, '''	J->SetStringField(TEXT("weapon"), EquippedWeapon.IsNone() ? FString() : EquippedWeapon.ToString());''',
'''	J->SetStringField(TEXT("weapon"), EquippedWeapon.IsNone() ? FString() : EquippedWeapon.ToString());
	{ TSharedRef<FJsonObject> It = MakeShared<FJsonObject>(); for (const TPair<FName, int32>& Kv : ItemCounts) It->SetNumberField(Kv.Key.ToString(), Kv.Value); J->SetObjectField(TEXT("items"), It); } // b137
	J->SetArrayField(TEXT("sel_weapons"), ZFNames(SelWeapons));
	J->SetBoolField(TEXT("items_seeded"), bItemsSeeded);''')
ed1(PC, '''	{ FString Wp; J->TryGetStringField(TEXT("weapon"), Wp); N.EquippedWeapon = Wp.IsEmpty() ? NAME_None : FName(*Wp); }''',
'''	{ FString Wp; J->TryGetStringField(TEXT("weapon"), Wp); N.EquippedWeapon = Wp.IsEmpty() ? NAME_None : FName(*Wp); }
	{	// b137: the item collection
		const TSharedPtr<FJsonObject>* It = nullptr;
		if (J->TryGetObjectField(TEXT("items"), It) && It && It->IsValid()) for (const TPair<FString, TSharedPtr<FJsonValue>>& Kv : (*It)->Values) if (Kv.Value.IsValid()) N.ItemCounts.Add(FName(*Kv.Key), FMath::Max(0, (int32)Kv.Value->AsNumber()));
		if (J->TryGetArrayField(TEXT("sel_weapons"), A) && A) N.SelWeapons = ZFReadNames(*A);
		J->TryGetBoolField(TEXT("items_seeded"), N.bItemsSeeded);
	}''')
# ---- skills: the weapon picks
SH = 'Public/ZoogiSkills.h'; S = 'Private/ZoogiSkills.cpp'
ed1(SH, '''	ZOOGIROLL_API bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B);''', '''	ZOOGIROLL_API bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B);
	/** b137: ITEM -> a weapon from the collection (grenade / sword / staff / gatling / disc): its pick, or straight away (sword) */
	ZOOGIROLL_API bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName Weapon);''')
ed1(S, '#include "ZoogiGrenadeItem.h"\n', '#include "ZoogiGrenadeItem.h"\n#include "ZoogiItems.h"\n')
ed1(S, '''			{ TEXT("bolt_balls"), EAim::Self, 0.f, 1, TEXT("") } }; // b108: SKILL LAB is a home button now, not a skill''',
'''			{ TEXT("bolt_balls"), EAim::Self, 0.f, 1, TEXT("") },
			{ TEXT("halo_disc"), EAim::Line, 0.f, 1, TEXT("SWIPE THE DISC'S PATH") } }; // b108: SKILL LAB is a home button now, not a skill; b137: halo_disc = the HALO DISC weapon (not a card)''')
ed1(S, '''	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0) Out.Add(FName(D.Id)); return Out; }''',
'''	TArray<FName> AllSkillIds() { TArray<FName> Out; for (const FDef& D : Defs) if (FCString::Strcmp(D.Id, TEXT("skill_lab")) != 0 && FCString::Strcmp(D.Id, TEXT("halo_disc")) != 0) Out.Add(FName(D.Id)); return Out; }''')
ed1(S, '''			bool bGrenade = false; // b130: the PRISM GRENADE throw pick (no card, no SP)''', '''			bool bGrenade = false; // b130: the PRISM GRENADE throw pick (no card, no SP)
			bool bWeapon = false; FName WeaponId; // b137: a weapon from the item collection (no card, no SP)''')
ed1(S, '''const bool bWasCombo = S.bComboCast || S.bGrenade;''', '''const bool bWasCombo = S.bComboCast || S.bGrenade || S.bWeapon;''')
ed1(S, '''			if (Cb && ZoogiGrenade::Take(G, Cb->PlayerIndex)) { ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.1f); ZoogiGrenade::Lob(G, Cb, At); }
			return;
		}''', '''			if (Cb && ZoogiItems::Take(G, Cb->PlayerIndex, FName(TEXT("grenade")))) { ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.9f, 1.1f); ZoogiGrenade::Lob(G, Cb, At); } // b137: pickups first, then the saved ones
			return;
		}
		if (bPress && S.FireBtn.bIsValid && S.FireBtn.IsInside(Ptr) && S.bWeapon)
		{	// b137: fire the weapon - one from the collection, full strength, no SP
			if (!CanFire()) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); return; }
			AZoogiBall* Cb = S.Caster.Get();
			if (!Cb || !ZoogiItems::Take(G, Cb->PlayerIndex, S.WeaponId)) { ZoogiUtil::PlaySfx(G, TEXT("UiCancel"), 0.6f); Cancel(TEXT("no item")); return; }
			S.bConfirmed = true; GAwakenNext = true;
			UE_LOG(LogZoogi, Log, TEXT("ITEM %s fired (%s)"), *S.WeaponId.ToString(), *S.Id.ToString());
			Execute(G, Cb, S.Id);
			return;
		}''')
ed1(S, '''		FString Title = S.bGrenade ? FString(TEXT("PRISM GRENADE")) : S.Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(); // b130''',
'''		FString Title = S.bGrenade ? FString(TEXT("PRISM GRENADE")) : S.bWeapon ? ZoogiItems::WeaponName(S.WeaponId) : S.Id.ToString().Replace(TEXT("_"), TEXT(" ")).ToUpper(); // b130; b137''')
ed1(S, '''		if (!G || !B || !Dg || Picking() || ZoogiGrenade::Count(G, B->PlayerIndex) <= 0) return false;''',
'''		if (!G || !B || !Dg || Picking() || ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) <= 0 || ZoogiItems::UsedThisTurn(G, B->PlayerIndex)) return false; // b137: saved grenades too''')
ed1(S, '''	bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B)
	{''', '''	bool BeginWeapon(AZoogiGameMode* G, AZoogiBall* B, FName W)
	{
		if (!G || !B || Picking() || ZoogiItems::UsedThisTurn(G, B->PlayerIndex) || ZoogiItems::Count(G, B->PlayerIndex, W) <= 0) return false;
		if (W == FName(TEXT("grenade"))) return BeginGrenade(G, B);
		const FName Sk = W == FName(TEXT("sword")) ? FName(TEXT("sword_slash")) : W == FName(TEXT("staff")) ? FName(TEXT("water_cannon")) : W == FName(TEXT("gatling")) ? FName(TEXT("magic_rays")) : FName(TEXT("halo_disc"));
		const FDef* D = Find(Sk);
		if (!D) return false;
		Reset();
		S.Slot = -1; S.Id = Sk; S.Aim = D->Aim; S.R = D->R; S.Count = D->Count; S.Caster = B; S.bWeapon = true; S.WeaponId = W;
		S.Origin = B->BallLocation(); S.FloorZ = B->BallLocation().Z - ZoogiConst::ZoogiRadius;
		S.Range = FMath::Max(400.f, G->ArcRange());
		if (D->Aim == EAim::Self)
		{	// the sword: straight away
			if (!ZoogiItems::Take(G, B->PlayerIndex, W)) { Reset(); return false; }
			S.bOn = true; S.bConfirmed = true; GAwakenNext = true;
			UE_LOG(LogZoogi, Log, TEXT("ITEM %s used (%s)"), *W.ToString(), *Sk.ToString());
			Execute(G, B, Sk);
			return true;
		}
		S.bOn = true; S.OpenedAt = FPlatformTime::Seconds();
		FVector Aim = S.Origin + FRotator(0.f, B->AimYaw, 0.f).Vector() * S.Range * 0.5f;
		float Best = 1e9f;
		for (AZoogiBall* O : G->Balls) if (O && O != B && !O->bOut && !G->IsAlly(B, O)) { const float Dd = FVector::Dist2D(O->BallLocation(), S.Origin); if (Dd < Best) { Best = Dd; Aim = O->BallLocation(); } }
		S.Ctr = ClampRange(Aim, 0.f); S.A = S.B = S.Ctr;
		ZoogiUtil::PlaySfx(G, TEXT("UiConfirm"), 0.7f, 1.2f);
		UE_LOG(LogZoogi, Log, TEXT("ITEM pick open %s P%d (%s)"), *W.ToString(), B->PlayerIndex + 1, *Sk.ToString());
		return true;
	}
	bool BeginGrenade(AZoogiGameMode* G, AZoogiBall* B)
	{''')
# the HALO DISC effect
ed1(S, '''		else if (Id == FName(TEXT("golden_apple")))
		{''', '''		else if (Id == FName(TEXT("halo_disc")))
		{	// b137: the HALO DISC weapon - cuts along the swiped line and back, knocking everything near the line aside
			const FVector A0 = At(P.A), B0 = At(P.B);
			const FVector2D Ab(B0.X - A0.X, B0.Y - A0.Y);
			const float Ln = FMath::Max(1.f, Ab.Size());
			const FVector2D Dn = Ab / Ln, Nn(-Dn.Y, Dn.X);
			auto Cut = [&](UPrimitiveComponent* Sp, const FVector& Lc, float Pw) -> bool
			{
				const FVector2D Q(Lc.X - A0.X, Lc.Y - A0.Y);
				const float Tq = FVector2D::DotProduct(Q, Dn), Dd = FVector2D::DotProduct(Q, Nn);
				if (Tq < -80.f || Tq > Ln + 80.f || FMath::Abs(Dd) > 170.f) return false;
				const FVector2D Side = Dd >= 0.f ? Nn : -Nn;
				Kick(Sp, FVector(Side.X, Side.Y, 0.f) * Pw + FVector(Dn.X, Dn.Y, 0.f) * 250.f + FVector(0.f, 0.f, 140.f));
				return true;
			};
			int32 Hit = 0;
			for (AZoogiBall* Ob : G->Balls) if (Ob && Ob != Caster && !Ob->bOut && Ob->Sphere && Cut(Ob->Sphere, Ob->BallLocation(), 900.f)) { Ob->LastHitPlayer = Pi; ++Hit; }
			for (AZoogiOrb* Oo : G->Orbs) if (Oo && !Oo->bOut && Oo->Sphere && Cut(Oo->Sphere, Oo->OrbLocation(), 1100.f)) { Oo->LastHitBy = Pi; ++Hit; }
			for (int32 k = 0; k <= 8; ++k) AZoogiFX::Spawn(W, FMath::Lerp(A0, B0, k / 8.f) + FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.85f, 0.35f), 90.f, 0.35f, true, 8.f);
			AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), B0, 0.3f);
			ZoogiUtil::PlaySfx(G, TEXT("Zap"), 0.9f, 1.4f);
			UE_LOG(LogZoogi, Log, TEXT("ITEM halo disc P%d: %.0f cm cut, %d hit"), Pi + 1, Ln, Hit);
		}
		else if (Id == FName(TEXT("golden_apple")))
		{''')
# ---- ITEM list
P5 = 'Private/ZoogiP5Command.cpp'
ed1(P5, '#include "ZoogiGrenadeItem.h"\n', '#include "ZoogiGrenadeItem.h"\n#include "ZoogiItems.h"\n')
ed1(P5, '''			if (W.Skills.IsValidIndex(R) && W.Skills[R].Slot == -100 && W.Skills[R].bOn)
				if (AZoogiBall* Hb = Human(G)) if (ZoogiSkills::BeginGrenade(G, Hb)) { Dismiss(G); return; }''',
'''			if (W.Skills.IsValidIndex(R) && W.Skills[R].Slot <= -100 && W.Skills[R].bOn)
			{	// b137: every item row: -100 - index into ZoogiItems::WeaponIds()
				const int32 Wi = -100 - W.Skills[R].Slot;
				if (AZoogiBall* Hb = Human(G)) if (ZoogiItems::WeaponIds().IsValidIndex(Wi) && ZoogiSkills::BeginWeapon(G, Hb, ZoogiItems::WeaponIds()[Wi]))
				{
					if (ZoogiSkills::Picking()) Dismiss(G); else ToSkillButton();
					return;
				}
			}''')
ed1(P5, '''			const int32 Ng = ZoogiGrenade::Count(G, B->PlayerIndex);
			if (Ng > 0) { FSkillRow R; R.Name = FString::Printf(TEXT("PRISM GRENADE  x%d"), Ng); R.Rule = TEXT("Throw it up to 2x the arc range - a shockwave pulls everything in, then blasts it away."); R.Slot = -100; R.bOn = G->Phase == EZoogiPhase::Aiming; W.Skills.Add(R); }
			else { FSkillRow R; R.Name = TEXT("NO ITEMS YET"); R.Rule = TEXT("From round 2 the orb generator drops a PRISM GRENADE - roll into it first to take it."); R.bOn = false; W.Skills.Add(R); }''',
'''			// b137 (Evan): the selected weapons from the collection (+ grenades picked up), one item per turn
			TArray<FName> Rows = ZoogiItems::Selected(G);
			if (ZoogiItems::Count(G, B->PlayerIndex, FName(TEXT("grenade"))) > 0) Rows.AddUnique(FName(TEXT("grenade")));
			const bool bUsed = ZoogiItems::UsedThisTurn(G, B->PlayerIndex);
			for (const FName& Wn : Rows)
			{
				const int32 Ng = ZoogiItems::Count(G, B->PlayerIndex, Wn);
				FSkillRow R; R.Name = FString::Printf(TEXT("%s  x%d"), *ZoogiItems::WeaponName(Wn), Ng); R.Rule = bUsed ? FString(TEXT("One item a turn - used this turn.")) : ZoogiItems::WeaponRule(Wn);
				R.Slot = -100 - ZoogiItems::WeaponIds().IndexOfByKey(Wn); R.bOn = Ng > 0 && !bUsed && G->Phase == EZoogiPhase::Aiming;
				W.Skills.Add(R);
			}
			if (Rows.Num() == 0) { FSkillRow R; R.Name = TEXT("NO ITEMS YET"); R.Rule = TEXT("Pick weapons in INVENTORY - WEAPON. From round 2 the orb generator drops a PRISM GRENADE."); R.bOn = false; W.Skills.Add(R); }''')
# ---- WEAPON tab: select up to 3 for the ITEM list, counts shown
F = 'Private/ZoogiFront.cpp'
ed1(F, '''				FItem I; I.Kind = 2; I.WeaponKey = W[i].K; I.Name = W[i].N; I.Desc = W[i].D; I.Rarity = W[i].R; I.Sub = FString::Printf(TEXT("%s WEAPON"), ZRarityName(W[i].R));
				I.Icon = FString(TEXT("Data/MenuArt/Weapons/w_")) + W[i].K; I.bOwned = true; I.bEquipped = EqW == FName(W[i].K);''',
'''				FItem I; I.Kind = 2; I.WeaponKey = W[i].K; I.Name = W[i].N; I.Desc = W[i].D; I.Rarity = W[i].R;
				const int32 Have = P ? P->Data().ItemCounts.FindRef(FName(W[i].K)) : 0;
				I.Sub = FString::Printf(TEXT("%s WEAPON  x%d"), ZRarityName(W[i].R), P && P->Data().bItemsSeeded ? Have : 3); // b137: the collection count
				I.Icon = FString(TEXT("Data/MenuArt/Weapons/w_")) + W[i].K; I.bOwned = true;
				I.bEquipped = P && P->Data().SelWeapons.Num() ? P->Data().SelWeapons.Contains(FName(W[i].K)) : EqW == FName(W[i].K); // b137: selected for the ITEM list''')
ed1(F, '''			if (I.bEquipped) { Say(TEXT("ALREADY EQUIPPED")); return; }
			P->Mutable().EquippedWeapon = FName(*I.WeaponKey);
			P->Save();
			Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s EQUIPPED!"), *I.Name)); Pop = EPop::None;''',
'''			// b137 (Evan: "selected weapons appear in ITEM as a list"): tap selects / unselects, up to 3; the last one picked is the robot's weapon
			TArray<FName>& Sel = P->Mutable().SelWeapons;
			if (Sel.Num() == 0 && !P->Data().EquippedWeapon.IsNone()) Sel.Add(P->Data().EquippedWeapon);
			const FName Wk(*I.WeaponKey);
			if (Sel.Contains(Wk)) { Sel.Remove(Wk); P->Save(); Sfx(TEXT("UiCancel")); Say(FString::Printf(TEXT("%s REMOVED FROM ITEMS"), *I.Name)); Pop = EPop::None; return; }
			if (Sel.Num() >= 3) { Say(TEXT("3 WEAPONS MAX - REMOVE ONE FIRST")); return; }
			Sel.Add(Wk);
			P->Mutable().EquippedWeapon = Wk;
			P->Save();
			Sfx(TEXT("UiConfirm")); Say(FString::Printf(TEXT("%s ADDED TO ITEMS (%d/3)"), *I.Name, Sel.Num())); Pop = EPop::None;''')
print('b137a ok')
