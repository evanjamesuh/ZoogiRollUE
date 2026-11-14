# Nov 11 (Claude): B250 = ROLLBACK to the B245 game (Evan: B247 won't open, "treat as if 246 didn't work"). Forward (B249) files saved in ios/b250/fwd.
import io, os, re, sys, shutil
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def rd(r): return io.open(S + r, encoding='utf-8-sig').read()
def wr(r, t): io.open(S + r, 'w', encoding='utf-8', newline='').write(t); print('ok', r)
def one(t, a, b):
    assert t.count(a) == 1, a[:80]; return t.replace(a, b)
t = rd('Private/ZoogiVoyagerLevels.cpp')
i = t.index('\n// ---------------------------------------------------------------- b246: Zoogi touch pad'); j = t.index('\nvoid UZoogiVoyagerLevelsSubsystem::Initialize(')
t = t[:i] + t[j:]
t = one(t, '	ExitWidget.Reset(); PadWidget.Reset(); ZoogiVoyPad::G.Reset(); NextDress = 0.0;\n', '	ExitWidget.Reset();\n')
t = one(t, '		if (ZoogiVoyPad::Wanted()) { PadWidget = SNew(SZoogiVoyPad); Vp->AddViewportWidgetContent(PadWidget.ToSharedRef(), 900); UE_LOG(LogZoogi, Log, TEXT("VOYPAD on")); }\n', '')
t = one(t, '''	ZoogiApplePad::WantFull(VoyWorld.IsValid() && !ZoogiVoyPad::Wanted()); // b246: the Zoogi pad replaces Apple's controller (-ZoogiNoVoyPad = Apple's)
	if (VoyWorld.IsValid()) { PadTick(); DressTick(); }''', '	ZoogiApplePad::WantFull(VoyWorld.IsValid()); // the Apple on-screen controller on Voyager maps (iOS)')
t = re.sub(r'#include "[^"]+" // b24[6789]\n', '', t)
wr('Private/ZoogiVoyagerLevels.cpp', t); print('lines', t.count('\n'))
h = rd('Public/ZoogiVoyagerLevels.h')
h = one(h, '''
	TSharedPtr<class SWidget> PadWidget; // b246
	double NextDress = 0.0; // b246
	void PadTick(); void DressTick(); // b246''', '')
wr('Public/ZoogiVoyagerLevels.h', h)
h = rd('Public/ZoogiVoyager.h')
h = one(h, '''
	/** b246: same dress from a Zoogi name + colours (the Voyager levels' own characters) */
	ZOOGIROLL_API void DressAs(USkeletalMeshComponent* Sk, const FString& ZName, int32 Seed, const FLinearColor& InPri, const FLinearColor& InSec, bool bHollow);''', '')
wr('Public/ZoogiVoyager.h', h)
c = rd('Private/ZoogiVoyager.cpp')
c = one(c, '''		DressAs(Sk, B ? B->Def.Name : FString(), B ? B->PlayerIndex : 0, B ? B->Def.Color : FLinearColor(0.2f, 0.6f, 1.f), B ? B->Def.Secondary : FLinearColor::White, bHollow);
	}
	void DressAs(USkeletalMeshComponent* Sk, const FString& ZName, int32 Seed, const FLinearColor& InPri, const FLinearColor& InSec, bool bHollow)
	{
''', '')
c = one(c, '		const FLinearColor Pri = bHollow ? FLinearColor(0.035f, 0.02f, 0.06f) : InPri;', '		const FLinearColor Pri = bHollow ? FLinearColor(0.035f, 0.02f, 0.06f) : (B ? B->Def.Color : FLinearColor(0.2f, 0.6f, 1.f));')
c = one(c, '		const FLinearColor Sec = bHollow ? FLinearColor(0.55f, 0.12f, 1.f) : InSec;', '		const FLinearColor Sec = bHollow ? FLinearColor(0.55f, 0.12f, 1.f) : (B ? B->Def.Secondary : FLinearColor::White);')
c = one(c, '		if (!bHollow && !ZName.IsEmpty() && Sk->GetBoneIndex(TEXT("head")) != INDEX_NONE)', '		if (!bHollow && B && Sk->GetBoneIndex(TEXT("head")) != INDEX_NONE)')
c = one(c, '			const FString N = ZName.ToLower();', '			const FString N = B->Def.Name.ToLower();')
c = one(c, ': (FMath::Abs(Seed) + 3) % 5;', ': (B->PlayerIndex + 3) % 5;')
c = one(c, 'bHollow ? TEXT("VOID HOLLOW") : (ZName.IsEmpty() ? TEXT("-") : *ZName)', 'bHollow ? TEXT("VOID HOLLOW") : (B ? *B->Def.Name : TEXT("-"))')
c = one(c, '				if (Nm.Contains(TEXT("Flash"))) continue; // b247: keep the Voyager hit flash\n', '')
wr('Private/ZoogiVoyager.cpp', c)
p = rd('Private/ZoogiP5Style.cpp')
p = one(p, '''				FSlateFontInfo NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f), 0.f);
				const FString Nm = Spec.Items[i].Name;
				FVector2D NS = Measure(Nm, NF);
				if (NS.X > BW - 140.f && NS.X > 1.f) { NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f) * (BW - 140.f) / NS.X, 0.f); NS = Measure(Nm, NF); } // b248: long names fit the ribbon''',
'''				const FSlateFontInfo NF = Font(bSel ? 1 : 0, BH * (bSel ? 0.5f : 0.56f), 0.f);
				const FString Nm = Spec.Items[i].Name;
				const FVector2D NS = Measure(Nm, NF);''')
wr('Private/ZoogiP5Style.cpp', p)
f = rd('Private/ZoogiFront.cpp'); a = f.index('			{ TEXT("B249"),'); b = f.index('			{ TEXT("B245"),')
f = f[:a] + '			{ TEXT("B250"), TEXT("BACK TO BUILD 245"), TEXT("Builds 246-249 would not open on iPhone, so this build is the build 245 game again. Voyager touch controls, auto cover and the new Voyager card art return once the cause is fixed.") },\n' + f[b:]
wr('Private/ZoogiFront.cpp', f)
C = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Content/Data/MenuArt/ModeSelect/'); O = os.path.expanduser('~/mnt/dev/ios/b247/oldcards/')
for n in os.listdir(O): shutil.copy(O + n, C + n)
print('ALL OK')
