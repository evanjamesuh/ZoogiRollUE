// Oct 29 (Evan): ZOOGI ROLL - DUAL-MODE HOLOGRAPHIC SPIRE at the centre of every classic arena.
//  SCOREBOARD mode (default / CPU and opponent turns): a beam of light on a star emitter, rotating energy rings, chevrons rising
//    up the beam, a curved hologram drum with every Zoogi's portrait + live orb share (% of the field's orbs they have scored)
//    + bar, a ROUND n / N tab, two side cards (orbs left / leader) and a glowing orb with an orbiting star on top.
//  AIM & LAUNCH mode (the local player's own aim step): the drum collapses into the beam and re-forms as a ring of the other
//    Zoogis' portraits, the side cards retract, the orb sinks, and a big AIM & LAUNCH / DRAG TO LAUNCH panel builds in from
//    the beam (scan-line reveal) with an animated hologram hand pulling the ball back and a launch arrow.
//    It reverses as soon as the shot is launched.
// Everything is real 3D in the world (procedural quads / curved strips + additive hologram material M_ZAFX_Add with runtime
// canvas render targets), no collision, no gameplay effect; the front elements turn to face the camera. Mobile: textures are
// redrawn only when the match data changes (4 Hz for the small animated bars, 30 Hz for the aim panel while it shows).
// Toggles: [ZoogiRoll.HoloSpire] bSpire (default on), Scale; -ZoogiNoSpire. Test: -ZoogiSpireShots (with -ZoogiShots=dir).
#pragma once
class AZoogiOrb;
/** b86 (Evan): the spire emits the new orbs at the start of every round (glowing arcs from its top to each orb's spot). */
namespace ZoogiSpireOrbs
{
	ZOOGIROLL_API void Emit(AZoogiOrb* O, float Delay);
	ZOOGIROLL_API void Pulse();
	ZOOGIROLL_API bool Emitting();
	/** b87 (Evan: "new purple orbs appear to increase SP by 2, randomly provided by the spire - orb generators"): a purple SP orb;
	 *  the first Zoogi to touch it absorbs it (+2 Card Energy, ZoogiCardSubsystem::AddEnergyBonus). -ZoogiNoSPOrbs = none. */
	ZOOGIROLL_API void MarkSP(AZoogiOrb* O);
	ZOOGIROLL_API bool IsSP(const AZoogiOrb* O);
}
/** b89b (Evan: "critical hit animation play on the scoreboard"): true while a critical anime clip plays and a spire is up to show it
 *  on its scoreboard drum (the HUD then draws no card for it). -ZoogiCritTop = the old top card / P5 cut-in. */
class UWorld;
namespace ZoogiCritSpire
{
	ZOOGIROLL_API bool Enabled();
	ZOOGIROLL_API bool On(const UWorld* W);
}

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZoogiHoloSpire.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class UTexture2D;
class UCanvas;
class AZoogiGameMode;

UCLASS()
class ZOOGIROLL_API AZoogiHoloSpire : public AActor
{
	GENERATED_BODY()
public:
	AZoogiHoloSpire();
	virtual void Tick(float Dt) override;
	void Build(float InScale);
	/** Test: writes the hologram render targets as PNGs (-ZoogiSpireShots). */
	void ExportDebug(const FString& Dir);

	/** 0 = scoreboard, 1 = aim & launch (eased). */
	float Morph = 0.f;
	bool bWantAim = false;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USceneComponent> Front;      // yaws toward the camera
	UPROPERTY() TObjectPtr<USceneComponent> DrumPivot;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Quads;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> RTs;
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> Portraits;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Sheath;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TopOrb;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BeamMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SheathMID;

private:
	int32 AddPart(UProceduralMeshComponent* P, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow);
	TArray<int32> MatKind;     // 0 additive glow (M_ZAFX_Add), 1 colour panel (M_FrozenDecal), 2 FX fallback
	TArray<float> BaseGlow;
	UProceduralMeshComponent* MakeFlat(USceneComponent* Parent, float W, float H, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx);
	UProceduralMeshComponent* MakeDisc(USceneComponent* Parent, float D, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx);
	UProceduralMeshComponent* MakeArc(USceneComponent* Parent, float R, float ArcDeg, float H, UTextureRenderTarget2D* Tex, const FLinearColor& Tint, float Glow, int32& OutIdx);
	UTextureRenderTarget2D* NewRT(int32 W, int32 H);
	void SetGlow(int32 Idx, float Glow);
	void SetTex(int32 Idx, UTextureRenderTarget2D* Tex);
	UTexture2D* Portrait(FName Id);

	void DrawStatic();
	void DrawScore(AZoogiGameMode* G);
	void DrawCards(AZoogiGameMode* G, float T);
	void DrawBand(AZoogiGameMode* G);
	void DrawAim(float T, float Reveal);
	FString ScoreSig(AZoogiGameMode* G) const;

	float S = 1.f;
	float AimZ = 700.f;          // aim panel centre height (lowered to fit close cameras)
	float Time = 0.f, CardClock = 0.f, AimClock = 0.f, FaceYaw = 0.f;
	bool bBuilt = false, bFaceInit = false;
	FString LastSig, LastBandSig;
	bool bBandIsPortraits = false;
	// render targets
	UTextureRenderTarget2D* RT_Base = nullptr;
	UTextureRenderTarget2D* RT_Ring = nullptr;
	UTextureRenderTarget2D* RT_Chevron = nullptr;
	UTextureRenderTarget2D* RT_Star = nullptr;
	UTextureRenderTarget2D* RT_Score = nullptr;
	UTextureRenderTarget2D* RT_Band = nullptr;
	UTextureRenderTarget2D* RT_CardL = nullptr;
	UTextureRenderTarget2D* RT_CardR = nullptr;
	UTextureRenderTarget2D* RT_Aim = nullptr;
	UTextureRenderTarget2D* RT_ScoreTxt = nullptr;   // text layers (composited additively into the panels)
	UTextureRenderTarget2D* RT_CardLTxt = nullptr;
	UTextureRenderTarget2D* RT_CardRTxt = nullptr;
	UTextureRenderTarget2D* RT_AimTxt = nullptr;
	FString CardTxtSig;
	int32 ScoreRedraw = 0, CardTxtRedraw = 0, AimTextRedraw = 8;
	void DrawAimText();
	// quads (indices into Quads / Mids)
	int32 QBase = -1, QBase2 = -1, QDrum = -1, QCardL = -1, QCardR = -1, QAim = -1, QStar = -1;
	TArray<int32> QRings, QChev, QOrbRings;
	TArray<float> RingSpeed;
	USceneComponent* CardLPivot = nullptr;
	USceneComponent* CardRPivot = nullptr;
	USceneComponent* AimPivot = nullptr;
	TArray<USceneComponent*> OrbRingPivots;
	bool bCritTex = false; // b89b: the drum shows the critical clip
};

UCLASS()
class ZOOGIROLL_API UZoogiHoloSpireSys : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	void TickSpireOrbs(float Dt); // b86
	virtual TStatId GetStatId() const override;
	static bool Enabled();
	TWeakObjectPtr<AZoogiHoloSpire> Spire;
private:
	TWeakObjectPtr<AActor> BuiltFor;
	// -ZoogiSpireShots
	FString ShotDir;
	int32 ShotN = 0;
	bool bWasAim = false;
	double NextShot = 0.0, ExportAt = 0.0;
	bool bExported = false;
	TArray<TPair<FString, double>> ShotQ;
};
