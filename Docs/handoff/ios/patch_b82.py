# b82 (Evan 09:31: "increase impact frame flash to 0.5 s"): the mobile canvas frame runs 0.5 s (was 0.26); PC Niagara frame slowed to match
import os
exec(open(os.path.expanduser('~/mnt/dev/ios/patch_b77.py')).read().split("P = 'Source/ZoogiRoll/Private/'")[0])
P = 'Source/ZoogiRoll/Private/'
ed(P + 'ZoogiFrozenFun.cpp', [
 ('''	static int32 GFrameSeed = 1;
	static void DrawMobileFrame(AHUD* HUD, UCanvas* Canvas)
	{
		if (!HUD || !Canvas || !HUD->GetWorld()) return;
		const double T = HUD->GetWorld()->GetRealTimeSeconds() - GFrameT0;
		if (T < 0.0 || T > 0.26) return;''',
  '''	static int32 GFrameSeed = 1;
	// b82 (Evan): the whole frame lasts 0.5 s (-ZoogiImpactFrameSec=N); the three beats keep their 0.07 / 0.07 / 0.12 proportions
	static double FrameSec() { static double S = -1.0; if (S < 0.0) { float F = 0.5f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiImpactFrameSec="), F); S = FMath::Clamp(F, 0.1f, 2.f); } return S; }
	static void DrawMobileFrame(AHUD* HUD, UCanvas* Canvas)
	{
		if (!HUD || !Canvas || !HUD->GetWorld()) return;
		const double T = (HUD->GetWorld()->GetRealTimeSeconds() - GFrameT0) * (0.26 / FrameSec()); // mapped onto the original 0.26 s timeline
		if (T < 0.0 || T > 0.26) return;'''),
 ('''		if (Now - GLastFrame < 0.3) return;''', '''		if (Now - GLastFrame < FMath::Max(0.3, FrameSec() + 0.05)) return; // b82: one full frame before the next'''),
 ('''		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, At, FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);''',
  '''		if (UNiagaraComponent* Nc = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, At, FRotator::ZeroRotator, FVector(1.f), true, true, ENCPoolMethod::AutoRelease))
			Nc->SetCustomTimeDilation((float)(0.26 / FrameSec())); // b82: same 0.5 s on PC'''),
])
print('b82 ok')
