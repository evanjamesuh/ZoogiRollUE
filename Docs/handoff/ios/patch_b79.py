# b79 (Claude): nothing over the pause menu; cog -> P5 MENU button (DEV + CAMERA inside the menu, TAC/CAM chrome gone);
# taps on MENU / avatars / sky never pick a target or move the player
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'; H = 'Source/ZoogiRoll/Public/'
GS_INC = '#include "Kismet/GameplayStatics.h"'
# ---- nothing over the pause menu
ed(P + 'ZoogiActionFun.cpp', [
 ('''	GFunIconsOff = true;
	if (!HUD || !C || HUD->GetWorld() != GetWorld()) return;''',
  '''	GFunIconsOff = true;
	if (!HUD || !C || HUD->GetWorld() != GetWorld()) return;
	if (UGameplayStatics::IsGamePaused(GetWorld())) return; // b79 (Evan: "nothing overlays the menu"): the pause menu is clear'''),
])
ed(P + 'ZoogiWolfPack.cpp', [
 ('''	if (!C || !HUD || HUD->GetWorld() != GetWorld()) return;
	AZoogiGameMode* G = GM(); if (!G || !ZoogiWolf::On()) return;''',
  '''	if (!C || !HUD || HUD->GetWorld() != GetWorld()) return;
	if (UGameplayStatics::IsGamePaused(GetWorld())) return; // b79: nothing over the pause menu
	AZoogiGameMode* G = GM(); if (!G || !ZoogiWolf::On()) return;'''),
], inc=GS_INC, after='#include "ZoogiWolfPack.h"')
ed(P + 'ZoogiClassicPad.cpp', [
 ('			return G && (G->ClassicButtonsOn() || PadTapMode() > 0) && !(G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn());',
  '			return G && !UGameplayStatics::IsGamePaused(G) && (G->ClassicButtonsOn() || PadTapMode() > 0) && !(G->FrozenRoamOn() && ZoogiFrozenRoam::BridgeEntryOn()); // b79: hidden under the pause menu'),
 ('''						const bool bOn = Shown() && (Which == 0 ? ReactionNeeded() : (G && G->ClassicButtonsOn()));''',
  '''						const bool bOn = Shown() && (Which == 0 ? ReactionNeeded() : (G && G->ClassicButtonsOn()));'''),
], inc=GS_INC, after='#include "ZoogiClassicPad.h"')
ed(P + 'ZoogiP5Command.cpp', [
 ('''		GS.G = G;
		SP5Cmd& W = *GS.W;
		const double Now = FPlatformTime::Seconds();''',
  '''		GS.G = G;
		SP5Cmd& W = *GS.W;
		if (UGameplayStatics::IsGamePaused(World)) { W.SetVisibility(EVisibility::Collapsed); GShowing = false; return true; } // b79: nothing over the pause menu
		const double Now = FPlatformTime::Seconds();'''),
])
# ---- MENU button + DEV / CAMERA in the pause menu + OverChrome
ed(H + 'ZoogiArenaHUDWidget.h', [
 ('	ZOOGIROLL_API bool AvatarRect(int32 Player, FVector2D& OutAbsPos, FVector2D& OutAbsSize);\n',
  '	ZOOGIROLL_API bool AvatarRect(int32 Player, FVector2D& OutAbsPos, FVector2D& OutAbsSize);\n	/** b79: viewport pixel is on the MENU button or a profile avatar (a tap there is UI, never a target pick) */\n	ZOOGIROLL_API bool OverChrome(const UWorld* World, const FVector2D& ViewportPx);\n'),
])
ed(P + 'ZoogiArenaHUDWidget.cpp', [
 ('''	static TWeakObjectPtr<UWidget> GAvatar[4];
	float TacOpacity()''',
  '''	static TWeakObjectPtr<UWidget> GAvatar[4];
	static TWeakObjectPtr<UWidget> GMenuPlate; // b79
	bool OverChrome(const UWorld* World, const FVector2D& ViewportPx)
	{
		UGameViewportClient* VC = World ? World->GetGameViewport() : nullptr;
		TSharedPtr<SViewport> VW = VC ? VC->GetGameViewportWidget() : nullptr;
		if (!VW.IsValid()) return false;
		const FGeometry& VG = VW->GetCachedGeometry();
		if (VG.GetLocalSize().X <= 0.f) return false;
		const FVector2D Abs = VG.LocalToAbsolute(ViewportPx / FMath::Max(VG.Scale, 0.01f));
		auto In = [&Abs](const UWidget* W, float Pad)
		{
			if (!W || !W->IsVisible() || !W->GetCachedWidget().IsValid() || W->GetRenderOpacity() < 0.05f) return false;
			const FGeometry& G = W->GetCachedGeometry();
			const FVector2D P = FVector2D(G.GetAbsolutePosition()), S = FVector2D(G.GetAbsoluteSize());
			return S.X > 4.f && Abs.X >= P.X - Pad && Abs.Y >= P.Y - Pad && Abs.X <= P.X + S.X + Pad && Abs.Y <= P.Y + S.Y + Pad;
		};
		if (In(GMenuPlate.Get(), 14.f)) return true;
		for (int32 i = 0; i < 4; ++i) if (In(GAvatar[i].Get(), 8.f)) return true;
		return false;
	}
	float TacOpacity()'''),
 ('''	// ---- Settings cog (circular dark plate + icon_gear / gear.png) - opens BuildPause
	At(Box(ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.96f), ZCHex(TEXT("8A90A0")), 3.f, Sz * 0.5f), CogX, CogY, Sz, Sz);''',
  '''	// b79 (Evan: "turn the cog button into an actual P5 menu button, put DEV and CAM inside it"): a tilted black / red / white MENU
	// plate opens the P5 pause menu (DEV MODE + CAMERA rows live there now). -ZoogiOldChrome = the cog + TAC/CAM button
	static const bool bOldChrome = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome"));
	if (!bOldChrome)
	{
		const float MW = 150.f, MH = 62.f, MX = CogX + Sz - MW, MY = CogY + (Sz - MH) * 0.5f;
		auto Tilt = [](UWidget* Wg, float Deg) { if (Wg) { Wg->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); Wg->SetRenderTransformAngle(Deg); } return Wg; };
		Tilt(At(Box(ZCHex(TEXT("0A0A0C")), FLinearColor::Transparent, 0.f, 0.f), MX + 7.f, MY + 7.f, MW, MH), -6.f);
		Tilt(At(Box(FLinearColor::White, FLinearColor::Transparent, 0.f, 0.f), MX - 4.f, MY - 4.f, MW + 8.f, MH + 8.f), -6.f);
		UWidget* Plate = Tilt(At(Box(ZCHex(TEXT("E0101E")), FLinearColor::Transparent, 0.f, 0.f), MX, MY, MW, MH), -6.f);
		Tilt(Text(TEXT("MENU"), MX, MY - 2.f, MW, MH, 34.f, FLinearColor::White, 2.f, 1), -6.f);
		ZoogiArenaHUD::GMenuPlate = Plate;
		Hit(MX - HitPad, MY - HitPad, MW + HitPad * 2.f, MH + HitPad * 2.f, [this]() { SetPaused(!bPaused); });
		if (bMark) MarkSplit(Mark, FVector2D::ZeroVector, 2);
		(void)TacX; (void)TacY;
		return;
	}
	// ---- Settings cog (circular dark plate + icon_gear / gear.png) - opens BuildPause
	At(Box(ZCHex(TEXT("1F232C")).CopyWithNewOpacity(0.96f), ZCHex(TEXT("8A90A0")), 3.f, Sz * 0.5f), CogX, CogY, Sz, Sz);'''),
 ('''	Rows.Add({ TEXT("RESUME"), 0, [this]() { SetPaused(false); } });''',
  '''	Rows.Add({ TEXT("RESUME"), 0, [this]() { SetPaused(false); } });
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldChrome")))
	{	// b79: the camera + DEV buttons moved in here from the HUD
		const AZoogiGameMode* Gc = GM();
		static FString CamLabel; CamLabel = FString::Printf(TEXT("CAMERA: %s"), (Gc && Gc->bCinematicCam) ? TEXT("CINEMATIC") : ((Gc && Gc->bBirdEyeCam) ? TEXT("TACTICAL") : TEXT("NORMAL")));
		Rows.Add({ *CamLabel, 2, [this]() { if (AZoogiGameMode* G = GM()) G->MenuAction(TEXT("CamStyle"), 0); BuiltSize = FVector2D::ZeroVector; } });
		Rows.Add({ ZoogiLayout::IsDevMode() ? TEXT("DEV MODE: ON") : TEXT("DEV MODE"), 1, [this]() { SetPaused(false); ZoogiLayout::SetDevMode(!ZoogiLayout::IsDevMode()); } });
	}'''),
 ('''	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const float Ang = (i % 2 == 0) ? -5.f : -2.5f, Dx = (i % 2 == 0) ? 0.f : 46.f;
		const float BW = PW - 60.f - Dx * 0.5f, BX = X0 + 20.f + Dx, BH = 76.f;''',
  '''	const float RowStep = Rows.Num() > 5 ? 80.f : 92.f; // b79: 7 rows fit
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const float Ang = (i % 2 == 0) ? -5.f : -2.5f, Dx = (i % 2 == 0) ? 0.f : 46.f;
		const float BW = PW - 60.f - Dx * 0.5f, BX = X0 + 20.f + Dx, BH = RowStep - 16.f;'''),
 ('''		Hit(BX, Y, BW, BH, Rows[i].Fn);
		Y += 92.f;''', '''		Hit(BX, Y, BW, BH, Rows[i].Fn);
		Y += RowStep;'''),
], inc='#include "ZoogiDevMode.h"\n#include "Widgets/SViewport.h"\n#include "Engine/GameViewportClient.h"', after='#include "ZoogiGameMode.h"')
ed(P + 'ZoogiHUD.cpp', [
 ('|| ZoogiP5Cmd::InputBlocked(); // b77: a P5 menu tap never reaches the game',
  '|| ZoogiP5Cmd::InputBlocked() || ZoogiArenaHUD::OverChrome(GetWorld(), ViewportPos); // b77: a P5 menu tap never reaches the game; b79: nor the MENU button / avatars'),
])
# ---- sky taps: no snap pick, no aim move
ed(P + 'ZoogiPlayerController.cpp', [
 ('''		// #44 forgiving snap-to-aim: if the press missed a precise hit, retry PickTarget (SnapAimMul widens the disc)

		if (!T) T = PickTarget(At.X, At.Y);''',
  '''		// b79 (Evan: "tapping the menu button or the sky should not count as a selected target and should not move the player"):
		// a tap above the horizon only picks a target drawn right under the finger
		{
			FVector Pf;
			if (!FloorPoint(At, Pf))
			{
				AActor* Tn = T ? T : PickTarget(At.X, At.Y);
				FVector2D Ts; int32 VX = 1920, VY = 1080; GetViewportSize(VX, VY);
				const float Px = FMath::Max(FMath::Min(VX, VY), 1) / 1080.f;
				if (!(Tn && ProjectWorldLocationToScreen(Tn->GetActorLocation(), Ts) && FVector2D::Distance(Ts, At) < 70.f * Px)) { UE_LOG(LogZoogi, Log, TEXT("INPUT sky tap ignored (no target, aim kept)")); return; }
				T = Tn;
			}
		}
		// #44 forgiving snap-to-aim: if the press missed a precise hit, retry PickTarget (SnapAimMul widens the disc)

		if (!T) T = PickTarget(At.X, At.Y);'''),
])
print('b79 ok')
# ---- after FIRE (and a self skill), the menu folds to the SKILL button so the effect is visible
ed(H + 'ZoogiP5Command.h', [
 ('	ZOOGIROLL_API void BackToSkills();\n', '	ZOOGIROLL_API void BackToSkills();\n	/** b79 (Evan: "after FIRE the skill menu should hide to the SKILL button so people can see the effect"): fold to the SKILL button */\n	ZOOGIROLL_API void ToSkillButton();\n'),
])
ed(P + 'ZoogiP5Command.cpp', [
 ('''	static int32 GuardSlot(AZoogiGameMode* G, AZoogiBall* B)''',
  '''	void ToSkillButton() { if (AZoogiGameMode* G = GS.G.Get()) { Dismiss(G); UE_LOG(LogZoogi, Log, TEXT("P5CMD folded to the SKILL button (skill played)")); } }
	static int32 GuardSlot(AZoogiGameMode* G, AZoogiBall* B)'''),
 ('''		else BackToSkills(); // b73 (Evan): played - the skill list stays up until the launch (deplete SP)''',
  '''		else ToSkillButton(); // b79 (Evan): played - fold to the SKILL button so the effect shows (tap it for the list again)'''),
])
ed(P + 'ZoogiSkills.cpp', [
 ('''			ZoogiP5Cmd::BackToSkills(); // b73 (Evan): back to the skill list until the launch (deplete SP)
			return;''',
  '''			ZoogiP5Cmd::ToSkillButton(); // b79 (Evan): fold to the SKILL button so the effect shows (b73 reopened the list)
			return;'''),
])
print('b79 fire ok')
# ---- Golden Apple: the Apple of Eden effect on the caster; the tapped orb (still the gold star worth double) blasts like one wolf detonation
ed(P + 'ZoogiSkills.cpp', [
 ('''				AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), O->OrbLocation(), Sc(Id, 0.3f), 3.f, O);
				AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), O->OrbLocation(), 1.f);
				O->bStar = true; O->PowerColor = FLinearColor(1.f, 0.8f, 0.2f);
				AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(1.f, 0.8f, 0.25f), 120.f, 30.f, true, 6.f, O);''',
  '''				// b79 (Evan: "Golden Apple's effect should fire for the player, not the orbs; the target orb's blast should act the same
				// as a single explosion from a wolf"): Apple of Eden on the caster, then a wolf-clone detonation at the orb
				static const bool bOldApple = FParse::Param(FCommandLine::Get(), TEXT("ZoogiOldGoldenApple"));
				if (bOldApple)
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), O->OrbLocation(), Sc(Id, 0.3f), 3.f, O);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), O->OrbLocation(), 1.f);
				}
				else
				{
					AZoogiFabFX::Play(W, FX(TEXT("NS_Apple_of_Eden")), Caster->BallLocation(), Sc(Id, 0.3f), 3.f, Caster);
					AZoogiFabFX::Sound(W, SFX(TEXT("S_Apple_of_Eden")), Caster->BallLocation(), 1.f);
					const FVector Oc = O->OrbLocation();
					const float R = 430.f; // AZoogiClone ShockR (one wolf, no pack multipliers)
					int32 Pushed = 0;
					for (AZoogiOrb* Ob : G->Orbs)
					{
						if (!Ob || Ob == O || Ob->bOut || !Ob->Sphere || !Ob->Sphere->IsSimulatingPhysics()) continue;
						FVector D = Ob->OrbLocation() - Oc; D.Z = 0.f;
						const float Dist = D.Size();
						if (Dist > R) continue;
						const FVector Dir = Dist > 1.f ? D / Dist : FVector(1.f, 0.f, 0.f);
						const float K = 1.f - Dist / R;
						const FVector V = Ob->Sphere->GetPhysicsLinearVelocity();
						Ob->Sphere->SetPhysicsLinearVelocity(FVector(V.X, V.Y, FMath::Max(V.Z, 0.f)) + Dir * (420.f + 1380.f * K) + FVector(0.f, 0.f, 160.f * K));
						Ob->LastHitBy = Pi;
						++Pushed;
					}
					const FVector Ground(Oc.X, Oc.Y, FZ + 4.f);
					const FLinearColor Gold(1.f, 0.8f, 0.25f);
					AZoogiFX::Spawn(W, Oc, FLinearColor(0.75f, 0.9f, 1.f), 120.f, 0.22f, false, 14.f);
					AZoogiFX::Spawn(W, Ground, Gold, R, 0.55f, true, 10.f);
					AZoogiFX::Shockwave(W, Ground, EZoogiShockwaveSize::Big, EZoogiShockwaveKind::Flat, &Gold, false);
					UZoogiScreenFx::Shockwave(W, Ground, R + 60.f, Gold);
					ZoogiUtil::PlaySfx(G, TEXT("CritBoom"), 0.8f, 1.1f);
					AZoogiFX::Stylized(W, TEXT("Mid_Explosion"), Ground, 0.42f);
					UE_LOG(LogZoogi, Log, TEXT("SKILL golden_apple: Eden on P%d, wolf blast at the orb pushed %d orbs"), Pi + 1, Pushed);
				}
				O->bStar = true; O->PowerColor = FLinearColor(1.f, 0.8f, 0.25f);
				AZoogiFX::Spawn(W, O->OrbLocation(), FLinearColor(1.f, 0.8f, 0.25f), 120.f, 30.f, true, 6.f, O);'''),
], inc='#include "ZoogiScreenFx.h"', after='#include "ZoogiSkills.h"')
print('b79 apple ok')
# ---- centre objects around the spire: no collision, fade with the scoreboard
ed(P + 'ZoogiHoloSpire.cpp', [
 ('void UZoogiHoloSpireSys::Tick(float DeltaTime)\n{',
  '''// b79 (Evan: "any object in the centre around the spire fades away with the scoreboard - the centre should be free for orbs and
// players to blast across"): small solid props standing in the court centre lose their collision and hide with the spire.
// -ZoogiKeepCentre = leave them.
namespace { TArray<TWeakObjectPtr<UPrimitiveComponent>> GCentreParts; }
static void ZoogiGatherCentre(UWorld* W, AZoogiArena* Ar, AActor* Spire)
{
	GCentreParts.Reset();
	static const bool bKeep = FParse::Param(FCommandLine::Get(), TEXT("ZoogiKeepCentre"));
	if (bKeep || !W || !Ar) return;
	const FVector C = Ar->GetActorLocation();
	const float R = 330.f * Ar->GetActorScale3D().X;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == Spire || A->IsA<AZoogiBall>() || A->IsA<AZoogiOrb>() || A->IsA<APawn>() || A->IsA<AController>()) continue;
		TInlineComponentArray<UPrimitiveComponent*> Pcs(A);
		for (UPrimitiveComponent* Pc : Pcs)
		{
			if (!Pc || !Pc->IsRegistered() || Pc->IsSimulatingPhysics()) continue;
			const FBoxSphereBounds& B = Pc->Bounds;
			if (B.SphereRadius > R * 1.1f || FVector::Dist2D(B.Origin, C) > R) continue;
			if (B.Origin.Z + B.BoxExtent.Z < C.Z + 12.f) continue; // flat floor dressing stays
			if (B.Origin.Z - B.BoxExtent.Z > C.Z + 900.f) continue; // high up (lights, sky bits)
			Pc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GCentreParts.Add(Pc);
		}
	}
	UE_LOG(LogZoogi, Log, TEXT("HOLOSPIRE centre cleared: %d props lose collision and fade with the scoreboard"), GCentreParts.Num());
}

void UZoogiHoloSpireSys::Tick(float DeltaTime)
{'''),
 ('if (AZoogiHoloSpire* N = W->SpawnActor<AZoogiHoloSpire>(AZoogiHoloSpire::StaticClass(), FTransform(Base), SP)) { N->Build(Sc * Cfg); Spire = N; BuiltFor = Ar; }',
  'if (AZoogiHoloSpire* N = W->SpawnActor<AZoogiHoloSpire>(AZoogiHoloSpire::StaticClass(), FTransform(Base), SP)) { N->Build(Sc * Cfg); Spire = N; BuiltFor = Ar; ZoogiGatherCentre(W, Ar, N); }'),
 ('''			for (UPrimitiveComponent* Pc : Parts) if (Pc) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f);''',
  '''			for (UPrimitiveComponent* Pc : Parts) if (Pc) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f);
			for (const TWeakObjectPtr<UPrimitiveComponent>& Wp : GCentreParts) if (UPrimitiveComponent* Pc = Wp.Get()) Pc->SetHiddenInGame(FadeK < 0.99f && E < 0.35f); // b79: the centre props go with it'''),
], inc='#include "EngineUtils.h"\n#include "GameFramework/Pawn.h"\n#include "GameFramework/Controller.h"', after='#include "ZoogiHoloSpire.h"')
print('b79 centre ok')
