# Nov 11 (Claude): B244 part 3 - DARK ARENA / SHOWTIME dim found no lights on Meadow ("DARK ARENA on (0 lights)": the arenas light with
# components, not ADirectionalLight actors) and the exposure bias did nothing with auto exposure off. Now every directional / sky light
# component in the world is dimmed and the post process tints the scene (works on every arena).
import io, os, sys
p = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private/ZoogiShowtime.cpp'); t = io.open(p, encoding='utf-8-sig').read()
def rep(a, b):
    global t
    if t.count(a) != 1: print('FAIL', t.count(a), a[:100]); sys.exit(1)
    t = t.replace(a, b)
rep('''			for (TActorIterator<ADirectionalLight> It(W); It; ++It) if (ULightComponent* L = It->GetLightComponent()) GLights.Add({ L, L->Intensity });
			GSkies.Reset();
			for (TActorIterator<ASkyLight> It(W); It; ++It) if (USkyLightComponent* L = It->GetLightComponent()) GSkies.Add({ L, L->Intensity });''',
'''			GSkies.Reset();
			for (TActorIterator<AActor> It(W); It; ++It)
			{	// b244: light COMPONENTS on any actor (the arenas build their lights as components)
				TInlineComponentArray<UDirectionalLightComponent*> Dl(*It); for (UDirectionalLightComponent* L : Dl) if (L) GLights.Add({ L, L->Intensity });
				TInlineComponentArray<USkyLightComponent*> Sl(*It); for (USkyLightComponent* L : Sl) if (L) GSkies.Add({ L, L->Intensity });
			}''')
rep('''				P->Settings.bOverride_AutoExposureBias = true; P->Settings.AutoExposureBias = -1.5f;''',
'''				P->Settings.bOverride_AutoExposureBias = true; P->Settings.AutoExposureBias = -1.5f;
				P->Settings.bOverride_SceneColorTint = true; P->Settings.SceneColorTint = FLinearColor(0.42f, 0.42f, 0.55f, 1.f); // b244: the dim shows even without auto exposure''')
rep('if (!GDarkOn || GDarkW.Get() != W || (GLights.Num() == 0 && GSkies.Num() == 0 && !GPP.IsValid()))',
    'static double NextTry = 0.0; const double Tn = FPlatformTime::Seconds();\n		if (!GDarkOn || GDarkW.Get() != W || !GPP.IsValid() || (GLights.Num() == 0 && Tn > NextTry))')
rep('{ DimLights(W, true); GDarkW = W; GDarkOn = true;', '{ if (GDarkOn) { SetDim(0.f); if (APostProcessVolume* Po = GPP.Get()) Po->Destroy(); } NextTry = Tn + 2.0; DimLights(W, true); GDarkW = W; GDarkOn = true;')
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ALL OK')
