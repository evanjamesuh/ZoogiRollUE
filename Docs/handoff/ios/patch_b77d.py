# b77d (Claude, Evan 00:57: "forget the wolf button - make the SUPERPOWER button its next activation, then any ultimate if the meter is full"):
# the WOLF button is off (-ZoogiWolfButton brings it back); a banked Wolf Pack = the next SUPERPOWER press while rolling, with a wolf badge on it
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAwakenWidget.cpp', [
 ('		const bool bWolfSlot = !bOldModes && ZoogiSkills::WolfSlot(G);',
  '		static const bool bWolfBtn = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfButton")); // b77d: the wolf rides on SUPERPOWER instead\n		const bool bWolfSlot = !bOldModes && bWolfBtn && ZoogiSkills::WolfSlot(G);'),
 ("+ (ZoogiSkills::WolfSlot(G) ? TEXT(\"|W\") : TEXT(\"\"));", "+ ((FParse::Param(FCommandLine::Get(), TEXT(\"ZoogiWolfButton\")) && ZoogiSkills::WolfSlot(G)) ? TEXT(\"|W\") : TEXT(\"\"));"),
])
ed(P + 'ZoogiClassicPad.cpp', [
 ('static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes")); AZoogiGameMode* G = PadGM(); return (!bOld && ZoogiSkills::WolfSlot(G)',
  'static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldModes")) || !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfButton")); AZoogiGameMode* G = PadGM(); return (!bOld && ZoogiSkills::WolfSlot(G)'),
 ('''			return G && (bPower ? G->ClassicSuperpowerReady() : ReactionNeeded());''',
  '''			return G && (bPower ? (G->ClassicSuperpowerReady() || ZoogiSkills::WolfReady(G)) : ReactionNeeded());'''),
 ('''				else if (bPower && WP && WP->HumanPackOut()) WP->OnPowerPress(); // Nov 1: tap = recall, hold = charge (on release)''',
  '''				else if (bPower && ZoogiSkills::WolfReady(G)) ZoogiSkills::WolfGo(G); // b77d (Evan): a banked Wolf Pack is SUPERPOWER's next press (while rolling)
				else if (bPower && WolfBanked(G) && G->Phase == EZoogiPhase::Aiming) { if (const AZoogiBall* Bw = G->GetActiveBall()) G->PublicPopup(TEXT("LAUNCH, THEN TAP SUPERPOWER FOR THE PACK"), Bw->BallLocation() + FVector(0, 0, 190.f), FLinearColor(0.8f, 0.55f, 1.f)); }
				else if (bPower && WP && WP->HumanPackOut()) WP->OnPowerPress(); // Nov 1: tap = recall, hold = charge (on release)'''),
 ('''			// Wolf Pack charges
			if (bPower)''',
  '''			// b77d (Evan: "bring back the wolf badges - how many wolves I have left"): banked Wolf Pack summons, purple badge top-left
			if (bPower)
				if (AZoogiGameMode* G = PadGM())
					if (const AZoogiBall* Bw = G->GetActiveBall())
						if (const int32 NW = ZoogiSkills::WolfCount(G, Bw->PlayerIndex))
						{
							const FVector2D BC = C + FVector2D(-R * 0.86f, -R * 0.76f);
							const FLinearColor Wc = FLinearColor::FromSRGBColor(FColor(160, 90, 255));
							Disc(BC, R * 0.36f, Wc, FLinearColor::White, R * 0.06f, Layer + 5);
							if (UTexture2D* Wt = WolfIcon())
							{
								FSlateBrush WB; WB.SetResourceObject(Wt); WB.ImageSize = FVector2D(300.f, 250.f);
								const float Ih = R * 0.4f, Iw = Ih * 1.2f;
								FSlateDrawElement::MakeBox(Out, Layer + 6, Geo.ToPaintGeometry(FVector2D(Iw, Ih), FSlateLayoutTransform(1.f, BC - FVector2D(Iw, Ih) * 0.5f - FVector2D(0.f, R * 0.06f))), &WB, ESlateDrawEffect::None, FLinearColor::White);
							}
							Label(FString::Printf(TEXT("x%d"), NW), FMath::Max(7.f, R0 * 0.24f), BC + FVector2D(R * 0.3f, R * 0.3f), FLinearColor::White, Layer + 7);
						}
			// Wolf Pack charges
			if (bPower)'''),
 ('''			Label(bPower ? TEXT("SUPERPOWER") :''', '''			Label(bPower ? ((PadGM() && ZoogiSkills::WolfReady(PadGM())) ? TEXT("WOLF PACK!") : TEXT("SUPERPOWER")) :'''),
 ('''	// one round button: plate + icon + label, fires on touch / click down anywhere inside its box''',
  '''	UTexture2D* WolfIcon();
	bool WolfBanked(const AZoogiGameMode* G) { const AZoogiBall* B = G ? G->GetActiveBall() : nullptr; return B && !B->bCPU && ZoogiSkills::WolfArmed(G, B->PlayerIndex); }
	// one round button: plate + icon + label, fires on touch / click down anywhere inside its box'''),
])
print('b77d ok')
