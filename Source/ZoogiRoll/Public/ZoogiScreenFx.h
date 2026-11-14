// Oct 29 (Evan): screen-space moments.
//  * SHATTER: "on moments it says shattered i want a actual effect that cracks the screen in four pieces then shatters down".
//    Whenever a SHATTERED moment happens (battle segment SHATTERED signal, a rock SHATTERED! popup, a GUARD SHATTERED callout)
//    the frame is grabbed (one SceneCapture2D, final colour, half res, the live camera), jagged cracks race out from the middle
//    to the four screen edges (0.14 s), the frozen frame holds a beat in four cracked pieces, then the pieces fall away with
//    gravity and spin, revealing the live game behind them (~1.9 s). The HUD widgets stay on top.
//  * SHOCKWAVE: the Wolf Pack clone detonation (ZoogiClone.cpp). An expanding ground ring that bends the picture (refraction band
//    sampled from a per-frame half-res capture while a wave is alive) with a tinted rim. Works on iPhone without any Fab content;
//    on PC the Vefects Easy Shockwaves Niagara (AZoogiFX::Shockwave) plays with it.
// Visual only (no game writes, no gameplay randomness). Drawn through AHUD::OnHUDPostRender.
// Toggles: [ZoogiRoll.ScreenFx] bShatter / bShockwave (default on), -ZoogiNoScreenShatter, -ZoogiNoScreenWave.
// Review: -ZoogiScreenFxTest (with -ZoogiShots=<dir>) fires a shatter then a shockwave in the first match and requests
// engine-viewport screenshots (screenfx_##_<stage>.png) at fixed moments.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiScreenFx.generated.h"

class AHUD;
class UCanvas;
class UTextureRenderTarget2D;
class ASceneCapture2D;

UCLASS()
class ZOOGIROLL_API UZoogiScreenFx : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Cracks the screen into four pieces that fall away (2.5 s cooldown). Why goes to the log. */
	static void Shatter(UWorld* World, const TCHAR* Why);
	/** Ground shockwave at Center (world cm) growing to Radius: refraction ring + tinted rim. */
	static void Shockwave(UWorld* World, const FVector& Center, float Radius, const FLinearColor& Tint);

	int32 Shatters = 0, Waves = 0;

private:
	struct FPiece
	{
		TArray<FVector2D> Poly;      // capture-pixel space
		TArray<float> F;             // per vertex: fraction along its crack (0 centre .. 1 edge), -1 = screen corner
		FVector2D Anchor = FVector2D::ZeroVector;
		FVector2D Out = FVector2D::ZeroVector, V0 = FVector2D::ZeroVector;
		float Spin = 0.f, FallAt = 0.5f;
		struct FLine { FVector2D A, B; float F; };
		TArray<FLine> Lines;         // hairline branch cracks that ride on this piece
	};
	struct FWave { FVector C; float R; FLinearColor Tint; double Start; };

	void OnHUDPostRender(AHUD* HUD, UCanvas* Canvas);
	bool Capture(AHUD* HUD, TObjectPtr<UTextureRenderTarget2D>& RT, float Div);
	void BuildPieces(const FVector2D& Size);
	void DrawShatter(UCanvas* Cv, float t);
	void DrawWaves(AHUD* HUD, UCanvas* Cv, double Now);
	void TickTest(double Now);

	FDelegateHandle hHUD;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> ShatterRT;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> WaveRT;
	UPROPERTY() TObjectPtr<ASceneCapture2D> CapActor;

	bool bShatterOn = true, bWaveOn = true;
	bool bPendingShatter = false;
	FString PendingWhy;
	double ShatterStart = -100.0, LastShatter = -100.0;
	bool bShatterLive = false;
	int32 Serial = 0;
	FVector2D CapSize = FVector2D(1.f, 1.f);
	FVector2D CrackC = FVector2D::ZeroVector;
	TArray<FPiece> Pieces;
	TArray<FWave> WaveList;
	int32 SeenSm = -1;

	// -ZoogiScreenFxTest
	bool bTest = false;
	FString ShotDir;
	int32 TestStage = 0, ShotN = 0;
	double TestT0 = -1.0;
	TArray<TPair<FString, double>> ShotQ;
};
