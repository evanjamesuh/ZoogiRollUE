# Nov 11 (Claude): B246 - Evan: "how do i control character in voyager mode on mobile and when will they look like zoogi character".
# 1) Zoogi touch pad on the Voyager maps (phones): floating move stick (left), drag-to-look (right), FIRE / AIM / JUMP-COVER / RELOAD /
#    SWAP / GRENADE / MELEE / SPRINT buttons, fed into Voyager's own Enhanced Input actions. -ZoogiNoVoyPad = Apple's controller again,
#    -ZoogiVoyPadTest = show it on PC. -ZoogiPadLook=k look speed, -ZoogiPadInvY flips look up/down.
# 2) The Voyager hero wears your equipped Zoogi (Tripo Baller head, its colours, pads + core); enemies are Void hollows. -ZoogiVoyNoDress = off.
import io, os, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def patch(rel, pairs, incs=()):
    p = S + rel; t = io.open(p, encoding='utf-8-sig').read()
    for a, b in pairs:
        if t.count(a) != 1: print('FAIL', rel, t.count(a), a[:100]); sys.exit(1)
        t = t.replace(a, b)
    for inc in incs:
        if '#include "%s"' % inc not in t:
            k = t.index('#include', t.index('#include') + 1); t = t[:k] + '#include "%s" // b246\n' % inc + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', rel)

# ---- DressAs
patch('Public/ZoogiVoyager.h', [('	ZOOGIROLL_API void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow = false);',
'''	ZOOGIROLL_API void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow = false);
	/** b246: same dress from a Zoogi name + colours (the Voyager levels' own characters) */
	ZOOGIROLL_API void DressAs(USkeletalMeshComponent* Sk, const FString& ZName, int32 Seed, const FLinearColor& InPri, const FLinearColor& InSec, bool bHollow);''')])
patch('Private/ZoogiVoyager.cpp', [
('''	void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDress"));
		if (!Sk || bOff || !On()) return;
		AActor* Ow = Sk->GetOwner(); if (!Ow) return;
		const FLinearColor Pri = bHollow ? FLinearColor(0.035f, 0.02f, 0.06f) : (B ? B->Def.Color : FLinearColor(0.2f, 0.6f, 1.f));
		const FLinearColor Sec = bHollow ? FLinearColor(0.55f, 0.12f, 1.f) : (B ? B->Def.Secondary : FLinearColor::White);''',
'''	void Dress(USkeletalMeshComponent* Sk, const AZoogiBall* B, bool bHollow)
	{
		DressAs(Sk, B ? B->Def.Name : FString(), B ? B->PlayerIndex : 0, B ? B->Def.Color : FLinearColor(0.2f, 0.6f, 1.f), B ? B->Def.Secondary : FLinearColor::White, bHollow);
	}
	void DressAs(USkeletalMeshComponent* Sk, const FString& ZName, int32 Seed, const FLinearColor& InPri, const FLinearColor& InSec, bool bHollow)
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoDress"));
		if (!Sk || bOff || !On()) return;
		AActor* Ow = Sk->GetOwner(); if (!Ow) return;
		const FLinearColor Pri = bHollow ? FLinearColor(0.035f, 0.02f, 0.06f) : InPri;
		const FLinearColor Sec = bHollow ? FLinearColor(0.55f, 0.12f, 1.f) : InSec;'''),
('		if (!bHollow && B && Sk->GetBoneIndex(TEXT("head")) != INDEX_NONE)', '		if (!bHollow && !ZName.IsEmpty() && Sk->GetBoneIndex(TEXT("head")) != INDEX_NONE)'),
('			const FString N = B->Def.Name.ToLower();', '			const FString N = ZName.ToLower();'),
(': (B->PlayerIndex + 3) % 5;', ': (FMath::Abs(Seed) + 3) % 5;'),
('bHollow ? TEXT("VOID HOLLOW") : (B ? *B->Def.Name : TEXT("-"))', 'bHollow ? TEXT("VOID HOLLOW") : (ZName.IsEmpty() ? TEXT("-") : *ZName)'),
])

# ---- the pad + dressing in the Voyager levels subsystem
patch('Public/ZoogiVoyagerLevels.h', [('	TSharedPtr<class SWidget> ExitWidget;', '''	TSharedPtr<class SWidget> ExitWidget;
	TSharedPtr<class SWidget> PadWidget; // b246
	double NextDress = 0.0; // b246
	void PadTick(); void DressTick(); // b246''')])
PAD = r'''
// ---------------------------------------------------------------- b246: Zoogi touch pad for the Voyager levels
namespace ZoogiVoyPad
{
	enum EBtn { B_Fire, B_Aim, B_Jump, B_Reload, B_Swap, B_Grenade, B_Melee, B_Sprint, B_Num };
	static const TCHAR* Label[B_Num] = { TEXT("FIRE"), TEXT("AIM"), TEXT("JUMP"), TEXT("RELOAD"), TEXT("SWAP"), TEXT("BOMB"), TEXT("MELEE"), TEXT("RUN") };
	static const TCHAR* Act[B_Num] = { TEXT("IA_Fire"), TEXT("IA_Aim"), TEXT("IA_JumpAndCover"), TEXT("IA_Reload"), TEXT("IA_ChangeWeapon"), TEXT("IA_TossGrenade"), TEXT("IA_MeleeAttack"), TEXT("IA_Sprint") };
	struct FState
	{
		bool Held[B_Num] = {}; bool bSprintOn = false;
		int32 MovePtr = INDEX_NONE, LookPtr = INDEX_NONE; TMap<int32, int32> BtnPtr;
		FVector2D MoveOrigin = FVector2D::ZeroVector, MoveNow = FVector2D::ZeroVector, LookLast = FVector2D::ZeroVector;
		FVector2D Look = FVector2D::ZeroVector; // accumulated drag since the last tick (slate units)
		float StickR = 60.f;
		void Reset() { *this = FState(); }
	};
	static FState G;
	static FVector2D V2(const FVector2f& V) { return FVector2D(V.X, V.Y); }
	bool Wanted()
	{
		static const bool bNo = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoVoyPad")), bTest = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVoyPadTest"));
		return !bNo && (bTest || PLATFORM_IOS || PLATFORM_ANDROID);
	}
	// button centre + radius in local units for a viewport of size Sz
	static void Place(int32 b, const FVector2D& Sz, FVector2D& C, float& R)
	{
		const float S = (float)FMath::Min(Sz.X, Sz.Y), W = (float)Sz.X, H = (float)Sz.Y;
		struct P { float X, Y, R; bool bLeft; };
		static const P Pl[B_Num] = { { .20f, .22f, .115f, false }, { .44f, .14f, .075f, false }, { .15f, .47f, .075f, false }, { .38f, .36f, .062f, false },
			{ .11f, .68f, .056f, false }, { .31f, .57f, .056f, false }, { .60f, .17f, .056f, false }, { .13f, .55f, .062f, true } };
		const P& p = Pl[b];
		C = FVector2D(p.bLeft ? p.X * S : W - p.X * S, H - p.Y * S); R = p.R * S;
	}
}

using ZoogiVoyPad::V2;
class SZoogiVoyPad : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SZoogiVoyPad) {} SLATE_END_ARGS()
	void Construct(const FArguments&)
	{
		SetVisibility(EVisibility::Visible);
		Disc = MakeShared<FSlateRoundedBoxBrush>(FLinearColor(1.f, 1.f, 1.f, 1.f), 8.f, FLinearColor(1.f, 1.f, 1.f, 1.f), 3.f);
		Disc->OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Ring = MakeShared<FSlateRoundedBoxBrush>(FLinearColor(0.f, 0.f, 0.f, 0.f), 8.f, FLinearColor(1.f, 1.f, 1.f, 1.f), 4.f);
		Ring->OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual bool SupportsKeyboardFocus() const override { return false; }

	int32 HitBtn(const FGeometry& Geo, const FVector2D& L) const
	{
		const FVector2D Sz = V2(Geo.GetLocalSize());
		for (int32 b = 0; b < ZoogiVoyPad::B_Num; ++b) { FVector2D C; float R; ZoogiVoyPad::Place(b, Sz, C, R); if (FVector2D::Distance(C, L) <= R * 1.15f) return b; }
		return INDEX_NONE;
	}
	FReply Down(const FGeometry& Geo, int32 Ptr, const FVector2D& Scr)
	{
		using namespace ZoogiVoyPad;
		const FVector2D L = V2(Geo.AbsoluteToLocal(Scr)), Sz = V2(Geo.GetLocalSize());
		if (L.X < 260.f && L.Y < 110.f) return FReply::Unhandled(); // EXIT
		const int32 b = HitBtn(Geo, L);
		if (b != INDEX_NONE)
		{
			G.BtnPtr.Add(Ptr, b);
			if (b == B_Sprint) G.bSprintOn = !G.bSprintOn; else G.Held[b] = true;
			return FReply::Handled();
		}
		if (L.X < Sz.X * 0.45f && G.MovePtr == INDEX_NONE) { G.MovePtr = Ptr; G.MoveOrigin = G.MoveNow = L; G.StickR = (float)FMath::Min(Sz.X, Sz.Y) * 0.12f; return FReply::Handled(); }
		if (G.LookPtr == INDEX_NONE) { G.LookPtr = Ptr; G.LookLast = L; return FReply::Handled(); }
		return FReply::Handled();
	}
	FReply Move(const FGeometry& Geo, int32 Ptr, const FVector2D& Scr)
	{
		using namespace ZoogiVoyPad;
		const FVector2D L = V2(Geo.AbsoluteToLocal(Scr));
		if (Ptr == G.MovePtr)
		{
			G.MoveNow = L; const FVector2D D = L - G.MoveOrigin; const float Len = (float)D.Size();
			if (Len > G.StickR * 1.6f) G.MoveOrigin = L - D / Len * G.StickR * 1.6f; // the stick follows a long drag
		}
		else if (Ptr == G.LookPtr) { G.Look += L - G.LookLast; G.LookLast = L; }
		return FReply::Handled();
	}
	FReply Up(int32 Ptr)
	{
		using namespace ZoogiVoyPad;
		if (const int32* b = G.BtnPtr.Find(Ptr)) { if (*b != B_Sprint) G.Held[*b] = false; G.BtnPtr.Remove(Ptr); }
		if (Ptr == G.MovePtr) { G.MovePtr = INDEX_NONE; G.bSprintOn = false; }
		if (Ptr == G.LookPtr) G.LookPtr = INDEX_NONE;
		return FReply::Handled();
	}
	virtual FReply OnTouchStarted(const FGeometry& Geo, const FPointerEvent& E) override { return Down(Geo, E.GetPointerIndex(), V2(E.GetScreenSpacePosition())); }
	virtual FReply OnTouchMoved(const FGeometry& Geo, const FPointerEvent& E) override { return Move(Geo, E.GetPointerIndex(), V2(E.GetScreenSpacePosition())); }
	virtual FReply OnTouchEnded(const FGeometry& Geo, const FPointerEvent& E) override { return Up(E.GetPointerIndex()); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Clip, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		using namespace ZoogiVoyPad;
		const FVector2D Sz = V2(Geo.GetLocalSize());
		const TSharedRef<FSlateFontMeasure> Fm = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		auto Circle = [&](const FSlateBrush* Br, const FVector2D& C, float R, const FLinearColor& Col, int32 Ly)
		{
			FSlateDrawElement::MakeBox(Out, Ly, Geo.ToPaintGeometry(FVector2f(2.f * R, 2.f * R), FSlateLayoutTransform(FVector2f(C - FVector2D(R, R)))), Br, ESlateDrawEffect::None, Col);
		};
		auto Text = [&](const FString& S, const FVector2D& C, float Size, const FLinearColor& Col, int32 Ly)
		{
			const FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Clamp((int32)Size, 8, 40));
			const auto Tm = Fm->Measure(S, F); const FVector2D Ts(Tm.X, Tm.Y);
			FSlateDrawElement::MakeText(Out, Ly, Geo.ToPaintGeometry(FVector2f(Ts), FSlateLayoutTransform(FVector2f(C - Ts * 0.5))), S, F, ESlateDrawEffect::None, Col);
		};
		// move stick: resting hint bottom-left, or the live floating stick
		const float S = (float)FMath::Min(Sz.X, Sz.Y);
		const FVector2D Rest(S * 0.25f, Sz.Y - S * 0.27f);
		const bool bLive = G.MovePtr != INDEX_NONE;
		const FVector2D O = bLive ? G.MoveOrigin : Rest; const float Rr = S * 0.12f;
		FVector2D K = bLive ? G.MoveNow : Rest; const FVector2D Dk = K - O; if (Dk.Size() > Rr) K = O + Dk.GetSafeNormal() * Rr;
		Circle(Disc.Get(), O, Rr * 1.35f, FLinearColor(0.08f, 0.02f, 0.18f, bLive ? 0.45f : 0.28f), Layer);
		Circle(Ring.Get(), O, Rr * 1.35f, FLinearColor(0.3f, 0.85f, 1.f, 0.7f), Layer + 1);
		Circle(Disc.Get(), K, Rr * 0.5f, FLinearColor(0.3f, 0.85f, 1.f, bLive ? 0.95f : 0.6f), Layer + 2);
		if (!bLive) Text(TEXT("MOVE"), O + FVector2D(0.f, Rr * 1.6f), S * 0.03f, FLinearColor(1.f, 1.f, 1.f, 0.7f), Layer + 3);
		if (G.LookPtr == INDEX_NONE) Text(TEXT("DRAG TO LOOK"), FVector2D(Sz.X * 0.62f, Sz.Y * 0.5f), S * 0.028f, FLinearColor(1.f, 1.f, 1.f, 0.28f), Layer + 3);
		static const FLinearColor Col[B_Num] = { FLinearColor(1.f, 0.12f, 0.35f), FLinearColor(0.2f, 0.75f, 1.f), FLinearColor(0.35f, 1.f, 0.45f), FLinearColor(1.f, 0.8f, 0.2f),
			FLinearColor(0.75f, 0.5f, 1.f), FLinearColor(1.f, 0.5f, 0.15f), FLinearColor(1.f, 0.35f, 0.75f), FLinearColor(0.3f, 1.f, 0.9f) };
		for (int32 b = 0; b < B_Num; ++b)
		{
			FVector2D C; float R; Place(b, Sz, C, R);
			const bool bOn = b == B_Sprint ? G.bSprintOn : G.Held[b];
			FLinearColor Fill = Col[b] * (bOn ? 1.f : 0.55f); Fill.A = bOn ? 0.9f : 0.5f;
			Circle(Disc.Get(), C, R, Fill, Layer);
			Circle(Ring.Get(), C, R, FLinearColor(1.f, 1.f, 1.f, bOn ? 1.f : 0.75f), Layer + 1);
			Text(Label[b], C, R * (b == B_Fire ? 0.42f : 0.48f), FLinearColor::White, Layer + 3);
		}
		return Layer + 4;
	}
	TSharedPtr<FSlateRoundedBoxBrush> Disc, Ring;
};

void UZoogiVoyagerLevelsSubsystem::PadTick()
{
	using namespace ZoogiVoyPad;
	UWorld* W = VoyWorld.Get();
	if (!W || !PadWidget.IsValid()) return;
	ULocalPlayer* LP = W->GetFirstLocalPlayerFromController(); if (!LP) return;
	UEnhancedInputLocalPlayerSubsystem* Ei = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP); if (!Ei) return;
	static TMap<FString, TWeakObjectPtr<UInputAction>> Cache;
	auto A = [](const TCHAR* Name, bool bAxis) -> UInputAction*
	{
		if (TWeakObjectPtr<UInputAction>* C = Cache.Find(Name)) if (C->IsValid()) return C->Get();
		const FString P = FString::Printf(TEXT("/Game/Voyager/Inputs/%s/%s.%s"), bAxis ? TEXT("AxisInputActions") : TEXT("DigitalBoolInputActions"), Name, Name);
		UInputAction* Ia = LoadObject<UInputAction>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Ia) UE_LOG(LogZoogi, Warning, TEXT("VOYPAD: no input action %s"), *P);
		Cache.Add(Name, Ia);
		return Ia;
	};
	for (int32 b = 0; b < B_Num; ++b)
		if (b == B_Sprint ? G.bSprintOn : G.Held[b]) if (UInputAction* Ia = A(Act[b], false)) Ei->InjectInputForAction(Ia, FInputActionValue(true));
	if (G.MovePtr != INDEX_NONE)
	{
		FVector2D D = (G.MoveNow - G.MoveOrigin) / FMath::Max(1.f, G.StickR);
		if (D.Size() > 1.0) D = D.GetSafeNormal();
		if (D.Size() > 0.12)
		{
			if (UInputAction* F = A(TEXT("IA_MoveForward"), true)) Ei->InjectInputForAction(F, FInputActionValue((float)-D.Y));
			if (UInputAction* R = A(TEXT("IA_MoveRight"), true)) Ei->InjectInputForAction(R, FInputActionValue((float)D.X));
		}
	}
	if (!G.Look.IsNearlyZero())
	{
		static float K = -1.f; static bool bInv = false;
		if (K < 0.f) { K = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiPadLook="), K); bInv = FParse::Param(FCommandLine::Get(), TEXT("ZoogiPadInvY")); }
		if (UInputAction* X = A(TEXT("IA_MouseRight"), true)) Ei->InjectInputForAction(X, FInputActionValue((float)(G.Look.X * K)));
		if (UInputAction* Y = A(TEXT("IA_MouseUp"), true)) Ei->InjectInputForAction(Y, FInputActionValue((float)(G.Look.Y * K * (bInv ? 1.f : -1.f))));
		G.Look = FVector2D::ZeroVector;
	}
}

void UZoogiVoyagerLevelsSubsystem::DressTick()
{	// b246: the Voyager hero = your equipped Zoogi, every other character = a Void hollow
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("ZoogiVoyNoDress"));
	UWorld* W = VoyWorld.Get();
	if (bOff || !W || FPlatformTime::Seconds() < NextDress) return;
	NextDress = FPlatformTime::Seconds() + 1.0;
	APawn* Me = nullptr; if (APlayerController* Pc = W->GetFirstPlayerController()) Me = Pc->GetPawn();
	static const FName Tag(TEXT("ZoogiDressed"));
	int32 N = 0;
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* Ch = *It; if (!Ch || Ch->Tags.Contains(Tag) || !Ch->GetMesh() || !Ch->GetMesh()->GetSkeletalMeshAsset()) continue;
		Ch->Tags.Add(Tag);
		if (Ch == Me)
		{
			const ZoogiFront::FHero& H = ZoogiFront::Equipped(W);
			const FZoogiDef* D = ZoogiUtil::FindZoogi(FName(H.RosterId));
			ZoogiVoyager::DressAs(Ch->GetMesh(), H.Name, 0, D ? D->Color : FLinearColor(0.2f, 0.6f, 1.f), D ? D->Secondary : FLinearColor::White, false);
			UE_LOG(LogZoogi, Log, TEXT("VOYAGER hero dressed as %s"), H.Name);
		}
		else ZoogiVoyager::DressAs(Ch->GetMesh(), FString(), ++N, FLinearColor::Black, FLinearColor::Black, true);
	}
}
'''
patch('Private/ZoogiVoyagerLevels.cpp', [
('void UZoogiVoyagerLevelsSubsystem::Initialize(', PAD + '\nvoid UZoogiVoyagerLevelsSubsystem::Initialize('),
('	ExitWidget.Reset();\n', '	ExitWidget.Reset(); PadWidget.Reset(); ZoogiVoyPad::G.Reset(); NextDress = 0.0;\n'),
('		Vp->AddViewportWidgetContent(ExitWidget.ToSharedRef(), 1000);\n',
'''		Vp->AddViewportWidgetContent(ExitWidget.ToSharedRef(), 1000);
		if (ZoogiVoyPad::Wanted()) { PadWidget = SNew(SZoogiVoyPad); Vp->AddViewportWidgetContent(PadWidget.ToSharedRef(), 900); UE_LOG(LogZoogi, Log, TEXT("VOYPAD on")); }
'''),
('	ZoogiApplePad::WantFull(VoyWorld.IsValid()); // the Apple on-screen controller on Voyager maps (iOS)',
'''	ZoogiApplePad::WantFull(VoyWorld.IsValid() && !ZoogiVoyPad::Wanted()); // b246: the Zoogi pad replaces Apple's controller (-ZoogiNoVoyPad = Apple's)
	if (VoyWorld.IsValid()) { PadTick(); DressTick(); }'''),
], incs=('Widgets/SLeafWidget.h', 'Brushes/SlateRoundedBoxBrush.h', 'Rendering/DrawElements.h', 'Fonts/FontMeasure.h', 'Framework/Application/SlateApplication.h',
         'EnhancedInputSubsystems.h', 'InputAction.h', 'InputActionValue.h', 'Engine/LocalPlayer.h', 'GameFramework/Character.h', 'GameFramework/PlayerController.h',
         'Components/SkeletalMeshComponent.h', 'EngineUtils.h', 'ZoogiVoyager.h', 'ZoogiFront.h', 'ZoogiTypes.h'))

f = io.open(S + 'Private/ZoogiFront.cpp', encoding='utf-8-sig').read(); k = '			{ TEXT("B245"),'
assert f.count(k) == 1
f = f.replace(k, '			{ TEXT("B246"), TEXT("VOYAGER TOUCH + ZOOGI HERO"), TEXT("Voyager levels on phones get Zoogi touch controls: drag the left side to move, drag the right side to look, and big FIRE / AIM / JUMP / RELOAD / SWAP / BOMB / MELEE / RUN buttons. Your hero now wears your equipped Zoogi (its Tripo head and colours) and the enemies are Void hollows.") },\n' + k)
io.open(S + 'Private/ZoogiFront.cpp', 'w', encoding='utf-8', newline='').write(f)
print('ALL OK')
