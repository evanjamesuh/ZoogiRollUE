// Nov 11 (Claude) - Evan: "the shooter modes ... nothing like the voyager demo ... I can't even hide behind cover or move like voyager".
// The SHOOTER cards now open REAL Voyager levels run by Voyager's own Blueprint framework (BP_GameModeDefault, the Voyager hero with
// cover / traverse / sprint / aim, its weapons and the AdvancedAI enemies) instead of the royale engine.
//   1 HOLD THE LINE  -> LevelWaveAssault      2 SHOOTER ROYALE -> LevelAdvancedAI      3 TEAM DEATHMATCH -> LevelCover (for now)
// Voyager maps get Voyager's gravity (-1800), Voyager's own main menu sends you back to Zoogi Roll, and a small EXIT button sits top-left.
// Flags: -ZoogiShooterOld = the b234 royale-engine shooter, -ZoogiVoyLevel=1|2|3 opens one at boot (capture runs),
// -ZoogiVoyShots=<dir> saves a screenshot every 2 s on Voyager maps, -ZoogiVoyMap=<LevelName> overrides the level.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZoogiVoyagerLevels.generated.h"

namespace ZoogiVoyLevels
{
	ZOOGIROLL_API bool On();
	ZOOGIROLL_API bool IsVoyagerMap(const UWorld* W);
	/** Kind 1..3 = the shooter card; returns false when the level is missing (then the old shooter runs) */
	ZOOGIROLL_API bool Open(UObject* Ctx, int32 Kind);
	ZOOGIROLL_API void BackToZoogi(UObject* Ctx);
}

UCLASS()
class ZOOGIROLL_API UZoogiVoyagerLevelsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	void OnPostLoadMap(UWorld* W);
	bool Tick(float Dt);
	FDelegateHandle LoadHandle;
	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<class SWidget> ExitWidget;
	TWeakObjectPtr<UWorld> VoyWorld;
	bool bBootDone = false;
	double NextShot = 0.0; int32 ShotN = 0;
};
