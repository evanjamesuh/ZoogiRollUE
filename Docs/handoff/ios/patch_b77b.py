# b77b (Claude): LAUNCH button gone (AUTO / [WOLF] / ARC); CPU avatar icons drawn under every menu / text (main HUD canvas, first)
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiAwakenWidget.cpp', [
 ('TArray<int32> Steps; if (bOldModes) Steps = { 0, 1, 2, 3 }; else Steps = { 0, 3 };',
  'TArray<int32> Steps; if (bOldModes) Steps = { 0, 1, 2, 3 }; else Steps = { 0 }; // b77 (Evan: "delete the LAUNCH button"): AUTO / [WOLF] / ARC'),
])
ed(P + 'ZoogiClassicPad.cpp', [
 ('			const int32 NSlots = 4;\n', '			const int32 NSlots = 3; // AUTO / WOLF / ARC (b77: LAUNCH deleted)\n'),
 ('// b77: the WOLF slot = slot 2 of the classic mode bar (ZoogiAwakenWidget::BuildAim: AUTO / WOLF / LAUNCH / ARC)', '// b77: the WOLF slot = slot 2 of the classic mode bar (ZoogiAwakenWidget::BuildAim: AUTO / WOLF / ARC)'),
])
ed(P + 'ZoogiActionFun.cpp', [
 ('''	// b75 (Evan: "turn them back into their ball form, with the avatar icon above their marble on the field - for CPU, not the user")
	static const bool bNoCpuIcons = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCpuIcons"));
	if (!bNoCpuIcons && PC && !Clash.bOn && !Rep.bOn && G->Phase != EZoogiPhase::Menu && G->Phase != EZoogiPhase::GameOver)
		for (const AZoogiBall* Bc : G->Balls)''',
  '''	if (false)
		for (const AZoogiBall* Bc : G->Balls)'''),
 ('''void UZoogiFunSys::DrawHud(AHUD* HUD, UCanvas* C)
{
	if (!HUD || !C || HUD->GetWorld() != GetWorld()) return;''',
  '''// b75 (Evan: "turn them back into their ball form, with the avatar icon above their marble on the field - for CPU, not the user")
// b77 (Evan: "the avi icons above the CPU can't overlay anything - they should not block any text or menu"): drawn first on the main HUD
// canvas (under the UMG / Slate menus and every later canvas text), not on this post-render canvas that sits over everything
static bool GFunIconsOff = true;
void ZoogiFunCpuIcons(UCanvas* C, AZoogiGameMode* G, APlayerController* PC)
{
	static const bool bNoCpuIcons = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoCpuIcons"));
	if (bNoCpuIcons || GFunIconsOff || !C || !G || !PC || G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver) return;
	if (ZoogiSkills::Picking()) return;
	const float Wd = C->ClipX, Ht = C->ClipY, U = FMath::Min(Wd, Ht) / 1080.f;
	for (const AZoogiBall* Bc : G->Balls)
	{
		if (!Bc || !Bc->bCPU || Bc->bOut || Bc->IsHidden()) continue;
		FVector2D Sp;
		if (!PC->ProjectWorldLocationToScreen(Bc->BallLocation() + FVector(0.f, 0.f, ZoogiConst::ZoogiRadius + 75.f), Sp, true)) continue;
		if (Sp.X < -50.f || Sp.Y < -50.f || Sp.X > Wd + 50.f || Sp.Y > Ht + 50.f) continue;
		if (Sp.Y < Ht * 0.17f) continue; // never up in the profile / banner band
		const float Sz = 44.f * U * (Ht > Wd ? 1.25f : 1.f);
		const FLinearColor Pcol = ZoogiUtil::PlayerColor(Bc->PlayerIndex);
		FCanvasTileItem Back(Sp - FVector2D(Sz * 0.5f + 4.f * U, Sz * 0.5f + 4.f * U), FVector2D(Sz + 8.f * U, Sz + 8.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Back.BlendMode = SE_BLEND_Translucent; C->DrawItem(Back);
		FCanvasTileItem Edge(Sp - FVector2D(Sz * 0.5f + 2.f * U, Sz * 0.5f + 2.f * U), FVector2D(Sz + 4.f * U, Sz + 4.f * U), FLinearColor(0.f, 0.f, 0.f, 0.85f)); Edge.BlendMode = SE_BLEND_Translucent; C->DrawItem(Edge);
		if (UTexture2D* Tx = ZoogiP5::Portrait(Bc->Def.Name))
		{
			FCanvasTileItem Ic(Sp - FVector2D(Sz, Sz) * 0.5f, Tx->GetResource(), FVector2D(Sz, Sz), FLinearColor::White); Ic.BlendMode = SE_BLEND_Translucent; C->DrawItem(Ic);
		}
		FCanvasTileItem Tip(FVector2D(Sp.X - 3.f * U, Sp.Y + Sz * 0.5f + 4.f * U), FVector2D(6.f * U, 12.f * U), FLinearColor(Pcol.R, Pcol.G, Pcol.B, 0.9f)); Tip.BlendMode = SE_BLEND_Translucent; C->DrawItem(Tip);
	}
}

void UZoogiFunSys::DrawHud(AHUD* HUD, UCanvas* C)
{
	GFunIconsOff = true;
	if (!HUD || !C || HUD->GetWorld() != GetWorld()) return;'''),
 ('''	APlayerController* PC = HUD->GetOwningPlayerController();
	const float Wd = C->ClipX, Ht = C->ClipY, Mn = FMath::Min(Wd, Ht);
	const float U = Mn / 1080.f;

	if (Rep.bOn)''',
  '''	APlayerController* PC = HUD->GetOwningPlayerController();
	const float Wd = C->ClipX, Ht = C->ClipY, Mn = FMath::Min(Wd, Ht);
	const float U = Mn / 1080.f;
	GFunIconsOff = Clash.bOn || Rep.bOn; // b77: read by ZoogiFunCpuIcons on the next main HUD pass

	if (Rep.bOn)'''),
])
ed(P + 'ZoogiHUD.cpp', [
 ('''		UpdateAwakenWidget(G, true);
		return;
	}
	{
		const AZoogiPlayerController* PCFlash''',
  '''		UpdateAwakenWidget(G, true);
		return;
	}
	{ extern void ZoogiFunCpuIcons(UCanvas*, AZoogiGameMode*, APlayerController*); ZoogiFunCpuIcons(Canvas, G, PlayerOwner); } // b77: CPU avatar icons under everything else
	{
		const AZoogiPlayerController* PCFlash'''),
])
print('b77b ok')
