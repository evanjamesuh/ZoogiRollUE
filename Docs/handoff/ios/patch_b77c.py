# b77c (Claude): wolf uses stack (each Wolf Pack cast = one more wolf summon) + count badge on the WOLF button; the pack counter heads come back with the summon
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(H + 'ZoogiSkills.h', [
 ('	ZOOGIROLL_API bool WolfArmed(const AZoogiGameMode* G, int32 Player);\n',
  '	ZOOGIROLL_API bool WolfArmed(const AZoogiGameMode* G, int32 Player);\n	/** b77 (Evan: "bring back the wolf badges that tell me how many wolves I have left"): summons banked */\n	ZOOGIROLL_API int32 WolfCount(const AZoogiGameMode* G, int32 Player);\n'),
])
ed(P + 'ZoogiSkills.cpp', [
 ('	static TSet<int32> GWolfArmed;\n', '	static TMap<int32, int32> GWolfArmed; // player -> summons banked\n'),
 ('		GWolfArmed.Add(Player);\n		UE_LOG(LogZoogi, Log, TEXT("WOLF button armed for P%d"), Player + 1);',
  '		++GWolfArmed.FindOrAdd(Player);\n		UE_LOG(LogZoogi, Log, TEXT("WOLF button armed for P%d (%d banked)"), Player + 1, GWolfArmed[Player]);'),
 ('	bool WolfArmed(const AZoogiGameMode* G, int32 Player) { return G && GWolfG.Get() == G && GWolfArmed.Contains(Player); }',
  '	int32 WolfCount(const AZoogiGameMode* G, int32 Player) { const int32* N = (G && GWolfG.Get() == G) ? GWolfArmed.Find(Player) : nullptr; return N ? *N : 0; }\n	bool WolfArmed(const AZoogiGameMode* G, int32 Player) { return WolfCount(G, Player) > 0; }'),
 ('		GWolfArmed.Remove(B->PlayerIndex);\n', '		if (int32* N = GWolfArmed.Find(B->PlayerIndex)) { if (--(*N) <= 0) GWolfArmed.Remove(B->PlayerIndex); }\n'),
])
ed(P + 'ZoogiClassicPad.cpp', [
 ('''			FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, FVector2D(TX, O.Y + (Sz.Y - TS.Y) * 0.5f))), S, F, ESlateDrawEffect::None, FLinearColor::White);
			return Layer + 5;''',
  '''			FSlateDrawElement::MakeText(Out, Layer + 4, Geo.ToPaintGeometry(TS, FSlateLayoutTransform(1.f, FVector2D(TX, O.Y + (Sz.Y - TS.Y) * 0.5f))), S, F, ESlateDrawEffect::None, FLinearColor::White);
			// wolves left badge (gold disc, top-right corner)
			const AZoogiBall* Bw = G ? G->GetActiveBall() : nullptr;
			const int32 NW = Bw ? ZoogiSkills::WolfCount(G, Bw->PlayerIndex) : 0;
			if (NW > 0)
			{
				const float BR = Sz.Y * 0.26f;
				const FVector2D BC = O + FVector2D(Sz.X - BR * 0.4f, BR * 0.3f);
				const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(255, 213, 74));
				Box(BC - FVector2D(BR, BR), FVector2D(BR * 2.f, BR * 2.f), Dark, Gold, FMath::Max(2.f, BR * 0.2f), BR, Layer + 5);
				const FSlateFontInfo BF = ZoogiModeSel::FontInfo(FMath::Max(8.f, BR * 1.1f), 1.f);
				const FString NS = FString::Printf(TEXT("x%d"), NW);
				const FVector2D NSz = FM->Measure(NS, BF);
				FSlateDrawElement::MakeText(Out, Layer + 6, Geo.ToPaintGeometry(NSz, FSlateLayoutTransform(1.f, BC - NSz * 0.5f)), NS, BF, ESlateDrawEffect::None, Gold);
			}
			return Layer + 7;'''),
])
print('b77c ok')
