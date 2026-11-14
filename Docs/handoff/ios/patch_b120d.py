# b120d (Evan 17:50, screenshot WOLF PACK button: "Button taps should never count as a target tap on field. If target selecting
# during aiming that counts as a target and if I push superpower it activates the effect."):
#  - ZoogiClassicPad::PointerOwned(): a SUPERPOWER / REACTION / WOLF press owns the pointer (held, or 0.35 s after the press);
#    AZoogiHUD::IsOverUI counts it, so no field tap / aim / target lock sees that touch
#  - Wolf Pack paint drags only count when the press began on the field (the held SUPERPOWER finger used to paint the orb under it)
#  - skill / redirect target picks ignore presses that belong to a button (pad, P5 command menu, AUTO / ARC bar)
#  - a target tapped while aiming (hard lock) + SUPERPOWER: the Wolf Pack sends the closest wolf straight at it (no pick step);
#    Bolt Balls fire at it
import os
R = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
def ed1(p, o, n):
    p = R + p; b = open(p, 'rb').read(); o = o.encode(); n = n.encode()
    if b'\r\n' in b: o = o.replace(b'\n', b'\r\n'); n = n.replace(b'\n', b'\r\n')
    assert b.count(o) == 1, (p, o[:80]); open(p, 'wb').write(b.replace(o, n))
CP = 'Private/ZoogiClassicPad.cpp'
ed1('Public/ZoogiClassicPad.h', '''	ZOOGIROLL_API void Ensure(UWorld* World);''', '''	ZOOGIROLL_API void Ensure(UWorld* World);
	/** b120: a pad button (REACTION / SUPERPOWER / WOLF) owns the pointer right now - never a field tap */
	ZOOGIROLL_API bool PointerOwned();''')
ed1(CP, '''	FDelegateHandle GCleanup;
''', '''	FDelegateHandle GCleanup;
	double GPadPressAt = -10.0; bool GPadHeld = false; // b120: the last pad press (PointerOwned)
''')
ed1(CP, '''		FReply Press()
		{
			bHeld = true;
			FlashAt = FPlatformTime::Seconds();''', '''		FReply Press()
		{
			bHeld = true;
			FlashAt = FPlatformTime::Seconds();
			GPadPressAt = FlashAt; GPadHeld = true; // b120''')
ed1(CP, '''		FReply Release()
		{
			bHeld = false;''', '''		FReply Release()
		{
			bHeld = false; GPadHeld = false; GPadPressAt = FPlatformTime::Seconds(); // b120: the lift belongs to the button too''')
ed1(CP, '''			bHeld = true; FlashAt = FPlatformTime::Seconds();
			if (AZoogiGameMode* G = PadGM())
			{
				if (ZoogiSkills::WolfGo(G))''', '''			bHeld = true; FlashAt = FPlatformTime::Seconds();
			GPadPressAt = FlashAt; GPadHeld = true; // b120
			if (AZoogiGameMode* G = PadGM())
			{
				if (ZoogiSkills::WolfGo(G))''')
ed1(CP, '''		FReply Release() { bHeld = false; return FReply::Handled().ReleaseMouseCapture(); }''', '''		FReply Release() { bHeld = false; GPadHeld = false; GPadPressAt = FPlatformTime::Seconds(); return FReply::Handled().ReleaseMouseCapture(); }''')
ed1(CP, '''void ZoogiClassicPad::Ensure(UWorld* World)
{''', '''bool ZoogiClassicPad::PointerOwned()
{
	const double Dt = FPlatformTime::Seconds() - GPadPressAt;
	return (GPadHeld && Dt < 6.0) || Dt < 0.35; // (a lost touch-end never blocks the field for more than 6 s)
}

void ZoogiClassicPad::Ensure(UWorld* World)
{''')
# HUD: pad presses are UI
H = 'Private/ZoogiHUD.cpp'
ed1(H, '''#include "ZoogiP5Command.h"''', '''#include "ZoogiP5Command.h"
#include "ZoogiClassicPad.h"''')
ed1(H, '''|| ZoogiP5Cmd::InputBlocked() || ZoogiArenaHUD::OverChrome(GetWorld(), ViewportPos); // b77''', '''|| ZoogiP5Cmd::InputBlocked() || ZoogiClassicPad::PointerOwned() || ZoogiArenaHUD::OverChrome(GetWorld(), ViewportPos); // b120: a pad button press is never a field tap; b77''')
# controller
C = 'Private/ZoogiPlayerController.cpp'
ed1(C, '''#include "ZoogiP5Command.h"''', '''#include "ZoogiP5Command.h"
#include "ZoogiClassicPad.h"''')
ed1(C, '''	if (ZoogiSkills::Picking()) { ZoogiSkills::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }
	if (ZoogiRedirect::Picking()) { ZoogiRedirect::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; } // b66''',
'''	// b120 (Evan: "button taps should never count as a target tap on field"): a press that belongs to a button never reaches a pick
	static bool bPickPressOk = true;
	const bool bBtnOwned = ZoogiClassicPad::PointerOwned() || ZoogiP5Cmd::InputBlocked() || (Hud && Hud->UIConsumedClick());
	if (bPtrPress) bPickPressOk = !bBtnOwned;
	const bool bPickPtr = bPickPressOk && !bBtnOwned;
	if (ZoogiSkills::Picking()) { if (bPickPtr || (!bPtrPress && !bPtrDown && !bPtrRelease)) ZoogiSkills::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; }
	if (ZoogiRedirect::Picking()) { if (bPickPtr || (!bPtrPress && !bPtrDown && !bPtrRelease)) ZoogiRedirect::Input(G, this, Ptr, bPtrPress, bPtrDown, bPtrRelease); HoverTarget.Reset(); return; } // b66''')
ed1(C, '''		if (bPtrPress && !(Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick()))) bFunTap = WP->OnTap(Ptr);
		else if (bPtrDown && WP->IsPainting()) WP->OnDrag(Ptr);''', '''		// b120: a paint drag only counts when its press began on the field (the held SUPERPOWER finger painted the orb under it)
		const bool bOverUiNow = Hud && (Hud->IsOverUI(Ptr) || Hud->UIConsumedClick());
		static bool bPaintDragOk = false;
		if (bPtrPress) bPaintDragOk = !bOverUiNow;
		if (bPtrPress && !bOverUiNow) bFunTap = WP->OnTap(Ptr);
		else if (bPtrDown && bPaintDragOk && !bOverUiNow && WP->IsPainting()) WP->OnDrag(Ptr);''')
# wolf pack: a locked target + SUPERPOWER = the closest wolf strikes it right away
WH = 'Public/ZoogiWolfPack.h'; W = 'Private/ZoogiWolfPack.cpp'
ed1(WH, '''	void PaintPick(const FVector2D& Screen, float MaxPx);''', '''	void PaintPick(const FVector2D& Screen, float MaxPx);
	void PickActor(AActor* Best); // b120: one target painted (tap or the aim's locked target)''')
ed1(W, '''	if (!Best) return;
	Painted.Add(Best);''', '''	if (!Best) return;
	PickActor(Best);
}

void UZoogiWolfPackSys::PickActor(AActor* Best)
{
	if (Phase != EPhase::Paint || !Best || Painted.Num() >= PaintMax) return;
	Painted.Add(Best);''')
ed1(W, '''	if (bDire) Toast(TEXT("PACK COMBO!"), TEXT("YOUR PARTNER'S PACK JOINS - DIRE WOLF"), PackGold, 2.f, 1.1f);''', '''	// b120 (Evan: "if target selecting during aiming that counts as a target and if I push superpower it activates the effect"):
	// the target tapped while aiming (hard lock) gets the closest wolf at once - no pick step; the rest stay with the player
	if (bFieldPack && bHumanOwner && G->Phase == EZoogiPhase::Aiming && G->TargetLock.IsValid() && !G->bSoftLock && !FParse::Param(FCommandLine::Get(), TEXT("ZoogiWolfNoLockFire")))
	{
		AActor* Lk = G->TargetLock.Get();
		if (Targets(B).Contains(Lk))
		{
			PickActor(Lk);
			UE_LOG(LogZoogi, Log, TEXT("WOLF summon P%d: aim target %s locked -> the closest wolf strikes it now"), Owner + 1, *Lk->GetName());
			Toast(TEXT("WOLF STRIKE!"), TEXT("YOUR TARGET"), PackTint(B), 1.4f, 1.f);
			Release();
			return;
		}
	}
	if (bDire) Toast(TEXT("PACK COMBO!"), TEXT("YOUR PARTNER'S PACK JOINS - DIRE WOLF"), PackGold, 2.f, 1.1f);''')
# bolt balls: the locked target first
S = 'Private/ZoogiSkills.cpp'
ed1(S, '''		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		FVector Dir = Best ?''', '''		if (!Best) for (AZoogiOrb* O : G->Orbs) if (O && !O->bOut) { const float D = FVector::DistSquared2D(O->OrbLocation(), From); if (D < BestD) { BestD = D; Best = O; } }
		if (G->TargetLock.IsValid() && !G->bSoftLock && G->TargetLock.Get() != B) Best = G->TargetLock.Get(); // b120: the target picked while aiming
		FVector Dir = Best ?''')
print('b120d ok')
