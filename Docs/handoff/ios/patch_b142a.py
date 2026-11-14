# b142a (Evan 11:33 "Put an icon above the golden apple and make the apple its self solid gold"): the apple left on the court is
# opaque metallic gold (was the additive FX glow) and a floating icon (the GOLDEN APPLE card art in a black P5 diamond, bobbing,
# with a pointer) sits above it. -ZoogiOldAppleLook = the b138 glow apple, no icon.
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b.count(o) != 1 and b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''#include "CanvasItem.h"''', '''#include "CanvasItem.h"
#include "ImageUtils.h"''')
ed1(S, '''						Am->SetMaterial(0, ZoogiUtil::FxMID(Ap, FLinearColor(1.f, 0.78f, 0.2f), 4.f));''', '''						// b142 (Evan: "make the apple itself solid gold"): opaque metallic gold with a faint warm glow so it reads on dark floors
						if (FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldAppleLook"))) Am->SetMaterial(0, ZoogiUtil::FxMID(Ap, FLinearColor(1.f, 0.78f, 0.2f), 4.f));
						else { const int32 Nm = FMath::Max(1, Am->GetNumMaterials()); for (int32 m = 0; m < Nm; ++m) Am->SetMaterial(m, ZoogiUtil::SurfaceMID(Ap, FLinearColor(1.f, 0.7f, 0.16f), 0.18f, FLinearColor(1.f, 0.62f, 0.12f), 0.35f, 1.f)); Am->SetCastShadow(true); }''')
ed1(S, '''		if (!Picking() || !G || !C || !PC) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;''', '''		if (G && C && PC && GoldApples.Num() && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldAppleLook")))
		{	// b142 (Evan: "put an icon above the golden apple"): the card art in a black P5 diamond, bobbing over each apple left on the court
			static UTexture2D* AppleIcon = nullptr; static bool bTriedIcon = false;
			if (!bTriedIcon)
			{
				bTriedIcon = true;
				const FString Pth = FPaths::ProjectContentDir() / TEXT("Data/CardArt/Icons/golden_apple.png");
				if (FPaths::FileExists(Pth)) AppleIcon = FImageUtils::ImportFileAsTexture2D(Pth);
				if (AppleIcon) AppleIcon->AddToRoot();
			}
			const float Ua = FMath::Min(C->ClipX, C->ClipY) / 1080.f;
			const float Tn = (float)FPlatformTime::Seconds();
			for (const FGoldApple& Ga : GoldApples)
			{
				if (!Ga.A.IsValid()) continue;
				FVector2D Sp, Sa;
				if (!PC->ProjectWorldLocationToScreen(Ga.P + FVector(0.f, 0.f, 150.f), Sp, true) || !PC->ProjectWorldLocationToScreen(Ga.P + FVector(0.f, 0.f, 40.f), Sa, true)) continue;
				Sp.Y = FMath::Min(Sp.Y, Sa.Y - 70.f * Ua) + 6.f * Ua * FMath::Sin(Tn * 3.2f);
				const float Sz = 74.f * Ua, Pulse = 0.85f + 0.15f * FMath::Sin(Tn * 5.f);
				ZoogiFunCanvasQuad(C, Sp + FVector2D(5.f, 5.f) * Ua, Sz * 1.25f, Sz * 1.25f, 45.f, 0.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
				ZoogiFunCanvasQuad(C, Sp, Sz * 1.3f, Sz * 1.3f, 45.f, 0.f, FLinearColor(1.f, 0.78f, 0.2f, Pulse));
				ZoogiFunCanvasQuad(C, Sp, Sz * 1.12f, Sz * 1.12f, 45.f, 0.f, FLinearColor(0.03f, 0.02f, 0.04f, 0.95f));
				if (AppleIcon)
				{
					FCanvasTileItem Ti(Sp - FVector2D(Sz, Sz * 0.833f) * 0.5f, AppleIcon->GetResource(), FVector2D(Sz, Sz * 0.833f), FLinearColor::White);
					Ti.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ti);
				}
				else ZoogiFunCanvasText(C, TEXT("G"), Sp.X, Sp.Y - 26.f * Ua, 48.f * Ua, FLinearColor(1.f, 0.8f, 0.25f), 1, 1);
				// pointer down to the apple
				FCanvasTriangleItem Tri(FVector2D(Sp.X - 12.f * Ua, Sp.Y + Sz * 0.86f), FVector2D(Sp.X + 12.f * Ua, Sp.Y + Sz * 0.86f), FVector2D(Sp.X, Sp.Y + Sz * 0.86f + 16.f * Ua), GWhiteTexture);
				Tri.SetColor(FLinearColor(1.f, 0.78f, 0.2f, Pulse)); C->DrawItem(Tri);
			}
		}
		if (!Picking() || !G || !C || !PC) return;
		const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;''')
print('b142a ok')
