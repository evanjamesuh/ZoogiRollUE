# b78 (Claude): comments (banter / P5 bubble / target tag) sit below the profile avatars; small wolf heads under the profile crystals
# = the banked Wolf Pack count (same number as the SUPERPOWER badge)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('UTexture2D* UZoogiArenaHUDWidget::Tex(const FString& Key)\n{',
  '''// b78: small white wolf head (ears up, snout down, eye holes) - tinted at draw time
static ZoogiUIFx::FPixels ZCWolfHeadPixels()
{
	ZoogiUIFx::FPixels P; P.W = P.H = 96; P.Px.SetNumZeroed(P.W * P.H);
	const TArray<FVector2D> Poly = { {12, 6}, {34, 30}, {62, 30}, {84, 6}, {84, 46}, {76, 64}, {60, 76}, {48, 92}, {36, 76}, {20, 64}, {12, 46} };
	auto Inside = [&Poly](float X, float Y)
	{
		bool bIn = false;
		for (int32 i = 0, j = Poly.Num() - 1; i < Poly.Num(); j = i++)
			if (((Poly[i].Y > Y) != (Poly[j].Y > Y)) && (X < (Poly[j].X - Poly[i].X) * (Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X)) bIn = !bIn;
		return bIn;
	};
	auto Eye = [](float X, float Y) { return FVector2D::Distance(FVector2D(X, Y), FVector2D(34, 48)) < 6.f || FVector2D::Distance(FVector2D(X, Y), FVector2D(62, 48)) < 6.f; };
	for (int32 y = 0; y < P.H; ++y)
		for (int32 x = 0; x < P.W; ++x)
		{
			int32 N = 0;
			for (int32 s = 0; s < 16; ++s) { const float Sx = x + ((s % 4) + 0.5f) / 4.f, Sy = y + ((s / 4) + 0.5f) / 4.f; if (Inside(Sx, Sy) && !Eye(Sx, Sy)) ++N; }
			if (N) P.Px[y * P.W + x] = FColor(255, 255, 255, (uint8)(N * 255 / 16));
		}
	return P;
}

UTexture2D* UZoogiArenaHUDWidget::Tex(const FString& Key)
{'''),
 ('	if (Key == TEXT("stone")) T = ZoogiUIFx::ToTexture(ZCStonePixels());',
  '	if (Key == TEXT("wolfhead")) T = ZoogiUIFx::ToTexture(ZCWolfHeadPixels());\n	else if (Key == TEXT("stone")) T = ZoogiUIFx::ToTexture(ZCStonePixels());'),
 ('''		U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 90.f, PS, PS));
	U.SP = Text(''',
  '''		U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("8D9099")), 2.5f, PS * 0.5f), PX + k * (PS + Gap), RY + RS + 90.f, PS, PS));
	{	// b78 (Evan: "the small wolf heads under the crystals, matching the number by the Wolf Pack button")
		const float WS = 22.f, WG = 6.f, WW = 4 * WS + 3 * WG;
		for (int32 k = 0; k < 4; ++k) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), Cx - WW * 0.5f + k * (WS + WG), RY + RS + 90.f + PS + 6.f, WS, WS, 0.f, false, ZCHex(TEXT("B57CFF")))) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
	}
	U.SP = Text('''),
 ('''				U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("555B6E")), 1.5f, PS * 0.5f), PX + k * (PS + PGap), RY + RS + 66.f, PS, PS));
			U.SP = Text(''',
  '''				U.Pips.Add(At(Box(FLinearColor::White, ZCHex(TEXT("555B6E")), 1.5f, PS * 0.5f), PX + k * (PS + PGap), RY + RS + 66.f, PS, PS));
			{	// b78: wolf heads under the crystals (banked Wolf Pack summons)
				const float WS = 16.f, WG = 5.f, WW = 4 * WS + 3 * WG;
				for (int32 k = 0; k < 4; ++k) if (UImage* Wi = Img(Tex(TEXT("wolfhead")), Cx - WW * 0.5f + k * (WS + WG), RY + RS + 66.f + PS + 4.f, WS, WS, 0.f, false, ZCHex(TEXT("B57CFF")))) { Wi->SetVisibility(ESlateVisibility::Collapsed); U.Wolves.Add(Wi); }
			}
			U.SP = Text('''),
], inc='#include "ZoogiSkills.h"', after='#include "ZoogiGameMode.h"')

p=os.path.join(ROOT,P+'ZoogiArenaHUDWidget.cpp'); raw=open(p,'rb').read(); bom=raw[:3]==b'\xef\xbb\xbf'; s=raw.decode('utf-8-sig')
pr=eval('['+" ('''\t\tconst int32 Charge = G->AwakenCharge[i];\n\t\tfor (int32 k = 0; k < U.Pips.Num(); ++k)''',\n  '''\t\t{\t// b78: one wolf head per banked Wolf Pack summon (the SUPERPOWER badge number)\n\t\t\tconst int32 NW = ZoogiSkills::WolfCount(G, i);\n\t\t\tfor (int32 k = 0; k < U.Wolves.Num(); ++k)\n\t\t\t\tif (UWidget* Wi = U.Wolves[k].Get()) { const ESlateVisibility V = k < NW ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed; if (Wi->GetVisibility() != V) Wi->SetVisibility(V); }\n\t\t}\n\t\tconst int32 Charge = G->AwakenCharge[i];\n\t\tfor (int32 k = 0; k < U.Pips.Num(); ++k)'''),\n"+']')[0]
a=pr[0].replace('\n','\r\n'); b=pr[1].replace('\n','\r\n')
assert s.count(a)==2; s=s.replace(a,b)
open(p,'w',encoding='utf-8-sig' if bom else 'utf-8',newline='').write(s)
print('b78 arena ok')
