// Nov 11 (Claude): the SHOOTER cards -> real Voyager levels. See ZoogiVoyagerLevels.h
#include "ZoogiVoyagerLevels.h"
#include "ZoogiRoll.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h" // b251
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Styling/CoreStyle.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "ZoogiApplePad.h"
#include "UObject/UnrealType.h"

namespace ZoogiVoyLevels
{
	static const TCHAR* GameMode = TEXT("/Game/Voyager/Blueprints/GameModes/ChildActors/BP_GameModeDefault.BP_GameModeDefault_C");
	bool On() { static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("ZoogiShooterOld")); return !bOld; }
	bool IsVoyagerMap(const UWorld* W) { return W && W->GetOutermost() && W->GetOutermost()->GetName().StartsWith(TEXT("/Game/Voyager/")); }
	static FString LevelFor(int32 Kind)
	{
		FString Ov; if (FParse::Value(FCommandLine::Get(), TEXT("ZoogiVoyMap="), Ov) && !Ov.IsEmpty()) return Ov;
		return Kind == 1 ? TEXT("LevelWaveAssault") : Kind == 2 ? TEXT("LevelAdvancedAI") : TEXT("LevelCover"); // City (MetaHumans) is too heavy for phones
	}
	static FString GWanted; static int32 GWantedKind = 0; // the level the SHOOTER card asked for
	bool Open(UObject* Ctx, int32 Kind)
	{
		if (!On() || !Ctx) return false;
		const FString Lv = LevelFor(Kind);
		const FString Pkg = TEXT("/Game/Voyager/Demo/Levels/") + Lv;
		if (!FPackageName::DoesPackageExist(Pkg)) { UE_LOG(LogZoogi, Warning, TEXT("VOYAGER level %s missing - old shooter"), *Pkg); return false; }
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER shooter %d -> %s"), Kind, *Pkg);
		GWanted = Pkg; GWantedKind = Kind;
		UGameplayStatics::OpenLevel(Ctx, FName(*Pkg), true, FString(TEXT("game=")) + GameMode);
		return true;
	}
	void BackToZoogi(UObject* Ctx)
	{
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER -> back to Zoogi Roll"));
		GWanted.Reset(); GWantedKind = 0;
		if (Ctx) UGameplayStatics::OpenLevel(Ctx, FName(TEXT("/Game/Maps/Meadow")), true, TEXT("game=/Script/ZoogiRoll.ZoogiGameMode"));
	}
}


void UZoogiVoyagerLevelsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Param(FCommandLine::Get(), TEXT("ZoogiKeepVoySaves")))
	{	// b251: launch-crash guard - Voyager's saved games are set aside at every start (Zoogi Roll's own saves stay)
		FString Report;
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
		TArray<FString> Files; IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.sav")), true, false);
		for (const FString& F : Files)
		{
			if (F.StartsWith(TEXT("Zoogi"), ESearchCase::IgnoreCase)) continue;
			const bool bOk = IFileManager::Get().Move(*(Dir / TEXT("VoyagerBackup") / F), *(Dir / F), true, true);
			Report += FString::Printf(TEXT("moved %s %s\n"), *F, bOk ? TEXT("ok") : TEXT("FAILED"));
		}
		if (UGameInstance* GI = GetGameInstance())
			if (FBoolProperty* Bp = CastField<FBoolProperty>(GI->GetClass()->FindPropertyByName(TEXT("LoadGameOnBeginPlay"))))
			{ Bp->SetPropertyValue_InContainer(GI, false); Report += TEXT("LoadGameOnBeginPlay off\n"); }
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER launch guard: %d save files seen\n%s"), Files.Num(), *Report);
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s launch guard, %d saves\n%s"), *FDateTime::Now().ToString(), Files.Num(), *Report), *(FPaths::ProjectSavedDir() / TEXT("ZoogiBoot.txt")));
	}
	LoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UZoogiVoyagerLevelsSubsystem::OnPostLoadMap);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UZoogiVoyagerLevelsSubsystem::Tick), 0.f);
	if (GEngine) GEngine->OnTravelFailure().AddWeakLambda(this, [](UWorld* W, ETravelFailure::Type, const FString& Err)
	{	// a Voyager level that is not in the build (e.g. its death flow asks for LevelCity): replay this match's level instead
		UE_LOG(LogZoogi, Warning, TEXT("VOYAGER travel failure: %s"), *Err);
		if (W && !ZoogiVoyLevels::GWanted.IsEmpty()) ZoogiVoyLevels::Open(W, ZoogiVoyLevels::GWantedKind);
	});
}

void UZoogiVoyagerLevelsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(LoadHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UZoogiVoyagerLevelsSubsystem::OnPostLoadMap(UWorld* W)
{
	if (!W || (W->WorldType != EWorldType::Game && W->WorldType != EWorldType::PIE)) return;
	ExitWidget.Reset();
	// Voyager's levels are lit for the extended luminance range (Voyager project setting); Zoogi's maps are not - switch per map
	static int32 ZoogiLum = -1;
	if (IConsoleVariable* Cv = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange")))
	{
		if (ZoogiLum < 0) ZoogiLum = Cv->GetInt();
		const int32 Want = ZoogiVoyLevels::IsVoyagerMap(W) ? 1 : ZoogiLum;
		if (Cv->GetInt() != Want) { Cv->Set(Want, ECVF_SetByCode); UE_LOG(LogZoogi, Log, TEXT("VOYAGER luminance range -> %d (now %d)"), Want, Cv->GetInt()); }
	}
	if (!ZoogiVoyLevels::IsVoyagerMap(W))
	{
		VoyWorld.Reset();
		int32 Boot = 0;
		if (!bBootDone && FParse::Value(FCommandLine::Get(), TEXT("ZoogiVoyLevel="), Boot) && Boot > 0) { bBootDone = true; ZoogiVoyLevels::Open(W, Boot); }
		return;
	}
	const FString Map = W->GetOutermost()->GetName();
	if (Map.EndsWith(TEXT("/LevelMainMenu"))) { ZoogiVoyLevels::BackToZoogi(W); return; } // Voyager's "main menu" = back to Zoogi Roll
	if (!ZoogiVoyLevels::GWanted.IsEmpty() && Map != ZoogiVoyLevels::GWanted)
	{	// Voyager's death / mission flow sends you to its own next level (LevelCity) - a Zoogi SHOOTER match restarts its own level
		UE_LOG(LogZoogi, Log, TEXT("VOYAGER %s is not this match's level - back to %s"), *Map, *ZoogiVoyLevels::GWanted);
		ZoogiVoyLevels::Open(W, ZoogiVoyLevels::GWantedKind); return;
	}
	VoyWorld = W;
	if (AWorldSettings* WS = W->GetWorldSettings())
	{	// Voyager is tuned for its project gravity (-1800); Zoogi's marbles keep -981
		WS->bGlobalGravitySet = true; WS->GlobalGravityZ = -1800.f; WS->bWorldGravitySet = false;
	}
	UE_LOG(LogZoogi, Log, TEXT("VOYAGER map up: %s, game mode %s"), *Map, W->GetAuthGameMode() ? *W->GetAuthGameMode()->GetClass()->GetName() : TEXT("none"));
	if (UGameViewportClient* Vp = W->GetGameViewport())
	{	// EXIT, top-left
		TWeakObjectPtr<UWorld> Ww = W;
		ExitWidget = SNew(SBox).HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(24.f, 18.f))
		[
			SNew(SButton).ButtonColorAndOpacity(FLinearColor(0.85f, 0.05f, 0.45f, 0.9f)).ContentPadding(FMargin(18.f, 8.f))
			.OnClicked_Lambda([Ww]() { if (UWorld* X = Ww.Get()) ZoogiVoyLevels::BackToZoogi(X); return FReply::Handled(); })
			[ SNew(STextBlock).Text(FText::FromString(TEXT("EXIT"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 20)).ColorAndOpacity(FLinearColor::White) ]
		];
		Vp->AddViewportWidgetContent(ExitWidget.ToSharedRef(), 1000);
	}
	NextShot = FPlatformTime::Seconds() + 2.0; ShotN = 0;
}

bool UZoogiVoyagerLevelsSubsystem::Tick(float Dt)
{
	ZoogiApplePad::WantFull(VoyWorld.IsValid()); // the Apple on-screen controller on Voyager maps (iOS)
	static FString Dir; static bool bRead = false;
	if (!bRead) { bRead = true; FParse::Value(FCommandLine::Get(), TEXT("ZoogiVoyShots="), Dir); }
	if (!Dir.IsEmpty() && VoyWorld.IsValid() && FPlatformTime::Seconds() >= NextShot)
	{
		NextShot = FPlatformTime::Seconds() + 2.0;
		const FString Map = FPaths::GetBaseFilename(VoyWorld->GetOutermost()->GetName());
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir, FString::Printf(TEXT("%s_%03d.png"), *Map, ++ShotN)), false, false);
	}
	return true;
}
