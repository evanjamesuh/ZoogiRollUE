// Debug UI (Oct 14, Evan 12:41 AM CT): a minimal Canvas overlay showing each Zoogi's Episode Energy (bar out of the
// [ZoogiRoll.EpisodeEnergy] Cap), the current battle turn state, and a brief flashing SHATTER signal when the state
// machine enters SHATTER_FAILURE. OFF by default; only created with -ZoogiDebugUI (game / PIE worlds).
// READ-ONLY: reads UZoogiBattleStateMachine public getters, never writes game state. Drawn through AHUD::OnHUDPostRender
// (ZoogiHUD untouched).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiBattleTypes.h"
#include "ZoogiUIStyle.h"
#include "ZoogiDebugEpisodeWidget.generated.h"

class AHUD;
class UCanvas;

UCLASS()
class ZOOGIROLL_API UZoogiDebugEpisodeWidget : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	static bool IsEnabledByCommandLine();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	void OnHUDPostRender(AHUD* HUD, UCanvas* Canvas);

	FDelegateHandle hHUD;
	EZoogiBattleState LastState = EZoogiBattleState::None;
	int32 LastShatterCount = 0;
	double RealClock = 0.0;
	double ShatterFlashUntil = -1.0;
	bool bLoggedFirstDraw = false;
	TArray<ZoogiUIStyle::FBarJuice> EnergyJuice; // uistyle: per-player bar juice (display only)
};
