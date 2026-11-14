# b90b (Evan 15:36: "when wood shield is activated that ends the turn and the effect stays"): casting WOOD SHIELD ends the caster's
# turn (no shot); the shield stays up (heavy, glowing) through every rival roll until the caster's next turn. -ZoogiOldWoodShield = b71.
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiGameMode.h', [
 ('	void PublicPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }\n',
  '	void PublicPopup(const FString& Text, const FVector& World, const FLinearColor& Color) { AddPopup(Text, World, Color); }\n	/** b90b: a skill that ends the turn without a shot (WOOD SHIELD) */\n	void SkillEndsTurn() { SetStage(EZoogiStage::End); }\n'),
])
ed(P + 'ZoogiSkills.cpp', [
 ('		struct FShield { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; bool bHeavy = false; float HitAt = -1.f; };',
  '		struct FShield { TWeakObjectPtr<AZoogiBall> B; float SavedMass = 0.f; bool bHeavy = false; float HitAt = -1.f; int32 Turn = -1; float NextFx = 0.f; };\n		bool WoodStays() { static const bool b = !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldWoodShield")); return b; }'),
 ('''			FShield Sh; Sh.B = Caster; Sh.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Shields.Add(Sh);
		}''',
  '''			FShield Sh; Sh.B = Caster; Sh.SavedMass = Caster->Sphere ? Caster->Sphere->GetMass() : 0.f; Sh.Turn = G->TurnCounter; Sh.NextFx = Now(G) + 1.3f; Shields.Add(Sh);
			if (WoodStays())
			{	// b90b (Evan): the cast ends the turn; the shield stays until this Zoogi's next turn
				const int32 Tc = G->TurnCounter;
				if (UZoogiFunSys* Fs = UZoogiFunSys::Get(G)) Fs->PublicCallout(TEXT("WOOD SHIELD!"), TEXT("TURN OVER - THE SHIELD HOLDS UNTIL YOUR NEXT TURN"), FLinearColor(0.55f, 0.85f, 0.35f), 1.6f, 1.f);
				Later(G, 1.2f, [WG, Tc]() { AZoogiGameMode* Gm = WG.Get(); if (Gm && Gm->TurnCounter == Tc && Gm->Phase == EZoogiPhase::Aiming) { UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield ends P%d's turn"), Gm->ActivePlayer + 1); Gm->SkillEndsTurn(); } });
			}
		}'''),
 ('''			if (Sh.HitAt >= 0.f && T - Sh.HitAt > 0.5f) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); Shields.RemoveAt(i); }''',
  '''			if (WoodStays())
			{	// b90b: it stays up - glow refreshed, a hit only flashes it - until the caster's own next turn
				if (T >= Sh.NextFx) { Sh.NextFx = T + 1.3f; if (AZoogiFabFX* F = AZoogiFabFX::Play(G->GetWorld(), FX(TEXT("NS_Wood_shield")), B->BallLocation() - FVector(0, 0, ZoogiConst::ZoogiRadius), 0.3f, 1.6f, B, FVector(0, 0, -ZoogiConst::ZoogiRadius))) F->StopAfter = 1.4f; }
				if (Sh.HitAt >= 0.f && T - Sh.HitAt > 0.6f) Sh.HitAt = -1.f;
				if (G->ActivePlayer == B->PlayerIndex && G->TurnCounter > Sh.Turn) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); UE_LOG(LogZoogi, Log, TEXT("SKILL wood shield on P%d drops (its turn)"), B->PlayerIndex + 1); Shields.RemoveAt(i); }
				continue;
			}
			if (Sh.HitAt >= 0.f && T - Sh.HitAt > 0.5f) { B->Sphere->SetMassOverrideInKg(NAME_None, Sh.SavedMass, true); Shields.RemoveAt(i); }'''),
])
print('b90b ok')
