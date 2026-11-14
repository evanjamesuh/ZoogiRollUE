# b172c (Evan: "Remove the blood moon" -> "2. Arena weather"): Blizzard (Frozen), Eruption (Volcanic), Bloom (Meadow), Low Gravity (Cosmos)
import sys,os
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
f='Private/ZoogiWolfPack.cpp'; s=open(R+f,'rb').read().decode('utf-8')
def ed(old,new):
    global s
    for o,n in ((old.replace('\n','\r\n'),new.replace('\n','\r\n')),(old,new)):
        if s.count(o)==1: s=s.replace(o,n); print('OK',old[:60].strip()); return
    print('MISS',repr(old[:70])); sys.exit(1)
ed('#include "Fonts/FontMeasure.h"\n','#include "Fonts/FontMeasure.h"\n#include "ZoogiPhysics.h"\n#include "Components/StaticMeshComponent.h"\n')
ed('''void UZoogiWolfPackSys::ApplyBloodMoon(AZoogiGameMode* G)
{
	if (bBloodDone || !G || !G->Arena || G->bRoyaleMode || G->Phase == EZoogiPhase::Menu) return;
	bBloodDone = true;
	if (!ZoogiWolf::BloodMoon()) return;''','''// b172c arena weather (one per map): 0 none, 1 Blizzard, 2 Eruption, 3 Bloom, 4 Low Gravity
static int32 GWeather = 0, GWeatherRound = 0;
static float GWeatherBaseR = 0.f, GWeatherGravity = 0.f;
static TArray<TWeakObjectPtr<UStaticMeshComponent>> GWeatherProps;
static void WeatherClear(UWorld* W)
{
	if (GWeather == 2 && GWeatherBaseR > 0.f) AZoogiArena::ActiveKnockoffR = GWeatherBaseR;
	if (GWeather == 4 && W) if (AWorldSettings* WS = W->GetWorldSettings()) { WS->bGlobalGravitySet = GWeatherGravity != 0.f; WS->GlobalGravityZ = GWeatherGravity; }
	for (TWeakObjectPtr<UStaticMeshComponent>& C : GWeatherProps) if (C.IsValid()) C->DestroyComponent();
	GWeatherProps.Reset(); GWeather = 0; GWeatherRound = 0; GWeatherBaseR = 0.f;
}

void UZoogiWolfPackSys::ApplyBloodMoon(AZoogiGameMode* G)
{
	if (G && (G->Phase == EZoogiPhase::Menu || G->Phase == EZoogiPhase::GameOver)) { if (GWeather) WeatherClear(GetWorld()); if (G->Phase == EZoogiPhase::Menu) bBloodDone = false; } // b172c: next match rolls again
	if (G && GWeather == 2 && G->Arena && G->RoundNumber != GWeatherRound && G->Phase != EZoogiPhase::Menu)
	{	// ERUPTION: the edge crumbles in a little every new round (down to 80 %)
		const bool bNew = GWeatherRound > 0; GWeatherRound = G->RoundNumber;
		if (bNew)
		{
			AZoogiArena::ActiveKnockoffR = FMath::Max(GWeatherBaseR * 0.8f, AZoogiArena::ActiveKnockoffR * 0.94f);
			Toast(TEXT("THE EDGE CRUMBLES"), FString::Printf(TEXT("ERUPTION - THE RING IS %d%% OF ITS SIZE"), FMath::RoundToInt(100.f * AZoogiArena::ActiveKnockoffR / FMath::Max(1.f, GWeatherBaseR))), FLinearColor(1.f, 0.45f, 0.1f), 2.4f, 1.f);
			UE_LOG(LogZoogi, Log, TEXT("WEATHER eruption: round %d, knockout radius %.0f"), G->RoundNumber, AZoogiArena::ActiveKnockoffR);
		}
	}
	if (bBloodDone || !G || !G->Arena || G->bRoyaleMode || G->Phase == EZoogiPhase::Menu) return;
	bBloodDone = true;
	if (!ZoogiWolf::BloodMoon())
	{	// b172c: no Blood Moon - some matches get the arena's own weather instead (-ZoogiWeather = always, -ZoogiNoWeather = never)
		WeatherClear(GetWorld());
		static const bool bForce = FParse::Param(FCommandLine::Get(), TEXT("ZoogiWeather"));
		static const bool bNoWeather = FParse::Param(FCommandLine::Get(), TEXT("ZoogiNoWeather"));
		float Chance = 0.35f; FParse::Value(FCommandLine::Get(), TEXT("ZoogiWeatherChance="), Chance);
		if (bNoWeather || (!bForce && FMath::FRand() > Chance)) { UE_LOG(LogZoogi, Log, TEXT("WEATHER clear skies")); return; }
		switch (G->Arena->GetTheme())
		{
		case EZoogiArena::Frozen:
			GWeather = 1;
			for (AZoogiBall* Bx : G->Balls) if (Bx && Bx->Sphere) { Bx->Sphere->SetLinearDamping(0.004f); Bx->Sphere->SetAngularDamping(0.05f); }
			if (G->Arena->Fog) G->Arena->Fog->SetFogInscatteringColor(FLinearColor(0.75f, 0.85f, 1.f));
			Toast(TEXT("BLIZZARD"), TEXT("THE ICE IS SLICK - MARBLES SLIDE FURTHER"), FLinearColor(0.55f, 0.85f, 1.f), 3.f, 1.2f);
			break;
		case EZoogiArena::Volcanic:
			GWeather = 2; GWeatherBaseR = AZoogiArena::ActiveKnockoffR; GWeatherRound = G->RoundNumber;
			if (G->Arena->Sun) G->Arena->Sun->SetLightColor(FLinearColor(1.f, 0.6f, 0.4f));
			Toast(TEXT("ERUPTION"), TEXT("THE EDGE CRUMBLES IN EVERY ROUND"), FLinearColor(1.f, 0.45f, 0.1f), 3.f, 1.2f);
			break;
		case EZoogiArena::Meadow:
		{	// BLOOM: four flower bumpers pop up on the court (solid, bouncy)
			GWeather = 3;
			UStaticMesh* Sph = ZoogiUtil::BasicShape(TEXT("Sphere"));
			UMaterialInterface* Petal = ZoogiUtil::SurfaceMID(G->Arena, ZoogiUtil::Hex(TEXT("#F472B6")), 0.5f, ZoogiUtil::Hex(TEXT("#F472B6")), 0.35f);
			UPhysicalMaterial* Bouncy = ZoogiPhysics::MakeMaterial(G->Arena, 0.3f, 0.75f, false);
			const float Sc = G->Arena->GetActorScale3D().X > 0.f ? G->Arena->GetActorScale3D().X : 1.f;
			const float Rr = AZoogiArena::ActiveKnockoffR * 0.55f / Sc, Off = FMath::FRandRange(0.f, 90.f);
			for (int32 k = 0; k < 4 && Sph; ++k)
			{
				const float A = FMath::DegreesToRadians(Off + 45.f + k * 90.f);
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(G->Arena);
				C->SetStaticMesh(Sph); C->SetMobility(EComponentMobility::Movable); C->SetupAttachment(G->Arena->GetRootComponent());
				C->SetCollisionProfileName(TEXT("BlockAll")); C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				C->SetRelativeLocation(FVector(FMath::Cos(A) * Rr, FMath::Sin(A) * Rr, 40.f)); C->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.9f));
				C->RegisterComponent(); if (Petal) C->SetMaterial(0, Petal); if (Bouncy) C->SetPhysMaterialOverride(Bouncy);
				GWeatherProps.Add(C);
			}
			Toast(TEXT("BLOOM"), TEXT("FLOWER BUMPERS POP UP ON THE COURT"), FLinearColor(1.f, 0.45f, 0.75f), 3.f, 1.2f);
			break;
		}
		case EZoogiArena::Cosmic:
			if (AWorldSettings* WS = GetWorld()->GetWorldSettings())
			{
				GWeather = 4; GWeatherGravity = WS->bGlobalGravitySet ? WS->GlobalGravityZ : 0.f;
				const float Gz = WS->GetGravityZ(); WS->bGlobalGravitySet = true; WS->GlobalGravityZ = Gz * 0.55f;
			}
			Toast(TEXT("LOW GRAVITY"), TEXT("MARBLES FLOAT - BIG HITS FLY FURTHER"), FLinearColor(0.7f, 0.55f, 1.f), 3.f, 1.2f);
			break;
		default: break;
		}
		UE_LOG(LogZoogi, Log, TEXT("WEATHER %d on arena %d"), GWeather, (int32)G->Arena->GetTheme());
		return;
	}''')
open(R+f,'wb').write(s.encode('utf-8')); print('WROTE')
